#ifndef hpp_CPP_SHA1_CPP_hpp
#define hpp_CPP_SHA1_CPP_hpp

// We need the base hash interface
#include "BaseHash.hpp"
// We need string for helper function
#include "../Strings/Strings.hpp"

namespace Hashing
{
    /** This structure store the current state of a SHA1 round and perform a SHA1 hashing
        Using this class is similar to any other hash class.
        @code
            Hashing::SHA1 hasher;
            uint8 * result = ...;
            // Fills up lookup tables
            hasher.Start();
            // Hash the given buffer
            hasher.Hash(myBuffer, myBufferSize);
            // Finally get the result
            hasher.Finalize((byte*)result);
        @endcode
    */
    struct SHA1 : public Hasher
    {
    private:
        /** The SHA1 constants */
        enum
        {
            SHA1BlockSize 	= 64,
            SHA1DigestSize 	= 20,
            Maximum16Bits	= 0x0000FFFF,
        };

        /** The work buffer */
        uint32 workBuffer[16];
        /** The current hash */
        uint32 hash[5];
        /** The current count */
        uint32 count[2];

    public:
        /** The public size */
        enum { BlockSize = SHA1BlockSize, DigestSize = SHA1DigestSize };

    public:
        /** Start the hashing */
        void Start();
        /** Hash the given buffer */
        void Hash(const uint8 * buffer, uint32 size);
        /** Finalize the hashing  and store the result */
        void Finalize(uint8 * outBuffer);
        /** Get the default hash size in byte */
        inline uint32  hashSize() const throw() { return SHA1DigestSize; }

    private:
        /** Perform a transform round */
        void Transform();
        /** Prepare a 32 bits SHA block to the right encoding */
        inline static uint32 SHABlock32(const uint32 nb) { return (Rotate32(nb, 8) & 0x00ff00ff) | (Rotate32(nb, 24) & 0xff00ff00); }
        /** Rotate a 32 bits number from the given amount, moving bit from left part to new right part */
        inline static uint32 Rotate32(const uint32 nb, const uint32 amount) { return (nb <<  amount) | (nb >> (32 - amount)); }

        /** The SHA1 mask */
        static const uint32 SHAMask[4];
        /** The SHA1 bits */
        static const uint32 SHABits[4];
    };
    /** Hash a string with SHA1 and get a hexadecimal string on output */
    Strings::FastString getSHA1Of(const Strings::FastString & data);
}

#endif
