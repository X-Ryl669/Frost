#ifndef hpp_Types_hpp
#define hpp_Types_hpp


// We need our configuration
#include "../ClassPathConfig.hpp"

// Configure the typical macros
#if __linux == 1
    #define _LINUX 1
#endif
#if __APPLE__ == 1
    #define _MAC 1
#endif

#if (_LINUX == 1) || (_MAC == 1)
#if __GNUC__ >= 4
    #define HAS_ATOMIC_BUILTIN 1
    #if __GCC_ATOMIC_LLONG_LOCK_FREE == 1
        #define HAS_ATOMIC_BUILTIN64 1
    #else
        #if __x86_64__
	       #define HAS_ATOMIC_BUILTIN64 1
        #elif __ARMEL__
	       #define NO_ATOMIC_BUILTIN64 1 // Should change that as soon as it's supported by the libs
        #else
            #if defined(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_8)
        	    #define HAS_ATOMIC_BUILTIN64 1
            #else
                #define NO_ATOMIC_BUILTIN64 1
            #endif
        #endif
    #endif
#endif
#elif defined(_WIN32)
    #if _M_AMD64
        #define HAS_ATOMIC_BUILTIN64 1
    #else
        #define NO_ATOMIC_BUILTIN64 1
    #endif
#endif

// Check whether we can include <atomic>
#if defined(__clang__)
    #if __has_include( <atomic> )
        #define HAS_STD_ATOMIC 1
    #else
        #define HAS_STD_ATOMIC 0
    #endif
#endif

#if __cplusplus >= 201103L
    #define HAS_STD_ATOMIC 1
#endif

// Safety checks for this configuration
#if (WantAtomicClass == 1 && HAS_ATOMIC_BUILTIN != 1 && !defined(_WIN32))
#error You can not build the AtomicClass on this platform, since it does not have atomic builtins.
#endif

// Don't test against Linux and Mac each time we need a Posix system
#if (_LINUX == 1) || (_MAC == 1)
#define _POSIX 1
#endif


#ifdef DontWantTypes
#define DontWantUINT8
#define DontWantUINT16
#define DontWantUINT32
#define DontWantUINT64
#define DontWantINT8
#define DontWantINT16
#define DontWantINT32
#define DontWantINT64
#endif

#ifdef _WIN32
    #ifndef WINVER
        #define WINVER  0x600
    #endif
    #ifndef _WIN32_WINNT
        #define _WIN32_WINNT 0x600
    #endif
    // We don't care about the unreferenced formal parameter warning
    #pragma warning(disable: 4100)
    // Don't want Windows to include its own min/max macro
    #define NOMINMAX
    #include <winsock2.h>
    // Remove the mess Microsoft left after it in rpcndr.h
    #undef small
    #undef hyper
    #include <windows.h>
    #include <Mmsystem.h>
    #include <Nb30.h>
    #include <ws2tcpip.h>
    #include <tchar.h>
    #include <iphlpapi.h>
    // Remove conflicting winnt.h definition
    #undef DELETE

    #if defined(_MSC_VER)
        #include <intrin.h>

        // This is required, because depending on the current RUNNING kernel (Vista vs XP),
        // either the function is present in kernel32.dll or not. So we provide our own when they are not.
        extern unsigned __int64 (*PTRInterlockedCompareExchange64)(volatile unsigned __int64 *, unsigned __int64, unsigned __int64);
        extern unsigned __int64 (*PTRInterlockedExchange64)(volatile unsigned __int64 *, unsigned __int64);
        extern unsigned __int64 (*PTRInterlockedExchangeAdd64)(volatile unsigned __int64 *, unsigned __int64);
        extern unsigned __int64 (*PTRInterlockedIncrement64)(volatile unsigned __int64 *);
        extern unsigned __int64 (*PTRInterlockedDecrement64)(volatile unsigned __int64 *);
    #endif
#elif defined(_POSIX)
    #include <arpa/inet.h>
    #include <dirent.h>
    #include <errno.h>
    #include <execinfo.h>
    #include <fcntl.h>
    #include <netdb.h>
    #include <netinet/in.h>
    #include <pthread.h>
    #include <signal.h>
    #include <sys/poll.h>
    #include <sys/socket.h>
    #include <sys/time.h>
    #include <sys/types.h>
    #include <unistd.h>
    #include <sys/ptrace.h>
    #include <aio.h>
    #include <ifaddrs.h>
  #if defined(_LINUX)
    #include <semaphore.h>
    typedef sem_t sig_sem;
    #define SemInit(s, c) sem_init((sem_t*)&s, 0, c)
    #define SemPost(s)    sem_post(&s)
    #define SemWait(s)    sem_wait(&s)
    #define SemDestroy(s) sem_destroy((sem_t*)&s);
  #elif defined(_MAC)
    #include <sys/sysctl.h>
    #include <sys/stat.h>
    #include <sys/uio.h>
    #include <net/route.h>
    #include <net/if_dl.h>
    #include <mach/semaphore.h>

    typedef semaphore_t sig_sem;
    #define SemInit(s, c) semaphore_create(mach_task_self(), (semaphore_t*)&s, SYNC_POLICY_FIFO, c)
    #define SemPost(s)    semaphore_signal(s)
    #define SemWait(s)    semaphore_wait(s)
    #define SemDestroy(s) semaphore_destroy(mach_task_self(), s)
  #endif
#endif


#if defined(_WIN32) || defined(_POSIX)
#include <limits.h>
#include <stdlib.h>
#include <memory.h>
#include <new>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#endif

#ifdef _WIN32
    #ifndef DontWantUINT8
        typedef unsigned char uint8;
    #endif
    #ifndef DontWantUINT32
        typedef unsigned int uint32;
    #endif
    #ifndef DontWantUINT16
        typedef unsigned short uint16;
    #endif
    #ifndef DontWantUINT64
        typedef unsigned __int64 uint64;
    #endif
    #ifndef DontWantINT8
        typedef signed char int8;
    #endif
    #ifndef DontWantINT32
        typedef signed int int32;
    #endif
    #ifndef DontWantINT16
        typedef signed short int16;
    #endif
    #ifndef DontWantINT64
        typedef signed __int64 int64;
    #endif
    #define PF_LLD  "%I64d"
    #define PF_LLU  "%I64u"

    /** Unless you run on embedded OS, you'll not need those functions
        @return int value you need to pass to leaveAtomicSection */
    #define enterAtomicSection() 1
    /** Unless you run on embedded OS, you'll not need those functions */
    #define leaveAtomicSection(X) do {} while(0)

#elif defined(_POSIX)
    #include <sys/types.h>
    #ifndef DontWantUINT8
        typedef u_int8_t uint8;
    #endif
    #ifndef DontWantUINT32
        typedef u_int32_t uint32;
    #endif
    #ifndef DontWantUINT16
        typedef u_int16_t uint16;
    #endif
    #ifndef DontWantUINT64
        typedef u_int64_t uint64;
    #endif
    #ifndef DontWantINT8
        typedef int8_t int8;
    #endif
    #ifndef DontWantINT32
        typedef int32_t int32;
    #endif
    #ifndef DontWantINT16
        typedef int16_t int16;
    #endif
    #ifndef DontWantINT64
        typedef int64_t int64;
    #endif

    #define PF_LLD  "%lld"
    #define PF_LLU  "%llu"

    /** Unless you run on embedded OS, you'll not need those functions
        @return int value you need to pass to leaveAtomicSection */
    #define enterAtomicSection() 1
    /** Unless you run on embedded OS, you'll not need those functions */
    #define leaveAtomicSection(X) do {} while(0)

#else
    // Prevent the types.h file to define the fd_set type (stupid definition by the way)
    #define __USE_W32_SOCKETS
	#include <stdio.h>
	#include <time.h>
	#include <stdlib.h>
	#include <string.h>
	#include <wchar.h>
    #include "pthreadRTOS.h"
    #include "lwip/sockets.h"

    /* lwIP includes */
    #include "lwip/tcpip.h"
    #include "lwip/netif.h"
    #include "lwip/ip_addr.h"
    #include "lwip/debug.h"
    #include "lwip/debug.h"

    #include "socket_map.h"
	#include "bsp/inc/hw_types.h"
	#include "driverlib/rom_map.h"
	#include "driverlib/interrupt.h"


	#ifndef DontWantUINT8
		typedef unsigned char uint8;
	#endif
	#ifndef DontWantUINT32
		typedef unsigned int uint32;
	#endif
	#ifndef DontWantUINT16
		typedef unsigned short uint16;
	#endif

	#ifndef DontWantUINT64
        typedef unsigned long long uint64;
    #endif

	#ifndef DontWantINT8
		typedef signed char int8;
	#endif
	#ifndef DontWantINT32
		typedef signed int int32;
	#endif
	#ifndef DontWantINT16
		typedef signed short int16;
	#endif

	#ifndef DontWantINT64
        typedef long long int64;
    #endif

    #define PF_LLD  "%lld"
    #define PF_LLU  "%llu"

    /** Unless you run on embedded OS, you'll not need those functions
        @return int value you need to pass to leaveAtomicSection */
    extern "C" int enterAtomicSection();
    /** Unless you run on embedded OS, you'll not need those functions */
    extern "C" void leaveAtomicSection(int);
#endif

#ifndef min
    template <typename T>
        inline T min(T a, T b) { return a < b ? a : b; }
    template <typename T>
        inline T max(T a, T b) { return a > b ? a : b; }
    template <typename T>
        inline T clamp(T a, T low, T high) { return a < low ? low : (a > high ? high : a); }

    #define minDefined
#endif

// Helper function that should never be omitted
template <typename T> inline T Abs(T a) { return a < 0 ? -a : a; }
// Easy method to get a int as big endian in all case
inline uint32 BigEndian(uint32 a)
{
    static unsigned long signature= 0x01020304UL;
    if (4 == (unsigned char&)signature) // little endian
        return ((a & 0xFF000000) >> 24) | ((a & 0x00FF0000) >> 8) | ((a & 0xFF00) << 8) | ((a & 0xFF) << 24);
    if (1 == (unsigned char&)signature) // big endian
        return a;
    // Error here
    return 0;
}
// Easy method to get a int as big endian in all case
inline uint16 BigEndian(uint16 a)
{
    static unsigned long signature= 0x01020304UL;
    if (4 == (unsigned char&)signature) // little endian
        return (a >> 8) | ((a & 0xFF) << 8);
    if (1 == (unsigned char&)signature) // big endian
        return a;
    return 0;
}
// Easy method to get a int as big endian in all case
inline uint64 BigEndian(uint64 a)
{
    static unsigned long signature= 0x01020304UL;
    if (4 == (unsigned char&)signature) // little endian
    {
        return ((a & 0x00000000000000ffULL) << 56) | ((a & 0x000000000000ff00ULL) << 40)
             | ((a & 0x0000000000ff0000ULL) << 24) | ((a & 0x00000000ff000000ULL) << 8)
             | ((a & 0x000000ff00000000ULL) >> 8)  | ((a & 0x0000ff0000000000ULL) >> 24)
             | ((a & 0x00ff000000000000ULL) >> 40) | ((a & 0xff00000000000000ULL) >> 56);
    }
    if (1 == (unsigned char&)signature) // big endian
        return a;
    return 0;
}




#ifdef _MSC_VER
    #define Deprecated(X) __declspec(deprecated) X
    #define Aligned(X) __declspec(align (X))
    #define ForcedInline(X) __forceinline X
    #define Unused(X) UNREFERENCED_PARAMETER(X)
#elif defined(__GNUC__)
    #define Deprecated(X) X __attribute__ ((deprecated))
    #define Aligned(X) __attribute__ ((aligned (X)))
    #define ForcedInline(X) X __attribute__ ((always_inline))
    #define Unused(X) X __attribute__ ((unused))
#else
    #define Deprecated(X) X
    #define Aligned(X)
    #define ForcedInline(X) X
    #define Unused(X) X
#endif
#define ForceUndefinedSymbol(x) void* __ ## x ## _fp =(void*)&x;

/** Delete a pointer and zero it */
template <typename T> inline void delete0(T*& t) { delete t; t = 0; }
/** Delete a pointer to an array and zero it */
template <typename T> inline void deleteA0(T*& t) { delete[] t; t = 0; }
/** Delete a pointer to an array, zero it, and zero the elements count too */
template <typename T, typename U> inline void deleteA0(T*& t, U & size) { delete[] t; t = 0; size = 0; }
/** Delete all items of an array, delete the array, zero it, and zero the elements count too */ 
template <typename T, typename U> inline void deleteArray0(T*& t, U & size) { for (U i = 0; i < size; i++) delete t[i]; delete[] t; t = 0; size = 0; }
/** Delete all array items of an array, delete the array, zero it, and zero the elements count too */ 
template <typename T, typename U> inline void deleteArrayA0(T*& t, U & size) { for (U i = 0; i < size; i++) delete[] t[i]; delete[] t; t = 0; size = 0; }


namespace Private
{
    // If the compiler stop here, you're actually trying to figure out the size of an pointer and not a compile-time array.
    template< typename T, size_t N >
    char (&ArraySize_REQUIRES_ARRAY_ARGUMENT(T (&)[N]))[N];
}

#define ArrSz(X) sizeof(Private::ArraySize_REQUIRES_ARRAY_ARGUMENT(X))
#ifndef ArraySize
   #define ArraySize ArrSz
#endif


#ifndef TypeDetection_Impl
#define TypeDetection_Impl
template <typename T> struct IsPOD { enum { result = 0 }; };
template <typename T> struct IsPOD<T*> { enum { result = 1}; };
#define MakePOD(X) template <> struct IsPOD<X > { enum { result = 1 }; }; \
                   template <> struct IsPOD<const X > { enum { result = 1 }; }
#define MakeIntPOD(X) template <> struct IsPOD<signed X > { enum { result = 1 }; }; \
                      template <> struct IsPOD<unsigned X > { enum { result = 1 }; }; \
                      template <> struct IsPOD<const signed X > { enum { result = 1 }; }; \
                      template <> struct IsPOD<const unsigned X > { enum { result = 1 }; }

MakePOD(bool);
MakeIntPOD(int);
MakeIntPOD(long);
MakeIntPOD(char);
MakeIntPOD(long long);
MakeIntPOD(short);
MakePOD(double);
MakePOD(long double);
MakePOD(float);

#undef MakePOD
#undef MakeIntPOD
#endif


#ifdef _WIN32
    typedef HANDLE              HTHREAD;
    typedef HANDLE              HMUTEX;
    typedef HANDLE              OEVENT;
#else
    typedef pthread_t           HTHREAD;
    typedef pthread_mutex_t     HMUTEX;
    typedef pthread_mutex_t     OEVENT;
#endif

#ifndef _REMOVE_USELESS_CHECK

    // This part is only used for debugging to compare installations
    #if (WantSSLCode == 1)
        #define _SSLFlag 1
        #define _SSLFlagName SSL
    #else
        #define _SSLFlag 0
        #define _SSLFlagName _
    #endif

    #if (WantAES == 1)
        #define _AESFlag 2
        #define _AESFlagName AES
    #else
        #define _AESFlag 0
        #define _AESFlagName _
    #endif

    #ifdef DontWantTypes
        #define _TypeFlag 4
        #define _TypeFlagName NoType
    #else
        #define _TypeFlag 0
        #define _TypeFlagName _
    #endif

    #if (WantFFMPEG == 1)
        #define _FFMPEGFlag 8
        #define _FFMPEGFlagName FFMPEG
    #else
        #define _FFMPEGFlag 0
        #define _FFMPEGFlagName _
    #endif

    #if (WantThreadLocalStorage == 1)
        #define _TLSFlag 16
        #define _TLSFlagName TLS
    #else
        #define _TLSFlag 0
        #define _TLSFlagName _
    #endif

    #if (WantBaseEncoding == 1)
        #define _BaseFlag 32
        #define _BaseFlagName Base
    #else
        #define _BaseFlag 0
        #define _BaseFlagName _
    #endif

    #if (WantFloatParsing == 1)
        #define _FloatFlag 64
        #define _FloatFlagName Float
    #else
        #define _FloatFlag 0
        #define _FloatFlagName _
    #endif

    #if (WantTimedProfiling == 1)
        #define _ChronoFlag 128
        #define _ChronoFlagName Chrono
    #else
        #define _ChronoFlag 0
        #define _ChronoFlagName _
    #endif

    #if (WantAtomicClass == 1)
        #define _AtomicFlag 256
        #define _AtomicFlagName Atomic
    #else
        #define _AtomicFlag 0
        #define _AtomicFlagName _
    #endif

    #if (WantMD5Hashing == 1)
        #define _MD5Flag 512
        #define _MD5FlagName MD5
    #else
        #define _MD5Flag 0
        #define _MD5FlagName _
    #endif

    #if (WantExtendedLock == 1)
        #define _ExLockFlag 1024
        #define _ExLockFlagName ExLock
    #else
        #define _ExLockFlag 0
        #define _ExLockFlagName _
    #endif

    #if (WantSOAP == 1)
        #define _SOAPFlag 2048
        #define _SOAPFlagName SOAP
    #else
        #define _SOAPFlag 0
        #define _SOAPFlagName _
    #endif

    #if (WantCompression == 1)
        #define _CompressFlag       4096
        #define _CompressFlagName   Compress
    #else
        #define _CompressFlag   0
        #define _CompressFlagName   _
    #endif

    #if (WantLightImageCode == 1)
        #define _OwnPicFlag       8192
        #define _OwnPicFlagName   OwnPic
    #else
        #define _OwnPicFlag   0
        #define _OwnPicFlagName   _
    #endif

    #if (WantRegularExpressions == 1)
        #define _RegExFlag        16384
        #define _RegExFlagName    RegEx
    #else
        #define _RegExFlag   0
        #define _RegExFlagName   _
    #endif
    
    #if (WantPingCode == 1)
        #define _PingFlag       32768
        #define _PingFlagName ICMP
    #else
        #define _PingFlag 0
        #define _PingFlagName _
    #endif
    
    #if (WantBSCCompression == 1)
        #define _BSCFlag       65536
        #define _BSCFlagName BSC
    #else
        #define _BSCFlag 0
        #define _BSCFlagName _
    #endif


    #if (DEBUG==1)
        #define _DebugFlag         1073741824
        #define _DebugFlagName     Debug
    #else
        #define _DebugFlag         0
        #define _DebugFlagName     Release
    #endif

    #ifdef _LINUX
       #define _Platform           Linux
    #elif defined(_MAC)
       #define _Platform           Mac
    #elif defined(_WIN32)
       #define _Platform           Win32
    #endif

    #if (_FILE_OFFSET_BITS == 64)
       #define _LargeFileOffset    LFS
    #else
       #define _LargeFileOffset    _
    #endif

    #if (HAS_STD_ATOMIC == 1)
       #define _Atomic             Atomic
    #else
       #define _Atomic             _
    #endif


    #define _ClassPathFlags (_SSLFlag + _AESFlag + _TypeFlag + _FFMPEGFlag + _TLSFlag + _BaseFlag + _FloatFlag + \
                             _ChronoFlag + _AtomicFlag + _MD5Flag + _ExLockFlag + _SOAPFlag + _CompressFlag + \
                             _OwnPicFlag + _RegExFlag + _PingFlag + _BSCFlag + _DebugFlag)


    #define _String(X) #X
    #define Stringize(X) _String(X) " "

    #define PASTER(x,y) x ## _ ## y
    #define EVALUATOR(x,y)  PASTER(x,y)
    #define NAME(fun, TERM) EVALUATOR(fun, TERM)

    namespace BuildInfo
    {
        // If compilation/linking stops here, it's because you are include the Classpath with different flags than when it was built.
        // This is going to break your software in a very subtle way, since the binary will not match the sources, so debugging will be painful, if not impossible.
        // The solution is simple, make sure you are using the same flags for the both projects.
        enum { ClassPathFlags = _ClassPathFlags };
        extern int NAME(NAME(NAME(NAME(NAME(NAME(NAME(NAME(NAME(NAME(NAME(NAME(NAME(NAME(NAME(NAME(NAME(NAME(_checkSameCompilationFlags_, _SSLFlagName), _AESFlagName), _TypeFlagName), _FFMPEGFlagName), _TLSFlagName), _BaseFlagName), _FloatFlagName), _ChronoFlagName), _AtomicFlagName), _MD5FlagName), _ExLockFlagName), _SOAPFlagName), _CompressFlagName), _OwnPicFlagName), _RegExFlagName), _PingFlagName), _BSCFlagName), _DebugFlagName);
        inline int getBuildFlags() { return NAME(NAME(NAME(NAME(NAME(NAME(NAME(NAME(NAME(NAME(NAME(NAME(NAME(NAME(NAME(NAME(NAME(NAME(_checkSameCompilationFlags_, _SSLFlagName), _AESFlagName), _TypeFlagName), _FFMPEGFlagName), _TLSFlagName), _BaseFlagName), _FloatFlagName), _ChronoFlagName), _AtomicFlagName), _MD5FlagName), _ExLockFlagName), _SOAPFlagName), _CompressFlagName), _OwnPicFlagName), _RegExFlagName), _PingFlagName), _BSCFlagName), _DebugFlagName); }
        extern const char * getBuildFlagsName();

        extern int NAME(NAME(NAME(NAME(_checkSameBuildFlags_, _DebugFlagName), _Platform), _LargeFileOffset), _Atomic);
        inline int checkBuildFlags() { return NAME(NAME(NAME(NAME(_checkSameBuildFlags_, _DebugFlagName), _Platform), _LargeFileOffset), _Atomic); }

        // If the linker stops here, it's because you are trying to link to the library that was built with some different flags than what you included.
        // The linker error shows the build flags the main program is expecting
        // To figure out the build flags the library was built with, use this command:
        // nm libClassPath.a 2>/dev/null | c++filt | grep "D BuildInfo::_checkSameBuildFlags"
        // And
        // nm libClassPath.a 2>/dev/null | c++filt | grep "D BuildInfo::_checkSameCompilationFlags"
        static ForceUndefinedSymbol(checkBuildFlags);
        static ForceUndefinedSymbol(getBuildFlags);
    }

    #undef NAME
    #undef EVALUATOR
    #undef PASTER
    #undef Stringize
    #undef _String
    #undef _SSLFlag
    #undef _SSLFlagName

    #undef _AESFlag
    #undef _AESFlagName

    #undef _TypeFlag
    #undef _TypeFlagName

    #undef _FFMPEGFlag
    #undef _FFMPEGFlagName

    #undef _TLSFlag
    #undef _TLSFlagName

    #undef _BaseFlag
    #undef _BaseFlagName

    #undef _FloatFlag
    #undef _FloatFlagName

    #undef _ChronoFlag
    #undef _ChronoFlagName

    #undef _AtomicFlag
    #undef _AtomicFlagName

    #undef _MD5Flag
    #undef _MD5FlagName

    #undef _ExLockFlag
    #undef _ExLockFlagName

    #undef _SOAPFlag
    #undef _SOAPFlagName

    #undef _CompressFlag
    #undef _CompressFlagName

    #undef _OwnPicFlag
    #undef _OwnPicFlagName

    #undef _PingFlag
    #undef _PingFlagName

    #undef _BSCFlag
    #undef _BSCFlagName

    #undef _DebugFlag
    #undef _DebugFlagName
    
    #undef _Platform
    #undef _LargeFileOffset
    #undef _Console
    #undef _Atomic

#endif

#endif

