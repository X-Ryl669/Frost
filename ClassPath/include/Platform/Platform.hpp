#ifndef hpp_CPP_Platform_CPP_hpp
#define hpp_CPP_Platform_CPP_hpp
// Types like size-t or NULL
#include "../Types.hpp"

/** The platform specific declarations */
namespace Platform
{
    /** The end of line marker */
    enum EndOfLine
    {
        LF   =   1,     //!< The end of line is a line feed (usually 10 or "\n")
        CR   =   2,     //!< The end of line is a carriage return (usually 13 or "\r")
        CRLF =   4,     //!< The end of line is both CR and LF ("\r\n")

#ifdef _WIN32
        Default = CRLF,
#else
        Default = LF,
#endif

        Any  =   0x7,   //!< Any end of line is accepted 
    };

#ifdef _WIN32
#define PathSeparator  "\\"
#else
#define PathSeparator  "/"
#endif
    
    /** File separator char */
    enum 
    {
#ifdef _WIN32
        Separator = '\\'
#else
        Separator = '/'
#endif
    };

    /** The simple malloc overload.
        If you need to use another allocator, you should define this method 
        @param size         Element size in bytes
        @param largeAccess  If set, then optimized functions are used for large page access.
                            Allocation for large access should call free with large access. */
    void * malloc(size_t size, const bool largeAccess = false);
    /** The simple calloc overload. 
        If you need to use another allocator, you should define this method 
        @param elementCount  How many element to allocate
        @param size          One element size in bytes
        @param largeAccess  If set, then optimized functions are used for large page access.
                            Allocation for large access should call free with large access. */
    void * calloc(size_t elementCount, size_t size, const bool largeAccess = false);
    /** The simple free overload. 
        If you need to use another allocator, you should define this method 
        @param p     A pointer to an area to return to the heap 
        @param largeAccess  If set, then optimized functions are used for large page access.
                            Allocation for large access should call free with large access. */
    void free(void * p, const bool largeAccess = false);
    /** The simple realloc overload. 
        If you need to use another allocator, you should define this method 
        @param p    A pointer to the allocated area to reallocate
        @param size The required size of the new area in bytes
        @warning Realloc is intrinsically unsafe to use, since it can leak memory in most case, use safeRealloc instead */
    void * realloc(void * p, size_t size);

    /** The safe realloc method.
        This method avoid allocating a zero sized byte array (like realloc(0, 0) does).
        It also avoid leaking memory as a code like (ptr = realloc(ptr, newSize) 
        (in case of error) does). */
    inline void * safeRealloc(void * p, size_t size) 
    {
        if (p == 0 && size == 0) return 0;
        if (size == 0)
        {
            free(p);
            return 0; // On FreeBSD realloc(ptr, 0) frees ptr BUT allocates a 0 sized buffer.
        }
        void * other = realloc(p, size);
        if (size && other == NULL)
            free(p); // Reallocation fails, let's free the previous pointer

        return other;
    }
    /** Ask for a hidden input that'll be stored in the UTF-8 buffer.
        This requires a console. 
        Under Windows, this requires the process to be run from a command line.
        This is typically required for asking a password. 
        New line are not retained in the output, if present.
        
        @param prompt   The prompt that's displayed on user console 
        @param buffer   A pointer to a buffer that's at least (size) byte large 
                        that'll be filled by the function
        @param size     On input, the buffer size, on output, it's set to the used buffer size 
        @return false if it can not hide the input, or if it can't get any char in it  */
    bool queryHiddenInput(const char * prompt, char * buffer, size_t & size);
    
	
	inline bool isUnderDebugger()
	{
#if (DEBUG==1)
    #ifdef _WIN32
		return (IsDebuggerPresent() == TRUE);
    #elif defined(_LINUX)
		static signed char testResult = 0;
		if (testResult == 0)
		{
			testResult = (char) ptrace (PT_TRACE_ME, 0, 0, 0);
			if (testResult >= 0)
			{
				ptrace (PT_DETACH, 0, (caddr_t) 1, 0);
				testResult = 1;
			}
		}
        return (testResult < 0);
    #elif defined (_MAC)
		static signed char testResult = 0;
		if (testResult == 0)
		{
			struct kinfo_proc info;
			int m[] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid() };
			size_t sz = sizeof (info);
			sysctl (m, 4, &info, &sz, 0, 0);
			testResult = ((info.kp_proc.p_flag & P_TRACED) != 0) ? 1 : -1;
		}

		return testResult > 0;
    #elif defined (NEXIO)
	    return true;
	#endif
#endif
	    return false;
	}
    
    /** This is used to trigger the debugger when called */
    inline void breakUnderDebugger()
    {
      
#if (DEBUG==1)
        if(isUnderDebugger())
    #ifdef _WIN32
            DebugBreak();
    #elif defined(_LINUX)
            raise(SIGTRAP);
    #elif defined (_MAC)
            __asm__("int $3\n" : : );
    #elif defined (NEXIO)
            __asm("bkpt");
    #else
            #error Put your break into debugger code here
    #endif
#endif
    }
    
#if defined(_POSIX)
    /** Useful RAII class for Posix file index */
    class FileIndexWrapper
    {
        int fd;

    public:
        inline operator int() const { return fd; }
        inline void Mutate(int newfd) { if (fd >= 0) close(fd); fd = newfd; }

        FileIndexWrapper(int fd) : fd(fd) {}
        ~FileIndexWrapper() { if (fd >= 0) close(fd); fd = -1; }
    };
#endif

}

#endif
