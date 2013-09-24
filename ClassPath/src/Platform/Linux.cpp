// We need our declaration
#include "../../include/Platform/Platform.hpp"
// We need locks too
#include "../../include/Threading/Lock.hpp"

#ifdef _POSIX
#include <termios.h>
namespace Platform
{
    void * malloc(size_t size, const bool)
    {
        return ::malloc(size);
    }
    
    void * calloc(size_t elementNumber, size_t size, const bool)
    {
        return ::calloc(elementNumber, size);
    }
   
    void free(void * p, const bool)
    {
        ::free(p);
    }
    void * realloc(void * p, size_t size)
    {
        return ::realloc(p, size);
    }
    
    bool queryHiddenInput(const char * prompt, char * buffer, size_t & size)
    {
        struct termios oflags, nflags;
        
        // Don't allow multiple thread from running here
        static Threading::Lock lock;
        Threading::ScopedLock scope(lock);

        // Disabling echo
        FILE *in = fopen("/dev/tty", "r+"), *out = in;
        if (in == NULL)
        {
            in = stdin;
            out = stderr;
        }

        tcgetattr(fileno(in), &oflags);
        nflags = oflags;
        nflags.c_lflag &= ~ECHO;
        nflags.c_lflag |= ECHONL;

        if (tcsetattr(fileno(in), TCSANOW, &nflags) != 0
            || fputs(prompt, out) < 0
            || fflush(out) != 0
            || fgets(buffer, size, stdin) == NULL
            || tcsetattr(fileno(stdin), TCSANOW, &oflags) != 0)
        {
            if (in != stdin) fclose(in);
            return false;
        }
 
        if (in != stdin) fclose(in);
        
        size = strlen(buffer);
        if (size && buffer[size - 1] == '\n')
            buffer[--size] = 0;
            
        return true;
    }    
}

#endif 
