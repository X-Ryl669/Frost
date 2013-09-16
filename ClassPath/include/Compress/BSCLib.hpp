#ifndef hpp_CPP_BSCLib_CPP_hpp
#define hpp_CPP_BSCLib_CPP_hpp

// We need base class declaration 
#include "BaseCompress.hpp"
// We need files too
#include "../File/File.hpp"

#if (WantCompression == 1) && (WantBSCCompression == 1)

// External object that's forward declared
namespace BSC { class EBSC; }

namespace Compression
{
    /** The string class we are using */
    typedef Strings::FastString String;

    /** Implementation class for Block Sorting Compressor.
        The algorithm was written by Ilya Grebnov.
        BSC is using Burrows Wheeler Transform for coder, and a Lempel Ziv based range coder.
        It gives better compression ratio than LZMA but is faster. */
    class BSCLib : public BaseCompressor
    {
        // Type definition and enumeration
    public:
        /** The last error */
        enum Error
        {
            Success         = 0,        //!< No error
            UnexpectedEOD   = -1,       //!< Unexpected end of data
            BadFormat       = -2,       //!< Bad format for the data
            DataCorrupt     = -3,       //!< Compressed data is corrupt
            NotEnoughMemory = -4,       //!< Not enough memory
        };
    
        // Helpers
    private:
        /** Basic set and mark error method */
        inline bool setError(Error val) { lastError = val; return val == Success; }
        /** Get the actual used buffer size */
        inline int getBufferSize() const { if (compressionFactor == -1) return 25*1024*1024; return ((int)(compressionFactor * 99) + 1) * 1024 * 1024; }

        // Members
    protected:
        /** The compression factor */
        int     compressionFactor;
        /** The last error */
        Error   lastError;
        
        
        /** The opaque holder */
        BSC::EBSC *  opaque;

    
        // Interface
    public:
        /** Set the compression factor (from 0.0 (fastest) to 1.0f (best) */
        virtual void setCompressionFactor(const float factor = 1.0f) { compressionFactor = (int)(factor * 9 + .5f); }
        /** Get the last error */
        Error getLastError() const { return lastError; }


        /** Decompress data.
            @param out      If not null, it points to a buffer of outSize bytes. Else, it's allocated and you must delete[] it.
            @param outSize  On input contains the size of the out buffer, on output contains the used size in the out buffer
            @param in       The pointer to the compressed buffer
            @param inSize   The compressed buffer size in bytes
            @return true on success, false on integrity checking mismatch or bad input buffer */
        virtual bool decompressData(uint8 *& out, size_t & outSize, const uint8 * in, const size_t inSize);

        /** Compress data.
            @param out          If not null, it points to a buffer of outSize bytes. Else, it's allocated and you must delete[] it.
            @param outSize      On input contains the size of the out buffer, on output contains the used size in the out buffer
            @param in           The pointer to the uncompressed buffer
            @param inSize       The uncompressed buffer size in bytes
            @return true on success, false on bad input buffer */
        virtual bool compressData(uint8 *& out, size_t & outSize, const uint8 * in, const size_t inSize);
        
        /** Continuous compression process.
            Not all compressor support this (in that case, it's probably emulated or might return false).

            @param outStream        The output stream to write to.
            @param inStream         The input stream to read from.
            @param amountToProcess  The number of bytes to compress. Set to 0 to process the whole stream.
            @return true on success, false if not supported or an error occurred on the input stream */
        virtual bool compressStream(Stream::OutputStream & outStream, const Stream::InputStream & inStream, const uint32 amountToProcess = 0);

        /** Continuous decompression process.
            Not all compressor support this (in that case, it's probably emulated or might return false).

            @param outStream        The output stream to write to.
            @param inStream         The input stream to read from.
            @param amountToProcess  The number of decompressed bytes to reach. Set to 0 to process the whole stream.
            @return true on success, false if not supported or an error occurred on the input stream */
        virtual bool decompressStream(Stream::OutputStream & outStream, const Stream::InputStream & inStream, const uint32 amountToProcess = 0);
        
        
        /** Default constructor */
        BSCLib();
        virtual ~BSCLib();
    };    
}
#endif

#endif
