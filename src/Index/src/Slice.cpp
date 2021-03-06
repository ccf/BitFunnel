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


#include "LoggerInterfaces/Logging.h"
#include "Shard.h"
#include "Slice.h"


namespace BitFunnel
{
    Slice::Slice(Shard& shard)
        : m_shard(shard),
          m_temporaryNextDocIndex(0U),
          m_capacity(shard.GetSliceCapacity()),
          m_refCount(1),
          m_buffer(shard.AllocateSliceBuffer()),
          m_unallocatedCount(shard.GetSliceCapacity()),
          m_commitPendingCount(0),
          m_expiredCount(0)
    {
        Initialize();

        // Perform start up initialization of the DocTable and RowTables after
        // the buffer has been allocated.
        GetDocTable().Initialize(m_buffer);
        for (Rank r = 0; r <= c_maxRankValue; ++r)
        {
            GetRowTable(r).Initialize(m_buffer, m_shard.GetTermTable());
        }
    }


    Slice::~Slice()
    {
        try
        {
            GetDocTable().Cleanup(m_buffer);
            m_shard.ReleaseSliceBuffer(m_buffer);
        }
        catch (...)
        {
            LogB(Logging::Error, "Slice", "Exception caught in Slice::~Slice()","");
        }
    }


    Shard& Slice::GetShard() const
    {
        return m_shard;
    }


    bool Slice::CommitDocument()
    {
        std::lock_guard<std::mutex> lock(m_docIndexLock);
        GetShard().TemporaryRecordDocument();

        LogAssertB(m_commitPendingCount > 0,
                   "CommitDocument with m_commitPendingCount == 0");

        --m_commitPendingCount;

        return (m_unallocatedCount + m_commitPendingCount) == 0;
    }


    /* static */
    void Slice::DecrementRefCount(Slice* slice)
    {
        const unsigned newRefCount = --(slice->m_refCount);
        if (newRefCount == 0)
        {
            slice->GetShard().RecycleSlice(*slice);
        }
    }


    bool Slice::ExpireDocument()
    {
        std::lock_guard<std::mutex> lock(m_docIndexLock);

        // Cannot expire more than what was committed.
        const DocIndex committedCount =
            m_capacity - m_unallocatedCount - m_commitPendingCount;
        LogAssertB(m_expiredCount < committedCount,
                   "Slice expired more documents than committed.");

        m_expiredCount++;

        return m_expiredCount == m_capacity;
    }


    DocTableDescriptor const & Slice::GetDocTable() const
    {
        return m_shard.GetDocTable();
    }


    RowTableDescriptor const & Slice::GetRowTable(Rank rank) const
    {
        return m_shard.GetRowTable(rank);
    }


    void* Slice::GetSliceBuffer() const
    {
        return m_buffer;
    }


    /* static */
    Slice* Slice::GetSliceFromBuffer(void* sliceBuffer, ptrdiff_t slicePtrOffset)
    {
        return Slice::GetSlicePointer(sliceBuffer, slicePtrOffset);
    }


    // We have GetSlicePointer, which is private, so that the constructor can
    // get a reference and modify the pointer.
    /* static */
    Slice*& Slice::GetSlicePointer(void* sliceBuffer, ptrdiff_t slicePtrOffset)
    {
        char* slicePtr = reinterpret_cast<char*>(sliceBuffer) + slicePtrOffset;
        return *reinterpret_cast<Slice**>(slicePtr);
    }


    /* static */
    void Slice::IncrementRefCount(Slice* slice)
    {
        ++(slice->m_refCount);
    }


    void Slice::Initialize()
    {
        // Place a pointer to a Slice in the last bytes of the SliceBuffer.
        Slice*& slicePtr = GetSlicePointer(m_buffer, m_shard.GetSlicePtrOffset());
        slicePtr = this;
    }


    bool Slice::IsExpired() const
    {
        return m_expiredCount == m_capacity;
    }


    bool Slice::TryAllocateDocument(size_t& index)
    {
        std::lock_guard<std::mutex> lock(m_docIndexLock);

        if (m_unallocatedCount == 0)
        {
            return false;
        }

        index = m_capacity - m_unallocatedCount;
        m_unallocatedCount--;
        m_commitPendingCount++;

        return true;
    }
}
