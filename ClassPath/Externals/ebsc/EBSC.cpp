// We need our interface declaration
#include "EBSC.hpp"
// We need the actual libbsc code
#include "internal.inc"
// We want to use our Adler32 implementation
#include "../../include/Hashing/Adler32.hpp"

namespace BSC
{
    static inline uint32 adler32(const uint8 * input, size_t n)
    {
        Hashing::Adler32 hash;
        hash.Start();
        hash.Hash(input, n);
        return hash.getChecksumLE();
    }
    

    int EBSC::store(const uint8 * input, uint8 * output, size_t n)
    {
        unsigned int adler32_data = adler32(input, n);


        memmove(output + HeaderSize, input, n);
        saveUint((output +  0), n + HeaderSize);
        saveUint((output +  4), n);
        saveUint((output +  8), 0);
        saveUint((output + 12), 0);
        saveUint((output + 16), adler32_data);
        saveUint((output + 20), adler32_data);
        saveUint((output + 24), adler32(output, 24));
        return n + HeaderSize;
    }

    int EBSC::compressInPlace(uint8 * data, const size_t n, int lzpHashSize, int lzpMinLen, BlockSorter blockSorter, Coder coder)
    {
        int     indexes[256];
        uint8   num_indexes;

        int mode = 0;

        switch (blockSorter)
        {
            case BWT : mode = BWT; break;
            default : return BadParameter;
        }

        switch (coder)
        {
            case QLFCStatic   : mode += (QLFCStatic   << 5); break;
            case QLFCAdaptive : mode += (QLFCAdaptive << 5); break;

            default : return BadParameter;
        }

        if (lzpMinLen != 0 || lzpHashSize != 0)
        {
            if (lzpMinLen < 4 || lzpMinLen > 255) return BadParameter;
            if (lzpHashSize < 10 || lzpHashSize > 28) return BadParameter;
            mode += (lzpMinLen << 8);
            mode += (lzpHashSize << 16);
        }
        if (n > 1073741824) return BadParameter;
        if (n <= HeaderSize)
        {
            return store(data, data, n);
        }

        unsigned int adler32_data = adler32(data, n);

        EOS lzSizeE = n; int lzSize = n;
        if (mode != (mode & 0xff))
        {
            uint8 * buffer = (uint8 *)LargeMalloc(n);
            if (buffer == NULL) return NotEnoughMemory;

            lzSizeE = lzp_compress(data, buffer, n, lzpHashSize, lzpMinLen, features);
            if (lzSizeE.getError() != Success)
            {
                lzSize = n; mode &= 0xff;
            }
            else
            {
                lzSize = lzSizeE.value();
                memcpy(data, buffer, lzSize);
            }

            LargeFree(buffer);
        }

        if (lzSize <= (int)HeaderSize)
        {
            blockSorter = BWT;
            mode = (mode & 0xffffffe0) | BWT;
        }

        EOS index = BadParameter; num_indexes = 0;
        switch (blockSorter)
        {
            case BWT : index = BWTEncode(data, lzSize, &num_indexes, indexes, features); break;
            default : return BadParameter;
        }

        if (n < 64 * 1024) num_indexes = 0;

        if (index.getError() != Success) return index.getError();

        if (uint8 * buffer = (uint8 *)LargeMalloc(lzSize + 4096))
        {
            EOS resultE = coder_compress(data, buffer, lzSize, coder, features);
            int result = resultE.value();
            if (resultE.getError() == Success) memcpy(data + HeaderSize, buffer, result);
            LargeFree(buffer);
            if ((resultE.getError() != Success) || (result + 1 + 4 * num_indexes >= (int)n))
            {
                return NotCompressible;
            }
            {
                if (num_indexes > 0)
                    memcpy(data + HeaderSize + result, indexes, 4 * num_indexes);
                data[HeaderSize + result + 4 * num_indexes] = num_indexes;
                result += 1 + 4 * num_indexes;
            }
            saveUint((data +  0), result + HeaderSize);
            saveUint((data +  4), n);
            saveUint((data +  8), mode);
            saveUint((data + 12), index.value());
            saveUint((data + 16), adler32_data);
            saveUint((data + 20), adler32(data + HeaderSize, result));
            saveUint((data + 24), adler32(data, 24));
            return result + HeaderSize;
        }

        return NotEnoughMemory;
    }

    int EBSC::compress(const uint8 * input, uint8 * output, const size_t n, int lzpHashSize, int lzpMinLen, BlockSorter blockSorter, Coder coder)
    {
        if (input == output)
        {
            return compressInPlace(output, n, lzpHashSize, lzpMinLen, blockSorter, coder);
        }

        int     indexes[256];
        uint8   num_indexes;

        int mode = 0;

        switch (blockSorter)
        {
            case BWT : mode = BWT; break;
            default : return BadParameter;
        }

        switch (coder)
        {
            case QLFCStatic   : mode += (QLFCStatic   << 5); break;
            case QLFCAdaptive : mode += (QLFCAdaptive << 5); break;

            default : return BadParameter;
        }

        if (lzpMinLen != 0 || lzpHashSize != 0)
        {
            if (lzpMinLen < 4 || lzpMinLen > 255) return BadParameter;
            if (lzpHashSize < 10 || lzpHashSize > 28) return BadParameter;
            mode += (lzpMinLen << 8);
            mode += (lzpHashSize << 16);
        }
        if (n > 1073741824) return BadParameter;
        if (n <= HeaderSize)
        {
            return store(input, output, n);
        }
        EOS lzSizeE = 0; int lzSize = 0;
        if (mode != (mode & 0xff))
        {
            lzSizeE = lzp_compress(input, output, n, lzpHashSize, lzpMinLen, features);
            if (lzSizeE.getError() != Success)
            {
                mode &= 0xff;
            }
            else lzSize = lzSizeE.value();
        }
        if (mode == (mode & 0xff))
        {
            lzSize = n; memcpy(output, input, n);
        }

        if (lzSize <= (int)HeaderSize)
        {
            blockSorter = BWT;
            mode = (mode & 0xffffffe0) | BWT;
        }

        EOS index = BadParameter; num_indexes = 0;
        switch (blockSorter)
        {
            case BWT : index = BWTEncode(output, lzSize, &num_indexes, indexes, features); break;
            default : return BadParameter;
        }

        if (n < 64 * 1024) num_indexes = 0;

        if (index.getError() != Success)
        {
            return index.getError();
        }

        if (uint8 * buffer = (uint8 *)LargeMalloc(lzSize + 4096))
        {
            EOS resultE = coder_compress(output, buffer, lzSize, coder, features);
            int result = resultE.value();
            if (resultE.getError() == Success) memcpy(output + HeaderSize, buffer, result);
            LargeFree(buffer);
            if ((resultE.getError() != Success) || (result + 1 + 4 * num_indexes >= (int)n))
            {
                return store(input, output, n);
            }
            {
                if (num_indexes > 0)
                    memcpy(output + HeaderSize + result, indexes, 4 * num_indexes);
                output[HeaderSize + result + 4 * num_indexes] = num_indexes;
                result += 1 + 4 * num_indexes;
            }
            saveUint((output +  0), result + HeaderSize);
            saveUint((output +  4), n);
            saveUint((output +  8), mode);
            saveUint((output + 12), index.value());
            saveUint((output + 16), adler32(input, n));
            saveUint((output + 20), adler32(output + HeaderSize, result));
            saveUint((output + 24), adler32(output, 24));
            return result + HeaderSize;
        }

        return NotEnoughMemory;
    }

    Error EBSC::getBlockInfo(const uint8 * blockHeader, const size_t headerSize, size_t & pBlockSize, size_t & pDataSize)
    {
        if (headerSize < HeaderSize)
        {
            return UnexpectedEOB;
        }

        if (readUint(blockHeader + 24) != adler32(blockHeader, 24))
        {
            return DataCorrupt;
        }

        int blockSize    = readInt(blockHeader +  0);
        int dataSize     = readInt(blockHeader +  4);
        int mode         = readInt(blockHeader +  8);
        int index        = readInt(blockHeader + 12);

        int lzpHashSize         = (mode >> 16) & 0xff;
        int lzpMinLen           = (mode >>  8) & 0xff;
        Coder coder             = (Coder)((mode >>  5) & 0x7);
        BlockSorter blockSorter = (BlockSorter)((mode >>  0) & 0x1f);

        int test_mode = 0;

        switch (blockSorter)
        {
            case BWT : test_mode = BWT; break;
            default : if (blockSorter > 0) return DataCorrupt;
        }

        switch (coder)
        {
            case QLFCStatic   : test_mode += (QLFCStatic   << 5); break;
            case QLFCAdaptive : test_mode += (QLFCAdaptive << 5); break;

            default : if (coder > 0) return DataCorrupt;
        }

        if (lzpMinLen != 0 || lzpHashSize != 0)
        {
            if (lzpMinLen < 4 || lzpMinLen > 255) return DataCorrupt;
            if (lzpHashSize < 10 || lzpHashSize > 28) return DataCorrupt;
            test_mode += (lzpMinLen << 8);
            test_mode += (lzpHashSize << 16);
        }

        if (test_mode != mode)
        {
            return DataCorrupt;
        }

        if (blockSize < (int)HeaderSize || blockSize > (int)HeaderSize + dataSize)
        {
            return DataCorrupt;
        }

        if (index < 0 || index > dataSize)
        {
            return DataCorrupt;
        }

        pBlockSize = blockSize;
        pDataSize = dataSize;

        return Success;
    }

    Error EBSC::decompressInPlace(uint8 * data, const size_t inputSize, const size_t outputSize)
    {
        int     indexes[256];
        uint8   num_indexes;

        size_t blockSize = 0, dataSize = 0;

        Error info = getBlockInfo(data, inputSize, blockSize, dataSize);
        if (info != Success)
        {
            return info;
        }

        if (inputSize < blockSize || outputSize < dataSize)
        {
            return UnexpectedEOB;
        }

        if (*(unsigned int *)(data + 20) != adler32(data + HeaderSize, blockSize - HeaderSize))
        {
            return DataCorrupt;
        }

        int mode = readInt(data + 8);
        if (mode == 0)
        {
            memmove(data, data + HeaderSize, dataSize);
            return Success;
        }

        int             index           = readInt(data + 12);
        unsigned int    adler32_data    = readUint(data + 16);

        num_indexes = data[blockSize - 1];
        if (num_indexes > 0)
        {
            memcpy(indexes, data + blockSize - 1 - 4 * num_indexes, 4 * num_indexes);
        }

        int lzpHashSize          = (mode >> 16) & 0xff;
        int lzpMinLen            = (mode >>  8) & 0xff;
        Coder coder              = (Coder)((mode >>  5) & 0x7);
        BlockSorter blockSorter  = (BlockSorter)((mode >>  0) & 0x1f);

        EOS lzSizeE = Success;
        {
            uint8 * buffer = (uint8 *)LargeMalloc(blockSize);
            if (buffer == NULL) return NotEnoughMemory;

            memcpy(buffer, data, blockSize);

            lzSizeE = coder_decompress(buffer + HeaderSize, data, coder, features);

            LargeFree(buffer);
        }
        if (lzSizeE.getError() != Success)
        {
            return lzSizeE.getError();
        }
        int lzSize = lzSizeE.value();

        EOS result = Success;
        switch (blockSorter)
        {
            case BWT : result = BWTDecode(data, lzSize, index, num_indexes, indexes, features); break;
            default : return DataCorrupt;
        }
        if (result.getError() != Success)
        {
            return result.getError();
        }

        if (mode != (mode & 0xff))
        {
            if (uint8 * buffer = (uint8 *)LargeMalloc(lzSize))
            {
                memcpy(buffer, data, lzSize);
                result = lzp_decompress(buffer, data, lzSize, lzpHashSize, lzpMinLen, features);
                LargeFree(buffer);
                if (result.getError() != Success)
                {
                    return result.getError();
                }
                return result.value() == dataSize ? (adler32_data == adler32(data, dataSize) ? Success : DataCorrupt) : DataCorrupt;
            }
            return NotEnoughMemory;
        }

        return lzSize == (int)dataSize ? (adler32_data == adler32(data, dataSize) ? Success : DataCorrupt) : DataCorrupt;
    }

    Error EBSC::decompress(const uint8 * input, const size_t inputSize, uint8 * output, const size_t outputSize)
    {
        int     indexes[256];
        uint8   num_indexes;

        if (input == output)
        {
            return decompressInPlace(output, inputSize, outputSize);
        }

        size_t blockSize = 0, dataSize = 0;

        Error info = getBlockInfo(input, inputSize, blockSize, dataSize);
        if (info != Success)
        {
            return info;
        }

        if (inputSize < blockSize || outputSize < dataSize)
        {
            return UnexpectedEOB;
        }

        if (*(unsigned int *)(input + 20) != adler32(input + HeaderSize, blockSize - HeaderSize))
        {
            return DataCorrupt;
        }

        int mode = readInt(input + 8);
        if (mode == 0)
        {
            memcpy(output, input + HeaderSize, dataSize);
            return Success;
        }

        int             index           = readInt(input + 12);
        unsigned int    adler32_data    = readUint(input + 16);

        num_indexes = input[blockSize - 1];
        if (num_indexes > 0)
        {
            memcpy(indexes, input + blockSize - 1 - 4 * num_indexes, 4 * num_indexes);
        }

        int lzpHashSize          = (mode >> 16) & 0xff;
        int lzpMinLen            = (mode >>  8) & 0xff;
        Coder coder              = (Coder)((mode >>  5) & 0x7);
        BlockSorter blockSorter  = (BlockSorter)((mode >>  0) & 0x1f);

        EOS lzSizeE = coder_decompress(input + HeaderSize, output, coder, features);
        if (lzSizeE.getError() != Success)
            return lzSizeE.getError();

        int lzSize = lzSizeE.value();

        EOS result = Success;
        switch (blockSorter)
        {
            case BWT : result = BWTDecode(output, lzSize, index, num_indexes, indexes, features); break;
            default : return DataCorrupt;
        }
        if (result.getError() != Success)
        {
            return result.getError();
        }

        if (mode != (mode & 0xff))
        {
            if (uint8 * buffer = (uint8 *)LargeMalloc(lzSize))
            {
                memcpy(buffer, output, lzSize);
                result = lzp_decompress(buffer, output, lzSize, lzpHashSize, lzpMinLen, features);
                LargeFree(buffer);
                if (result.getError() != Success)
                {
                    return result.getError();
                }
                return result.value() == dataSize ? (adler32_data == adler32(output, dataSize) ? Success : DataCorrupt) : DataCorrupt;
            }
            return NotEnoughMemory;
        }

        return lzSize == (int)dataSize ? (adler32_data == adler32(output, dataSize) ? Success : DataCorrupt) : DataCorrupt;
    }
    
    Error EBSC::postProcess(uint8 * input, const size_t inputSize, const int8 sortingContext, const int8 recordSize)
    {
        if (sortingContext == 2)
        {
            EOS result = reverse_block(input, inputSize, features);
            if (result.getError() != Success) return result.getError();
        }
        if (recordSize > 1)
        {
            EOS result = reorder_reverse(input, inputSize, recordSize, features);
            if (result.getError() != Success) return result.getError();
        }
        return Success;
    }

}
