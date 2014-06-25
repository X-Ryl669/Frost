// We need our declaration
#include "../../include/Compress/ZLib.hpp"

#if (WantCompression == 1)
#define EZ_COMPRESSMAXDESTLENGTH(n) (n+(((n)/1000)+1)+12)

extern "C" 
{
    int ezcompress( unsigned char* pDest, long* pnDestLen, const unsigned char* pSrc, long nSrcLen, const int factor );
    int ezuncompress( unsigned char* pDest, long* pnDestLen, const unsigned char* pSrc, long nSrcLen );
    int ezgzcompress( z_stream * stream, unsigned char* pDest, long* pnDestLen, const unsigned char* pSrc, long * nSrcLen, const int factor, const int endNow, const int skipLoop);
    int ezgzuncompress( z_stream * stream, unsigned char* pDest, long* pnDestLen, const unsigned char* pSrc, long * nSrcLen, const int endNow, const int skipLoop);
    
    int setDeflateState( z_stream * ptr, const char * fileName, const unsigned int time );
    int getDeflateState( z_stream * ptr, unsigned int * time );
    int getInflateState( z_stream * ptr, char * fileName, unsigned int * time );
    z_stream * initGZStream(int factor);
    void releaseGZStream(z_stream *);
    z_stream * initZStream(int factor, int withHeader);
    void releaseZStream(z_stream *);
    int isDecompressing(z_stream *);
}

namespace Compression
{
    bool ZLib::decompressData(uint8 *& out, size_t & outSize, const uint8 * in, const size_t inSize)
    {
        unsigned char ch = 0;
        unsigned char * dest = out ? out : &ch;
        long destLen = (long)(out ? outSize : sizeof(ch));
        lastError = (Error)ezuncompress(dest, &destLen, in, (long)inSize);
        if (lastError != BufferError && lastError != Success) return false;
        if (!out || (long)outSize < destLen)
        {
            if (!out && outSize) { outSize = destLen; lastError = MemoryError; return false; }
            outSize = destLen;
            delete[] out;
            out = new uint8[outSize];
            if (!out) { lastError = MemoryError; return false; }
        }
        lastError = (Error)ezuncompress(out, &destLen, in, (long)inSize);
        return lastError == Success;
    }
    bool ZLib::compressData(uint8 *& out, size_t & outSize, const uint8 * in, const size_t inSize)
    {
        unsigned char ch = 0;
        unsigned char * dest = out ? out : &ch;
        long destLen = (long)(out ? outSize : sizeof(ch));
        lastError = (Error)ezcompress(dest, &destLen, in, (long)inSize, compressionFactor);
        if (lastError != BufferError && lastError != Success) return false;
        if (!out || (long)outSize < destLen)
        {
            if (!out && outSize) { outSize = destLen; lastError = MemoryError; return false; }
            outSize = destLen;
            delete[] out;
            out = new uint8[outSize];
            if (!out) { lastError = MemoryError; return false; }
        }
        lastError = (Error)ezcompress(out, &destLen, in, (long)inSize, compressionFactor);
        return lastError == Success;
    }
    
    ZLib::ZLib() : CommonZlib("zlib")
    {
        opaque = initZStream(-1, 1);
    }
    ZLib::~ZLib() { releaseZStream(opaque); }
    void ZLib::setCompressionFactor(const float factor)
    {
        CommonZlib::setCompressionFactor(factor);
        releaseZStream(opaque);
        opaque = initZStream(min(compressionFactor, 9), (factor != 2.0f)); // Factor of 2 means headerless
    }
    void ZLib::Reset(const bool isCompressing)
    {
        if (isDecompressing(opaque) != (isCompressing ? 0 : 1))
        {
            releaseZStream(opaque);
            opaque = initZStream(isCompressing ? min(compressionFactor, 9) : -2, compressionFactor != 18);
        }
    }

    
    // Set the file source information 
    void GZip::setFileSourceInfo(const File::Info & info) { fileName = info.getFullPath(); setDeflateState(opaque, fileName, (unsigned int)(modifTime = info.modification)); }
    void GZip::setFileSourceInfo(const String & name, const double _modifTime) { fileName = name; modifTime = _modifTime; setDeflateState(opaque, fileName, (unsigned int)modifTime); }
    void GZip::setCompressionFactor(const float factor) 
    { 
        compressionFactor = (int)(factor * 9 + .5f); 
        Reset(true); 
    }
    void GZip::Reset(const bool isCompressing)
    {
        expectedFileSize = 0; 
        releaseGZStream(opaque); 
        workBufferLength = 0;
        opaque = initGZStream(isCompressing ? compressionFactor : -2); 
        if (isCompressing) setDeflateState(opaque, fileName, (unsigned int)modifTime); 
    }
    
    
    GZip::GZip() : CommonZlib("gzip"), modifTime(0), expectedFileSize(0)
    {
        opaque = initGZStream(-1);
    }
    GZip::~GZip() { releaseGZStream(opaque); }
    
    bool GZip::decompressData(uint8 *& out, size_t & outSize, const uint8 * in, const size_t _inSize)
    {
        // Need to exchange the opaque object to a decompression one
        Reset(false);
        expectedFileSize = in ? *(uint32*)&in[_inSize - 4] : 0;

        unsigned char ch = 0;
        unsigned char * dest = out ? out : &ch;
        long destLen = (long)(out ? outSize : sizeof(ch)), inSize = (long)_inSize;
        lastError = (Error)ezgzuncompress(opaque, dest, &destLen, in, &inSize, 1, 0);
        if (lastError != BufferError && lastError != Success) return false;
        
        if (!out || (long)outSize < destLen)
        {
            if (!out && outSize) { outSize = destLen; lastError = MemoryError; return false; }
            outSize = destLen;
            delete[] out;
            out = new uint8[outSize];
            if (!out) { lastError = MemoryError; return false; }
            // Force recreating the stream now
            Reset(false);
            inSize = (long)_inSize;
        }
        lastError = (Error)ezgzuncompress(opaque, out, &destLen, in, &inSize, 1, 0);

        // Fetch the expected filename
        unsigned int time = 0;
        char filename[256] = {0};
        getInflateState(opaque, filename, &time);
        fileName = filename; 
        modifTime = time;
        return lastError == Success;
    }
    bool GZip::compressData(uint8 *& out, size_t & outSize, const uint8 * in, const size_t _inSize)
    {
        unsigned char ch = 0;
        unsigned char * dest = out ? out : &ch;
        long destLen = (long)(out ? outSize : sizeof(ch)), inSize = (long)_inSize;
        lastError = (Error)ezgzcompress(opaque, dest, &destLen, in, &inSize, compressionFactor, 1, 0);
        if (lastError != BufferError && lastError != Success) return false;
        
        if (!out || (long)outSize < destLen)
        {
            if (!out && outSize) { outSize = destLen; lastError = MemoryError; return false; }
            outSize = destLen;
            delete[] out;
            out = new uint8[outSize];
            if (!out) { lastError = MemoryError; return false; }
            dest = out; destLen = (uint32)outSize;
            // Force recreating the stream now
            Reset(true);
            inSize = (long)_inSize;
        }
        
        
        lastError = (Error)ezgzcompress(opaque, dest, &destLen, in, &inSize, compressionFactor, 1, 0);
        return lastError == Success;
    }

    bool GZip::decompressStream(Stream::OutputStream & outStream, const Stream::InputStream & inStream, const uint32 amountToProcess)
    {
        // Check if we are compressing or decompressing
        bool ret = CommonZlib::decompressStream(outStream, inStream, amountToProcess);
        if (ret) 
        {
            unsigned int time = 0;
            char filename[256] = {0};
            getInflateState(opaque, filename, &time);
            fileName = filename; 
            modifTime = time;
        }
        return ret;
    }

    // Required 
    const float CommonZlib::HeaderLess = 2.0f;
    bool CommonZlib::decompressStream(Stream::OutputStream & outStream, const Stream::InputStream & inStream, uint32 amountToProcess)
    {
        // Special cases first
        if (!opaque) return false;
        if (!isDecompressing(opaque))
            Reset(false);
        
        // We are either limited to the quantity of output data to process or the whole input stream
        bool consumeAllInput = amountToProcess == 0;

        // Read some data to decompress
        uint8 baseBuffer[8192];
        uint32 inputAmount = consumeAllInput ? (uint32)inStream.fullSize() : sizeof(baseBuffer);
        
        // That fills the buffer of the min(buffer size, amountLeft)
        uint8 * buffer = baseBuffer; uint64 inSize = 0;

        while (consumeAllInput || amountToProcess > 0)
        {
            // Flush the output, if any first
            uint32 outAmount = min(workBufferLength, amountToProcess);
            if (outStream.write(workBuffer, outAmount) != outAmount) return false;
            if (amountToProcess <= outAmount)
            {
                memmove(workBuffer, &workBuffer[amountToProcess], workBufferLength - amountToProcess);
                workBufferLength -= outAmount;
                return true; // Done decompressing the asked size
            }
            workBufferLength = 0;
            amountToProcess -= outAmount;
            
            unsigned char * dest = workBuffer;
            long destLen = (long)sizeof(workBuffer);
            
            // Check if there is still some data required
            if (!inSize)
            {
                buffer = baseBuffer;
                inSize = inStream.read(buffer, (uint64)min((uint32)sizeof(baseBuffer), inputAmount));
                if (inSize == (uint64)-1) return false;
                if (inSize == 0) // No more data in input
                    break;
            }
                
            long dataSize = (long)inSize;
            lastError = (Error)ezgzuncompress(opaque, dest, &destLen, buffer, &dataSize, 0, 1);
            workBufferLength = destLen;

            if (lastError != Success && lastError != EndOfStream)
                return false;
            
            // Check if we actually got any data, and if not, let's exit the loop anyway
            if (lastError == EndOfStream)
            {
                if (destLen)
                {
                    destLen = min(amountToProcess, (uint32)destLen);
                    if (outStream.write(dest, destLen) != destLen) return false;
                    memmove(workBuffer, &workBuffer[destLen], workBufferLength - destLen);
                    workBufferLength -= destLen;
                }
                return true;
            }
                
            // Adjust buffer head
            buffer += inSize - (uint64)dataSize;
            if (amountToProcess < workBufferLength)
            {
                // Need to copy the decoded part
                if (outStream.write(dest, amountToProcess) != amountToProcess) return false;
                memmove(workBuffer, &workBuffer[amountToProcess], workBufferLength - amountToProcess);
                workBufferLength -= amountToProcess;
                return true; // Done decompressing the asked size
            }
            inSize = (uint64)dataSize;
        }
        
        // Flush the stream ?
        if (inStream.fullSize() == 0) 
        {
            // Need to flush the output now
            while (1)
            {
                // Flush the output
                if (outStream.write(workBuffer, workBufferLength) != workBufferLength) return false;
                workBufferLength = 0;
                
                // Then compress and finish the output
                unsigned char * dest = workBuffer;
                long destLen = sizeof(workBuffer), dataSize = 0;
                lastError = (Error)ezgzuncompress(opaque, dest, &destLen, 0, &dataSize, 1, 1);
                workBufferLength = destLen;

                if (lastError == EndOfStream)
                    return (workBufferLength = (uint32)outStream.write(dest, (uint64)destLen)) != (uint64)destLen;
                if (lastError != Success)
                    return false;
            }
        }
        
        return true;
    }

    bool CommonZlib::compressStream(Stream::OutputStream & outStream, const Stream::InputStream & inStream, uint32 amountToProcess, const bool lastCall)
    {
        // Special cases first
        if (!opaque) return false;
        
        if (!amountToProcess) amountToProcess = (uint32)inStream.fullSize();

        // Read some data to compress
        uint8 baseBuffer[8192];
        uint8 * buffer = baseBuffer;
        uint64 inSize = inStream.read(buffer, (uint64)min((uint32)sizeof(baseBuffer), amountToProcess));
        if (inSize == (uint64)-1) return false;
        
        while (amountToProcess)
        {
            // Flush the output first
            if (outStream.write(workBuffer, workBufferLength) != workBufferLength) return false;
            workBufferLength = 0;
            
            unsigned char * dest = workBuffer;
            long destLen = sizeof(workBuffer);
            
            
            if (!inSize)
            {   // Refill the input buffer
                buffer = baseBuffer;
                inSize = inStream.read(buffer, (uint64)min((uint32)sizeof(baseBuffer), amountToProcess));
                if (inSize == (uint64)-1) return false;
            }
            long dataSize = (long)inSize;
            lastError = (Error)ezgzcompress(opaque, dest, &destLen, buffer, &dataSize, compressionFactor, 0, 1);
            workBufferLength = destLen;

            if (lastError == EndOfStream)
            {
                workBufferLength = 0;
                return (uint32)outStream.write(dest, (uint64)destLen) != (uint64)destLen;
            }
            
            if (lastError != Success)
                return false;
                
            buffer += inSize - (uint64)dataSize;
            amountToProcess -= (uint32)inSize - (uint32)dataSize;
            inSize = (uint64)dataSize;
        }
        
        // Flush the stream ?
        if (inStream.fullSize() == 0) 
        {
            // Need to flush the output now
            while (1)
            {
                // Flush the output
                if (outStream.write(workBuffer, workBufferLength) != workBufferLength) return false;
                workBufferLength = 0;
                
                // Then compress and finish the output
                unsigned char * dest = workBuffer;
                long destLen = sizeof(workBuffer), dataSize = 0;
                lastError = (Error)ezgzcompress(opaque, dest, &destLen, 0, &dataSize, compressionFactor, 1, 1);
                workBufferLength = destLen;

                if (lastError == EndOfStream)
                    return (workBufferLength = (uint32)outStream.write(dest, (uint64)destLen)) != (uint64)destLen;
                if (lastError != Success)
                    return false;
            }
        }
        
        return true;
    } 
#if 0
        // Helpers
    public:
        /** The magic codes */
	    enum 
	    { 
	        Magic1 = 0x1f, 
	        Magic2 = 0x8b,   
	        
		    Deflated = 8, 
		    Slow     = 2, 
		    Fast     = 4, 
        };
        /** The used flags */
        enum Flags
        {
            AsciiText   = 1,    //!< Optional, mark the file as ascii text
		    CRC16Header = 2,    //!< Optional, a 16 bits CRC checksum for the header is present
		    ExtraFields = 4,    //!< Extra fields are present
		    Filename    = 8,    //!< A file name is present 
		    Comments    = 16,   //!< File comments are present too
        };
        /** Write header */
        size_t writeHeader(uint8 * out, size_t & outSize);
        /** Write footer */
        size_t writeFooter(uint8 * out, size_t & outSize);
        /** Consume headers & footers */
        bool consumeHeadersAndFooter(const uint8 * & out, size_t & outSize);
        /** Consume a given amount of bytes */
        bool consumeData(const uint8 * & in, size_t & inSize, void * out, size_t outSize);

    // Consume a given amount of bytes 
    bool GZip::consumeData(const uint8 * & in, size_t & inSize, void * out, size_t outSize)
    {
        if (inSize < outSize) return false;
        if (!in) return false;
        if (out) memcpy(out, in, outSize);
        in += outSize;
        inSize -= outSize;
        return true;
    }

    // Consume headers & footers
    bool GZip::consumeHeadersAndFooter(const uint8 * & out, size_t & outSize)
    {
        if (!out || outSize < 18) return false;
        // Check the magic first
        if (out[0] != Magic1 || out[1] != Magic2) return false;
        if (out[2] != Deflated) return false; // We only support Deflate compression method
        uint8 flag = out[3];
        if (flag & (225 | CRC16Header)) // Anything except supported flags
            return false;
        
        // Modification time
        out += 4; outSize -= 4;
        uint32 mtime; 
        if (!consumeData(out, outSize, &mtime, sizeof(mtime))) return false; 
        fileModifTime = (double)mtime;
        
        // We don't care about extra flags or OS type
        if (!consumeData(out, outSize, 0, 2)) return false;
        
        if (flag & ExtraFields)
        {
            uint16 length; 
            if (!consumeData(out, outSize, &length, sizeof(length))) return false;
            // We drop the resulting length anyway
            if (!consumeData(out, outSize, 0, length)) return false;
        }
        
        if (flag & Filename)
        {   
            size_t nameLen = 0;
            while (out[nameLen++] != 0 && nameLen < outSize);
            if (nameLen == outSize) return false;
            
            fileName = String(out, (int)nameLen);
            out += nameLen; outSize -= nameLen; 
        }
        
        if (flag & Comments)
        {   
            size_t nameLen = 0;
            while (out[nameLen++] != 0 && nameLen < outSize);
            if (nameLen == outSize) return false;
            out += nameLen; outSize -= nameLen;
        }
        // Ok, we've read the header now, let's fill the expected stream size (Little Endian)
        if (outSize < 8) return false;
        expectedFileSize = *(uint32*)&out[outSize - 5];
        expectedChecksum = *(uint32*)&out[outSize - 9];
        // CRC check not done
        firstPart = false;
        return true;
    }

    size_t GZip::writeHeader(uint8 * out, size_t & outSize)
    {
        size_t requiredSize = 10 + (fileName ? fileName.getLength() + 1 : 0);
        if (!firstPart || outSize < requiredSize) return 0;
        size_t pos = 0;
#define PutW(T, X) { T _X = (T)X;  memcpy(&out[pos], &_X, sizeof(_X));  pos += sizeof(_X); }
        PutW(uint8, Magic1);
        PutW(uint8, Magic2);
        PutW(uint8, Deflated);
        PutW(uint8, (uint8)(fileName ? Filename : 0));
        PutW(uint32, fileModifTime)
        PutW(uint8, compressionFactor == 1 ? Fast : (compressionFactor == 9 ? Slow : 0)); 
        PutW(uint8, 255); // Don't try to fool the end-of-line

        if (fileName) 
        {
            memcpy(&out[pos], (const char*)fileName, fileName.getLength() + 1);
            pos += fileName.getLength()+1;
        }
        return pos;
    }
    
    size_t GZip::writeFooter(uint8 * out, size_t & outSize)
    {
        size_t pos = outSize - 8;
        uint8 outCRC[4] = {0};
        crc32.Finalize(outCRC);
        // Already inverted in the hasher
        PutW(uint8, outCRC[0]); PutW(uint8, outCRC[1]); PutW(uint8, outCRC[2]); PutW(uint8, outCRC[3]); 
        PutW(uint32, fullSize);
#undef PutW     
        return pos;
    }
#endif
    
}


#endif


