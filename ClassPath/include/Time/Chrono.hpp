#ifndef hpp_BOX_CHRONO_hpp
#define hpp_BOX_CHRONO_hpp

// We need time declaration
#include "Time.hpp"

#if (WantTimedProfiling==1)
// We need Logger
#include "../Logger/Logger.hpp"
// We need container for the program scope profiler
#include "../Hash/HashTable.hpp"
#endif


namespace Time
{
    /** Compute the difference in time between start and stop event.
        This object starts counting when constructed, unless you call the constructor with one parameter.
        In that case use startTimer() to start counting.

        Example usage:
        @code
        AccurateChrono chrono;
        // Code to profile
        [...]
        uint32 elapsedTime = chrono.stopTimer();
        // Or
        AccurateChrono chrono(true);
        // some code...

        // Then start profiling
        chrono.startTimer();
        // Code to profile
        [...]
        uint32 elapsedTime = chrono.checkPoint();
        // You can accumulate results too
        [...]
        uint32 elapsedTime2 = chrono.stopTimer();

        // You can also use a ScopeChrono that output the results to the Logger on destruction
        {
            ScopedChrono chrono("Message to profile")
            [...]
        } // Output "Message to profile [.234s]"
        @endcode
        @warning The accuracy is not the precision. It's not because the returned base is high that you'll get that precision.
        @sa startTimer, stopTimer */
    template <int basePrecision = 1000000, bool scoped = false>
    class AccurateChronoBase
    {
        // Members
    private:
        /** Remember the start time */
        uint32 startTime;

        // Interface
    public:
        /** Compute the time elapsed since the last startTimer() call (or construction).
            It also reset the timer, like if you called startTimer() after it
            @return the elapsed duration in the defined base (default in uS) */
        uint32 stopTimer()
        {
            uint32 time = checkPoint();
            startTime += time;
            return time;
        }
        /** Initialize the time
            Manage the multi processor hardware */
        void startTimer()
        {
            startTime = getTimeWithBase(basePrecision);
        }
        /** Get a checkpoint
            @return the elapsed duration in the defined base (default in uS) */
        uint32 checkPoint()
        {
            return getTimeWithBase(basePrecision) - startTime;
        }

        /** Get the current chrono time base */
        uint32 getBase() const { return basePrecision; }

    public:
        /** Default constructor start counting */
        AccurateChronoBase() { startTimer(); }
        /** This one is delayed */
        AccurateChronoBase(bool delayed) : startTime(0) { }
    };

    /** A chrono with millisecond resolution */
    typedef AccurateChronoBase<1000> AccurateChronoMs;
    /** A chrono with microsecond resolution */
    typedef AccurateChronoBase<1000000> AccurateChronoUs;
    /** A chrono with nanosecond resolution */
    typedef AccurateChronoBase<1000000000> AccurateChronoNs;

#if (WantTimedProfiling==1)
    /** A scoped chrono is like an AccurateChronoBase, but with RAII implementation.
        It's mainly used to profile some code.
        @code
        // You can also use a ScopeChrono that output the results to the Logger on destruction
        {
            ScopedChrono chrono("Message to profile")
            [...]
        } // Output "Message to profile [.234s]"
        @endcode */
    class ScopedChrono : public AccurateChronoMs
    {
        /** The message to display */
        const Strings::FastString message;
    public:
        /** Construct */
        ScopedChrono(const Strings::FastString & message) : message(message) {}
        ~ScopedChrono()
        {
            Logger::log(Logger::Dump, "%s[%.3lfs]", (const char*)message, (double)checkPoint() / (double)getBase());
        }
    };

    /** Cumulative profiling.
        Profiling is good to spot performance issues, but you often need cumulative results to spot out the "place" where time is spent.
        So, you will likely use a ScopedProfiler in this case, that'll accumulate the results over the whole runtime of the software.
        Example code:
        @code
        for (int i = 0; i < 3; i++)
        {
            // Code to profile is now moved in a scope
            ScopedProfiler profiler("Basic loop");
            [...] // Your code to profile here
        }
        // When the program will terminate, this will print:
        // Starting ScopedProfiler final data dump:
        // =========================================================
        // [0.032123s or 24535335 cycles over 3 calls] Basic loop
        // =========================================================
        @endcode
        You can also use the PROFILER("Basic loop") macro that's just ignored in Release mode. */
    class ScopedProfiler
    {
        /** The profiled item */
        struct Item
        {
            uint64 count;
            uint64 callCount;
            Item(uint64 count = 0, uint64 callCount = 1) : count(count), callCount(callCount) {}
        };

        /** The "memory" of our profiler */
        struct Memory
        {
            typedef Container::HashTable<Item, Strings::FastString, Container::HashKey<Strings::FastString>, Container::DeletionWithDelete<Item> > HashTableT;
            HashTableT hashTable;
            uint32 frequency;

            /** Increment an element */
            bool addElementCount(const uint32 duration, const Strings::FastString & name)
            {
                Item * counter = hashTable.getValue(name);
                if (counter)
                {
                    counter->count += duration; counter->callCount++;
                }
                else
                    // Need to create the entry in the hash table
                    return hashTable.storeValue(name, new Item(duration), false);
                return true;
            }
            /** @internal */
            int iterateEntry(const Strings::FastString & name, const Item * entry)
            {
                Logger::log(Logger::Dump | Logger::Tests, "[%.6lfs or " PF_LLD " over " PF_LLD " calls] Entry [%s]", (double)(entry->count) / (double)frequency, entry->count, entry->callCount, (const char*)name);
                return 1;
            }
            /** Log the current table */
            void logProfilingTable()
            {
                Logger::log(Logger::Dump | Logger::Tests, "Starting ScopedProfiler final data dump:");
                Logger::log(Logger::Dump | Logger::Tests, "================================================");
                hashTable.iterateAllEntries(*this, &Memory::iterateEntry);
                Logger::log(Logger::Dump | Logger::Tests, "================================================");
            }

            Memory(const uint32 frequency = 1000000) : frequency(frequency) {}
            ~Memory() { logProfilingTable(); }
        };
        static Memory & getProfilerMemory() { static Memory mem; return mem; }
        uint32 startTime;
        Strings::FastString name;
    public:
        /** This constructor takes the profiling name that'll be output on the report */
        ScopedProfiler(const Strings::FastString & name) : startTime(getTimeWithBase(getProfilerMemory().frequency)), name(name) {}
        ~ScopedProfiler() { getProfilerMemory().addElementCount(getTimeWithBase(getProfilerMemory().frequency) - startTime, name); }
    };

#if (DEBUG==1) || defined(EnableProfiling)
#define _CONCAT(X,Y) X ## _ ## Y
#define _EVAL(X,Y) _CONCAT(X,Y)
#define PROFILER(X) ::Time::ScopedProfiler _EVAL(__profiler__,__LINE__) (X)
#else
/** Use this macro to disable profiling when on release mode (else #define EnableProfiling 1) */
#define PROFILER(X) (void)(X)
#endif

    #define HasTimedProfiling 1
#endif
}

#endif
