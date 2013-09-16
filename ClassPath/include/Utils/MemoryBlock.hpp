#ifndef hpp_MemoryBlock_hpp
#define hpp_MemoryBlock_hpp

// We need platform stuff
#include "../Platform/Platform.hpp"
// And encoding stuff
#include "../Encoding/Encode.hpp"

/** All the small and useful tools like the MemoryBlock, dump stuff.
    You'll find the MemoryBlock to handle dynamic byte array, 
    dumpToHexString to, well, dump a byte array to a hexadecimal string, 
    hexDump to generate a nice, and standard hexadecimal with char array dump of binary array. */
namespace Utils
{
    /** A memory block is a dynamic array of bytes with many byte array manipulation interface.
        Using a MemoryBlock instead of a dynamically allocated byte array is safer, usually faster. 

        You'll use Append and Extract and stripTo to add / remove data from the array.
        The method lookFor is useful for searching a pattern in the memory array.

        You also have all the base conversion methods that are very useful for text exporting and 
        importing (in XML, database, whatever) binary data.
    */
    struct MemoryBlock
    {
        // Type definition and enumeration
    public:
        /** The maximal allowed buffer's delta size.
            When releasing data from the block, releasing doesn't realloc the block to a smaller value */ 
        enum { MaxAllowedDelta = 4096 };
            
        // Members 
    private:
        /** The internal buffer */
        uint8 *     buffer;
        /** The buffer size */
        uint32      size;
        /** The allocated size */
        uint32      allocSize;

        // Helpers
    private:
        /** Resize the buffer */
        bool resizeBuffer(const uint32 newSize);
        
        // Interface
    public:    
        /** Append data to the buffer 
            @param buffer   The buffer to append inside this memory block. Can be 0 to reserve space in the block.
            @param size     The buffer size in byte (or the reservation size if buffer is 0).
            @return false if the allocation failed. */
        bool Append(const uint8 * buffer, const uint32 size);
        /** Empty some data from buffer.
            @note If you need to reset the block, you can call "block.Extract(0, block.getSize())" 
            @param buffer   The buffer to extract the memory block into. Can be 0 to free some space from the beginning of the block.
            @param size     The buffer size in byte (or the freeing size if buffer is 0).
            @return false if the (re)allocation failed, or you tried to extract more data than the block contains. */
        bool Extract(uint8 * buffer, const uint32 size);
        /** Check if a given pattern can be found in the block.
            This is using a O(M*N) algorithm, so don't use this on large array, you should better use a BoyerMoore class for that.
            @return the pattern position or -1 if not found */
        uint32 lookFor(const uint8 * pattern, const uint32 patternLen, const uint32 startPos = 0) const;
        
        /** Get the used size */
        uint32 getSize() const { return size; }
        /** Get a pointer on the buffer.
            @warning    You'll need this if you're calling a function that can't tell beforehand how much data it'll consume.
                        However, you must call Extract(0, usedSize) after the operation. */
        uint8 * getBuffer() { return buffer; }
        /** Get a const pointer on the buffer.
            @warning    You'll need this if you're calling a function that can't tell beforehand how much data it'll consume.
                        However, you must call Extract(0, usedSize) after the operation. */
        const uint8 * getConstBuffer() const { return buffer; }
        /** Strip the buffer to the given size. 
            @warning the stripped data isn't cleared, only the size is set */
        void stripTo(const uint32 newSize) { size = newSize < size ? newSize : size; }
        /** Ensure the given size.
            The buffer is enlarged if too small, or shrunk if too large.
            The consumed size is not changed (unless the new size is smaller to the current size)
            @param newSize     The new allocated size for this buffer
            @param setSizeToo  If true, the consumed size is set to the allocated size */
        bool ensureSize(const uint32 newSize, const bool setSizeToo = false) { if (!resizeBuffer(newSize)) return false; if (setSizeToo) size = newSize; return true; }

#if (HasBaseEncoding == 1)
        /** Create a memory block from an encoded string.
            @param input    A pointer on a base64 string 
            @param inputLen The base64 string length
            @return A pointer on a new allocated memory block you must delete when no more used */
        static MemoryBlock * fromBase64(const uint8 * input, const uint32 inputLen);
        /** Create a memory block from an encoded string.
            @param input    A pointer on a base85 string 
            @param inputLen The base85 string length
            @return A pointer on a new allocated memory block you must delete when no more used */
        static MemoryBlock * fromBase85(const uint8 * input, const uint32 inputLen);
        /** Create a memory block from an encoded string.
            @param input    A pointer on a base16 string 
            @param inputLen The base16 string length
            @return A pointer on a new allocated memory block you must delete when no more used */
        static MemoryBlock * fromBase16(const uint8 * input, const uint32 inputLen);
        /** Create an base64 encoded string from this binary buffer.
            @return A pointer on a new allocated memory block you must delete when no more used */
        MemoryBlock * toBase64() const;
        /** Create an base85 encoded string from this binary buffer.
            @return A pointer on a new allocated memory block you must delete when no more used */
        MemoryBlock * toBase85() const;
        /** Create an base16 encoded string from this binary buffer.
            @return A pointer on a new allocated memory block you must delete when no more used */
        MemoryBlock * toBase16() const;

        /** Build from the given base 85 buffer 
            @param input    A pointer on a base85 string 
            @param inputLen The base85 string length */
        bool rebuildFromBase85(const uint8 * input, const uint32 inputLen);
        /** Build from the given base 64 buffer 
            @param input    A pointer on a base64 string 
            @param inputLen The base64 string length */
        bool rebuildFromBase64(const uint8 * input, const uint32 inputLen);
        /** Build from the given base 16 buffer 
            @param input    A pointer on a base16 string 
            @param inputLen The base16 string length */
        bool rebuildFromBase16(const uint8 * input, const uint32 inputLen);
#endif
        /** Check if two memory block are identical */
        const bool operator == (const MemoryBlock & other) const;

        // Construction and destruction
    public:    
        /** Default construction */
        MemoryBlock(const uint32 size = 0) : buffer((uint8*)Platform::safeRealloc(0, size)), size(size), allocSize(size) {}
        /** Copy constructor */
        MemoryBlock(const MemoryBlock & other) : buffer((uint8*)Platform::safeRealloc(0, other.size)), size(other.size), allocSize(other.size) { if (size) memcpy(buffer, other.buffer, size); }
        /** Destruction */
        ~MemoryBlock() { Platform::safeRealloc(buffer, 0); size = allocSize = 0; }
    };

    /** This deletion function zero the memory block before deleting it.
        This is particularly useful for Crypto code, as you'll usually need to clear private key's memory. */
    void cleanAndDelete(MemoryBlock *const block);

}


#endif
