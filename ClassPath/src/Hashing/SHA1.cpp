
#include <memory.h>
// We need SHA1 declaration
#include "../../include/Hashing/SHA1.hpp"


// Is the system little or Big endian.
#if (('1234' >> 24) == '1')
    #define  SYSTEM_LITTLE_ENDIAN   1234
#elif (('4321' >> 24) == '1')
    #define SYSTEM_BIG_ENDIAN       4321
#endif

// SHA constants.
#ifdef SYSTEM_LITTLE_ENDIAN
const uint32 Hashing::SHA1::SHAMask[4]={0x00000000, 0x000000ff, 0x0000ffff, 0x00ffffff};
const uint32 Hashing::SHA1::SHABits[4]={0x00000080, 0x00008000, 0x00800000, 0x80000000};
#else
const uint32 Hashing::SHA1::SHAMask[4]={0x00000000, 0xff000000, 0xffff0000, 0xffffff00};
const uint32 Hashing::SHA1::SHABits[4]={0x80000000, 0x00800000, 0x00008000, 0x00000080};
#endif

// Start the hashing
void Hashing::SHA1::Start()
{
    hash[0] = 0x67452301;
    hash[1] = 0xefcdab89;
    hash[2] = 0x98badcfe;
    hash[3] = 0x10325476;
    hash[4] = 0xc3d2e1f0;

    count[0] = count[1] = 0;
}

#define F0to19(x,y,z)       (((x) & (y)) ^ (~(x) & (z)))
#define F20to39(x,y,z)      ((x) ^ (y) ^ (z))
#define F40to59(x,y,z)      (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define F60to79(x,y,z)       F20to39(x,y,z)


#define sha_round(func,k)  t = a; a = Rotate32(a,5) + func(b,c,d) + e + k + w[i]; e = d; d = c; c = Rotate32(b, 30); b = t;

// Perform a SHA round
void Hashing::SHA1::Transform()
{
    uint32   w[80], i, a, b, c, d, e, t;

    for (i = 0; i < SHA1BlockSize / 4; ++i)
        w[i] = SHABlock32(workBuffer[i]);

    for (i = SHA1BlockSize / 4; i < 80; ++i)
        w[i] = Rotate32(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

    a = hash[0];
    b = hash[1];
    c = hash[2];
    d = hash[3];
    e = hash[4];

    for(i = 0; i < 20; ++i)
    {
        sha_round(F0to19, 0x5a827999);
    }

    for(i = 20; i < 40; ++i)
    {
        sha_round(F20to39, 0x6ed9eba1);
    }

    for(i = 40; i < 60; ++i)
    {
        sha_round(F40to59, 0x8f1bbcdc);
    }

    for(i = 60; i < 80; ++i)
    {
        sha_round(F60to79, 0xca62c1d6);
    }

    hash[0] += a;
    hash[1] += b;
    hash[2] += c;
    hash[3] += d;
    hash[4] += e;
}

// Hash the given data
void Hashing::SHA1::Hash(const uint8 * buffer, uint32 size)
{
    uint32 ipos = (uint32)(count[0] & (SHA1BlockSize - 1));
    uint32 ispace = SHA1BlockSize - ipos;

    uint8 * pData=(uint8 *)buffer;
    if((count[0] += size) < size) ++count[1];

    while(size >= ispace)
    {
        memcpy(((uint8*)workBuffer) + ipos, pData, ispace);
        ipos = 0;
        size -= ispace;
        pData += ispace;
        ispace = SHA1BlockSize;

        Transform();
    }

    memcpy(((uint8*)workBuffer) + ipos, pData, size);
}

// Finalize the hashing
void Hashing::SHA1::Finalize(uint8 * outBuffer)
{
    uint32    i = (uint32)(count[0] & (SHA1BlockSize - 1));
    workBuffer[i >> 2] = (workBuffer[i >> 2] & SHAMask[i & 3]) | SHABits[i & 3];

    if(i > SHA1BlockSize - 9)
    {
        if(i < 60) workBuffer[15] = 0;
        Transform();
        i = 0;
    }
    else i = (i >> 2) + 1;

    while(i < 14) workBuffer[i++] = 0;


    workBuffer[14] = SHABlock32((count[1] << 3) | (count[0] >> 29));
    workBuffer[15] = SHABlock32(count[0] << 3);

    Transform();


    for(i = 0; i < SHA1DigestSize; ++i)
        outBuffer[i] = (uint8)(hash[i >> 2] >> 8 * (~i & 3));
}

/*
void Hashing::SHA1Hash(unsigned char *_pOutDigest, const unsigned char *_pData, uint32 nSize)
{

    // Be safe
    if ( !_pOutDigest || !_pData )
        return;

    SHA1 csha1;
    memset(&csha1, 0, sizeof(csha1));

    csha1.Start();
    csha1.Hash(_pData,nSize);
    csha1.Finalize(_pOutDigest);
}
*/
