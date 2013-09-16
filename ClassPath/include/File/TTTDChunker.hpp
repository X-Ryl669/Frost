#ifndef hpp_CPP_TTTDChunker_CPP_hpp
#define hpp_CPP_TTTDChunker_CPP_hpp

// We need the base chunker
#include "BaseChunker.hpp"


namespace File
{
    /** Two threshold, two divisor chunker.
        We are using a rolling checksum here to find out, in the data flow, where to split chunks. */
    class TTTDChunker : public BaseChunker
    {
        // Type definition and enumeration
    public:
        
        // Members
    private:
        /** The minimum chunk size not to split below */
        uint32 minChunkSize;
        /** The maximum chunk size threshold */
        uint32 maxChunkSize;
        /** The first divider for boundary finding */
        uint32 highDivider;
        /** The low divider for boundary finding */
        uint32 lowDivider;
        
        
        // Interface
    public:
        /** Extract a chunk from the given input stream */
        bool createChunk(::Stream::InputStream & input, Chunk & chunk) const;
        /** Get the minimum chunk size this chunker can spit out */
        inline size_t getMinimumChunkSize() const { return minChunkSize; }
        /** Get the maximum chunk size this chunker can spit out */
        inline size_t getMaximumChunkSize() const { return maxChunkSize; }
                
        // Construction
    public:
        /** Build a Two Threshold Two Divider chunker
            @param name  The chunker name and any additional options that'll 
                         be used to match the chunk process. 
                         If you only provide one value, it's the average chunk size */
        TTTDChunker(const String & _options = "4096");
    };
    
}

#endif