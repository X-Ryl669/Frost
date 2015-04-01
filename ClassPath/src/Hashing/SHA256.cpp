
#include <memory.h>
// We need SHA256 declaration
#include "../../include/Hashing/SHA256.hpp"


template <class T>
static inline T rotrFixed(T x, unsigned int y)
{
  // assert(y < sizeof(T)*8);
  return (x>>y) | (x<<(sizeof(T)*8-y));
}

const uint32 Hashing::SHA256::K[64] =
{
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

// Start the hashing
void Hashing::SHA256::Start()
{
    hash[0] = 0x6a09e667;
    hash[1] = 0xbb67ae85;
    hash[2] = 0x3c6ef372;
    hash[3] = 0xa54ff53a;
    hash[4] = 0x510e527f;
    hash[5] = 0x9b05688c;
    hash[6] = 0x1f83d9ab;
    hash[7] = 0x5be0cd19;

    count = 0;
    length = 0;
}

#define Ch(x,y,z)       (z ^ (x & (y ^ z)))
#define Maj(x,y,z)      (((x | y) & z) | (x & y))
#define S(x, n)         RORc((x),(n))
#define R(x, n)         (((x) & 0xFFFFFFFF)>>(n))
#define Sigma0(x)       (S(x, 2) ^ S(x, 13) ^ S(x, 22))
#define Sigma1(x)       (S(x, 6) ^ S(x, 11) ^ S(x, 25))
#define Gamma0(x)       (S(x, 7) ^ S(x, 18) ^ R(x, 3))
#define Gamma1(x)       (S(x, 17) ^ S(x, 19) ^ R(x, 10))
#define LOAD32H(x, y)   { x = ((uint32)((y)[0] & 255)<<24) | ((uint32)((y)[1] & 255)<<16) | ((uint32)((y)[2] & 255)<<8)  | ((uint32)((y)[3] & 255)); }
#define STORE32H(x, y)  { (y)[0] = (uint8)(((x)>>24)&255); (y)[1] = (uint8)(((x)>>16)&255); (y)[2] = (uint8)(((x)>>8)&255); (y)[3] = (uint8)((x)&255); }
#define STORE64H(x, y)                                                                     \
                        { (y)[0] = (uint8)(((x)>>56)&255); (y)[1] = (uint8)(((x)>>48)&255);     \
                          (y)[2] = (uint8)(((x)>>40)&255); (y)[3] = (uint8)(((x)>>32)&255);     \
                          (y)[4] = (uint8)(((x)>>24)&255); (y)[5] = (uint8)(((x)>>16)&255);     \
                          (y)[6] = (uint8)(((x)>>8)&255); (y)[7] = (uint8)((x)&255); }

#if defined(_MSC_VER)
    /* instrinsic rotate */
    #include <stdlib.h>
    #pragma intrinsic(_lrotr,_lrotl)
    #define RORc(x,n) _lrotr(x,n)

#elif defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__)) && !defined(INTEL_CC) && !defined(LTC_NO_ASM)

    static inline unsigned RORc(unsigned word, const int i)
    {
      asm ("rorl %%cl,%0"
        :"=r" (word)
        :"0" (word),"c" (i));
      return word;
    }
#elif defined(_POSIX)
    #define RORc(x, y) \
            ( ((((unsigned long) (x) & 0xFFFFFFFFUL) >> (unsigned long) ((y) & 31)) | \
            ((unsigned long) (x) << (unsigned long) (32 - ((y) & 31)))) & 0xFFFFFFFFUL)
#endif

// Perform a SHA round
void Hashing::SHA256::Transform(const uint8 * block)
{
    uint32 S[8], W[64], t0, t1;
    uint32 t; int32 i;

    // Copy state in temp
    memcpy(S, hash, sizeof(hash));

    // copy the state into 512-bits into W[0..15]
    for (i = 0; i < 16; i++)
        LOAD32H(W[i], block + (4*i));

    // fill W[16..63]
    for (i = 16; i < 64; i++)
        W[i] = Gamma1(W[i - 2]) + W[i - 7] + Gamma0(W[i - 15]) + W[i - 16];


    // Compress
    #define RND(a,b,c,d,e,f,g,h,i)                        \
        t0 = h + Sigma1(e) + Ch(e, f, g) + K[i] + W[i];   \
        t1 = Sigma0(a) + Maj(a, b, c);                    \
        d += t0;                                          \
        h  = t0 + t1;

    for (i = 0; i < 64; ++i)
    {
        RND(S[0],S[1],S[2],S[3],S[4],S[5],S[6],S[7],i);
        t = S[7]; S[7] = S[6]; S[6] = S[5]; S[5] = S[4];
        S[4] = S[3]; S[3] = S[2]; S[2] = S[1]; S[1] = S[0]; S[0] = t;
    }

    // feedback
    for (i = 0; i < 8; i++)
        hash[i] += S[i];
}

// Hash the given data
void Hashing::SHA256::Hash(const uint8 * buffer, uint32 size)
{
    if (!buffer || !size) return;
    uint32 n;
    while (size > 0)
    {
        if (count == 0 && size >= SHA256BlockSize)
        {
            Transform((unsigned char *)buffer);
            length      += SHA256BlockSize * 8;
            buffer        += SHA256BlockSize;
            size          -= SHA256BlockSize;
        } else
        {
            n = min(size, (SHA256BlockSize - count));
            memcpy(workBuffer + count, buffer, n);
            count += n;
            buffer   += n;
            size     -= n;
            if (count == SHA256BlockSize)
            {
                Transform(workBuffer);
                length += SHA256BlockSize * 8;
                count = 0;
            }
        }
    }

/*
    // Current index in work buffer
    uint32 ipos = (uint32)(count & (SHA256BlockSize - 1));
    // And the remaining space before transforming
    uint32 ispace = SHA256BlockSize - ipos;

    uint8 * pData = (uint8 *)buffer;
    if ((count += size) < size) ++length;

    while(size >= ispace)
    {
        memcpy(((uint8*)workBuffer) + ipos, pData, ispace);
        ipos = 0;
        size -= ispace;
        pData += ispace;
        ispace = SHA256BlockSize;

        Transform();
    }

    memcpy(((uint8*)workBuffer) + ipos, pData, size);
*/


}

// Finalize the hashing
void Hashing::SHA256::Finalize(uint8 * outBuffer)
{
    if (!outBuffer) return;

    int i;

    // increase the length of the message
    length += count * 8;

    // append the '1' bit
    workBuffer[count++] = (uint8)0x80;

    /* if the length is currently above 56 bytes we append zeros
    * then compress.  Then we can fall back to padding zeros and length
    * encoding like normal.
    */
    if (count > 56)
    {
        memset(&workBuffer[count], 0, 64 - count);
        Transform(workBuffer);
        count = 0;
    }

    // pad upto 56 bytes of zeroes
    if (count < 56)
    {
        memset(&workBuffer[count], 0, 56 - count);
        count = 56;
    }

    // Store length
    STORE64H(length, workBuffer+56);
    Transform(workBuffer);

    /* copy output */
    for (i = 0; i < 8; i++)
        STORE32H(hash[i], outBuffer + (4*i));

    Start();
}

/*
void Hashing::SHA256Hash(unsigned char *_pOutDigest, const unsigned char *_pData, uint32 nSize)
{

    // Be safe
    if ( !_pOutDigest || !_pData )
        return;

    SHA256 cSHA256;
    memset(&cSHA256, 0, sizeof(cSHA256));

    cSHA256.Start();
    cSHA256.Hash(_pData,nSize);
    cSHA256.Finalize(_pOutDigest);
}
*/
