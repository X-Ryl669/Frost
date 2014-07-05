// We need our declaration
#include "../../include/Compress/BSCLib.hpp"

#if (WantCompression == 1 && WantBSCCompression == 1)
// We need EBSC declaration
#include "../../Externals/ebsc/EBSC.hpp"
#include "../../include/Utils/HeapBlock.hpp"
// We need Assert too
#include "../../include/Utils/Assert.hpp"

namespace Compression
{
    // Default parameters, from the command line.
    // blockSize = 25*1024*1024;
    // coder = Adaptive;
    // bscFileSignature = { 'b', 's', 'c', 0x31 };
    // BSC files are made of:
    // bscSign    . 4 bytes
    // blockCount . 4 bytes
    // For each block:
    //  - Block header: inputOffset    . 8 bytes
    //                  recordSize     . 1 byte 
    //                  sortingContext . 1 byte
    //  - Header . 28 bytes
    //  - data   . ... bytes
    
#if 0
    bool BSCLib::decompressData(uint8 *& out, size_t & outSize, const uint8 * in, const size_t inSize)
    {
        // We need to first read the number of blocks
        if (!in || inSize < sizeof(uint32)) return setError(BadFormat);
        uint32 nBlocks = 0;
        size_t processedSize = 0;
        #define POP(X, S) if ((processedSize + S) <= inSize) { memcpy(X, &in[processedSize], S); processedSize += S; } else return setError(UnexpectedEOD); 
        POP(&nBlocks, sizeof(nBlocks));

        bool dryRun = out == 0;
        size_t requiredOut = 0;
        int8 recordSize = 1, sortingContext = 1;
        int64 inPos = 0;
        
        Utils::HeapBlock workBuffer(BSC::HeaderSize);
        size_t bufferSize = BSC::HeaderSize;
        
        uint32 curBlock = 0;
        do 
        {
            // Check the block header
#ifndef BreakCompatibility
            POP(&inPos, sizeof(inPos));
            POP(&recordSize, sizeof(recordSize));
            POP(&sortingContext, sizeof(sortingContext));
#endif
            // Unsupported features ?
            if (recordSize < 1 || sortingContext < 1 || sortingContext > 2) return setError(BadFormat);
            
            // Read the block header
            POP(workBuffer, BSC::HeaderSize);
            
            // Figure out the block informations
            size_t blockSize = 0, dataSize = 0;
            if (opaque->getBlockInfo(workBuffer, BSC::HeaderSize, blockSize, dataSize) != BSC::Success)
                return setError(BadFormat);

            // From now, we need to allocate the work buffer
            if (blockSize > bufferSize) bufferSize = blockSize;
            if (dataSize > bufferSize)  bufferSize = dataSize;
            if (!workBuffer.resize(bufferSize)) return setError(NotEnoughMemory);
            
            POP(workBuffer + BSC::HeaderSize, blockSize - BSC::HeaderSize);
            
            if (dataSize != 0)
            {
                // Now decompress the cruft
                BSC::Error err = opaque->decompress(workBuffer, blockSize, workBuffer, dataSize);
                if (err != BSC::Success)
                {
                    return err == BSC::NotEnoughMemory ? setError(NotEnoughMemory) : setError(DataCorrupt);
                }
                
                // Depending on the sorting context, we might need to reverse the blocks
                err = opaque->postProcess(workBuffer, dataSize, sortingContext, recordSize);
                if (err != BSC::Success)
                {
                    return err == BSC::NotEnoughMemory ? setError(NotEnoughMemory) : setError(DataCorrupt);
                }
                
                // Write data now!
                if (out && requiredOut + dataSize <= outSize)
                    memcpy(&out[requiredOut], workBuffer, dataSize);
                    
                requiredOut += dataSize;
            }
            curBlock++;
            if (curBlock == nBlocks || dataSize == 0)
            {
                // Check for dry run
                if (dryRun)
                {
                    if (!out && outSize) { outSize = requiredOut; return setError(UnexpectedEOD); }
                    outSize = requiredOut;
                    delete[] out;
                    out = new uint8[requiredOut];
                    if (!out) return setError(NotEnoughMemory);
                    // Ok, try again for real now
                    dryRun = false;
                    curBlock = 0; requiredOut = 0; processedSize = sizeof(nBlocks);
                } else break; // Done
            }
        } while (true);
#undef POP
        return setError(Success);
    }

    bool BSCLib::compressData(uint8 *& out, size_t & outSize, const uint8 * in, size_t inSize)
    {
        size_t blockSize = min(inSize, (size_t)getBufferSize());
        uint32 nBlocks = blockSize > 0 ? (uint32)((inSize + blockSize - 1) / blockSize) : 0;
        // Prepare block processing
        Utils::HeapBlock buffer(blockSize + BSC::HeaderSize);
        if (!buffer) return setError(NotEnoughMemory);

        // We skip the file signature, since we have no idea if this will be used for the file.
        
        bool dryRun = out == 0;
        size_t requiredOut = 0;
        int8 recordSize = 1, sortingContext = 1;
        do
        {
            uint8 * ptrOut = out;
        #define PUSH(X, S) if (ptrOut && (requiredOut + S) <= outSize) { memcpy(ptrOut, X, S); ptrOut += S; } requiredOut += S;
            PUSH(&nBlocks, sizeof(nBlocks));
            
            // We need to process blocks now
            size_t processedSize = 0;
            while (processedSize < inSize)
            {
                int compSize = opaque->compress(&in[processedSize], buffer, blockSize, BSC::defaultLZPHashSize, BSC::defaultLZPMinLen, BSC::defaultBlockSorter, BSC::QLFCAdaptive);
                if (compSize == BSC::NotCompressible)
                    compSize = opaque->store(&in[processedSize], buffer, blockSize);
                if (compSize < 0) return setError(DataCorrupt);
                
                // Block header for compatibility reason (could be removed)
#ifndef BreakCompatibility
                int64 inPos = processedSize;
                PUSH(&inPos, sizeof(inPos));
                PUSH(&recordSize, sizeof(recordSize));
                PUSH(&sortingContext, sizeof(sortingContext));
#endif
                PUSH(buffer, compSize);
#undef PUSH
                processedSize += blockSize;
            }

            if (dryRun)
            {
                if (!out && outSize) { outSize = requiredOut; return setError(UnexpectedEOD); }
                outSize = requiredOut;
                delete[] out;
                out = new uint8[requiredOut];
                if (!out) return setError(NotEnoughMemory);
                // Ok, try again for real now
                dryRun = false;
                requiredOut = 0;
                processedSize = 0;
            }
            else break;
        } while (true);
        outSize = requiredOut;
        return setError(Success);
    }
    
#endif

    // It's implemented with two heads, with no circularity for simplicity
    // When refilling the remaining data is moved to front, so for maximum
    // performance, you should empty the buffer completely
    struct BSCLib::MemoryBuffer
    {
        Utils::HeapBlock buffer;
        size_t           total;
        size_t           fill;
        size_t           consumed;
    
        operator uint8 * () { return &((uint8*)buffer)[consumed]; }
        const size_t available() const { return fill - consumed; }
        bool canFit(const size_t amount) const { return available() + amount <= total; }
        bool full() const { return available() == total; }
        
        void use(const size_t amount) { consumed += amount; if (consumed > fill) consumed = fill; }
        size_t refill(const Stream::InputStream & is, size_t readPossible)
        {
            if (!canFit(readPossible))
                readPossible = total - available();
            if (readPossible <= 0) return 0;
            
            // Move the data that was consumed out of buffer
            if (consumed)
            {
                memmove((uint8*)buffer, &((uint8*)buffer)[consumed], fill - consumed);
                fill -= consumed;
            }
            consumed = 0;
            
            uint64 ret = is.read(&((uint8*)buffer)[fill], readPossible);
            if (ret != (uint64)-1) fill += (size_t)ret;
            return (size_t)ret;
        }
        void setFill(const size_t amount) { fill = min(amount, total); }
        bool resize(const size_t size) { if (buffer.resize(size)) { total = size; empty(); return true; } return false; }
        void empty() { fill = consumed = 0; }
        
        
        MemoryBuffer(const size_t size) : buffer(size), total(size), consumed(0), fill(0) {}
    };
    
    bool BSCLib::compressData(uint8 *& out, size_t & outSize, const uint8 * in, size_t inSize)
    {
        Stream::MemoryBlockStream inStream(in, inSize);
        if (out == 0)
        {
            Stream::NullOutputStream outStream;
            if (!compressStream(outStream, inStream)) return false;
            if (outSize) { outSize = (size_t)outStream.fullSize(); return setError(UnexpectedEOD); }
            // Then allocate the output buffer size
            out = new uint8[outStream.fullSize()];
            outSize = (size_t)outStream.fullSize();
            inStream.setPosition(0);
        }
        // Compress the stream now
        Stream::MemoryBlockOutStream finalStream(out, (uint64)outSize);
        return compressStream(finalStream, inStream);
    }
    
    bool BSCLib::decompressData(uint8 *& out, size_t & outSize, const uint8 * in, const size_t inSize)
    {
        Stream::MemoryBlockStream inStream(in, inSize);
        if (out == 0)
        {
            Stream::NullOutputStream outStream;
            if (!decompressStream(outStream, inStream)) return false;
            if (outSize) { outSize = (size_t)outStream.fullSize(); return setError(UnexpectedEOD); }
            // Then allocate the output buffer size
            out = new uint8[outStream.fullSize()];
            outSize = (size_t)outStream.fullSize();
            inStream.setPosition(0);
        }
        // Compress the stream now
        Stream::MemoryBlockOutStream finalStream(out, (uint64)outSize);
        return decompressStream(finalStream, inStream);
    }    
    
    BSCLib::BSCLib(const uint64 dataSize) : BaseCompressor("BSC"), compressionFactor(-1), lastError(Success), memBuffer(new MemoryBuffer(getBufferSize())), outBuffer(new MemoryBuffer(getBufferSize())), dataSize((int64)dataSize < 0 ? 0 : -(int64)dataSize), headerWritten(false), opaque(0)
    {
        opaque = new BSC::EBSC();
    }
    BSCLib::~BSCLib() { delete0(opaque); delete0(memBuffer); delete0(outBuffer); }
    
    void BSCLib::resizeBuffer(const size_t add)
    {
        if (!memBuffer || !outBuffer) return;
        if (!memBuffer->resize(getBufferSize() + add))
            delete0(memBuffer);
        else if (!outBuffer->resize(getBufferSize() + (add == 0 ? BSC::HeaderSize : 0)))
            delete0(outBuffer);
    }
    /*
    int BSCLib::refillBuffer(const Stream::InputStream & inStream, const uint32 inSize)
    {
        memBlock.stripTo(0);
        int readByte = (int)(int64)inStream.read(memBlock.getBuffer(), inSize);
        if (readByte > 0) memBlock.Append(0, readByte);
        return readByte;
    }*/
    
    bool BSCLib::processBlock(Stream::OutputStream & outStream)
    {
        int8 recordSize = 1, sortingContext = 1;
#define PUSH(X, S) if (outStream.write(X, (uint64)S) != (uint64)S) return setError(UnexpectedEOD);
        // Read the input stream
        size_t readSize = (int)memBuffer->available();
        if (readSize <= 0) return setError(Success);
        
        int compSize = opaque->compress(*memBuffer, *outBuffer, (int)readSize, BSC::defaultLZPHashSize, BSC::defaultLZPMinLen, BSC::defaultBlockSorter, BSC::QLFCAdaptive);
        if (compSize == BSC::NotCompressible)
            compSize = opaque->store(*memBuffer, *outBuffer, (int)readSize);
        // Ok, it's now consumed
        memBuffer->use(readSize);
        if (compSize < 0) return setError(DataCorrupt);
        
        // Block header for compatibility reason (could be removed)
#ifndef BreakCompatibility
        PUSH(&dataSize, sizeof(dataSize));
        PUSH(&recordSize, sizeof(recordSize));
        PUSH(&sortingContext, sizeof(sortingContext));
#endif
        PUSH((uint8*)*outBuffer, compSize);
        dataSize += readSize;
        return setError(Success);
    }
    
    bool BSCLib::compressStream(Stream::OutputStream & outStream, const Stream::InputStream & inStream, const uint32 amountToProcess, const bool lastCall)
    {
        if (!memBuffer || !outBuffer) return setError(NotEnoughMemory);
        // If we were decompressing, let's change that
        if (decHeader.valid) { resizeBuffer(); decHeader.valid = false; }
        // Detect end of buffer
        // Try to be smart, else try to read as much as possible
        uint64 inSize = amountToProcess ? min((uint64)amountToProcess, inStream.fullSize()) : inStream.fullSize();
        if (inSize == 0)
        {
            if (!dataSize && !memBuffer->available()) return true;
            // Need to process the remaining block, if any
            if (!processBlock(outStream)) return false;
            // End of stream, let's rewind and write the header
            if (outStream.setPosition(0))
            {
                uint32 nBlocks = (uint32)((dataSize + getBufferSize() - 1) / getBufferSize());
                PUSH(&nBlocks, sizeof(nBlocks));
                dataSize = 0;
                headerWritten = false;
            }
            return true;
        }

        // Check if we need to write the number of blocks now (or not)
        if (!headerWritten)
        {
            // Safety first
            if (dataSize == 0 && !outStream.setPosition(outStream.currentPosition()))
            {
                // If you don't provide the input size and the output stream can not be rewind to save
                // the header in the end, then it'll not work.
                Assert(dataSize == 0 && outStream.setPosition(outStream.currentPosition()));
                return false;
            }
        
            uint32 nBlocks = (uint32)((-dataSize + getBufferSize() - 1) / getBufferSize()); //blockSize > 0 ? (uint32)((inSize + blockSize - 1) / blockSize) : 0;
            PUSH(&nBlocks, sizeof(nBlocks));
            dataSize = 0;
            headerWritten = true;
        }
        
        // Check if we can cache the data to avoid small compression block
        if (!lastCall && memBuffer->canFit(inSize))
        {   // Accumulate the input stream
            return memBuffer->refill(inStream, inSize) == inSize;
        }

        while ((int64)inSize > 0)
        {
            // Read as much as possible to the memory buffer, and save that
            size_t readSize = memBuffer->available();
            if (!memBuffer->full())
            {
                readSize = memBuffer->refill(inStream, inSize);
                inSize -= (uint64)readSize;
            }
            if (memBuffer->available() >= getBufferSize() || lastCall)
            {
                if (!processBlock(outStream)) return false;
            }
        }
        return setError(Success);
    }
    
    
    /** Continuous decompression process.
        Not all compressor support this (in that case, it's probably emulated or might return false).

        @param outStream        The output stream to write to.
        @param inStream         The input stream to read from.
        @param amountToProcess  The number of decompressed bytes to reach. Set to 0 to process the whole stream.
        @return true on success, false if not supported or an error occurred on the input stream */
    bool BSCLib::decompressStream(Stream::OutputStream & outStream, const Stream::InputStream & inStream, const uint32 amountToProcess)
    {
        // We need to first read the number of blocks
        if (!memBuffer || !outBuffer) return setError(NotEnoughMemory);

        const size_t minBlockHeaderSize = (sizeof(dataSize) + sizeof(decHeader.recordSize) + sizeof(decHeader.sortingContext) + BSC::HeaderSize);
        if (memBuffer->total <= outBuffer->total)
        {
            // If we were compressing, let's enlarge that
            resizeBuffer(minBlockHeaderSize);
            decHeader.curBlock = 0;
        }
        
        uint32 nBlocks = (uint32)dataSize;
        
#define POP(X, S) if (inStream.read(X, (uint64)S) != (uint64)S) return setError(Success); // No more data to read
        if (dataSize <= 0)
        {
            POP(&nBlocks, sizeof(nBlocks));
            dataSize = nBlocks; // Crude off estimation
            memBuffer->empty();
        }
        if (!nBlocks) return setError(BadFormat);
        
        // Check if we have enough data in our memory block to answer directly
        uint64 outSize = amountToProcess ? min((uint64)amountToProcess, outStream.fullSize()) : outStream.fullSize();
        if (outSize > 0xFFFFFFFF) outSize = 0xFFFFFFFF; // We can only deal with 4GB at a time

        // If we reach here, we have not enough data in the memory block (we need to refill it),
        int64 inPos = 0;
        
        do
        {
            // Check if we have a valid header
            if (!decHeader.valid && decHeader.curBlock < nBlocks)
            {
                if (memBuffer->available() < minBlockHeaderSize
                    && (memBuffer->refill(inStream, minBlockHeaderSize - memBuffer->available()) == (size_t)-1 || memBuffer->available() < minBlockHeaderSize))
                    return setError(UnexpectedEOD);
                
                // The decode the header
#define POPM(X, S) memcpy(X, *memBuffer, S); memBuffer->use(S);

    #ifndef BreakCompatibility
                POPM(&inPos, sizeof(inPos));
                POPM(&decHeader.recordSize, sizeof(decHeader.recordSize));
                POPM(&decHeader.sortingContext, sizeof(decHeader.sortingContext));
    #endif
                // Unsupported features ?
                if (decHeader.recordSize < 1 || decHeader.sortingContext < 1 || decHeader.sortingContext > 2) return setError(BadFormat);
                
                // Figure out the block informations
                if (opaque->getBlockInfo(*memBuffer, BSC::HeaderSize, decHeader.blockSize, decHeader.dataSize) != BSC::Success)
                    return setError(BadFormat);
                
                //memBuffer->empty();
                decHeader.valid = true;
            }
            // Check if we have decompressed data ready for output stream direct write
            if (outBuffer->available())
            {
                // Data is ready in the buffer, let's extract it
                size_t doneSize = min(outBuffer->available(), (size_t)outSize);
                if (outStream.write(*outBuffer, doneSize) != (uint64)doneSize) return setError(UnexpectedEOD);
                outBuffer->use(doneSize);
                
                if (doneSize == outSize) break; // Success
                outSize -= doneSize;
                // If we reach here, then the outBuffer is empty anyway, let's mark as is
                outBuffer->empty();
                
                // Need to refill a header now
                continue;
            }

            // We need to fetch a complete buffer from the input stream
            size_t need = decHeader.blockSize - memBuffer->available();
            if (memBuffer->refill(inStream, need) != need)
            {
                // Could not read enough from the input stream to fill a block.
                // Let's output the amount we succeed to write from it.
                if (amountToProcess - outSize > 0) return setError(Success);
                return setError(UnexpectedEOD);
            }
            
            if (decHeader.dataSize != 0 && decHeader.curBlock < dataSize)
            {
                // Now decompress the cruft
                BSC::Error err = opaque->decompress(*memBuffer, decHeader.blockSize, *outBuffer, decHeader.dataSize);
                if (err != BSC::Success)
                    return err == BSC::NotEnoughMemory ? setError(NotEnoughMemory) : setError(DataCorrupt);
                
                // Depending on the sorting context, we might need to reverse the blocks
                err = opaque->postProcess(*outBuffer, decHeader.dataSize, decHeader.sortingContext, decHeader.recordSize);
                if (err != BSC::Success)
                    return err == BSC::NotEnoughMemory ? setError(NotEnoughMemory) : setError(DataCorrupt);
                
                outBuffer->setFill(decHeader.dataSize);
                memBuffer->empty(); // Done with this data buffer
                decHeader.valid = false; // The block header is now invalid too
                decHeader.curBlock++;
            }
            else break; // No more data to decompress
        } while (true);
#undef POP
        // Check if we've finished sending the complete decompressed input stream
        if (decHeader.curBlock == dataSize && outBuffer->available() == 0)
        {
            // We can reset the decompression stream if it needs to be re-used
            decHeader.curBlock = 0;
            dataSize = 0;
        }
        if (outBuffer->available() == 0) outBuffer->empty();
        return setError(Success);
    }
    
}


#endif


