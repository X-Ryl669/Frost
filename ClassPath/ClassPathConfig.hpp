/** This file sets up the features to enable in this class path.
    You will probably want to read the instructions for each features below */
#ifdef hpp_ClassPathConfig_hpp
#warning hpp_ClassPathConfig_hpp is deprecated, #define HasClassPathConfig 1 instead
#endif

#ifndef HasClassPathConfig
#define HasClassPathConfig 1

/** If you want SSL code to be added to the networking code, enable this.
    This declares SSL socket and context in the Network code and client (like HTTP client).
    However this adds the dependency on OpenSSL's ssleay library to your application.
    Default: enabled */
#define WantSSLCode 1

/** If you want ICMP ping code to be added to the networking code, enable this.
    This add a pingDevice method to the IPV4 class but add a dependency on IPHelper.dll on Windows.
    Default: enabled */
#define WantPingCode 1

/** If you want the cryptographic streams to be added to the stream code, enable this.
    This declares the AES input and output stream, providing on-the-fly encryption and decryption for streams.
    Unless you don't use the crypto code at all, you'll likely enable this.
    Default: enabled */
#define WantAES     1

/** If you want the unsafe MD5 hashing (in addition to the safer SHA1 and SHA256), enable this.
    This declares the Hashing::MD5 class. 
    This is required if you need to create Authentication headers to HTTP/RTSP servers
    Default: enabled */
#define WantMD5Hashing 1
    

/** By default ClassPath declares its own plain old data types (like uint32, int16 etc...).
    If you're using a project declaring its own types, and you do provide them before including the ClassPath, 
    then you'll likely prevent redefinition by defining this option 
    Default: disabled */
// #define DontWantTypes 1

/** If you want to use FFMPEG or LibAV, this enhance the MetaDataExtractor class.
    The VideoComponent in Juce also requires this to be enabled 
    Default: enabled */
//#define WantFFMPEG  1

/** If you want thread local variable handling.
    This code allows true cross platform C++ based thread local variable (even for C++98), 
    that are destructed on thread ending. This is required for database code 
    Default: enabled */
#define WantThreadLocalStorage 1

/** If you need encoding binary data from memory block into different bases for serializing them
    to text only format like MIME messages, or XML files, you'll need to enable this 
    Default: enabled */
#define WantBaseEncoding 1

/** When using the FastString class, parsing from and to floats uses snprintf / sscanf code which might 
    or might not be available on your platform. In that case, you will not define this, 
    and replacement functions will be used instead 
    Default: enabled */
#define WantFloatParsing 1

/** When using the FastString class, you can use regular expression to match strings against
    some regular expression rules. This requires you build the slre library that's included in the package.
    Default: enabled */
#define WantRegularExpressions 1

/** If you need time profiling classes, you might want to enable this. 
    @sa Time::ScopedChrono 
    Default: enabled */
#define WantTimedProfiling 1

/** If you need Atomic class to handle atomic types with 32 or 64 bits.
    Default: enabled */
#define WantAtomicClass 1  

/** Enable this if you need read/write lock or the ScopedPingPong class.
    R/W Locks are used in multiple consumer / few producer pattern to avoid stagnation.
    If not enabled, the library defaults to the basic FastLock (a single consumer or producer at a time), but performance suffers on part using them.
    The ScopedPingPong is a simple system to synchronize two threads without locking.
    Default: enabled */
#define WantExtendedLock 1

/** Enable SOAP communication building.
    SOAP is a big beast that allows remote procedure calling. 
    It comes with a lot of dependencies (like XML (with namespaces), UUID, HTTP and a lot of network code).
    Default: disabled */
#define WantSOAP 1

/** If you want buffer compression and decompression code, enable this.
    This is required for some image decoding algorithm, and (optionally) in network code.
    Default: enabled */
#define WantCompression 1

/** If you want buffer compression and decompression code with external BSC library, enable this.
    This gives better compression ratio than Zlib code (better than LZMA) and yet is faster than LZMA compression.
    Default: enabled */
#define WantBSCCompression 1

/** If you want our own imaging code (tools to manipulate images and load from JPEG and PNG pictures), enable this.
    If enabled, a small (but less efficient than the FFMPEG version) JPEG and PNG decoder are built, and so is a PNG encoder.
    If you don't enable FFMPEG either, no decoder and no encoder is built. 
    Default: disabled */
#define WantLightImageCode 1

 

/** If you want ...
    Default: disabled */
#define ___PlaceHolderHere____    1




#endif
