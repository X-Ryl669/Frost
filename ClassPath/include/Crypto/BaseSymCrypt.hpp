#ifndef hpp_CPP_BaseSymCrypt_CPP_hpp
#define hpp_CPP_BaseSymCrypt_CPP_hpp

namespace Crypto
{
    /** Base interface for symmetric ciphers. */
    struct BaseSymCrypt
    {
        /** The operation mode */
        enum OperationMode
        {
            ECB = 0,        //!< In Electronic Code Book (ECB) mode if the same block is encrypted twice with the same key, the resulting cyphered text blocks are the same.
            CBC = 1,        //!< In Cipher Block Chaining (CBC) mode, a cyphered text block is obtained by first XORing the plaintext block with the previous cyphered text block, and encrypting the resulting value.
            CFB = 2,        //!< In Cipher Feedback Block (CFB) mode, a cyphered text block is obtained by encrypting the previous cyphered text block and XORing the resulting value with the plaintext.
        };

        /** The sizes */
        enum BlockSize
        {
            /** The default block size is 128 bits */
            DefaultBlockSize = 16,
            /** The medium block size is 192 bits */
            MediumBlockSize = 24,
            /** The maximum block size is 256 bits */
            MaxBlockSize = 32,
        };
        
        /** Get the configured block size */
        virtual BlockSize getBlockSize() const = 0;

        /** Set the key from the given buffer
            @param key  The 128/192/256-bit user-key to use.
            @param keyLength    The length of the key in uint8
            @param chain        The initial chain block for CBC and CFB modes.
            @param blockSize    The expected block size in uint8s (16, 24 or 32 uint8s). */
        virtual void setKey(const uint8 * key, const BlockSize keyLength, const uint8 * chain, const BlockSize blockSize) = 0;

        /** Encrypt the given buffer
            @param in The message of size n
            @param n The message size in uint8 (should be a multiple of BlockSize)
            @param result The cyphered message of size n (at least)
            @param mode The operation mode
            @return false if the key isn't set up or if the input doesn't fit a block size */
        virtual bool Encrypt(const uint8* in, uint8* result, size_t n, const OperationMode & mode = ECB) = 0;

        /** Decrypt the given buffer
            @param in The cyphered message of size n
            @param n The message size in uint8 (should be a multiple of BlockSize)
            @param result The clear message of size n (at least)
            @param mode The operation mode
            @return false if the key isn't set up or if the input doesn't fit a block size */
        virtual bool Decrypt(const uint8* in, uint8* result, size_t n, const OperationMode & mode = ECB) = 0;

        /** Required destructor */
        virtual ~BaseSymCrypt() {}
    };
    
    /** Useful method to perform CTR mode if the underlying primitive does not support it.
        Don't set any IV for the algorithm cipher (it's ignored anyway), but you need to 
        set the key.
        
        To encrypt call this way:
        @code
            uint8 nonce[BlockSize] = {0};
            fillRandomBytes(nonce, BlockSize / 2);
            // Set the cipher
            cipher.setKey(key, BlockSize, 0, BlockSize);
            for (uint128 i = 0; i < blocksToEncrypt; i++)
            {
                uint8 plainText[BlockSize] = ...;
                uint8 cipherText[BlockSize]; 
                uint8 processingBuffer[BlockSize];
                
                fillCounter(nonce + BlockSize / 2, i); // eq. to: *(uint128*)&nonce[BlockSize/2] = i;
                
                if (!CTR_BlockProcess(cipher, nonce, processingBuffer)) return false;
                Xor(cipherText, plainText, processingBuffer); // cipherText = plainText ^ processingBuffer
                memset(processingBuffer, 0, sizeof(processingBuffer)); // Don't let the algorithm code leak
                
                saveCipherText(cipherText);
            }
        @endcode
        
        To decrypt, call this way:
        @code
            uint8 nonce[BlockSize] = {0};
            getRandomBytesUsedForEncryption(nonce, BlockSize / 2);
            // Set the cipher
            cipher.setKey(key, BlockSize, 0, BlockSize);
            for (uint128 i = 0; i < blocksToEncrypt; i++)
            {
                uint8 plainText[BlockSize];
                uint8 cipherText[BlockSize] = ...;
                uint8 processingBuffer[BlockSize];
                
                fillCounter(nonce + BlockSize / 2, i); // eq. to: *(uint128*)&nonce[BlockSize/2] = i;
                
                if (!CTR_BlockProcess(cipher, nonce, processingBuffer)) return false;
                Xor(plainText, cipherText, processingBuffer); // plainText = cipherText ^ processingBuffer
                
                savePlainText(cipherText);
            }
        @endcode */
    static inline bool CTR_BlockProcess(BaseSymCrypt & cipher, const uint8 * nonceCounter, uint8 * result)
    {
        return cipher.Encrypt(nonceCounter, result, (size_t)cipher.getBlockSize());
    }
    
    /** Xor a memory block.
        This one is used to perform the operation out = a ^ b, with both out, a, and b being 
        stack allocated like this:
        @code
            uint8 out[2], a[2], b[2];
            Xor(out, a, b);
        @endcode */
    template <const size_t size>
    static inline void Xor(uint8  (&out)[size], const uint8 (&a)[size], const uint8 (&b)[size])
    {
        for(size_t i = 0; i < size; i++) out[i] = a[i] ^ b[i];
    }
    /** Xor a memory block.
        This one is used to perform the operation out = a ^ b, with both out, a, and b being 
        heap allocated like this:
        @code
            uint8 * out = ..., * a = ..., * b = ...;
            Xor(out, a, b, size);
        @endcode */
    static inline void Xor(uint8 * out, const uint8 * a, const uint8 * b, size_t size)
    {
        for(size_t i = 0; i < size; i++) out[i] = a[i] ^ b[i];
    }
    
}



#endif
