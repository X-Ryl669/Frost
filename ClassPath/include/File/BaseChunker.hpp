#ifndef hpp_CPP_BaseChunker_CPP_hpp
#define hpp_CPP_BaseChunker_CPP_hpp

// We need File declaration
#include "File.hpp"
// We need SHA1 too
#include "../Hashing/SHA1.hpp"
// We need SHA256 too
#include "../Hashing/SHA256.hpp"
// We need Memory block too for Multichunk
#include "../Utils/MemoryBlock.hpp"
// We need streams too
#include "../Streams/Streams.hpp"


namespace File
{


#pragma pack(push, 1)
    /** Each chunk that's created from a chunker will use this structure */
    struct Chunk
    {
        enum
        {
            MaximumChunkSize = 11299, //!< This comes from Heckel thesis, and is fixed to avoid memory allocation overhead while chunking
            HeaderSize = Hashing::SHA1::DigestSize + 2, //!< This is to avoid sum everywhere
        };
        /** The SHA-1 checksum for this chunk (the rolling checksum is never saved) */
        uint8  checksum[Hashing::SHA1::DigestSize];
        /** The chunk data size */
        uint16 size;
        /** The chunk data */
        uint8  data[MaximumChunkSize];
    };
#pragma pack(pop)


    /** A Chunker cuts a file in chunks and allow to enumerate them.
        It's possible to merge chunks back to a file, using the chunker's descriptive output. */
    class BaseChunker
    {
        // Type definition and enumeration
    public:

        // Members
    protected:
        /** The chunker name, used in the descriptive output, for both cutting and rebuilding chunks */
        String name;
        /** The chunker options (split using the comma operator) */
        Strings::StringArray options;


        // Interface
    public:
        /** Extract a chunk from the given input stream.
            @return false if the input stream is exhausted, or if it does not support rewinding */
        virtual bool createChunk(::Stream::InputStream & input, Chunk & chunk) const = 0;
        /** Get the minimum chunk size this chunker can spit out */
        virtual size_t getMinimumChunkSize() const = 0;
        /** Get the maximum chunk size this chunker can spit out */
        virtual size_t getMaximumChunkSize() const = 0;



        // Construction
    public:
        /** Build a base chunker
            @param name  The chunker name and any additional options that'll be used to match the chunk process */
        BaseChunker(const String & name, const String & options)
            : name(name), options(options, ",") {}

        /** Required virtual destructor */
        virtual ~BaseChunker() {}
    };

    /** Chunks can be stored in multichunk to avoid small file transfer overhead.
        On one side, sending numerous chunk will prove faster, as we only pay the
        transfer setup overhead once per multichunk, but, on the other side,
        retrieving a single chunk out of the multichunk requires downloading the
        whole multichunk.

        With assymetric lines, where download bandwidth is way higher than upload,
        this should not be a issue, and the benefit outweight the induced cost.

        The multichunk size is preallocated, and chunks are fit to the allocated
        space to avoid per-chunk allocation and deallocation.
    */
    struct MultiChunk
    {
        // Type definition and enumeration
    public:
        static uint32 MaximumSize; //!< Heckel thesis selected 250KB as a good tradeof

        /** The chunk position in the array */
        typedef Container::PlainOldData<uint32>::Array ChunkPos;

        // Members
    public:
        /** Store the chunks in the fixed array */
        Utils::MemoryBlock chunkArray;
        /** The chunk boundaries */
        ChunkPos    chunkPos;
        /** The multichunk filter list */
        uint32      filterListID;
        /** Some opaque value that's accessible from the user */
        uint64      opaque;

        // Interface
    public:
        /** Get the next chunk from this multichunk */
        uint8 * getNextChunkData(uint16 dataSize, const uint8 * checksum);
        /** Create the next chunk from the given input stream.
            @param input    The input stream to read. This must be a seekable stream.
            @param chunker  The chunker to use on the stream
            @return A pointer on a managed chunk (you must not delete this pointer) or 0 on error.
                    If it returns 0, either this multichunk is full (and the input stream is rewound), use getFreeSpace() to figure it out,
                    either the input stream is exhausted. */
        Chunk * createNextChunk(::Stream::InputStream & input, const BaseChunker & chunker);
        /** Get the i-th chunk.
            This is a O(1) operation. */
        Chunk * getChunk(const size_t index) const;

        /** Get the storable multichunk data.
            This does not apply any filter.
            You should consider applying filters on the output stream yourself. */
        bool writeDataTo(::Stream::OutputStream & output) const;
        /** Get the multichunk header (usually stored before any data).
            The header follows this pattern:
             - 16 bits:   Chunk count. If higher than 65534, then 65535 is stored here, and the actual count is written in the optional field.
             - 16 bits:   FilterList ID.
             - (32 bits): If chunk count is higher than 65534, then the actual count is written here, else, this field is non present
             - For each chunk:
             - 160 bits   SHA1 for the chunk
             - 16 bits    Chunk size in bytes */
        bool writeHeaderTo(::Stream::OutputStream & output) const;

        /** Load the multichunk header out of the given input stream.
            This does not apply any filter
            You should consider applying filters on the input stream yourself.
            @sa writeHeaderTo for the header format */
        bool loadHeaderFrom(const ::Stream::InputStream & input);
        /** Load the multichunk data out of the given input stream
            This does not apply any filter
            You should consider applying filters on the input stream yourself. */
        bool loadDataFrom(const ::Stream::InputStream & input);

        /** Find the chunk with the given hash.
            Since this is supposed to be used only when restoring, it does not
            have to be utterly fast.
            This performs a O(n) search in the chunk array if no offset provided,
            else it check the chunk at the given offset and might work in O(log(N))
            @param checksum     The chunk checksum to extract
            @param likelyOffset If provided, will check this offset first, and if the
                                checksum is good will return the chunk at the given offset,
                                else, will search in the complete array as if it was not provided.
            @return 0 if not found, or a pointer on the chunk if found. */
        Chunk * findChunk(const uint8 * checksum, const size_t likelyOffset = (size_t)-1) const;

        /** Set the filter list ID.
            FilterList are used to store the processing steps that are applied
            to this multichunk before hitting the final storage.
            The mapping to the operations is done outside this class. */
        inline void setFilterListID(const uint32 filterID) { filterListID = filterID; }
        /** Get the current used size for this multichunk */
        inline size_t getSize() const { return (size_t)chunkArray.getSize(); }
        /** Get the remaining free space in this multichunk (beware of chunk header size).
            @sa canFit */
        inline size_t getFreeSpace() const { return MaximumSize - chunkArray.getSize(); }
        /** Check if we can fit a given chunk size inside this multichunk. */
        inline bool canFit(const size_t chunkSize) const { return (MaximumSize - chunkArray.getSize()) >= (chunkSize + Chunk::HeaderSize); }

        /** Reset this multichunk */
        inline void Reset() { chunkArray.stripTo(0); chunkPos.Clear(); filterListID = 0; }
        /** Get the complete data's SHA-256 checksum */
        void getChecksum(uint8 (&checksum)[Hashing::SHA256::DigestSize]) const;

        /** Get the opaque value */
        uint64 getOpaque() const { return opaque; }
        /** Set the opaque value */
        void setOpaque(uint64 newVal) { opaque = newVal; }

        /** Set the maximum size for a multichunk */
        static void setMaximumSize(uint32 size) { MaximumSize = size; }
        /** Get this multichunk's entropy in a normalized range [0; 1[ */
        double getEntropy() const { return computeEntropy(chunkArray.getConstBuffer(), chunkArray.getSize()) / 8.0; }
        /** Get entropy for the i-th chunk in normalized range [0; 1[ */
        static double getChunkEntropy(const Chunk * chunk) { return chunk ? computeEntropy(chunk->data, chunk->size) / 8.0 : 1.0; }

        // Helpers
    private:
        /** Compute the entropy for the given data (might be useful to enable compression or not)
            @return Entropy value in range [0 ; 8[  */
        static double computeEntropy(const uint8 * buffer, const uint32 size);

        // Construction
    public:
        /** Default construction */
        MultiChunk() : chunkArray(MaximumSize), filterListID(0), opaque(0) { chunkArray.stripTo(0); }
    };


    /** The factory used to build chunkers */
    struct ChunkerFactory
    {
        /** Construct a chunker based on the given name and options */
        BaseChunker * buildChunker(const String & name, const String & options);
    };
}


#endif
