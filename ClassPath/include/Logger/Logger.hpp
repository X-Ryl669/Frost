#ifndef hpp_Logger_hpp_
#define hpp_Logger_hpp_

#include <stdio.h>
#include <time.h>
#ifdef _WIN32
#include <share.h>
#endif

// We need locks to allow multithreading logging to file
#include "../Threading/Lock.hpp"
#include "../Threading/Threads.hpp"
// We need strings too
#include "../Strings/Strings.hpp"


/** If you intend to log something to a console, or a file, or both, this is where to look for */
namespace Logger
{
    /** The allowed flags to filter upon.
        When using Logger::log, you need to "tag" your message with some flags which are checked against the current application's mask.
        If they fit the mask, the log message will go through.  */
    enum Flags
    {
        Error       =   0x00000001,   //!< A typical error
        Warning     =   0x00000002,   //!< A typical warning
        File        =   0x00000004,   //!< The log is related to file operations
        Network     =   0x00000008,   //!< The log is related to network operations
        Directory   =   0x00000010,   //!< The log is related to directory operations
        Cache       =   0x00000020,   //!< The log is related to cache operations
        Content     =   0x00000040,   //!< The log is related to content operations
        Function    =   0x00000080,   //!< Typically used to show user-specified function
        Dump        =   0x00000100,   //!< Probably the most verbose log
        Creation    =   0x00000200,   //!< Log related to creating objects
        Deletion    =   0x00000400,   //!< Log related to deleting objects
        Timeout     =   0x00000800,   //!< Log related to time outs
        Connection  =   0x00001000,   //!< Log related to connections (network / local)
        Tests       =   0x00002000,   //!< Special Tests class for some logs
        Database    =   0x00004000,   //!< Logs database queries (warning, this can be a security risk)
        Config      =   0x00008000,   //!< Config related logs
        Crypto      =   0x00010000,   //!< Crypto based logs (warning, this can be a security risk)
        Packet      =   0x00020000,   //!< Log Packet related operations

        // Compound

        AllFlags    =   0xFFFFFFFF,
    };




    /** The logger output sink interface */
    struct OutputSink
    {
        /** The allowed mask to log */
        const unsigned int logMask;
        /** Get an UTF-8 message, without end-of-line, to sink to output */
        virtual void gotMessage(const char * message, const unsigned int flags) = 0;
        /** Required virtual destructor */
        virtual ~OutputSink() {};

        /** Define the log mask while creating */
        OutputSink(const unsigned int logMask) : logMask(logMask) {}
    };

    /** The output sink to the console */
    struct ConsoleSink : public OutputSink
    {
        // Members
    private:
        Threading::Lock lock;
    public:
        virtual void gotMessage(const char * message, const unsigned int flags)
        {
            if (logMask & flags)
            {
                Threading::ScopedLock scope(lock);
                #if defined(NEXIO)
                    dbg_printf("%s\n", (const char*)message);
                #elif defined(_WIN32)
                    OutputDebugStringA((const char*)message);
                    OutputDebugStringA("\n");
                #else
                    fprintf(stdout, "%s\n", (const char*)message);
                #endif
            }
        }
        ConsoleSink(const unsigned int logMask) : OutputSink(logMask) {}
    };

#if defined(_WIN32) || defined(_POSIX)
    /** The Tee sink */
    struct TeeSink : public OutputSink
    {
        OutputSink * sinks[2];
        bool         ownSinks;

        virtual void gotMessage(const char * message, const unsigned int flags)
        {
            if (sinks[0]) sinks[0]->gotMessage(message, flags);
            if (sinks[1]) sinks[1]->gotMessage(message, flags);
        }
        TeeSink(OutputSink * first, OutputSink * second) : OutputSink(0), ownSinks(true) { sinks[0] = first; sinks[1] = second; }
        TeeSink(OutputSink & first, OutputSink & second) : OutputSink(0), ownSinks(false) { sinks[0] = &first; sinks[1] = &second; }
        ~TeeSink() { if (ownSinks) { delete0(sinks[0]); delete0(sinks[1]); } }
    };

#ifdef _WIN32
    /** The output sink to the console */
    struct DebugConsoleSink : public OutputSink
    {
        // Members
    private:
        Threading::Lock lock;
    public:
        virtual void gotMessage(const char * message, const unsigned int flags)
        {
            if (logMask & flags)
            {
                Threading::ScopedLock scope(lock);
                OutputDebugStringA((const char*)message);
                OutputDebugStringA("\r\n");
            }
        }
        DebugConsoleSink(const unsigned int logMask) : OutputSink(logMask) {}
    };
#else
    typedef ConsoleSink DebugConsoleSink;
#endif

    /** The output sink to the error console */
    struct ErrorConsoleSink : public OutputSink
    {
        // Members
    private:
        Threading::Lock lock;
    public:
        virtual void gotMessage(const char * message, const unsigned int flags)
        {
            if (logMask & flags)
            {
                Threading::ScopedLock scope(lock);
                fprintf(stderr, "%s\n", (const char*)message);
            }
        }
        ErrorConsoleSink(unsigned int logMask) : OutputSink(logMask) {}
    };

#if defined(_WIN32) || defined(_POSIX)
    /** The output sink to a file */
    struct FileOutputSink : public OutputSink
    {
        // Members
    private:
        FILE *          file;
        Threading::Lock lock;

        // OutputSink interface
    public:
        virtual void gotMessage(const char * message, const unsigned int flags);
        FileOutputSink(unsigned int logMask, const Strings::FastString & fileName, const bool appendToFile = true) : OutputSink(logMask), file(fopen(fileName, appendToFile ? "ab" : "wb")) {}
        ~FileOutputSink() { Threading::ScopedLock scope(lock); if (file) fclose(file); file = 0; }
    };

    /** Structured output sink */
    struct StructuredFileOutputSink : public OutputSink
    {
        // Members
    private:
        Threading::Lock lock;

        // Used to rotate the log
        Strings::FastString baseFileName;
        int  breakSize;
        int  currentSize;
        bool flipFlop;
        int                 lastMessageCount;
        Strings::FastString lastMessage;
        uint32              lastTime;
        unsigned int        lastFlags;
        FILE *  file;

        // Helpers
    private:
        /** Flush the last message */
        void flushLastMessage();

        // OutputSink interface
    public:
        /** Get a message to log */
        virtual void gotMessage(const char * message, const unsigned int flags);
        /** Build a structured output sink for file.
            @param logMask      The log mask to filter logs
            @param fileName     The file to log into. The name is used as a base name which is rotated
                                every breakSize bytes are output.
                                Typically, for a log called "test.log", the rotated files will be called
                                "test.0.log" then "test.1.log" then "test.0.log" (and not "test.log").
                                The initial log file ("test.log"), if already above the breakSize is erased.
            @param appendToFile If true, the specified log file is open for appending, else it's erased.
                                If the log file is already bigger than the breakSize, it's erased whatever the state of this flag
            @param breakSize    The number of byte allowed to fit in the log file. */
        StructuredFileOutputSink(unsigned int logMask, const Strings::FastString & fileName, const bool appendToFile = true, const int breakSize = 2*1024*1024);
        ~StructuredFileOutputSink() { if (file) { flushLastMessage(); fclose(file); } }
    };
#endif // Files
#endif // Win32 & Linux

    /** Set the sink to use */
    extern void setDefaultSink(OutputSink * newSink);
    /** Get a reference on the currently selected default sink */
    extern OutputSink & getDefaultSink();

    /** This is the main function for logging any information to the selected sink
        You'll use it like any other printf like function.
        @param flags    Any combination of the Logger::Flags value (the sink will check its own mask against these flags to allow logging or not)
        @param format   The printf like format */
    void log(const unsigned int flags, const char * format, ...);
/*
    static void dlog(const char * file, const int line, const unsigned int flags, const char * format, ...)
    {
        char buffer[2048];
        va_list argp;
        va_start(argp, format);
        vsprintf(buffer, format, argp);
        va_end(argp);
        if (defaultSink)
            defaultSink->gotMessage(buffer, flags);
#ifdef _WIN32 // Send output to debug anyway
        OutputDebugStringA((const char*)buffer);
        OutputDebugStringA("\r\n");
#endif
    }

#if _MSC_VER <= 1200
    #define log Logger::rlog
#else
    #if (defined DEBUG)
        #define log(flag, format, ...) dlog(__FILE__, __LINE__, flag, format, __VA_ARGS__)
    #else
        #define log(flag, format, ...) rlog(flag, format, __VA_ARGS__)
    #endif

#endif
    */

}

#endif
