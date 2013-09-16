#ifndef hpp_CPP_ZLib_CPP_hpp
#define hpp_CPP_ZLib_CPP_hpp

// We need base class declaration 
#include "BaseCompress.hpp"
// We need files too
#include "../File/File.hpp"

#if (WantCompression == 1)

// External object that's forward declared
extern "C" { struct z_stream; }

namespace Compression
{
    /** The string class we are using */
    typedef Strings::FastString String;

    /** Common implementation for both ZLib and GZip. 
        This is made to avoid code duplication */
    class CommonZlib : public BaseCompressor
    {
        // Type definition and enumeration
    public:
        /** The possible error code */
        enum Error
        {
            Success         = 0,    //!< No error 
            EndOfStream     = 1,    //!< Not an error, but a typical end of stream is found
            StreamError     = -2,   //!< An error appeared on the stream
            DataError       = -3,   //!< The data show errors (likely checksum failed)
            MemoryError     = -4,   //!< A memory error happened
            BufferError     = -5,   //!< Error in buffer management
        };
        /** Special meaning for headerless compression / decompression */
        static const float HeaderLess;

        // Members
    protected:
        /** The last operation error */
        Error   lastError;
        /** The compression factor */
        int     compressionFactor;
        
        /** The opaque holder */
        z_stream *  opaque;
        /** An opaque buffer used in stream mode */
        uint8       workBuffer[32768];
        /** The opaque buffer usage */
        uint32      workBufferLength;
    
        // Interface
    public:
        /** Get the last error */
        inline const Error getLastError() const { return lastError; }
        /** Set the compression factor (from 0.0 (fastest) to 1.0f (best) */
        virtual void setCompressionFactor(const float factor = 1.0f) { compressionFactor = (int)(factor * 9 + .5f); }
        /** Reset the object for a specific operation */
        virtual void Reset(const bool isCompressing) = 0;
    
        /** Continuous compression process.
            Not all compressor support this (in that case, it's probably emulated or might return false).
            
            @param outStream        The output stream to write to.
            @param inStream         The input stream to read from.
            @param amountToProcess  The number of bytes of the input stream to compress. Set to 0 to process the whole stream.
            @return true on success, false if not supported or an error occurred on the input stream */
        virtual bool compressStream(Stream::OutputStream & outStream, const Stream::InputStream & inStream, const uint32 amountToProcess = 0); 
        /** Continuous decompression process.
            Not all compressor support this (in that case, it's probably emulated or might return false).
            
            @param outStream        The output stream to write to.
            @param inStream         The input stream to read from.
            @param amountToProcess  The number of bytes to decompress on the output stream. Set to 0 to process the whole stream.
            @return true on success, false if not supported or an error occurred on the input stream */
        virtual bool decompressStream(Stream::OutputStream & outStream, const Stream::InputStream & inStream, const uint32 amountToProcess = 0);
        
        /** Default constructor */
        CommonZlib(const char * name) : BaseCompressor(name), lastError(Success), compressionFactor(-1), opaque(0), workBufferLength(0) {}
    };

    /** Implements ZLib compression. 
        Zlib is the public domain compression/decompression engine written by Gailly and Adler.
        It's used in the deflate algorithm in HTTP protocol. 
        It's made to compress stream, and not files. 
        
        It's (a bit) lighter than Gzip if you don't need to store the filename and modification 
        time of the file source.
        
        You can set the compression factor with setCompressionFactor() method.
        You can fetch the last processing error with getLastError() method. 
        
        The specification for this file format is in RFC1950 */
    class ZLib : public CommonZlib
    {
        // Interface
    public:
        /** Decompress data. 
            @param out      If not null, it points to a buffer of outSize bytes. Else, it's allocated and you must delete[] it.
            @param outSize  On input contains the size of the out buffer, on output contains the used size in the out buffer 
            @param in       The pointer to the compressed buffer
            @param inSize   The compressed buffer size in bytes 
            @return true on success, false on integrity checking mismatch or bad input buffer */
        virtual bool decompressData(uint8 *& out, size_t & outSize, const uint8 * in, const size_t inSize);
        /** Compress data.
            @param out      If not null, it points to a buffer of outSize bytes. Else, it's allocated and you must delete[] it.
            @param outSize  On input contains the size of the out buffer, on output contains the used size in the out buffer 
            @param in       The pointer to the uncompressed buffer
            @param inSize   The uncompressed buffer size in bytes 
            @return true on success, false bad input buffer */
        virtual bool compressData(uint8 *& out, size_t & outSize, const uint8 * in, const size_t inSize);
        
        
        // Helpers
    public:
        /** Implementation has to do something specific */ 
        virtual void setCompressionFactor(const float factor = 1.0f);
        /** Reset the object for a specific operation */
        virtual void Reset(const bool isCompressing);
        
        // Construction and destruction
    public:
        /** ZLib construction */
        ZLib();
        ~ZLib();
    };
    
    /** Implements GZip compression. 
        GZip is the public domain compression/decompression engine written by Gailly and Adler.
        It's used in the gzip algorithm in HTTP protocol. 
        It's made to store files, and it also stores metadata whenever appropriate.
        
        You can set the compression factor with setCompressionFactor() method.
        You can fetch the last processing error with getLastError() method.
        You can set specific file information with setFileSourceInfo() and retrieve them with 
        getFileName() and getFileTime()
        
        @warning If you intend to use the same compressor for different files, you must call 
                 "Reset()" between each file in order to prepare the stream 
        
        @warning Due to limitation in the GZip format, you can't store more than 4GB of data at once.
                 If you need to do so, you'd split your data first.
        
        The specification for this file format is in RFC1952. */
    class GZip : public CommonZlib
    {
        
        // Members
    private:
        /** The file information (used here to avoid memory allocation in the opaque structure) */
        String      fileName;
        /** The file time in second since Epoch */
        double      modifTime;
        /** The expected file size */
        uint32      expectedFileSize;
        
    
        // Interface
    public:
        /** Set the file source information */
        void setFileSourceInfo(const File::Info & info);
        /** Set the file source information */
        void setFileSourceInfo(const String & name, const double modifTime);
        /** Get the linked file name */
        inline const String & getFileName() const { return fileName; }
        /** Get the last modification time */
        inline const double getFileTime() const { return modifTime; }
        /** Reset the object for a specific operation */
        virtual void Reset(const bool isCompressing);
        
        
        
        /** Decompress data. 
            @param out      If not null, it points to a buffer of outSize bytes. Else, it's allocated and you must delete[] it.
            @param outSize  On input contains the size of the out buffer, on output contains the used size in the out buffer 
            @param in       The pointer to the compressed buffer
            @param inSize   The compressed buffer size in bytes 
            @return true on success, false on integrity checking mismatch or bad input buffer */
        virtual bool decompressData(uint8 *& out, size_t & outSize, const uint8 * in, const size_t inSize);
        /** Compress data.
            @param out      If not null, it points to a buffer of outSize bytes. Else, it's allocated and you must delete[] it.
            @param outSize  On input contains the size of the out buffer, on output contains the used size in the out buffer 
            @param in       The pointer to the uncompressed buffer
            @param inSize   The uncompressed buffer size in bytes 
            @return true on success, false bad input buffer */
        virtual bool compressData(uint8 *& out, size_t & outSize, const uint8 * in, const size_t inSize);
        
        /** Implementation has to do something specific */ 
        virtual void setCompressionFactor(const float factor = 1.0f);

        /** Specific implementation for getting the special info like file name and time */        
        bool decompressStream(Stream::OutputStream & outStream, const Stream::InputStream & inStream, const uint32 amountToProcess = 0);

        // Construction and destruction
    public:
        /** Gzip construction */
        GZip();
        ~GZip();
    };    
    
}
#endif

#endif
