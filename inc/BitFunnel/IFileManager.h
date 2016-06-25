#pragma once

#include <istream>
#include <ostream>
#include <string>

#include "BitFunnel/BitFunnelTypes.h"     // For ShardId, SliceId.
#include "BitFunnel/NonCopyable.h"


namespace BitFunnel
{
    class IParameterizedFile0;
    template <typename P1> class IParameterizedFile1;
    template <typename P1, typename P2> class IParameterizedFile2;

    class FileDescriptor0;
    template <typename P1> class FileDescriptor1;
    template <typename P1, typename P2> class FileDescriptor2;

    //*************************************************************************
    //
    // IFileManager
    //
    // Interface for objects that generate parameterized file names and open 
    // and close streams associated with these files.
    //
    // DESIGN NOTE: None of the methods are const in order to allow greater
    // flexibility for mocks and other classes that implement IFileManager.
    //
    // DESIGN NOTE: The methods don't use the GetFoo() naming convention for
    // two reasons. The first is that they are not really getters for 
    // properties. The implementation could choose to synthesize new
    // FileDescriptors for each call. The second reason is for brevity.
    //
    //*************************************************************************
    class IFileManager
    {
    public:
        virtual ~IFileManager() {};

        // These methods return descriptors for files that are only
        // parameterized by the IFileManager. The returned FileDescriptor0
        // objects provide methods to generate the file names and open the
        // files.
        virtual FileDescriptor0 BandTable() = 0;
        virtual FileDescriptor0 CommonNegatedTerms() = 0;
        virtual FileDescriptor0 CommonPhrases() = 0;
        virtual FileDescriptor0 DocFreqTable() = 0;
        virtual FileDescriptor0 DocumentHistogram() = 0;
        virtual FileDescriptor0 L1RankerConfig() = 0;
        virtual FileDescriptor0 Manifest() = 0;
        virtual FileDescriptor0 Model() = 0;
        virtual FileDescriptor0 PlanDescriptors() = 0;
        virtual FileDescriptor0 PostingCounts() = 0;
        virtual FileDescriptor0 ShardDefinition() = 0;
        virtual FileDescriptor0 ShardDocCounts() = 0;
        virtual FileDescriptor0 ShardedDocFreqTable() = 0;
        virtual FileDescriptor0 SortRankerConfig() = 0;
        virtual FileDescriptor0 StreamNameToSuffixMap() = 0;
        virtual FileDescriptor0 SuffixToClassificationMap() = 0;
        virtual FileDescriptor0 ClickStreamSuffixToMarketMap() = 0;
        virtual FileDescriptor0 TierDefinition() = 0;
        virtual FileDescriptor0 TermDisposeDefinition() = 0;
        virtual FileDescriptor0 MetaWordTierHintMap() = 0;
        virtual FileDescriptor0 TermTableStats() = 0;
        virtual FileDescriptor0 PostingAndBitStats() = 0;
        virtual FileDescriptor0 StrengtheningMetawords() = 0;

        // These methods return descriptors for files that are parameterized
        // by a shard number.  The returned FileDescriptor1 objects provide 
        // methods to generate the file names and open the files.
        virtual FileDescriptor1<ShardId> DocTable(ShardId shard) = 0;
        virtual FileDescriptor1<ShardId> ScoreTable(ShardId shard) = 0;
        virtual FileDescriptor1<ShardId> TermTable(ShardId shard) = 0;

        virtual FileDescriptor2<ShardId, SliceId> IndexSlice(ShardId shard, 
                                                             SliceId slice) = 0;
    };


    class IParameterizedFile0
    {
    public:
        virtual ~IParameterizedFile0() {};

        virtual std::string GetName() = 0;
        virtual std::istream* OpenForRead() = 0;
        virtual std::ostream* OpenForWrite() = 0;
        virtual std::ostream* OpenTempForWrite() = 0;
        virtual void Commit() = 0;
        virtual bool Exists() = 0;
        virtual void Delete() = 0;
    };

    template <typename P1>
    class IParameterizedFile1
    {
    public:
        virtual ~IParameterizedFile1() {};

        virtual std::string GetName(P1 p1) = 0;
        virtual std::istream* OpenForRead(P1 p1) = 0;
        virtual std::ostream* OpenForWrite(P1 p1) = 0;
        virtual std::ostream* OpenTempForWrite(P1 p1) = 0;
        virtual void Commit(P1 p1) = 0;
        virtual bool Exists(P1 p1) = 0;
        virtual void Delete(P1 p1) = 0;
    };

    template <typename P1, typename P2>
    class IParameterizedFile2
    {
    public:
        virtual ~IParameterizedFile2() {};

        virtual std::string GetName(P1 p1, P2 p2) = 0;
        virtual std::istream* OpenForRead(P1 p1, P2 p2) = 0;
        virtual std::ostream* OpenForWrite(P1 p1, P2 p2) = 0;
        virtual std::ostream* OpenTempForWrite(P1 p1, P2 p2) = 0;
        virtual void Commit(P1 p1, P2 p2) = 0;
        virtual bool Exists(P1 p1, P2 p2) = 0;
        virtual void Delete(P1 p1, P2 p2) = 0;
    };


    // DESIGN NOTE: Deliberately using inline template method definition for
    // brevity.
    class FileDescriptor0 : NonCopyable
    {
    public:
        FileDescriptor0(IParameterizedFile0& file)
            : m_file(file)
        {
        }

        std::string GetName() { return m_file.GetName(); }
        std::istream* OpenForRead() { return m_file.OpenForRead(); }
        std::ostream* OpenForWrite() { return m_file.OpenForWrite(); }
        std::ostream* OpenTempForWrite() { return m_file.OpenTempForWrite(); }
        void Commit() { return m_file.Commit(); }
        bool Exists() { return m_file.Exists(); }
        void Delete() { m_file.Delete(); }

    private:
        IParameterizedFile0& m_file;
    };


    // DESIGN NOTE: Deliberately using inline template method definition for
    // brevity.
    template <typename P1>
    class FileDescriptor1 : NonCopyable
    {
    public:
        FileDescriptor1(IParameterizedFile1<P1>& file, P1 p1)
            : m_file(file),
              m_p1(p1)
        {
        }

        std::string GetName() { return m_file.GetName(m_p1); }
        std::istream* OpenForRead() { return m_file.OpenForRead(m_p1); }
        std::ostream* OpenForWrite() { return m_file.OpenForWrite(m_p1); }
        std::ostream* OpenTempForWrite() { return m_file.OpenTempForWrite(m_p1); }
        void Commit() { return m_file.Commit(m_p1); }
        bool Exists() { return m_file.Exists(m_p1); }
        void Delete() { m_file.Delete(m_p1); }

    private:
        IParameterizedFile1<P1>& m_file;
        P1 m_p1;
    };


    // DESIGN NOTE: Deliberately using inline template method definition for
    // brevity.
    template <typename P1, typename P2>
    class FileDescriptor2 : NonCopyable
    {
    public:
        FileDescriptor2(IParameterizedFile2<P1, P2>& file, P1 p1, P2 p2)
            : m_file(file),
              m_p1(p1),
              m_p2(p2)
        {
        }

        std::string GetName() { return m_file.GetName(m_p1, m_p2); }
        std::istream* OpenForRead() { return m_file.OpenForRead(m_p1, m_p2); }
        std::ostream* OpenForWrite() { return m_file.OpenForWrite(m_p1, m_p2); }
        std::ostream* OpenTempForWrite() { return m_file.OpenTempForWrite(m_p1, m_p2); }
        void Commit() { return m_file.Commit(m_p1, m_p2); }
        bool Exists() { return m_file.Exists(m_p1, m_p2); }
        void Delete() { m_file.Delete(m_p1, m_p2); }

    private:
        IParameterizedFile2<P1, P2>& m_file;
        P1 m_p1;
        P2 m_p2;
    };
}