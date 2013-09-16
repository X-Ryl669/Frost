#ifndef hpp_CPP_SHA256_CPP_hpp
#define hpp_CPP_SHA256_CPP_hpp

// We need the base hash interface
#include "BaseHash.hpp"

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
    struct SHA256 : public Hasher
    {
    private:
        /** The SHA1 constants */
        enum
        {
            SHA256BlockSize 	= 64,
            SHA256DigestSize 	= 32,
            Maximum16Bits	= 0x0000FFFF,
        };

        /** The work buffer */
        uint8  workBuffer[64];
        /** The current hash */
        uint32 hash[8];
        /** The current count */
        uint32 count;
        /** The length */
        uint64 length;

    public:
        /** The public size */
        enum { BlockSize = SHA256BlockSize, DigestSize = SHA256DigestSize };

    public:
        /** Start the hashing */
        void Start();
        /** Hash the given buffer */
        void Hash(const uint8 * buffer, uint32 size);
        /** Finalize the hashing  and store the result */
        void Finalize(uint8 * outBuffer);
        /** Get the default hash size in byte */
        inline uint32  hashSize() const throw() { return SHA256DigestSize; }

    private:
        /** Perform a transform round */
        void Transform(const uint8 * block);
        /** Prepare a 32 bits SHA block to the right encoding */
        inline static uint32 SHABlock32(const uint32 nb) { return (Rotate32(nb, 8) & 0x00ff00ff) | (Rotate32(nb, 24) & 0xff00ff00); }
        /** Rotate a 32 bits number from the given amount, moving bit from left part to new right part */
        inline static uint32 Rotate32(const uint32 nb, const uint32 amount) { return (nb <<  amount) | (nb >> (32 - amount)); }

        /** The SHA256 mask */
        static const uint32 K[64];
    public:
        /** Constructor */
        SHA256(){}
        /** Destructor */
        virtual ~SHA256(){}
    };
}

#endif
