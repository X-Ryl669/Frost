#include <memory.h>
// We don't need our declaration expressively

namespace Crypto
{
#ifndef _WIN32
    void SafeMemclean(unsigned char * buffer, const unsigned int size) __attribute__((__used__));
#endif


#ifdef _WIN32
    #pragma optimize("g", off)
#endif
    void SafeMemclean(unsigned char * buffer, const unsigned int size)
    {
        memset(buffer, 0, size);
        // This is here to force the compiler not to remove the call above,
        // as we are reading the supposed clean area
        if (size > 2)
            buffer[0] = buffer[1] + buffer[2];
    }

}
