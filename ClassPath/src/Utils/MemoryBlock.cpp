#include "../../include/Types.hpp"
// We need our declaration 
#include "../../include/Utils/MemoryBlock.hpp"
// We need encoding code too
#include "../../include/Encoding/Encode.hpp"

namespace Utils
{
    // Resize the buffer 
    bool MemoryBlock::resizeBuffer(const uint32 newSize)
    {
        // If the difference in size is not worth reallocating, skip it
        if (newSize <= allocSize  // New size is lower than the allocated size, but larger than the allowed delta, or allocSize is smaller than the allowed delta
            && ( ((allocSize > MaxAllowedDelta) && newSize > (allocSize - MaxAllowedDelta)) || allocSize <= MaxAllowedDelta )
            )
        {
            size = newSize;
            return true;
        }
        // If reducing the size, let's keep track of this
        if (newSize < size) size = newSize;
        // Expand or shrink the buffer
        allocSize = max((uint32)64, newSize);
        buffer = (uint8*)Platform::safeRealloc(buffer, allocSize);
        if (!buffer) { size = 0; allocSize = 0; return false; }
        return true;
    }


    // Append data to the buffer 
    bool MemoryBlock::Append(const uint8 * _buffer, const uint32 _size)
    {
        if (!_size) return true;
        bool overlap = (buffer <= _buffer && buffer + allocSize > _buffer);
        size_t dist = overlap ? (size_t)(_buffer - buffer) : 0;

        if ((size + _size) > allocSize && !resizeBuffer((uint32)((size + _size) * 1.2f))) return false;
        if (_buffer)
        {
            if (overlap) memmove(&buffer[size], buffer + dist, _size); // If the buffer was reallocated, we need to update the pointer
            else         memcpy(&buffer[size], _buffer, _size);
        }
        size += _size;
        return true;
    }
    // Empty some data from buffer 
    bool MemoryBlock::Extract(uint8 * _buffer, const uint32 _size)
    {
        if (_size > size) return false;
        if (_buffer) memmove(_buffer, buffer, _size);
        memmove(buffer, &buffer[_size], (size - _size));
        size -= _size;
        if (allocSize - size > MaxAllowedDelta && !resizeBuffer((uint32)(size * 1.2f)))
            return false;
        return true;
    }
    // Check if a given pattern can be found in the block.
    uint32 MemoryBlock::lookFor(const uint8 * pattern, const uint32 patternLen, const uint32 startPos) const
    {
        if (!pattern || (patternLen + startPos) > size) return (uint32)-1;
        
        uint32 pos = startPos;
        while (pos < size)
        {
            void * foundBuff = memchr(&buffer[pos], pattern[0], (size - pos));
            if (foundBuff && (uint32)((uint8*)foundBuff - buffer) + patternLen <= size)
            {
                if (memcmp(foundBuff, pattern, patternLen) == 0)
                    return (uint32)((uint8*)foundBuff - buffer);
                pos = (uint32)((uint8*)foundBuff - buffer) + 1;
            } 
            else return (uint32)-1;
        }
        return (uint32)-1;
    }
    // Check if two memory block are identical 
    const bool MemoryBlock::operator == (const MemoryBlock & other) const
    {
        return size == other.size && memcmp(buffer, other.buffer, size) == 0;
    }
    
#if (HasBaseEncoding == 1)    
    // Create a memory block from an encoded string.
    MemoryBlock * MemoryBlock::fromBase64(const uint8 * input, const uint32 inputLen) 
    {
        uint32 size = 0;
        if (!Encoding::decodeBase64(input, inputLen, 0, size)) return 0;
        MemoryBlock * block = new MemoryBlock(size);
        if (!Encoding::decodeBase64(input, inputLen, block->buffer, block->size)) { delete block; return 0; }
        return block;
    }
    // Create a memory block from an encoded string.
    MemoryBlock * MemoryBlock::fromBase85(const uint8 * input, const uint32 inputLen) 
    {
        uint32 size = 0;
        if (!Encoding::decodeBase85(input, inputLen, 0, size)) return 0;
        MemoryBlock * block = new MemoryBlock(size);
        if (!Encoding::decodeBase85(input, inputLen, block->buffer, block->size)) { delete block; return 0; }
        return block;
    }
    // Create a memory block from an encoded string.
    MemoryBlock * MemoryBlock::fromBase16(const uint8 * input, const uint32 inputLen) 
    {
        uint32 size = 0;
        if (!Encoding::decodeBase16(input, inputLen, 0, size)) return 0;
        MemoryBlock * block = new MemoryBlock(size);
        if (!Encoding::decodeBase16(input, inputLen, block->buffer, block->size)) { delete block; return 0; }
        return block;
    }
    // Create a memory block from an encoded string.
    bool MemoryBlock::rebuildFromBase64(const uint8 * input, const uint32 inputLen) 
    {
        if (!inputLen) { size = 0; return true; }
        if (!Encoding::decodeBase64(input, inputLen, 0, size)) return false;
        if (!resizeBuffer((uint32)(size * 1.2f))) return false;
        if (!Encoding::decodeBase64(input, inputLen, buffer, size)) return false; 
        return true;
    }
    // Create a memory block from an encoded string.
    bool MemoryBlock::rebuildFromBase85(const uint8 * input, const uint32 inputLen) 
    {
        if (!inputLen) { size = 0; return true; }
        if (!Encoding::decodeBase85(input, inputLen, 0, size)) return false;
        if (!resizeBuffer((uint32)(size * 1.2f))) return false;
        if (!Encoding::decodeBase85(input, inputLen, buffer, size)) return false; 
        return true;
    }
    // Create a memory block from an encoded string.
    bool MemoryBlock::rebuildFromBase16(const uint8 * input, const uint32 inputLen) 
    {
        if (!inputLen) { size = 0; return true; }
        if (!Encoding::decodeBase16(input, inputLen, 0, size)) return false;
        if (!resizeBuffer((uint32)(size * 1.2f))) return false;
        if (!Encoding::decodeBase16(input, inputLen, buffer, size)) return false; 
        return true;
    }

    // Create an encoded string from a memory block 
    MemoryBlock * MemoryBlock::toBase64() const
    {
        uint32 _size = 0;
        if (!Encoding::encodeBase64(buffer, size, 0, _size)) return 0;
        MemoryBlock * block = new MemoryBlock(_size);
        if (!Encoding::encodeBase64(buffer, size, block->buffer, block->size)) { delete block; return 0; }
        return block;
    }
    // Create an encoded string from a memory block 
    MemoryBlock * MemoryBlock::toBase85() const 
    {
        uint32 _size = 0;
        if (!Encoding::encodeBase85(buffer, size, 0, _size)) return 0;
        MemoryBlock * block = new MemoryBlock(_size);
        if (!Encoding::encodeBase85(buffer, size, block->buffer, block->size)) { delete block; return 0; }
        return block;
    }
    // Create an encoded string from a memory block 
    MemoryBlock * MemoryBlock::toBase16() const
    {
        uint32 _size = 0;
        if (!Encoding::encodeBase16(buffer, size, 0, _size)) return 0;
        MemoryBlock * block = new MemoryBlock(_size);
        if (!Encoding::encodeBase16(buffer, size, block->buffer, block->size)) { delete block; return 0; }
        return block;
    }
#endif

    void cleanAndDelete(MemoryBlock *const block) 
    {
        if (block) memset(block->getBuffer(), 0, block->getSize());
        delete block;
    }

}

