#ifndef hpp_CPP_Adler32_CPP_hpp
#define hpp_CPP_Adler32_CPP_hpp

// We need the base hash interface
#include "BaseHash.hpp"


namespace Hashing
{
    /** Hash the given buffer with Adler32 rolling checksum bits.
     
        Using this class is a bit similar than the other hash class.
        @code
            Hashing::Adler32 hasher;
            uint32 result = 0;
            // Fills up lookup tables
            hasher.Start();
            // Hash the given buffer
            hasher.Hash(myBuffer, myBufferSize);
            // Finally get the result
            hasher.Finalize((byte*)&result);
        @endcode */
    struct Adler32 : public Hasher
    {
        // Members
    private:
        enum
        {
            Base = 65521, //!< The base used for the modulo operation
        };
        
        /** The first part of the checksum */
        int32       a;
        /** The second part of the checksum */
        int32       b;
        
    public:
        /** Start the hashing */
        virtual void Start();
        /** Hash the given buffer */
        virtual void Hash(const uint8 * buffer, uint32 size);
        /** Finalize the hashing  and store the result */
        virtual void Finalize(uint8 * outBuffer);
        /** Append a single byte for the computed checksum */
        virtual void Append(uint8 ch);
        /** Get the default hash size in byte */
        inline uint32 hashSize() const throw() { return sizeof(uint32); }
        /** Get the checksum. 
            This is exactly equivalent to Finalize, but is likely more convenient for a fast access */
        inline uint32 getChecksum() const { return BigEndian((uint32)((b << 16) | a)); }
        /** Get the checksum as little endian.
            This returns a reversed checksum as specified in the default implementation (so it's not interoperable) but it's faster  */
        inline uint32 getChecksumLE() const { return (uint32)((b << 16) | a); }
    };
    
}

#endif
