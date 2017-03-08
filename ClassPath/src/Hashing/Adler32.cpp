
// We need our declaration
#include "../../include/Hashing/Adler32.hpp"


namespace Hashing
{
    void Adler32::Start()
    {
        a = 1; b = 0;
    }
    
    
#define DO1(buf, i) { a += (buf)[i]; b += a; }
#define DO2(buf, i) DO1(buf, i); DO1(buf, i + 1);
#define DO4(buf, i) DO2(buf, i); DO2(buf, i + 2);
#define DO8(buf, i) DO4(buf, i); DO4(buf, i + 4);
#define DO16(buf)   DO8(buf, 0); DO8(buf, 8);
#define MOD(a)      a %= Base
    
    void Adler32::Hash(const uint8 * buffer, uint32 size)
    {
        if (!buffer || !size) return;
        
        // Faster implementation
        const uint32 nMax = 5552;
        while (size >= nMax)
        {
            for (int i = 0; i < nMax / 16; i++)
            {
                DO16(buffer); buffer += 16;
            }
            MOD(a); MOD(b); size -= nMax;
        }
        
        while (size >= 16)
        {
            DO16(buffer); buffer += 16; size -= 16;
        }
        
        while (size > 0)
        {
            DO1(buffer, 0); buffer ++; size --;
        }
        MOD(a); MOD(b);
    }
    
    void Adler32::Finalize(uint8 * outBuffer)
    {
        if (!outBuffer) return;
        uint32 ret = getChecksum();
        memcpy(outBuffer, &ret, sizeof(ret));
    }
    
    void Adler32::Append(uint8 ch)
    {
        a += ch;
        if (a > Base) a -= Base;
        b += a;
        if (b > Base) b -= Base;
    }
}