// We need our declaration
#include "../../include/Compress/BSCLib.hpp"

#if (WantCompression == 1 && WantBSCCompression == 1)
// We need EBSC declaration
#include "../../Externals/ebsc/EBSC.hpp"
#include "../../include/Utils/HeapBlock.hpp"


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
    
    BSCLib::BSCLib() : BaseCompressor("BSC"), compressionFactor(-1), lastError(Success), opaque(0)
    {
        opaque = new BSC::EBSC();
    }
    BSCLib::~BSCLib() { delete0(opaque); }
    
    bool BSCLib::compressStream(Stream::OutputStream & outStream, const Stream::InputStream & inStream, const uint32 amountToProcess)
    {
        // Try to be smart, else try to read as much as possible
        uint64 inSize = amountToProcess ? min((uint64)amountToProcess, inStream.fullSize()) : inStream.fullSize();
        size_t blockSize = (size_t)min(inSize, (uint64)getBufferSize());
        uint32 nBlocks = blockSize > 0 ? (uint32)((inSize + blockSize - 1) / blockSize) : 0;
        // Prepare block processing
        Utils::HeapBlock buffer(blockSize + BSC::HeaderSize);
        if (!buffer) return setError(NotEnoughMemory);
        Utils::HeapBlock inBuffer(blockSize + BSC::HeaderSize);
        if (!inBuffer) return setError(NotEnoughMemory);

        // We skip the file signature, since we have no idea if this will be used for the file.
        
        int8 recordSize = 1, sortingContext = 1;

#define PUSH(X, S) if (outStream.write(X, (uint64)S) != (uint64)S) return setError(UnexpectedEOD);
        PUSH(&nBlocks, sizeof(nBlocks));
        
        // We need to process blocks now
        uint64 processedSize = 0;
        while (processedSize < inSize)
        {
            // Read the input stream
            uint64 readSize = inStream.read(inBuffer, blockSize);
            if (readSize == (uint64)-1)
            {
                // We are done with reading, let's exit now
                return setError(Success);
            }
            int compSize = opaque->compress(inBuffer, buffer, readSize, BSC::defaultLZPHashSize, BSC::defaultLZPMinLen, BSC::defaultBlockSorter, BSC::QLFCAdaptive);
            if (compSize == BSC::NotCompressible)
                compSize = opaque->store(inBuffer, buffer, readSize);
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
            processedSize += readSize;
            if (readSize < blockSize) return setError(Success);
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
        uint32 nBlocks = 0;
        
        #define POP(X, S) if (inStream.read(X, (uint64)S) != (uint64)S) return setError(UnexpectedEOD); 
        POP(&nBlocks, sizeof(nBlocks));

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
                    return err == BSC::NotEnoughMemory ? setError(NotEnoughMemory) : setError(DataCorrupt);
                
                // Depending on the sorting context, we might need to reverse the blocks
                err = opaque->postProcess(workBuffer, dataSize, sortingContext, recordSize);
                if (err != BSC::Success)
                    return err == BSC::NotEnoughMemory ? setError(NotEnoughMemory) : setError(DataCorrupt);
                
                // Write data now!
                if (outStream.write(workBuffer, (uint64)dataSize) != (uint64)dataSize) return setError(UnexpectedEOD);
            }
            curBlock++;
            // Done ?
            if (curBlock == nBlocks || dataSize == 0)
                return setError(Success);
        
        } while (true);
#undef POP
        return setError(Success);
    }
    
}


#endif


