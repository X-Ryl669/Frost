#include "../../include/Logger/Logger.hpp"
#ifdef _WIN32
#include <io.h>
#endif

// Microsoft oddities
#ifdef _WIN32
#define ENDOFLINE   "\r\n"
#else
#define ENDOFLINE   "\n"
#endif

namespace Logger
{
    // The main log function
    void log(const unsigned int flags, const char * format, ...)
    {
        size_t startSize = 512;

        for (;;)
        {
            va_list argp;
            va_start(argp, format);
            char * buffer = (char*)malloc(startSize);
            if(!buffer) return;

            const int err = (int)vsnprintf(buffer, startSize - 1, format, argp);
            va_end(argp);

            // Not enough space
            if (err <= 0)
            {
                // Safety check to exit when vsnprintf fails to print anything
                if (err == 0 || startSize > 131072) return;
                startSize <<= 1;
                free(buffer);
                continue;
            }
            getDefaultSink().gotMessage(buffer, flags);
			free(buffer);
            return;
        }
    }


    // Get a console sink that's build on the main stack (BSS section)
    static OutputSink * getStaticSink()
    {
        static ConsoleSink sink(~(Logger::Database | Logger::Packet | Logger::Dump));
        return &sink;
    }
    // Get a reference on the default sink pointer
    static OutputSink *& getDefaultSinkPointer()
    {
        static OutputSink * defaultSink = getStaticSink();
        return defaultSink;
    }
    // Get the default sink
    OutputSink & getDefaultSink()
    {
        return *getDefaultSinkPointer();
    }
    // Change the default sink
    void setDefaultSink(OutputSink * newSink)
    {
        // If it's not the static sink, let's delete it
        if (&getDefaultSink() != getStaticSink()) delete &getDefaultSink();
        getDefaultSinkPointer() = newSink;
    }

#if defined(_WIN32) || defined(_POSIX)
    void FileOutputSink::gotMessage(const char * message, const unsigned int flags)
    {
        if ((logMask & flags) && file)
        {
            Threading::ScopedLock scope(lock);
            fprintf(file, "%s" ENDOFLINE, (const char*)message);
        }
    }

    void StructuredFileOutputSink::flushLastMessage()
    {
        if (!lastMessage) return;

        if (lastMessageCount > 1)
            fprintf(file, "[%08X][%08X] %s (last message repeated %d times)" ENDOFLINE, lastTime, lastFlags, (const char*)lastMessage.upToLast("\n"), lastMessageCount);
        else
            fprintf(file, "[%08X][%08X] %s" ENDOFLINE, lastTime, lastFlags, (const char*)lastMessage);

        if ((lastFlags & Error) != 0) fflush(file); // Errors messages should be flushed as soon as possible
    }

    void StructuredFileOutputSink::gotMessage(const char * message, const unsigned int flags)
    {
        if ((logMask & flags) == 0 || file == 0) return;
        // Need to structure the file
        // Find out the time
        time_t now = time(NULL);
        // Don't allow 64 bits time
        uint32 nowTime = (uint32)now;

        Threading::ScopedLock scope(lock);
        if (lastMessage == message && lastFlags == flags)
        {
            lastMessageCount++;
            return;
        }
        flushLastMessage();

        // Split the log file if required
        currentSize += lastMessage.getLength();
        if (currentSize >= breakSize)
        {
            Strings::FastString fileName = baseFileName + (flipFlop ? ".1" : ".0");
#ifdef _WIN32
            FILE * newFile = _fsopen(fileName, "wb", _SH_DENYWR);
#elif defined(_POSIX)
            FILE * newFile = fopen(fileName, "wb");
            if (newFile)
            {
                struct flock fl;
                fl.l_whence = SEEK_SET;
                fl.l_start = 0; fl.l_len = 0; fl.l_type = F_WRLCK;
                fcntl(fileno(newFile), F_SETLK, &fl);

                // Prevent any child process from inheriting the file on forking
                int flags = fcntl(fileno(newFile), F_GETFD);
                flags |= FD_CLOEXEC;
                fcntl(fileno(newFile), F_SETFD, flags);
                fclose(file);
                file = newFile;
            }
#endif
            flipFlop = !flipFlop;
            currentSize = 0;
        }
        lastMessage = message;
        lastFlags = flags;
        lastMessageCount = 1;
        lastTime = nowTime;
    }
    StructuredFileOutputSink::StructuredFileOutputSink(unsigned int logMask, const Strings::FastString & fileName, const bool appendToFile, const int breakSize)
        : OutputSink(logMask), baseFileName(fileName), breakSize(breakSize), currentSize(0), flipFlop(false), lastMessageCount(0), lastTime(0), lastFlags(0),
#ifdef _WIN32
        file(_fsopen(fileName, appendToFile ? "ab" : "wb", _SH_DENYWR))
#elif defined(_POSIX)
        file(fopen(fileName, appendToFile ? "ab" : "wb"))
#endif
	{
        if (file)
        {
#if defined(_POSIX)
            struct flock fl;
            fl.l_whence = SEEK_SET;
            fl.l_start = 0;
            fl.l_len = 0;
            fl.l_type = F_WRLCK;
            fcntl(fileno(file), F_SETLK, &fl);

            // Prevent any child process from inheriting the file on forking
            int flags = fcntl(fileno(file), F_GETFD);
            flags |= FD_CLOEXEC;
            fcntl(fileno(file), F_SETFD, flags);
            flags = fcntl(fileno(file), F_GETFD);
#endif
            // Find out the file size (and leave the pointer at end of file)
            fseek(file, 0, SEEK_END);
            currentSize = (int)ftell(file);
            if (currentSize > breakSize)
            {
                // Truncate the file
#ifdef _WIN32
                _chsize_s(_fileno(file), 0);
                currentSize = 0;
#elif defined(_POSIX)
                ftruncate(fileno(file), 0);
                currentSize = 0;
#endif
            }
        } else fprintf(stderr, "Logger can not open file '%s'\n", (const char*)fileName);
    }
#endif
#undef ENDOFLINE

}
