// The MIT License (MIT)

// Copyright (c) 2016, Microsoft

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.


#include "BitFunnel/Exceptions.h"
#include "BitFunnel/Index/IRecycler.h"
#include "BitFunnel/Index/ISliceBufferAllocator.h"
#include "BitFunnel/Index/ITermTable.h"
#include "BitFunnel/Index/Row.h"
#include "BitFunnel/Index/RowIdSequence.h"
#include "BitFunnel/Index/Token.h"
#include "BitFunnel/Term.h"
#include "IRecyclable.h"
#include "LoggerInterfaces/Logging.h"
#include "Recycler.h"
#include "Shard.h"


namespace BitFunnel
{
    // Extracts a RowId used to mark documents as active/soft-deleted.
    static RowId RowIdForActiveDocument(ITermTable const & termTable)
    {
        RowIdSequence rows(termTable.GetDocumentActiveTerm(), termTable);

        auto it = rows.begin();
        if (it == rows.end())
        {
            RecoverableError error("RowIdForDeletedDocument: expected at least one row.");
            throw error;
        }
        const RowId rowId = *it;

        if (rowId.GetRank() != 0)
        {
            RecoverableError error("RowIdForDeletedDocument: soft deleted row must be rank 0..");
            throw error;
        }

        ++it;
        if (it != rows.end())
        {
            RecoverableError error("RowIdForDeletedDocument: expected no more than one row.");
            throw error;

        }

        return rowId;
    }


    Shard::Shard(IRecycler& recycler,
                 ITokenManager& tokenManager,
                 ITermTable const & termTable,
                 IDocumentDataSchema const & docDataSchema,
                 ISliceBufferAllocator& sliceBufferAllocator,
                 size_t sliceBufferSize)
        : m_recycler(recycler),
          m_tokenManager(tokenManager),
          m_termTable(termTable),
          m_sliceBufferAllocator(sliceBufferAllocator),
          m_documentActiveRowId(RowIdForActiveDocument(termTable)),
          m_activeSlice(nullptr),
          m_sliceBuffers(new std::vector<void*>()),
          m_sliceCapacity(GetCapacityForByteSize(sliceBufferSize,
                                                 docDataSchema,
                                                 termTable)),
          m_sliceBufferSize(sliceBufferSize),
          // TODO: will need one global, not one per shard.
          m_docFrequencyTableBuilder(new DocumentFrequencyTableBuilder())
    {
        const size_t bufferSize =
            InitializeDescriptors(this,
                                  m_sliceCapacity,
                                  docDataSchema,
                                  termTable);

        LogAssertB(bufferSize <= sliceBufferSize,
                   "Shard sliceBufferSize too small.");
    }


    Shard::~Shard() {
        delete static_cast<std::vector<void*>*>(m_sliceBuffers);
    }


    DocumentHandleInternal Shard::AllocateDocument(DocId id)
    {
        std::lock_guard<std::mutex> lock(m_slicesLock);
        DocIndex index;
        if (m_activeSlice == nullptr || !m_activeSlice->TryAllocateDocument(index))
        {
            CreateNewActiveSlice();

            LogAssertB(m_activeSlice->TryAllocateDocument(index),
                       "Newly allocated slice has no space.");
        }

        return DocumentHandleInternal(m_activeSlice, index, id);
    }


    void* Shard::AllocateSliceBuffer()
    {
        return m_sliceBufferAllocator.Allocate(m_sliceBufferSize);
    }

    // Must be called with m_slicesLock held.
    void Shard::CreateNewActiveSlice()
    {
        Slice* newSlice = new Slice(*this);

        std::vector<void*>* oldSlices = m_sliceBuffers;
        std::vector<void*>* const newSlices = new std::vector<void*>(*m_sliceBuffers);
        newSlices->push_back(newSlice->GetSliceBuffer());

        m_sliceBuffers = newSlices;
        m_activeSlice = newSlice;

        // TODO: think if this can be done outside of the lock.
        std::unique_ptr<IRecyclable>
            recyclableSliceList(new DeferredSliceListDelete(nullptr,
                                                            oldSlices,
                                                            m_tokenManager));

        m_recycler.ScheduleRecyling(recyclableSliceList);
    }


    /* static */
    DocIndex Shard::GetCapacityForByteSize(size_t bufferSizeInBytes,
                                           IDocumentDataSchema const & schema,
                                           ITermTable const & termTable)
    {
        DocIndex capacity = 0;
        for (;;)
        {
            const DocIndex newSuggestedCapacity = capacity +
                Row::DocumentsInRank0Row(1, termTable.GetMaxRankUsed());
            const size_t newBufferSize =
                InitializeDescriptors(nullptr,
                                      newSuggestedCapacity,
                                      schema,
                                      termTable);
            if (newBufferSize > bufferSizeInBytes)
            {
                break;
            }

            capacity = newSuggestedCapacity;
        }

        LogAssertB(capacity > 0, "Shard with 0 capacity.");

        return capacity;
    }


    DocTableDescriptor const & Shard::GetDocTable() const
    {
        return *m_docTable;
    }


    ptrdiff_t Shard::GetRowOffset(RowId rowId) const
    {
        // LogAssertB(rowId.IsValid(), "GetRowOffset on invalid row.");

        return GetRowTable(rowId.GetRank()).GetRowOffset(rowId.GetIndex());
    }


    RowTableDescriptor const & Shard::GetRowTable(Rank rank) const
    {
        return m_rowTables.at(rank);
    }


    std::vector<void*> const & Shard::GetSliceBuffers() const
    {
        return *m_sliceBuffers;
    }


    DocIndex Shard::GetSliceCapacity() const
    {
        return  m_sliceCapacity;
    }


    ptrdiff_t Shard::GetSlicePtrOffset() const
    {
        // A pointer to a Slice is placed in the end of the slice buffer.
        return m_sliceBufferSize - sizeof(void*);
    }


    RowId Shard::GetDocumentActiveRowId() const
    {
        return m_documentActiveRowId;
    }


    ITermTable const & Shard::GetTermTable() const
    {
        return m_termTable;
    }


    size_t Shard::GetUsedCapacityInBytes() const
    {
        // TODO: does this really need to be locked?
        std::lock_guard<std::mutex> lock(m_slicesLock);
        return m_sliceBuffers.load()->size() * m_sliceBufferSize;
    }


    /* static */
    size_t Shard::InitializeDescriptors(Shard* shard,
                                        DocIndex sliceCapacity,
                                        IDocumentDataSchema const & docDataSchema,
                                        ITermTable const & termTable)
    {
        ptrdiff_t currentOffset = 0;

        // Start of the DocTable is at offset 0.
        if (shard != nullptr)
        {
            shard->m_docTable.reset(new DocTableDescriptor(sliceCapacity,
                                                           docDataSchema,
                                                           currentOffset));
        }

        currentOffset += DocTableDescriptor::GetBufferSize(sliceCapacity, docDataSchema);

        for (Rank r = 0; r <= c_maxRankValue; ++r)
        {
            // TODO: see if this alignment matters.
            // currentOffset = RoundUp(currentOffset, c_rowTableByteAlignment);

            const RowIndex rowCount = termTable.GetTotalRowCount(r);

            if (shard != nullptr)
            {
                shard->m_rowTables.emplace_back(sliceCapacity, rowCount, r, currentOffset);
            }

            currentOffset += RowTableDescriptor::GetBufferSize(sliceCapacity, rowCount, r);
        }

        // A pointer to a Slice is placed at the end of the slice buffer.
        currentOffset += sizeof(void*);

        const size_t sliceBufferSize = static_cast<size_t>(currentOffset);

        return sliceBufferSize;
    }


    void Shard::RecycleSlice(Slice& slice)
    {
        std::vector<void*>* oldSlices = nullptr;
        size_t newSliceCount;

        {
            std::lock_guard<std::mutex> lock(m_slicesLock);

            if (!slice.IsExpired())
            {
                throw RecoverableError("Slice being recycled has not been fully expired");
            }

            std::vector<void*>* const newSlices = new std::vector<void*>();
            newSlices->reserve(m_sliceBuffers.load()->size() - 1);

            for (const auto it : *m_sliceBuffers)
            {
                if (it != slice.GetSliceBuffer())
                {
                    newSlices->push_back(it);
                }
            }

            if (m_sliceBuffers.load()->size() != newSlices->size() + 1)
            {
                throw RecoverableError("Slice buffer to be removed is not found in the active slice buffers list");
            }

            newSliceCount = newSlices->size();

            oldSlices = m_sliceBuffers.load();
            m_sliceBuffers = newSlices;

            if (m_activeSlice == &slice)
            {
                // If all of the above validations are true, then this was the
                // last Slice in the Shard.
                m_activeSlice = nullptr;
            }
        }

        // Scheduling the Slice and the old list of slice buffers can be
        // done outside of the lock.
        std::unique_ptr<IRecyclable>
            recyclableSliceList(new DeferredSliceListDelete(&slice,
                                                            oldSlices,
                                                            m_tokenManager));

        m_recycler.ScheduleRecyling(recyclableSliceList);
    }


    void Shard::ReleaseSliceBuffer(void* sliceBuffer)
    {
        m_sliceBufferAllocator.Release(sliceBuffer);
    }


    void Shard::AddPosting(Term const & term,
                           DocIndex index,
                           void* sliceBuffer)
    {
        if (m_docFrequencyTableBuilder.get() != nullptr)
        {
            std::lock_guard<std::mutex> lock(m_temporaryFrequencyTableMutex);
            m_docFrequencyTableBuilder->OnTerm(term);
        }


        RowIdSequence rows(term, m_termTable);

        for (auto const row : rows)
        {
            m_rowTables[row.GetRank()].SetBit(sliceBuffer,
                                              row.GetIndex(),
                                              index);
        }
    }


    void Shard::AssertFact(FactHandle fact, bool value, DocIndex index, void* sliceBuffer)
    {
        Term term(fact, 0u, 0u, 1u);
        RowIdSequence rows(term, m_termTable);
        auto it = rows.begin();

        if (it == rows.end())
        {
            RecoverableError error("Shard::AssertFact: expected at least one row.");
            throw error;
        }

        const RowId row = *it;

        ++it;
        if (it != rows.end())
        {
            RecoverableError error("Shard::AssertFact: expected no more than one row.");
            throw error;

        }

        RowTableDescriptor const & rowTable =
            m_rowTables[row.GetRank()];

        if (value)
        {
            rowTable.SetBit(sliceBuffer,
                            row.GetIndex(),
                            index);
        }
        else
        {
            rowTable.ClearBit(sliceBuffer,
                              row.GetIndex(),
                              index);
        }
    }


    void Shard::TemporaryRecordDocument()
    {
        m_docFrequencyTableBuilder->OnDocumentEnter();
    }


    void Shard::TemporaryWriteDocumentFrequencyTable(std::ostream& out,
                                                     TermToText const * termToText) const
    {
        // TODO: 0.0 is the truncation frequency, which shouldn't be fixed at 0.
        m_docFrequencyTableBuilder->WriteFrequencies(out, 0.0, termToText);
    }


    void Shard::TemporaryWriteIndexedIdfTable(std::ostream& out) const
    {
        // TODO: 0.0 is the truncation frequency, which shouldn't be fixed at 0.
        m_docFrequencyTableBuilder->WriteIndexedIdfTable(out, 0.0);
    }


    void Shard::TemporaryWriteCumulativeTermCounts(std::ostream& out) const
    {
        m_docFrequencyTableBuilder->WriteCumulativeTermCounts(out);
    }
}
