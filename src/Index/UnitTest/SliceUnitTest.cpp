#include <unordered_set>

// #include "BitFunnel/Factories.h"
// #include "BitFunnel/Row.h"
// #include "BitFunnel/TermInfo.h"
// #include "DocumentDataSchema.h"
// #include "EmptyTermTable.h"
// #include "ExpectException.h"
// #include "IndexWrapper.h"
// #include "LoggerInterfaces/Logging.h"
// #include "Random.h"
// #include "Slice.h"
// #include "Shard.h"
// #include "ThrowingLogger.h"
// #include "TrackingSliceBufferAllocator.h"

#include "gtest/gtest.h"

#include "BitFunnel/Index/Factories.h"
#include "BitFunnel/Index/IIngestor.h"
#include "Mocks/EmptyTermTable.h"
#include "BitFunnel/ITermTable.h"
#include "Ingestor.h"
#include "ISliceBufferAllocator.h"
#include "Shard.h"
#include "Slice.h"
#include "TrackingSliceBufferAllocator.h"

namespace BitFunnel
{
    namespace SliceUnitTest
    {

        TEST(SliceAllocateCommitExpire, Trivial)
        {
            /*
            std::unique_ptr<IIngestor> ingestor(Factories::CreateIngestor());
            Shard* shard = new Shard(*ingestor, 0u);
            throw shard;
            */

            // pSlice* slice = Slice(nullptr);

            // static const size_t c_systemRowCount = 3; // TODO: why is this 3?

            // 1 row reserved for soft-deleted row.
            // TODO: what does the above comment mean?
            // static const std::vector<size_t> rowCounts = { c_systemRowCount, 0, 0, 0, 0, 0, 0 };
            // std::shared_ptr<ITermTable const> termTable(new EmptyTermTable(rowCounts));

            // TODO: figure out what this should be.
            static const size_t sliceBufferSize = 32;
            std::unique_ptr<ISliceBufferAllocator> trackingAllocator(
                new TrackingSliceBufferAllocator(sliceBufferSize));

            // TODO: use unique_ptr.
            std::unique_ptr<IIngestor> ingestor(
                Factories::CreateIngestor(*trackingAllocator));
            Shard& shard = ingestor->GetShard(0);

            static const size_t c_sliceCapacity = 16;

            // Basic tests - allocate, commit, expire.
            {
                Slice slice(shard);
                EXPECT_EQ(shard.GetSliceCapacity(), c_sliceCapacity);
                EXPECT_FALSE(slice.IsExpired());


                std::unordered_set<DocIndex> allocatedDocIndexes; // TODO: what is this set for?
                for (DocIndex i = 0; i < c_sliceCapacity; ++i)
                {
                    DocIndex index;
                    ASSERT_TRUE(slice.TryAllocateDocument(index));
                    auto p = allocatedDocIndexes.insert(index);
                    EXPECT_TRUE(p.second);
                    EXPECT_LT(index, c_sliceCapacity);

                    EXPECT_TRUE(!slice.IsExpired());
                }

                // All indices allocated.
                {
                    DocIndex index;
                    EXPECT_FALSE(slice.TryAllocateDocument(index));
                }

                // Commit DocIndex'es - can commit in any order.
                for (DocIndex i = 0; i < c_sliceCapacity; ++i)
                {
                    const bool isSliceFull = slice.CommitDocument();

                    // Slice is full when all docIndex'es are allocated and committed.
                    const bool isSliceExpectedToBeFull = i == c_sliceCapacity - 1;
                    EXPECT_EQ(isSliceFull, isSliceExpectedToBeFull);

                    EXPECT_FALSE(slice.IsExpired());
                }

            }

            // TODO: figure out why the exceptions below cause this test to die.
            // An exception correctly occurs inside the EXPECT_ANY_THROW, however
            // that exception kills the test and causes the test to fail!
            // // Test boundary conditions and error cases.
            // {
            //     Slice slice(*shard);
            //     EXPECT_EQ(shard->GetSliceCapacity(), c_sliceCapacity);

            //     DocIndex index;
            //     EXPECT_TRUE(slice.TryAllocateDocument(index));

            //     // TODO: expect specific exception.
            //     EXPECT_ANY_THROW(slice.ExpireDocument());

            //     // Commit and now we can expire.
            //     EXPECT_FALSE(slice.CommitDocument());
            //     EXPECT_FALSE(slice.ExpireDocument());

            //     // But not more than that was committed.
            //     // TODO: expect specific exception.
            //     EXPECT_ANY_THROW(slice.ExpireDocument());

            //     EXPECT_TRUE(slice.TryAllocateDocument(index));
            //     EXPECT_FALSE(slice.CommitDocument());

            //     // Cannot commit more than what was allocated.
            //     // TODO: expect specific exception.
            //     EXPECT_ANY_THROW(slice.CommitDocument());

            //     EXPECT_FALSE(slice.ExpireDocument());
            // }


            // DocumentDataSchema schema;
            // static const size_t c_blockAllocatorBlockCount = 32; // TODO: what should this value be?
            // IndexWrapper index(c_sliceCapacity, termTable, schema, c_blockAllocatorBlockCount);
            // Shard& shard = index.GetShard();
        }


        Slice* FillUpAndExpireSlice(Shard& shard, DocIndex sliceCapacity)
        {
            Slice* firstSlice = nullptr;
            for (DocIndex i = 0; i < sliceCapacity; ++i)
            {
                const DocumentHandleInternal handle = shard.AllocateDocument();
                if (i == 0)
                {
                    // Saving the value of the Slice* for subsequent comparison.
                    firstSlice = handle.GetSlice();
                    EXPECT_NE(firstSlice, nullptr);
                }

                // Make sure we are in the same Slice.
                EXPECT_EQ(firstSlice, handle.GetSlice());

                firstSlice->CommitDocument();
                firstSlice->ExpireDocument();
            }

            return firstSlice;
        }


        TEST(RefCountTest, Trivial)
        {
            // static const DocIndex c_sliceCapacity = Row::DocumentsInRank0Row(1);
            static const size_t c_sliceCapacity = 16;

            //const size_t sliceBufferSize = GetBufferSize(c_sliceCapacity, rowCounts, schema);
            static const size_t sliceBufferSize = 1024;

            std::unique_ptr<ISliceBufferAllocator> sliceBufferAllocator(
                new TrackingSliceBufferAllocator(sliceBufferSize));
            TrackingSliceBufferAllocator& trackingAllocator
                = dynamic_cast<TrackingSliceBufferAllocator&>(*sliceBufferAllocator);

            std::unique_ptr<IIngestor> ingestor(
                Factories::CreateIngestor(*sliceBufferAllocator));
            std::unique_ptr<Shard> shard = std::unique_ptr<Shard>(new Shard(*ingestor, 0u, *sliceBufferAllocator, sliceBufferAllocator->GetSliceBufferSize()));

            // TODO: determine why 0 is expected here.
            EXPECT_EQ(trackingAllocator.GetInUseBuffersCount(), 0U);

            {
                Slice* const slice = FillUpAndExpireSlice(*shard, c_sliceCapacity);
                EXPECT_EQ(trackingAllocator.GetInUseBuffersCount(), 1U);

                Slice::DecrementRefCount(slice);
                // TODO: implement Recycler.
                // EXPECT_EQ(trackingAllocator.GetInUseBuffersCount(), 0u);
            }

            {
                Slice * const slice = FillUpAndExpireSlice(*shard, c_sliceCapacity);
                // TODO: implement Recycler.
                // EXPECT_EQ(trackingAllocator.GetInUseBuffersCount(), 1U);

                // Simulate another reference holder of the slice, such as backup writer.
                Slice::IncrementRefCount(slice);

                // // The slice should not be recycled since there are 2 reference holders.
                // EXPECT_EQ(trackingAllocator.GetInUseBuffersCount(), 1u);

                // // Decrement the ref count, at this point there should be 1 ref count and the
                // // Slice must not be recycled.
                // Slice::DecrementRefCount(slice);

                // // Slice should still be alive.
                // EXPECT_EQ(trackingAllocator.GetInUseBuffersCount(), 1u);

                // // Decrement the last ref count, Slice should be scheduled for recycling.
                // Slice::DecrementRefCount(slice);
                // EXPECT_EQ(trackingAllocator.GetInUseBuffersCount(), 0u);
            }
        }


        /*
        size_t GetBufferSize(DocIndex capacity,
                             std::vector<RowIndex> const & rowCounts,
                             IDocumentDataSchema const & schema)
        {
            EmptyTermTable termTable(rowCounts);
            return Shard::InitializeDescriptors(nullptr, capacity, schema, termTable);
        }


        TEST(BufferSizeTest, Trivial)
        {
            static const DocIndex c_capacityQuanta = 4096;
            static const size_t c_sizeOfSlicePtr = sizeof(void*);
            DocumentDataSchema schema;
            schema.RegisterVariableSizeBlob();
            schema.RegisterFixedSizeBlob(10);

            {
                // When there are no rows, buffer size is driven only by DocTable.
                const size_t expectedBufferSize = DocTableDescriptor::GetBufferSize(c_capacityQuanta * 1, schema) + c_sizeOfSlicePtr;
                EXPECT_EQ(GetBufferSize(c_capacityQuanta * 1, { 0, 0, 0, 0, 0, 0, 0 }, schema), expectedBufferSize);
            }

            {
                // 1 row at rank 0.
                const size_t expectedBufferSize = DocTableDescriptor::GetBufferSize(c_capacityQuanta * 1, schema) + 4096 / 8 + c_sizeOfSlicePtr;
                EXPECT_EQ(GetBufferSize(c_capacityQuanta * 1, { 1, 0, 0, 0, 0, 0, 0 }, schema), expectedBufferSize);
            }

            {
                // 10 row at rank 0.
                const size_t expectedBufferSize = DocTableDescriptor::GetBufferSize(c_capacityQuanta * 1, schema) + 4096 / 8 * 10 + c_sizeOfSlicePtr;
                EXPECT_EQ(GetBufferSize(c_capacityQuanta * 1, { 10, 0, 0, 0, 0, 0, 0 }, schema), expectedBufferSize);
            }

            {
                // 1 row at rank 0, 10 rows at rank 3.
                const size_t expectedBufferSize = DocTableDescriptor::GetBufferSize(c_capacityQuanta * 1, schema) + 4096 / 8 + 4096 / 8 / 8 * 10 + c_sizeOfSlicePtr;
                EXPECT_EQ(GetBufferSize(c_capacityQuanta * 1, { 1, 0, 0, 10, 0, 0, 0 }, schema), expectedBufferSize);
            }

            {
                // 20 rows at rank 6.
                const size_t expectedBufferSize = DocTableDescriptor::GetBufferSize(c_capacityQuanta * 2, schema) + 2 * 4096 / 8 / 64 * 20 + c_sizeOfSlicePtr;
                EXPECT_EQ(GetBufferSize(c_capacityQuanta * 2, { 0, 0, 0, 0, 0, 0, 20 }, schema), expectedBufferSize);
            }
        }

        const size_t c_blockAllocatorBlockCount = 10;


        Slice* FillUpAndExpireSlice(Shard& shard, DocIndex sliceCapacity)
        {
            Slice* firstSlice = nullptr;
            for (DocIndex i = 0; i < sliceCapacity; ++i)
            {
                const DocumentHandleInternal handle = shard.AllocateDocument();
                if (i == 0)
                {
                    // Saving the value of the Slice* for subsequent comparison.
                    firstSlice = handle.GetSlice();
                }

                // Make sure we are in the same Slice.
                EXPECT_EQ(firstSlice, handle.GetSlice());

                firstSlice->CommitDocument(handle.GetIndex());
                firstSlice->ExpireDocument(handle.GetIndex());
            }

            return firstSlice;
        }


        TEST(RefCountTest, Trivial)
        {
            static const DocIndex c_sliceCapacity = Row::DocumentsInRank0Row(1);

            static const std::vector<RowIndex> rowCounts = { 100, 0, 0, 200, 0, 0, 300 };
            std::shared_ptr<ITermTable const> termTable(new EmptyTermTable(rowCounts));

            DocumentDataSchema schema;

            const size_t sliceBufferSize = GetBufferSize(c_sliceCapacity, rowCounts, schema);
            std::unique_ptr<ISliceBufferAllocator> sliceBufferAllocator(
                new TrackingSliceBufferAllocator(sliceBufferSize));
            TrackingSliceBufferAllocator& trackingAllocator
                = dynamic_cast<TrackingSliceBufferAllocator&>(*sliceBufferAllocator);

            IndexWrapper index(c_sliceCapacity, termTable, schema, sliceBufferAllocator);
            Shard& shard = index.GetShard();

            EXPECT_EQ(trackingAllocator.GetInUseBuffersCount(), 0);

            {
                Slice* const slice = FillUpAndExpireSlice(shard, c_sliceCapacity);
                EXPECT_EQ(trackingAllocator.GetInUseBuffersCount(), 1);

                Slice::DecrementRefCount(slice);
                EXPECT_EQ(trackingAllocator.GetInUseBuffersCount(), 0);
            }

            {
                Slice * const slice = FillUpAndExpireSlice(shard, c_sliceCapacity);
                EXPECT_EQ(trackingAllocator.GetInUseBuffersCount(), 1);

                // Simulate another reference holder of the slice, such as backup writer.
                Slice::IncrementRefCount(slice);

                // The slice should not be recycled since there are 2 reference holders.
                EXPECT_EQ(trackingAllocator.GetInUseBuffersCount(), 1);

                // Decrement the ref count, at this point there should be 1 ref count and the
                // Slice must not be recycled.
                Slice::DecrementRefCount(slice);

                // Slice should still be alive.
                EXPECT_EQ(trackingAllocator.GetInUseBuffersCount(), 1);

                // Decrement the last ref count, Slice should be scheduled for recycling.
                Slice::DecrementRefCount(slice);
                EXPECT_EQ(trackingAllocator.GetInUseBuffersCount(), 0);
            }
        }


        TEST(BasicIntegration, Trivial)
        {
            static const DocIndex c_sliceCapacity = Row::DocumentsInRank0Row(1);

            static const std::vector<RowIndex> rowCounts = { c_systemRowCount, 0, 0, 1, 0, 0, 1 };
            std::shared_ptr<ITermTable const> termTable(new EmptyTermTable(rowCounts));

            DocumentDataSchema schema;
            const VariableSizeBlobId varBlobId0 = schema.RegisterVariableSizeBlob();

            IndexWrapper index(c_sliceCapacity, termTable, schema, c_blockAllocatorBlockCount);
            Shard& shard = index.GetShard();

            Slice slice(shard);
            void* sliceBuffer = slice.GetSliceBuffer();

            EXPECT_EQ(&slice.GetShard(), &shard);
            TestAssert(sliceBuffer != nullptr);

            // Test placement of Slice* in the last bytes of the slice buffer.
            EXPECT_EQ(Slice::GetSliceFromBuffer(sliceBuffer, shard.GetSlicePtrOffset()), &slice);

            TermInfo termInfo(ITermTable::GetMatchAllTerm(), *termTable);
            LogAssertB(termInfo.MoveNext());
            const RowId matchAllRowId = termInfo.Current();
            LogAssertB(!termInfo.MoveNext());

            {
                const ptrdiff_t offset = shard.GetRowTable(0).GetRowOffset(matchAllRowId.GetIndex());
                unsigned __int8* matchAllRowData = reinterpret_cast<unsigned __int8*>(sliceBuffer) + offset;
                for (unsigned i = 0; i < shard.GetSliceCapacity() / 8; ++i)
                {
                    EXPECT_EQ(*matchAllRowData, 0xFF);
                    matchAllRowData++;
                }
            }

            const DocId c_anyDocId = 1234;
            const DocId c_anyDocIndex = 32;
            const size_t c_anyBlobSize = 10;
            const char c_anyBlobValue = 32;

            // DocTable operations.
            slice.GetDocTable().SetDocId(sliceBuffer, c_anyDocIndex, c_anyDocId);
            EXPECT_EQ(slice.GetDocTable().GetDocId(sliceBuffer, c_anyDocIndex), c_anyDocId);

            void* blobValue = slice.GetDocTable().GetVariableSizeBlob(sliceBuffer, c_anyDocIndex, varBlobId0);
            TestAssert(blobValue == nullptr);

            blobValue = slice.GetDocTable().AllocateVariableSizeBlob(sliceBuffer, c_anyDocIndex, varBlobId0, c_anyBlobSize);
            TestAssert(blobValue != nullptr);
            memset(blobValue, c_anyBlobValue, c_anyBlobSize);

            // RowTable operations.
            for (DocIndex i = 0; i < c_sliceCapacity; ++i)
            {
                for (Rank rank = 0; rank <= c_maxRankValue; ++rank)
                {
                    RowTableDescriptor const & rowTable = slice.GetRowTable(rank);

                    for (RowIndex row = 0; row < rowCounts[rank]; ++row)
                    {
                        if (row == matchAllRowId.GetIndex())
                        {
                            TestAssert(rowTable.GetBit(sliceBuffer, row, i) == 1);
                        }
                        else
                        {
                            TestAssert(rowTable.GetBit(sliceBuffer, row, i) == 0);
                        }
                    }
                }
            }

            for (DocIndex i = 0; i < c_sliceCapacity; ++i)
            {
                for (Rank rank = 0; rank <= c_maxRankValue; ++rank)
                {
                    RowTableDescriptor const & rowTable = slice.GetRowTable(rank);

                    for (RowIndex row = 0; row < rowCounts[rank]; ++row)
                    {
                        rowTable.SetBit(sliceBuffer, row, i);
                        TestAssert(rowTable.GetBit(sliceBuffer, row, i) > 0);
                    }
                }
            }
        }


        void FillUpRandom(char* blob, size_t blobSize, RandomInt<unsigned>& random)
        {
            for (size_t i = 0; i < blobSize; ++i)
            {
                *blob = random() % UINT8_MAX;
                blob++;
            }
        }


        // Test serialization/deserialization of the Slice.
        TEST(RoundTripTest, Trivial)
        {
            static const DocIndex c_sliceCapacity = Row::DocumentsInRank0Row(1);

            static const size_t c_fixedBlob0Size = 20;

            // Number of rows to set bits in for each column during the test.
            static const unsigned c_testRowCount = 10;

            static const std::vector<RowIndex> rowCounts = { 100, 0, 0, 200, 0, 0, 300 };
            std::shared_ptr<ITermTable const> termTable(new EmptyTermTable(rowCounts));

            DocumentDataSchema schema;
            const VariableSizeBlobId varBlobId0 = schema.RegisterVariableSizeBlob();
            const FixedSizeBlobId fixedBlobId0 = schema.RegisterFixedSizeBlob(c_fixedBlob0Size);

            IndexWrapper index(c_sliceCapacity, termTable, schema, c_blockAllocatorBlockCount);
            Shard& shard = index.GetShard();

            Slice slice(shard);
            void* sliceBuffer = slice.GetSliceBuffer();

            RowIndex totalRowCount = 0;
            for (const auto r : rowCounts)
            {
                totalRowCount += r;
            }

            std::vector<size_t> varBlobSizes;
            RandomInt<unsigned> random(10000, 0, 10000);
            {
                for (DocIndex i = 0; i < c_sliceCapacity; ++i)
                {
                    DocIndex index;
                    TestAssert(slice.TryAllocateDocument(index));

                    // Set randomly picked c_testRowCount bits.
                    for (unsigned j = 0; j < c_testRowCount; ++j)
                    {
                        // Pick an absolute row index. Walk over the row counts for
                        // each rank to determine the rank which this index belongs and
                        // a rank-relative RowIndex.
                        unsigned rowIndex = random() % totalRowCount;
                        Rank rank = 0;
                        for (rank = 0; rank <= c_maxRankValue; ++rank)
                        {
                            if (rowIndex < rowCounts[rank])
                            {
                                break;
                            }

                            rowIndex -= rowCounts[rank];
                        }

                        slice.GetRowTable(rank).SetBit(sliceBuffer, rowIndex, index);
                    }

                    unsigned blobSize = random() % 20;
                    varBlobSizes.push_back(blobSize);

                    // Deliberately allow blobSize = 0 to simulate a case when no
                    // blob is allocated for a document.
                    if (blobSize > 0)
                    {
                        char* blob0 = reinterpret_cast<char*>(
                            slice.GetDocTable().AllocateVariableSizeBlob(sliceBuffer, index, varBlobId0, blobSize));
                        FillUpRandom(blob0, blobSize, random);
                    }

                    char* fixedBlob0 = reinterpret_cast<char*>(
                        slice.GetDocTable().GetFixedSizeBlob(sliceBuffer, index, fixedBlobId0));
                    FillUpRandom(fixedBlob0, c_fixedBlob0Size, random);

                    slice.CommitDocument(index);
                }
            }

            std::stringstream ss;
            slice.Write(ss);

            Slice slice1(shard, ss);
            void* sliceBuffer1 = slice1.GetSliceBuffer();

            EXPECT_EQ(&slice.GetShard(), &slice1.GetShard());

            // Make sure new Slice got put in a different buffer.
            TestNotEqual(sliceBuffer, sliceBuffer1);

            // Make sure Slice is initialized as sealed.
            {
                DocIndex index;
                TestAssert(!slice1.TryAllocateDocument(index));
            }

            Slice* slicePtrActual = Slice::GetSliceFromBuffer(sliceBuffer1, shard.GetSlicePtrOffset());
            EXPECT_EQ(&slice1, slicePtrActual);

            {
                DocTableDescriptor const & docTable = slice1.GetDocTable();
                for (DocIndex i = 0; i < c_sliceCapacity; ++i)
                {
                    for (Rank r = 0; r <= c_maxRankValue; ++r)
                    {
                        RowTableDescriptor const & rowTable = slice.GetRowTable(r);
                        for (RowIndex row = 0; row < rowCounts[r]; ++row)
                        {
                            EXPECT_EQ(rowTable.GetBit(sliceBuffer, row, i),
                                      rowTable.GetBit(sliceBuffer1, row, i));
                        }
                    }

                    void* varBlob0Slice0 = docTable.GetVariableSizeBlob(sliceBuffer, i, varBlobId0);
                    void* varBlob0Slice1 = docTable.GetVariableSizeBlob(sliceBuffer1, i, varBlobId0);

                    const size_t blobSize = varBlobSizes[i];
                    if (blobSize == 0)
                    {
                        TestAssert(varBlob0Slice1 == nullptr);
                    }
                    else
                    {
                        EXPECT_EQ(memcmp(varBlob0Slice0, varBlob0Slice1, blobSize), 0);
                    }

                    EXPECT_EQ(memcmp(docTable.GetFixedSizeBlob(sliceBuffer, i, fixedBlobId0),
                                     docTable.GetFixedSizeBlob(sliceBuffer1, i, fixedBlobId0),
                                     c_fixedBlob0Size), 0);
                }
            }

            {
                // Simulate a case when the persisted Slice is not compatible with the new
                // configuration - choose a different slice capacity, and hence, slice buffer size.
                // Since termTable is passed by a unique_ptr and the IndexWrapper took over a reference
                // over it, create another instance with the same rowCounts.
                std::shared_ptr<ITermTable const> termTable1(new EmptyTermTable(rowCounts));
                IndexWrapper index1(2 * Row::DocumentsInRank0Row(1),
                                    termTable1,
                                    schema,
                                    1);

                Shard& shard = index1.GetShard();
                ss.seekg(0, std::ios::beg);
                ExpectException([&]() { Slice slice1(shard, ss); });
            }
        }
        */
    }
}
