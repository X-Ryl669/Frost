#ifndef hpp_CPP_BaseHash_CPP_hpp
#define hpp_CPP_BaseHash_CPP_hpp

// We need type declaration
#include "../Types.hpp"

/** All cryptographic hash functions are defined here.
    There is voluntary no MD5 here, as MD5 proved to be too weak. */
namespace Hashing
{
    /** This structure define the interface each hashing algorithm must respect

        Using this any hasher is the same :
        @code
            Hashing::SomeHasherYouWant hasher;
            // Fills up lookup tables
            hasher.Start();
            // Hash the given buffer
            hasher.Hash(myBuffer, myBufferSize);
            // Finally get the result
            hasher.Finalize((byte*)result);
        @endcode
        @sa Hashing::CRC32, Hashing::SHA1, Hashing::CRC16
    */
    struct Hasher
    {

    public:
        /** Start the hashing */
        virtual void    Start() = 0;
        /** Hash the given buffer */
        virtual void    Hash(const uint8 * buffer, uint32 size) = 0;
        /** Finalize the hashing  and store the result */
        virtual void    Finalize(uint8 * outBuffer) = 0;
        /** Get the default hash size in byte */
        virtual uint32  hashSize() const throw() = 0;
        /** Constructor */
        Hasher(){}
        /** Destructor */
        virtual ~Hasher(){}
    };
    
    /** A rolling-hash algorithm interface.
        Rolling hash compute the hash of a sequence byte-per-byte, but there is a mathematical
        property that Hash(block_i+1) = function(Hash(block_i), block[i+1]) is very efficient.
    */
    struct RollingHasher : public Hasher
    {
    public:
        /** Append a single byte for the computed checksum */
        virtual void    Append(uint8 ch) = 0;
    };
    

    struct SHA1;
    /** Key derivation function.
        This is used to avoid leaking a secret key.
        The secret key is "derived" to the final size.
        Please notice that derivation is constant, that is the
        function doesn't change output for a same input.
        
        You might want to use finalizeWithExtraInfo to given extra 
        informations to harden the hash.

        @param outputSizeInBit  The expected output hash size in bits
        @param inputSizeInBit   This is usually the secret size in bits
        @param BaseHasher       The internal hasher method to use (defaults to SHA1)
        @warning The hash method limits input to the given size
                 in bit, so if you pass more data to hash, it's ignored. */
    template <const int outputSizeInBit, const int inputSizeInBit, class BaseHasher = SHA1 >
    struct KDF1 : public Hashing::Hasher
    {
    protected:
        enum { OutputSize = outputSizeInBit / 8, InputSize = inputSizeInBit / 8 };

        BaseHasher  hasher;
        uint8       hashInput[InputSize];
        uint32      inputLen;

        /** The current little-endian to big endian conversion */
        inline uint32 Swap32(const uint32 X) { return (( (X) & 0xFF000000)>>24) | (( (X) & 0x00FF0000)>>8) | (( (X) & 0x0000FF00)<<8) | (( (X) & 0x000000FF)<<24); }

    public:
        /** Start the hashing */
        virtual void    Start() { inputLen = 0; memset(hashInput, 0, sizeof(hashInput)); }
        /** Hash the given buffer */
        virtual void    Hash(const uint8 * buffer, uint32 size)
        {
            uint32 _size = min(size + inputLen, (uint32)InputSize) - inputLen;
            memcpy(&hashInput[inputLen], buffer, _size);
            inputLen += _size;
        }
        /** Finalize the hashing  and store the result */
        virtual void    Finalize(uint8 * outBuffer) { finalizeWithExtraInfo(outBuffer, 0, 0); }
        /** Set the extra info (if any) */
        virtual void    finalizeWithExtraInfo(uint8 * outBuffer, const uint8 * extra, const uint32 extraLen)
        {
            uint8 hashArray[OutputSize + 64] = {0};
            uint32 tmpSize = InputSize + 4 + extraLen;
            uint8 hashTmpArray[InputSize + 4 + BaseHasher::DigestSize] = {0};
            uint8 * hashTmp = hashTmpArray;
            if ((extraLen && extra) || hasher.hashSize() > sizeof(hashTmpArray))
            {
                hashTmp = new uint8[max(tmpSize, hasher.hashSize())];
                if (!hashTmp)
                {
                    if (outBuffer) memset(outBuffer, 0, hashSize());
                    return; // Far more error to expect when stack or heap is exhausted
                }
                memset(hashTmp, 0, tmpSize);
            }
            uint32 d = (OutputSize + hasher.hashSize() - 1) / hasher.hashSize();
            for (uint32 i = 0; i < d; i++)
            {
                // Computing the inner hash
                memcpy(hashTmp, hashInput, InputSize);
                uint32 swappedI = Swap32(i);
                memcpy(&hashTmp[InputSize], &swappedI, sizeof(swappedI)); // Big endian
                if (extra && extraLen)
                    memcpy(&hashTmp[InputSize + 4], extra, extraLen);
                // Hash such string now
                hasher.Start();
                hasher.Hash(hashTmp, tmpSize);
                // Overwrite the data with the hash
                hasher.Finalize(hashTmp);

                memcpy(&hashArray[i * hasher.hashSize()], hashTmp, hasher.hashSize());
                memset(hashTmp, 0, tmpSize);
            }
            if ((extraLen && extra) || hasher.hashSize() > sizeof(hashTmpArray)) delete[] hashTmp;
            if (outBuffer) memcpy(outBuffer, hashArray, OutputSize);
        }
        /** Get the default hash size in byte */
        virtual uint32  hashSize() const throw() { return OutputSize; }

        KDF1() : inputLen(0) { memset(hashInput, 0, sizeof(hashInput)); }
        virtual ~KDF1() { inputLen = 0; memset(hashInput, 0, sizeof(hashInput)); }
    };
    
    /** Password based KDF function, following RSA's PBKDF1 recommandation.
        This is used to get a hash out of a smaller (likely a password) input, 
        in a way that's quite hard to brute-force all the possible passwords.
        
        Unlike the KDF1 case above, this one is way slower by design, to avoid
        a fast brute force search for all the "low entropy" passwords.
        
        You might want to use finalizeWithExtraInfo to given extra 
        informations to harden the hash.
        
        Beware that if you don't provide extra info, a default (fixed) salt
        will be used so the security will be low as one could make a dictionary 
        of common password padded with this default salt to force it.  
          
        @param outputSizeInBit  The expected output hash size in bits
        @param inputSizeInBit   This is usually the secret size in bits
        @param BaseHasher       The internal hasher method to use (defaults to SHA1)
        @warning The hash method limits input to the given size
                 in bit, so if you pass more data to hash, it's ignored. */
    template <const int outputSizeInBit, const int inputSizeInBit, class BaseHasher = SHA1, const int iterations = 1000 >
    struct PBKDF1 : public KDF1<outputSizeInBit, inputSizeInBit, BaseHasher>
    {
    private:
        uint8       defaultSalt[8];
 
    public:
        /** Set the extra info (if any) */
        virtual void finalizeWithExtraInfo(uint8 * outBuffer, const uint8 * extra, const uint32 extraLen)
        {
            this->hasher.Start();
            this->hasher.Hash(this->hashInput, this->InputSize);
            if (extra && extraLen)
               this->hasher.Hash(extra, min(extraLen, (uint32)ArrSz(defaultSalt)));
            else this->hasher.Hash(defaultSalt, ArrSz(defaultSalt));
            
            uint8 hashArray[outputSizeInBit / 8] = {0};
            this->hasher.Finalize(hashArray);

            for (int i = 1; i < iterations; i++)
            {
                this->hasher.Start();
                this->hasher.Hash(hashArray, ArrSz(hashArray));
                this->hasher.Finalize(hashArray);
            }
            
            if (outBuffer) memcpy(outBuffer, hashArray, this->OutputSize);
        }
        PBKDF1() { uint8 defaultSaltImpl[] = { 0xC1, 0xA5, 0x50, 'p', 'a', 'T', 'h', 0x8E }; memcpy(defaultSalt, defaultSaltImpl, ArrSz(defaultSaltImpl)); }
    };
    


    /** The classical MAC (message authentication code) functions.
        Those function are use to assert authentication of a given message */
    template <class BaseHasher = SHA1>
    struct HMAC : public Hashing::Hasher
    {
    private:
        uint8       inputPad[BaseHasher::BlockSize];
        BaseHasher  hasher;
        bool        hashedInput;

    public:
        /** Start the hashing */
        virtual void    Start() { hashedInput = false; hasher.Start(); }
        /** Hash the given buffer */
        virtual void    Hash(const uint8 * buffer, uint32 size)
        {
            if (!buffer || size == 0) return;
            if (!hashedInput) hasher.Hash(inputPad, sizeof(inputPad));
            hasher.Hash(buffer, size);
            hashedInput = true;
        }
        /** Finalize the hashing  and store the result */
        virtual void    Finalize(uint8 * outBuffer)
        {
            if (!hashedInput) { hasher.Hash(inputPad, sizeof(inputPad)); hashedInput = true; }
            uint8 hashTmpArray[BaseHasher::DigestSize] = {0};
            uint8 keyTmp[BaseHasher::BlockSize] = {0};

            // Hash such string now
            hasher.Finalize(hashTmpArray);
            hasher.Start();
            for (int i = 0; i < BaseHasher::BlockSize; i++) keyTmp[i] = (inputPad[i] ^ 0x6A);
            hasher.Hash(keyTmp, BaseHasher::BlockSize);
            hasher.Hash(hashTmpArray, sizeof(hashTmpArray));
            hasher.Finalize(outBuffer);
        }
        /** Get the default hash size in byte */
        virtual uint32  hashSize() const throw() { return hasher.hashSize(); }

        HMAC(const uint8 * key, const uint32 keyLength) : hashedInput(false)
        {
            if (key && keyLength)
            {
                uint32 i = 0;
                if (keyLength <= BaseHasher::BlockSize)
                {
                    for (; i < keyLength; i++)              inputPad[i] = key[i] ^ 0x36;
                    for (; i < BaseHasher::BlockSize; i++)  inputPad[i] = 0x36;
                } else
                {
                    hasher.Start(); hasher.Hash(key, keyLength); hasher.Finalize(inputPad); hasher.Start();
                    for (; i < BaseHasher::DigestSize; i++) inputPad[i] = inputPad[i] ^ 0x36;
                    for (; i < BaseHasher::BlockSize; i++)  inputPad[i] = 0x36;
                }
            } else memset(inputPad, 0x36, sizeof(inputPad));
        }
        virtual ~HMAC()
        {
            memset(inputPad, 0x36, sizeof(inputPad));
        }
    };

    /** Get the hash for the given algorithm. 
        This is a shortcut to calling all methods successively.
        @param inBuffer     The input buffer to hash with the method 
        @param inSize       The input buffer size in bytes 
        @param output       On output, contains the hash. The output buffer must be at least hash size's byte */
    template <class Hasher>
    void getHashFor(const uint8 * inBuffer, const uint32 inSize, uint8 * output)
    { Hasher hash; hash.Start(); hash.Hash(inBuffer, inSize); hash.Finalize(output); }
}

#endif
