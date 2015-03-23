// Do no modify this file
#include "../include/Types.hpp"

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
        #define _BSCFlag       131072
        #define _BSCFlagName BSC
    #else
        #define _BSCFlag 0
        #define _BSCFlagName _
    #endif
    

    #if (WantDynamicEngine == 1)
        #define _JSFlag       65536
        #define _JSFlagName JS
    #else
        #define _JSFlag 0
        #define _JSFlagName _
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
       #define _PlatformVal        2
    #elif defined(_MAC)
       #define _Platform           Mac
       #define _PlatformVal        4
    #elif defined(_WIN32)
       #define _Platform           Win32
       #define _PlatformVal        8
    #else
       #define _PlatformVal        0
    #endif

    #if (_FILE_OFFSET_BITS == 64)
       #define _LargeFileOffset    LFS
       #define _LargeFileOffsetVal 16
    #else
       #define _LargeFileOffset    _
       #define _LargeFileOffsetVal 0
    #endif

    #if (HAS_STD_ATOMIC == 1)
       #define _Atomic             Atomic
       #define _AtomicVal          64
    #else
       #define _Atomic             _
       #define _AtomicVal          0
    #endif

    #define _String(X) #X
    #define Stringize(X) _String(X) " "

    #define PASTER(x,y) x ## _ ## y
    #define EVALUATOR(x,y)  PASTER(x,y)
    #define NAME(fun, TERM) EVALUATOR(fun, TERM)

    #define FlagNames Stringize(_SSLFlagName) Stringize(_AESFlagName) Stringize(_TypeFlagName) Stringize(_FFMPEGFlagName) Stringize(_TLSFlagName) Stringize(_BaseFlagName) Stringize(_FloatFlagName) Stringize(_ChronoFlagName) Stringize(_AtomicFlagName) Stringize(_MD5FlagName) Stringize(_ExLockFlagName) Stringize(_SOAPFlagName) Stringize(_CompressFlagName) Stringize(_OwnPicFlagName) Stringize(_RegExFlagName) Stringize(_PingFlagName) Stringize(_BSCFlagName) Stringize(_JSFlagName) Stringize(_DebugFlagName)
    #define _ClassPathBuildFlags (_DebugFlag + _PlatformVal + _LargeFileOffsetVal + _AtomicVal)

    namespace BuildInfo
    {
        int NAME(NAME(NAME(NAME(NAME(NAME(NAME(NAME(NAME(NAME(NAME(NAME(NAME(NAME(NAME(NAME(NAME(NAME(NAME(_checkSameCompilationFlags_, _SSLFlagName), _AESFlagName), _TypeFlagName), _FFMPEGFlagName), _TLSFlagName), _BaseFlagName), _FloatFlagName), _ChronoFlagName), _AtomicFlagName), _MD5FlagName), _ExLockFlagName), _SOAPFlagName), _CompressFlagName), _OwnPicFlagName), _RegExFlagName), _PingFlagName), _BSCFlagName), _JSFlagName), _DebugFlagName) = ClassPathFlags;
        const char * getBuildFlagsName() { return FlagNames; }
        int NAME(NAME(NAME(NAME(_checkSameBuildFlags_, _DebugFlagName), _Platform), _LargeFileOffset), _Atomic) = _ClassPathBuildFlags;
        #pragma message("Using ClassPath with flags " FlagNames)
#if defined(GitRef)
        const char * getBuildRepoVer() { return Stringize(GitRef); }
        #pragma message("Based on GIT rev: " Stringize(GitRef))
#else
        const char * getBuildRepoVer() { return ""; }
#endif
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

    #undef _JSFlag
    #undef _JSFlagName
    
    #undef _BSCFlag
    #undef _BSCFlagName

    #undef _DebugFlag
    #undef _DebugFlagName
    
#endif
