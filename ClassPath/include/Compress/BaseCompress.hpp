#ifndef hpp_BaseCompressor_hpp
#define hpp_BaseCompressor_hpp

// We need basic types
#include "../Types.hpp"
// We need memory blocks too
#include "../Utils/MemoryBlock.hpp"
// We need streams too
#include "../Streams/Streams.hpp"

#if (WantCompression == 1)

/** Lossless compression and decompression of unknown data primitives.
    All (de)compressor implements the BaseCompressor interface.
    Some compressor have specific behaviour/tweak per compress.
    Unless specified otherwise, all the compressor and decompressor are thread safe
    (while not shared across thread without external locking).

    You might be interested in using Stream based (de)compression to transparent on-the-fly (de)compression. */
namespace Compression
{
    /** The base compression interface.
        All (de)compressors implements this interface.
        Since each compressor might show different options, you should check each compressor explicit documentation.
        @sa ZLib, GZip, BSCLib
        @sa Tests::CompressTests */
    class BaseCompressor
    {
        // Members
    private:
        /** The compressor name */
        const char * name;

        // Helpers
    protected:
        /** The processing function */
        typedef bool (BaseCompressor::*processingFunc)(uint8 *&, size_t &, const uint8 *, const size_t);
        /** Processing method for streams */
        bool processData(Stream::OutputStream & outStream, const Stream::InputStream & inStream, const uint32 amountToCompress, processingFunc func)
        {
            uint8 * outBuffer = 0; size_t outSize = 0;
            uint8 buffer[2048];
            uint64 totalSize = 0;
            while (!amountToCompress || totalSize < amountToCompress)
            {
                uint64 size = (uint64)min((uint32)sizeof(buffer), amountToCompress);
                uint64 blockSize = inStream.read(buffer, size);
                if (blockSize == (uint64)-1) { delete[] outBuffer; return true; } // Error while reading
                if (!(this->*func)(outBuffer, outSize, buffer, (size_t)blockSize)) { delete[] outBuffer; return false; }
                if (outStream.write(outBuffer, outSize) != outSize) { delete[] outBuffer; return false; }
                totalSize += size;
            }
            delete[] outBuffer;
            return true;
        }
        /** Processing method for blocks */
        Utils::MemoryBlock * processBlock(const Utils::MemoryBlock & in, processingFunc func)
        {
            Utils::MemoryBlock * out = 0;
            uint8 * outPtr = 0; size_t outSize = 1;
            if (!(this->*func)(outPtr, outSize, in.getConstBuffer(), (size_t)in.getSize()))
            {
                out = new Utils::MemoryBlock((uint32)outSize); if (!out) return 0; outPtr = out->getBuffer();
                return (this->*func)(outPtr, outSize, in.getConstBuffer(), (size_t)in.getSize()) ? out : 0;
            }
            return new Utils::MemoryBlock();
        }

        // Interface
    public:
        /** Get the compressor name */
        inline const char * getName() const { return name; }
        /** Decompress data.
            @param out      If not null, it points to a buffer of outSize bytes. Else, it's allocated and you must delete[] it.
            @param outSize  On input contains the size of the out buffer, on output contains the used size in the out buffer
            @param in       The pointer to the compressed buffer
            @param inSize   The compressed buffer size in bytes
            @return true on success, false on integrity checking mismatch or bad input buffer */
        virtual bool decompressData(uint8 *& out, size_t & outSize, const uint8 * in, const size_t inSize) = 0;
        /** Compress data.
            @param out          If not null, it points to a buffer of outSize bytes. Else, it's allocated and you must delete[] it.
            @param outSize      On input contains the size of the out buffer, on output contains the used size in the out buffer
            @param in           The pointer to the uncompressed buffer
            @param inSize       The uncompressed buffer size in bytes
            @return true on success, false on bad input buffer */
        virtual bool compressData(uint8 *& out, size_t & outSize, const uint8 * in, const size_t inSize) = 0;


        /** Helpers using a memory block.
            @param in   The input block to compress
            @return A pointer on a new allocated memory block */
        virtual Utils::MemoryBlock * compressData(const Utils::MemoryBlock & in)
        {
            return processBlock(in, &BaseCompressor::compressData);
        }
        /** Helpers using a memory block.
            @param in   The input block to decompress
            @return A pointer on a new allocated memory block */
        virtual Utils::MemoryBlock * decompressData(const Utils::MemoryBlock & in)
        {
            return processBlock(in, &BaseCompressor::decompressData);
        }

        /** Continuous compression process.
            Not all compressor support this (in that case, it's probably emulated or might return false).

            @param outStream        The output stream to write to.
            @param inStream         The input stream to read from.
            @param amountToProcess  The number of bytes to compress. Set to 0 to process the whole stream.
            @param lastCall         This can be set to false, if you are going to compress more stream afterward (in that case, the compression might occur later on)
            @return true on success, false if not supported or an error occurred on the input stream */
        virtual bool compressStream(Stream::OutputStream & outStream, const Stream::InputStream & inStream, const uint32 amountToProcess = 0, const bool lastCall = true)
        {
            return processData(outStream, inStream, amountToProcess, &BaseCompressor::compressData);
        }
        /** Continuous decompression process.
            Not all compressor support this (in that case, it's probably emulated or might return false).

            @param outStream        The output stream to write to.
            @param inStream         The input stream to read from.
            @param amountToProcess  The number of decompressed bytes to reach. Set to 0 to process the whole stream.
            @return true on success, false if not supported or an error occurred on the input stream */
        virtual bool decompressStream(Stream::OutputStream & outStream, const Stream::InputStream & inStream, const uint32 amountToProcess = 0)
        {
            return processData(outStream, inStream, amountToProcess, &BaseCompressor::decompressData);
        }


        // Construction and destruction
    public:
        /** Common constructor */
        BaseCompressor(const char * name = "") : name(name) {}
        virtual ~BaseCompressor() {}
    };
}

#define HasCompression 1
#endif

#endif
