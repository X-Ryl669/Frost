// We need our declaration
#include "../../include/File/TTTDChunker.hpp"
// We need the Adler32 algorithm too for rolling checksum
#include "../../include/Hashing/Adler32.hpp"
// We need Assert too
#include "../../include/Utils/Assert.hpp"
// We need OpenSSL code for faster hasher
#include "../../include/Crypto/OpenSSLWrap.hpp"

namespace File
{
    // Build a Two Threshold Two Divider chunker
    TTTDChunker::TTTDChunker(const String & _options)
        : BaseChunker("TTTD", _options)
    {
        // Extract the parameters from the options
        if (options.getSize() >= 4)
        {
            minChunkSize = (uint32)(int)options[0];
            maxChunkSize = (uint32)(int)options[1];
            highDivider  = (uint32)(int)options[2];
            lowDivider   = (uint32)(int)options[3];
        } else if (options.getSize() <= 1)
        {
            int avgChunkSize = options.getSize() ? (int)options[0] : 4096;
            minChunkSize = (uint32)(460.0 * avgChunkSize / 1015.0 + .5);
            maxChunkSize = (uint32)(2800.0 * avgChunkSize / 1015.0 + .5);
            highDivider = (uint32)(540.0 * avgChunkSize / 1015.0 + .5);
            lowDivider = (uint32)(270.0 * avgChunkSize / 1015.0 + .5);

            options.Clear();
            options.appendLines(String::Print("%d\n%d\n%d\n%d", minChunkSize, maxChunkSize, highDivider, lowDivider));
        }

        // Make sure we don't overcome the implementation limits
        Assert(maxChunkSize < 65535);
    }


    // Extract a chunk from the given input stream
    bool TTTDChunker::createChunk(::Stream::InputStream & input, Chunk & chunk) const
    {
        // The 2 hash algorithm we are using for this algorithm
        Hashing::Adler32 rolling;
        rolling.Start();

        Crypto::OSSL_SHA1 bigChecksum;
        bigChecksum.Start();

        // The algorithm reads first the minimum amount of data out of the input stream
        uint64 curPos = input.currentPosition();
        uint64 read = input.read(chunk.data, (uint64)min((uint32)ArrSz(chunk.data), maxChunkSize));
        if (read == (uint64)-1 || !read) return false;

        // Depending on the amount read, let's act accordingly
        if (read <= minChunkSize)
        {
            chunk.size = (uint16)read;
            // Compute the SHA1 for this chunk,
            bigChecksum.Hash(chunk.data, (uint32)read);
            bigChecksum.Finalize(chunk.checksum);
            // No need to rewind the stream, or anything like that
            return true;
        }

        // Real algorithm here
        uint16 backupBreak = 0; // Using 16-bits because chunk don't overcome the 64KB limit
        uint16 breakPos = 0;
        for (uint16 i = minChunkSize; i < (uint16)read; i++)
        {
            rolling.Append(chunk.data[i]);
            // Get the checksum to check for dividers
            uint32 checksum = rolling.getChecksumLE();
            if ((checksum % lowDivider) == (lowDivider - 1))
                backupBreak = i+1;
            if ((checksum % highDivider) == (highDivider - 1))
            {
                breakPos = i+1;
                break;
            }
        }

        // If we can't find the high divider break, use the low divider break, or at least the chunk size
        if (breakPos == 0)
        {
            breakPos = backupBreak ? backupBreak : (uint16)read;
        }

        chunk.size = breakPos;
        // Compute the SHA1 for this chunk,
        bigChecksum.Hash(chunk.data, (uint32)chunk.size);
        bigChecksum.Finalize(chunk.checksum);
        // Then rewind the stream a bit to match the breakpoint
        return input.setPosition(curPos + breakPos);
    }
}

