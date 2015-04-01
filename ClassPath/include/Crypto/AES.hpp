#ifndef hpp_CPP_AESCyphering_CPP_hpp
#define hpp_CPP_AESCyphering_CPP_hpp

// We need types
#include "../Types.hpp"
// We need base types
#include "BaseSymCrypt.hpp"
#include <stdio.h>
#include <string.h>


/** Cryptographic functions.
    These classes are all using a similar format, so changing the crypto routine, is as easy as search and replace.
    <br>
    You'll find a good explaination of the assymetric cryptography (RSA, DSA) in RSA documentation.
    For basic symetric cryptography, you'll use AES, as it proved fast and secure.

    To assert the authenticity of some message, please refer to BaseSign interface.<br>
    To cypher something so it's inintelligible to anyone except the recipient, use BaseAsymCrypt or BaseSymCrypt.<br>
    To establist a secret between two parties on unsecure communication channel, use BaseSecret.<br>

    @sa RSA, DSAT, BaseSign, BaseAsymCrypt, BaseSymCrypt, BaseSecret
*/
namespace Crypto
{
    /** The Rijndael (AES) algorithm is used cypher and uncypher from 128 to 256 bits messages.
        */
    struct AES : public BaseSymCrypt
    {
        // Static look up tables
    private:
        /** From AES standard */
        static const int32  sm_alog[256];
        /** From AES standard */
        static const int32  sm_log[256];
        /** From AES standard */
        static const int8   sm_S[256];
        /** From AES standard */
        static const int8   sm_Si[256];
        /** From AES standard */
        static const int32  sm_T1[256];
        /** From AES standard */
        static const int32  sm_T2[256];
        /** From AES standard */
        static const int32  sm_T3[256];
        /** From AES standard */
        static const int32  sm_T4[256];
        /** From AES standard */
        static const int32  sm_T5[256];
        /** From AES standard */
        static const int32  sm_T6[256];
        /** From AES standard */
        static const int32  sm_T7[256];
        /** From AES standard */
        static const int32  sm_T8[256];
        /** From AES standard */
        static const int32  sm_U1[256];
        /** From AES standard */
        static const int32  sm_U2[256];
        /** From AES standard */
        static const int32  sm_U3[256];
        /** From AES standard */
        static const int32  sm_U4[256];
        /** From AES standard */
        static const int8   sm_rcon[30];
        /** From AES standard */
        static const int32  sm_shifts[3][4][2];
    public:
        /** Empty output chain */
        static const uint8  sm_chain0[33];

        // Members
    private:
        /** The private constants */
        enum
        {
            MaxKC = 8,
            MaxBC = 8,
            /** The maximum number of rounds as defined in the specification */
            MaxRounds = 14,
        };

        /** Is the key initialized ? */
        bool            keySetUp;
        /** The block size */
        uint32          blockSize;
        /** The currently selected number of rounds */
        uint32          roundsCount;
        /** The key length in uint8s */
        uint32          keyLength;
        /** The buffers */
        /** Encryption round key */
        int32           encKe[MaxRounds + 1][MaxBC];
        /** Decryption round key */
        int32           decKd[MaxRounds + 1][MaxBC];
        /** The initial chain block */
        uint8           chain0[MaxBlockSize];
        /** The working chain block */
        uint8           chain[MaxBlockSize];
        /** Some temporary buffer */
        int32           tk[MaxKC];
        int32           a[MaxBC];
        int32           t[MaxBC];

        // Accessors
    public:
        /** Get the current key length in uint8s */
        inline uint32 getKeyLength() const throw()          { return keySetUp ? keyLength : 0; }

        // Helpers
    private:
        /** Multiply two elements of GF(2^m) */
        inline int32 Multiply(const int32 a, const int32 b) const       {  return (a != 0 && b != 0) ? sm_alog[(sm_log[a & 0xFF] + sm_log[b & 0xFF]) % 255] : 0; }

        /** Convenience method used in generating Transposition Boxes */
        inline int32 Mul4(int32 a, const int8 b[]) const
        {
            if(a == 0)  return 0;
            a = sm_log[a & 0xFF];
            int a0 = (b[0] != 0) ? sm_alog[(a + sm_log[b[0] & 0xFF]) % 255] & 0xFF : 0;
            int a1 = (b[1] != 0) ? sm_alog[(a + sm_log[b[1] & 0xFF]) % 255] & 0xFF : 0;
            int a2 = (b[2] != 0) ? sm_alog[(a + sm_log[b[2] & 0xFF]) % 255] & 0xFF : 0;
            int a3 = (b[3] != 0) ? sm_alog[(a + sm_log[b[3] & 0xFF]) % 255] & 0xFF : 0;
            return a0 << 24 | a1 << 16 | a2 << 8 | a3;
        }

        /** XOR the given buffer with the given chain */
        inline void Xor(uint8* buff, const uint8* chain) const
        {
            if (!keySetUp) return;
            for(uint32 i = 0; i < blockSize; i++) *(buff++) ^= *(chain++);
        }

        /** Encrypt one block of plain text for default block size 128 bits
            @param in The message of 16 uint8s
            @param result The cyphered message with our key of 16 uint8s */
        void encryptDefaultBlock(const uint8* in, uint8* result);

        /** Encrypt one block of plain text for default block size 128 bits
            @param in The cyphered message of 16 uint8s
            @param result The clear message with our key of 16 uint8s */
        void decryptDefaultBlock(const uint8* in, uint8* result);

        // Interface
    public:
        /** Get the configured block size */
        virtual BlockSize getBlockSize() const { return (BlockSize)blockSize; }

        /** Set the key from the given buffer
            @param key  The 128/192/256-bit user-key to use.
            @param keyLength The length of the key in uint8
            @param chain The initial chain block for CBC and CFB modes.
            @param blockSize  The expected block size in uint8s (16, 24 or 32 uint8s). */
        void setKey(const uint8 * key, const BlockSize keyLength = DefaultBlockSize, const uint8 * chain = sm_chain0, const BlockSize blockSize = DefaultBlockSize);


        /** Encrypt one block of plain text
            @param in The message of 16 uint8s
            @param result The cyphered message with our key */
        void encryptOneBlock(const uint8* in, uint8* result);

        /** Encrypt one block of plain text
            @param in The cyphered message
            @param result The clear message with our key */
        void decryptOneBlock(const uint8* in, uint8* result);

        /** Encrypt the given buffer
            @param in The message of size n
            @param n The message size in uint8 (should be a multiple of BlockSize)
            @param result The cyphered message of size n (at least)
            @param mode The operation mode
            @return false if the key isn't set up or if the input doesn't fit a block size */
        bool Encrypt(const uint8* in, uint8* result, size_t n, const OperationMode & mode = ECB);

        /** Decrypt the given buffer
            @param in The cyphered message of size n
            @param n The message size in uint8 (should be a multiple of BlockSize)
            @param result The clear message of size n (at least)
            @param mode The operation mode
            @return false if the key isn't set up or if the input doesn't fit a block size */
        bool Decrypt(const uint8* in, uint8* result, size_t n, const OperationMode & mode = ECB);

        // Construction
    public:
        /** Default constructor */
        AES() : keySetUp(false), blockSize(0), roundsCount(0), keyLength(0)
        {
#define ZeroMem(X) memset(X, 0, sizeof(X))
            ZeroMem(encKe); ZeroMem(decKd); ZeroMem(chain0); ZeroMem(chain); ZeroMem(tk); ZeroMem(a); ZeroMem(t);
#undef ZeroMem
        }
    };

}

#define HasAES   1


#endif

