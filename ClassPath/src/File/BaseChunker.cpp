// We need math for log function
#include <math.h>
// We need our declaration
#include "../../include/File/BaseChunker.hpp"
// We also need an implementation
#include "../../include/File/TTTDChunker.hpp"
// We need OpenSSL code for faster hasher
#include "../../include/Crypto/OpenSSLWrap.hpp"

namespace File
{
    uint32 MultiChunk::MaximumSize = 250*1024;
 
    // Construct a chunker based on the given name and options
    BaseChunker * ChunkerFactory::buildChunker(const String & name, const String & options)
    {
        if (name == "TTTD") return new TTTDChunker(options);
        return 0;
    }
    
    double MultiChunk::computeEntropy() const
    {
        const uint8 * data = chunkArray.getConstBuffer();
        uint32 histogram[256]; memset(histogram, 0, sizeof(histogram));
        
        for (uint32 i = 0; i < chunkArray.getSize(); i++)
            histogram[data[i]]++;
        
        double entropy = 0; double invLog2 = 1.0 / log(2.0);
        double invSize = 1.0 / (double)chunkArray.getSize();
        for (uint32 i = 0; i < 256; i++)
        {
            double p = histogram[i] * invSize;
            if (p > 0)
                entropy = entropy - p * log(p) * invLog2;
        }
        return entropy;
    }
    
    void MultiChunk::getChecksum(uint8 (&checksum)[Hashing::SHA256::DigestSize]) const
    {
        Crypto::OSSL_SHA256 sha1;
        sha1.Start();
        sha1.Hash(chunkArray.getConstBuffer(), chunkArray.getSize());
        sha1.Finalize(checksum);
    }
    
    // Get the next chunk from this multichunk
    uint8 * MultiChunk::getNextChunkData(uint16 dataSize, const uint8 * checksum)
    {
        // Check if we can store this chunk.
        size_t chunkSize = dataSize + Chunk::HeaderSize;
        if (getFreeSpace() < chunkSize) return 0; // Can't any way

        uint32 arraySize = chunkArray.getSize();
        if (!chunkArray.Append(0, (uint32)chunkSize)) return 0;

        chunkPos.Append(arraySize);
        
        Chunk * chunk = (Chunk*)&chunkArray.getBuffer()[arraySize];
        memcpy(chunk->checksum, checksum, ArrSz(chunk->checksum));
        chunk->size = dataSize;
        
        return chunk->data;
    }

    // Create the next chunk from the given input stream.
    Chunk * MultiChunk::createNextChunk(::Stream::InputStream & input, const BaseChunker & chunker)
    {
        // Check if we can store this chunk.
        if (getFreeSpace() < chunker.getMinimumChunkSize()) return 0; // Can't any way
        
        // Now, either we can fit the chunk inside our buffer, either we can't, so let's save the input
        // stream position to restore later on if we fail
        uint64 streamPos = input.currentPosition();
        // We first overallocate a chunk, and we'll adjust later on
        Chunk temp = {};
        if (!chunker.createChunk(input, temp))
            // End of stream or stream not rewinding capable, so let's get out.
            return 0;
        
        // Now, check the real size for this multichunk
        uint32 realChunkSize = temp.size + ArrSz(temp.checksum) + sizeof(temp.size);
        if (getFreeSpace() < realChunkSize)
        {
            // Rewind the input stream and fail
            input.setPosition(streamPos); // No error checking here, as it would have hit earlier 
            return 0;
        }
        
        // Ok, store the chunk in our array
        uint32 arraySize = chunkArray.getSize();
        if (!chunkArray.Append(0, realChunkSize)) return 0;
        chunkPos.Append(arraySize);
        
        memcpy(&chunkArray.getBuffer()[arraySize], &temp, realChunkSize);
        return (Chunk*)&chunkArray.getBuffer()[arraySize];
    }

    
    // Get the storable multichunk data.
    bool MultiChunk::writeDataTo(::Stream::OutputStream & output) const
    {
        return output.write(chunkArray.getConstBuffer(), chunkArray.getSize()) == chunkArray.getSize();
    }
    
    // Get the multichunk header (usually stored before any data).
    bool MultiChunk::writeHeaderTo(::Stream::OutputStream & output) const
    {
        uint32 chunkAndFilter = (filterListID & 0xFFFF) | ((chunkPos.getSize() & 0xFFFF) << 16);
        if (chunkPos.getSize() >= 0xFFFF)
        {   // Escape code if the number of chunks exceed the available space
            chunkAndFilter = 0xFFFF0000 | (filterListID & 0xFFFF);
            if (!output.write(chunkAndFilter)) return false;
            chunkAndFilter = (uint32)chunkPos.getSize();
        }
        
        if (!output.write(chunkAndFilter)) return false;
        for (size_t i = 0; i < chunkPos.getSize(); i++)
        {
            Chunk * chunk = (Chunk*)&chunkArray.getConstBuffer()[chunkPos[i]];
            if (!output.write(chunk->checksum)) return false;
            if (!output.write(chunk->size)) return false;
        }
        return true;
    }
    
    // Load the multichunk header out of the given input stream.
    bool MultiChunk::loadHeaderFrom(const ::Stream::InputStream & input)
    {
        chunkPos.Clear();
        chunkArray.stripTo(0); // Reset the chunkArray size
        
        uint32 chunkAndFilter = 0;
        if (!input.read(chunkAndFilter)) return false;
        filterListID = chunkAndFilter & 0xFFFF;
        uint32 count = chunkAndFilter >> 16;
        if ((chunkAndFilter >> 16) == 0xFFFF)
        {   // Escape code in case of high number of chunks
            if (!input.read(chunkAndFilter)) return false;
            count = chunkAndFilter;
        }
        
        for (uint32 i = 0; i < count; i++)
        {
            uint16 size = 0; uint8 checksum[Hashing::SHA1::DigestSize];
            if (!input.read(checksum)) return false;
            if (!input.read(size)) return false;
            // This force creating an empty chunk
            getNextChunkData(size, checksum);
        }
        return true;
    }
    // Load the multichunk data out of the given input stream
    bool MultiChunk::loadDataFrom(const ::Stream::InputStream & input)
    {
        return input.read(chunkArray.getBuffer(), chunkArray.getSize()) == chunkArray.getSize();
    }
    
    
    // Find the chunk with the given hash.
    Chunk * MultiChunk::findChunk(const uint8 * checksum, const size_t likelyOffset) const
    {
        if (likelyOffset != (size_t)-1)
        {   // This is O(log(N))
            size_t index = chunkPos.indexOfSorted(likelyOffset);
            Chunk * chunk = getChunk(index);
            if (chunk && memcmp(chunk->checksum, checksum, ArrSz(chunk->checksum)) == 0) return chunk;
        }
        for (size_t i = 0; i < chunkPos.getSize(); i++)
        {
            Chunk * chunk = getChunk(i);
            if (memcmp(chunk->checksum, checksum, ArrSz(chunk->checksum)) == 0) return chunk;
        }
        return 0;
    }
    
    // Get the i-th chunk.
    Chunk * MultiChunk::getChunk(const size_t index) const
    {
        if (index >= chunkPos.getSize()) return 0;
        return (Chunk*)&chunkArray.getConstBuffer()[chunkPos[index]];
    }
}
