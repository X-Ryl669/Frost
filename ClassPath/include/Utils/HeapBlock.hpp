#ifndef hpp_HeapBlock_hpp
#define hpp_HeapBlock_hpp

// We need the type declaration and platform code
#include "../Platform/Platform.hpp"

namespace Utils
{
    /** The HeapBlock class just manages an allocated block, making sure it's freed on destruction.
        It's a simple RAII technic, simplifying the final code, avoiding memory leak.
        It's also able to resize an allocated memory block.
        Unlike the MemoryBlock, this does not remember the allocation size, has no fancy method for
        growing/searching/converting to different format. */
    class HeapBlock
    {
        // Members
    private:
        /** The managed buffer */
        uint8 * buffer;

        // Interface
    public:
        /** Accessing the buffer is done natively */
        ForcedInline(operator uint8 *()) { return buffer; }
        /** It's possible to reallocate the buffer */
        bool resize(const size_t newSize) { return (buffer = (uint8*)Platform::safeRealloc(buffer, newSize)) != 0; }
        /** Allocate the buffer */
        HeapBlock(const size_t size = 0) : buffer(size ? (uint8*)Platform::malloc(size) : (uint8*)0) {}
        /** Clean the buffer */
        ~HeapBlock() { Platform::free(buffer); }
    };
}


#endif
