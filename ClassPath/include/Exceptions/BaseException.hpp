#ifndef hpp_BaseException_hpp
#define hpp_BaseException_hpp

// We need Threading code for stack capture
#include "../Threading/Threads.hpp"

/** Some useful helper when creating exceptions (capturing the stack, capturing file name and line, etc...) */
namespace Exception
{
    /** The String class we are using */
    typedef Strings::FastString String;

    /** Capture the file and line here */
    struct FileLine
    {
        const char * File;
        const int    line;

        /** Get the textual version of it */
        String toString() const { return File ? String::Print("%s:%d", File, line) : String(""); }

        FileLine(const char * file = 0, const int line = 0) : File(file), line(line) {}
    };

    /** This is used as a Thread Local Storage object that says if the next Throw will capture the stack or not.
        This is defined in Threads.cpp file because of the TLSDecl macro below
        @sa tryNoStack and Throw macro */
    struct WithStackM { static TLSDecl bool w; };

    /** Capture the stack, if available on the platform.
        If also using the Throw macro, it captures the file and line where the object was thrown.
        The only requirement is that your exception have a "virtual const char * what() const" method,
        which is likely the case if you derive from std::exception.

        So the code like this:
        @code
        struct Except // optional : public std::exception
        {
             const char * msg; // This is bad, but it's ok for an example
             virtual const char * what() const { return msg; }
             Except(const char * msg) : msg(msg) {}
        };

        try
        {
            [...]
            throw Except("whatever");
        } catch (const Except & a)
        {
             Logger::log(Logger::Error, "Error with %s", a.what());
             // Outputs "Error with whatever"
        }
        @endcode

        becomes:
        @code
        struct Except // optional : public std::exception
        {
             const char * msg; // This is bad, but it's ok for an example
             virtual const char * what() const { return msg; }
             Except(const char * msg) : msg(msg) {}
        };

        try
        {
            [...]
            Throw(Except("whatever")); // <= This is the only required change!
        } catch (const Except & a)
        {
             Logger::log(Logger::Error, "Error with %s", a.what());
             // Outputs "Error with whatever [in /path/to/your/file.cpp:34] callstack:"
             //          [0x08353233] in buildComposite<>
             //          [0x04339458] in <your func>::lineTo
             //          [...]
             //          [0x04230000] in main()
        }
        @endcode

        If you want more information from the callstack, you should use the CrashAnalyst tool and feed it the above callstack.
        Since capturing the stack can be a time consuming operation and it's not possible to know at the throwing time if the
        exception will be caught, code that use exceptions as a normal logic flow (this is bad... anyway) will see a penalty here.
        In that case, use the "tryNoStack" in place of "try" and handler will no more capture the stack (but still capture the
        file & line of the throwing point)

        @code
        // Before
        bool someFunc(...)
        {
            try
            {
                 // Some code that throw non-exceptional stuff
            } catch(const SomeException & e) { return false; }
            return true;
        }

        // After
        bool someFunc(...)
        {
            tryNoStack
            {
                 // Some code that throw non-exceptional stuff
            } catch(const SomeException & e) { return false; }
            return true;
        }
        @endcode
        */
    template <typename T>
    struct WithStack : public T
    {
        virtual const char * what() const throw() { return message; }
        const String message;
        WithStack(const T & t, const char * file, const int line) : T(t), message(String::Print("%s [in %s:%d]%s", T::what(), file, line, (const char*)(WithStackM::w ? String(" callstack\n") + Threading::Thread::getCurrentThreadStack() : String("")))) { WithStackM::w = true; }
        virtual ~WithStack() throw() {}
    };

    template <typename T>
    WithStack<T> captureStack(const T & t, const char * file, const int line) { return WithStack<T>(t, file, line); }

}

/** Provided you are not throwing basic type (int, float, etc...) and have a "virtual const char * what() const throw()" in the exception structure,
    this capture the stack and the throwing position */
#define Throw(X) throw ::Exception::captureStack(X, __FILE__, __LINE__)
/** If you don't want to capture the stack in an exception intensive code (since capturing the stack is an expensive operation that should be exceptional)
    use this instead of "try" */
#define tryNoStack     for (::Exception::WithStackM::w = false;!::Exception::WithStackM::w;::Exception::WithStackM::w = true) try



#endif
