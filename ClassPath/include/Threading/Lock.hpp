#ifndef h_CPP_Lock_CPP_h
#define h_CPP_Lock_CPP_h

// Include object naming
#include "../Types.hpp"
// We need time declaration too
#include <time.h>
// And the compile time assertion code to prevent bad usage of Atomic class
#include "../Utils/StaticAssert.hpp"

// You must include this file on StellarisWare
// #include "driverlib/interrupt.h"

#ifndef __cplusplus
    #error "This file shouldn't be included in C code"
#endif

#if HAS_STD_ATOMIC == 1
#include <atomic>
#endif


namespace Threading
{
#ifndef _WIN32
    inline bool stillBefore(struct timeval * tOut);
#endif
    /** The timeout to wait for */
    enum TimeOut 
    { 
        InstantCheck = 0, 
#ifdef _WIN32
        Infinite = INFINITE,  
#else
        Infinite = 0xFFFFFFFF,
#endif
    };
    

    /** This class implements inter-thread event objects 
        
        Events can be in 2 states (Set or Unset)
        State transitions are atomic

        Any thread can wait on an event.
        Only one thread can change the event's state to Set at a given time
    */
    class Event
    {
    private:
#if (DEBUG==1)
        /** This event name (used in debug mode) */
        const char*     name;
#endif
        /** The event object */
        OEVENT          event;
        /** Should the event automatically reset ? */
        bool            manualReset;

#ifdef _WIN32       
        inline bool _Wait(const TimeOut length) volatile{ return WaitForSingleObject(event, (DWORD)length) == WAIT_OBJECT_0; }
        inline bool _Reset() volatile                   { return ResetEvent(event) == TRUE; }
        inline bool _Set(void *) volatile           { return SetEvent(event) == TRUE; }
        
#elif defined (_POSIX)
        /** The event state */
        volatile bool   state;
        /** A synchronized condition to avoid polling on the mutexed state */
        pthread_cond_t  condition;

        /** Those methods are too big to be included here, please refer to lock.cpp for details */
        bool _Wait(const TimeOut & length) volatile;
        bool _Reset() volatile;
        bool _Set(void * arg) volatile;
#else
        /** The queue to manage the state of the event*/
        xQueueHandle xQueue;

        /** Those methods are too big to be included here, please refer to lock.cpp for details */
        bool _Wait(const TimeOut & length) volatile;
        bool _Reset() volatile;
        bool _Set(void * arg) volatile;
#endif

    
    public:
        /** The event type */
        enum Type
        {
            ManualReset = 1, //!< The event needs to be reset by calling Reset() when Set()
            AutoReset   = 0, //!< The event automatically reset when a Wait succeed after a Set()
        };
        
        /** The initial state */
        enum InitialState
        {
            InitiallyFree = 0, //!< The event was created initially free
            InitiallySet = 1,  //!< The event was created initially set
        };


        /** Build an event.
            Typically, manualReset is used when you want to wait on multiple event at once 
            (so it's easier to say when you're reading processing event by calling Reset).
            For atomic / single thread unlock on Waiting, you must set manualReset to false.
            @param name         The event name (only useful for debugging) 
            @param manualReset  When true, the transition from Set to Unset requires calling Reset, else it's done automatically in a successful Wait
            @param initialState The event Set state when created */
        Event(const char * name = NULL, const Type type = ManualReset, const InitialState initialState = InitiallyFree)
            : 
    #if (DEBUG==1)
            name(name),
    #endif
            manualReset(type == ManualReset),
#ifdef _WIN32
            event(NULL)
            {   event = CreateEvent(NULL, manualReset ? TRUE : FALSE, initialState ? TRUE : FALSE, NULL);    }
#elif defined (_POSIX)
            state(initialState == InitiallySet)
/*          , event(PTHREAD_MUTEX_INITIALIZER)
            , condition(PTHREAD_COND_INITIALIZER)*/
            {
                pthread_mutex_init(&event, NULL);
                pthread_cond_init(&condition, NULL);
                if (initialState == InitiallySet)
                    _Set(0);
            }
#else
            xQueue(0)
            {
                // The queue can only have one value
                xQueue = xQueueCreate( 1, sizeof( unsigned portLONG ) );
            }
#endif

        ~Event()
#ifdef _WIN32
        { if (event != NULL) CloseHandle(event); }
#elif defined (_POSIX)
        { 
            // Lock the mutex
            while (pthread_mutex_lock(&event) != 0) { sched_yield(); }
            while (pthread_cond_destroy(&condition) == EBUSY) { pthread_cond_broadcast(&condition); pthread_mutex_unlock(&event); sched_yield(); while(pthread_mutex_lock(&event) != 0) sched_yield();  }
            pthread_mutex_unlock(&event);
            while (pthread_mutex_destroy(&event) == EBUSY) { sched_yield(); }
        }
#else
        {
            // Free memory
            vQueueDelete(xQueue);
        }
#endif
        /** Wait a given amount of time for this event to be set 
            @param length   Time to wait for (can be InstantCheck i.e. doesn't wait, or any number in ms or Infinite)
            @return if length = Infinite, only returns when event's state was set
                    else, return true on event set while waiting, false otherwise
        */
        inline bool Wait(const TimeOut length = Infinite) volatile      { return _Wait(length);        }
        /** Set this event (goes to Set state) */
        inline bool Set() volatile                                      { return _Set(0); }
        /** Set this event (goes to Set state) from an ISR */
        inline bool Set(void *arg) volatile                             { return _Set(arg); }
        /** Reset this event (only if autoreset is false) */
        inline bool Reset() volatile                                    { return _Reset();  }
    };

    /** Common inter-thread locking class 
        This will wrap platform specific mutexes 
    */
    class MutexLock
    {
    private:
        /** The mutex object */
        HMUTEX  mutex;
#ifdef MUTEX_DEBUG
        /** Is the mutex locked ? */
        bool    locked;
#endif
#if (DEBUG==1)
        /** The object name (for debugging purpose) */
        const char*   name;
#endif

#ifdef _WIN32
        inline bool _Lock(const TimeOut length) volatile    { return WaitForSingleObject(mutex, (DWORD)length) == WAIT_OBJECT_0; }
        inline void _Unlock(void * = 0) volatile             { ReleaseMutex(mutex); }
#else
        /** Those methods are too big to be included here, please refer to lock.cpp for details */
        bool _Lock(const TimeOut & rcxTO) volatile;
        void _Unlock(void *arg = 0) volatile;
#endif

    public:
        MutexLock(const char * name = NULL, const bool initialOwner = false)  
    #if (DEBUG==1)
        :   name(name)
    #endif
    #ifdef MUTEX_DEBUG
        #if (DEBUG==1)
            ,
        #else
            :
        #endif
            locked(initialOwner == TRUE) 
    #endif
#ifdef _WIN32
    #if ((DEBUG==1) || defined (MUTEX_DEBUG))
        , mutex(NULL)
    #else
        : mutex(NULL)
    #endif
        { mutex = ::CreateMutex(NULL, initialOwner ? TRUE : FALSE, NULL); }
        ~MutexLock() { if (mutex != NULL) { HMUTEX hMut = mutex; Acquire(); mutex = NULL; ReleaseMutex(hMut); CloseHandle(hMut); } }
#else
        /* , mutex(0) */
        { 
            pthread_mutex_init(&mutex, NULL); if (initialOwner) Acquire();
        }

        ~MutexLock() { Acquire(); Release(); while (pthread_mutex_destroy(&mutex) == EBUSY) sched_yield();  }
#endif

    #ifdef MUTEX_DEBUG
        #define Acquire()           dAcquire(__FILE__, __LINE__)
        inline bool dAcquire(const char * sFile, int iLine) volatile
        { 
            bool bLocked = _Lock(Infinite);
            if ( bLocked && locked)
            {
                // Reentrant mutex, should not be allowed
                MessageBoxA(NULL, "Reentrant mutex locking detected !", sFile, 0);
                // Return true so that it is still possible to continue (false would have created bad ref counting)
                return true;
            }

            if (bLocked) { locked = true; return true; } 
            return false; 
        }
        inline bool Release() volatile  { if (locked) { locked = false; _Unlock(); return true; } return false; }
        /** Release the (acquired) lock from a interrupt service routine */
        inline bool Release(void * arg) volatile { Release(); return false; }
    #else
        /** Acquire the lock
            @return true on successful acquisition (no timeout by default), false on error
        */
        inline bool Acquire() volatile  { return _Lock(Infinite); }
        /** Try to acquire the lock 
            @return true on successful acquisition (no timeout by default), false on error */
        inline bool TryAcquire(const TimeOut length) volatile { return _Lock(length); }
        /** Release the (acquired) lock or do nothing if unlocked */
        inline bool Release() volatile  { _Unlock(); return true; }
        /** Release the (acquired) lock from a interrupt service routine */
        inline bool Release(void * arg) volatile { _Unlock(arg); return true; }
    #endif 
    };


    /** Common inter-thread locking class 
        This will wrap platform specific mutexes.
        @sa ScopedLock
    */
    class FastLock
    {
    private:

#ifdef MUTEX_DEBUG
        /** Is the mutex locked ? */
        bool    locked;
#endif
#if (DEBUG==1)
        /** The object name (for debugging purpose) */
        const char *  name;
#endif

#ifdef _WIN32
        /** The mutex object */
        SRWLOCK mutex;
        inline bool _Lock(const TimeOut dwLength) volatile   { if (dwLength == Infinite) { AcquireSRWLockExclusive((SRWLOCK*)&mutex); return true; }
                                                               else if (dwLength == InstantCheck) return TryAcquireSRWLockExclusive((SRWLOCK*)&mutex) != 0;
                                                               return false; }
        inline void _Unlock(void * = 0) volatile             { ReleaseSRWLockExclusive((SRWLOCK*)&mutex); }
#else
        HMUTEX mutex;
        /** Those methods are too big to be included here, please refer to lock.cpp for details */
        bool _Lock(const TimeOut & rcxTO) volatile;
        void _Unlock(void *arg = 0) volatile;
#endif

    public:
        /** Build a lock.
            @param name         The lock name (only useful for debugging)
            @param initialOwner When true, this is atomically equivalent to "Lock lock; lock.Acquire();" */
        FastLock(const char * name = NULL, const bool initialOwner = false)
    #if (DEBUG==1)
        :   name(name)
    #endif
    #ifdef MUTEX_DEBUG
        #if (DEBUG==1)
            ,
        #else
            :
        #endif
            locked(initialOwner == TRUE) 
    #endif
#ifdef _WIN32
        // Check return type of InitializeCriticalSection in case it fails (no more memory available), 
        // sleep a bit to let other process return some memory, and try again, this time with exception on error
        { InitializeSRWLock(&mutex); if (initialOwner) Acquire(); }
        ~FastLock() { }
#else
        /* , mutex(0) */
        { 
            pthread_mutex_init(&mutex, NULL); if (initialOwner) Acquire();
        }

        ~FastLock() { Acquire(); Release(); while (pthread_mutex_destroy(&mutex) == EBUSY) sched_yield();  }
#endif

    #ifdef MUTEX_DEBUG
        #define Acquire()           dAcquire(__FILE__, __LINE__)
        inline bool dAcquire(const char * sFile, int iLine) volatile
        { 
            bool bLocked = _Lock(Infinite);
            if ( bLocked && locked)
            {
                // Reentrant mutex, should not be allowed
                MessageBoxA(NULL, "Reentrant mutex locking detected !", sFile, 0);
                // Return true so that it is still possible to continue (false would have created bad ref counting)
                return true;
            }

            if (bLocked) { locked = true; return true; } 
            return false; 
        }
        /** Try to acquire the lock 
            @return true on successful acquisition (no timeout by default), false on error */
        inline bool TryAcquire(const TimeOut length) volatile { return _Lock(length); }
        /** Release the (acquired) lock from a interrupt service routine */
        inline bool Release() volatile  { if (locked) { locked = false; _Unlock(); return true; } return false; }
        /** Release the (acquired) lock from a interrupt service routine */
        inline bool Release(void * arg) volatile { Release(); return false; }
    #else
        /** Try to acquire the lock
            @return true on successful acquisition (no timeout by default), false on error 
            @warning only return false if the locking primitive is deleted, or on recursive locking. 
            @warning If it ever return false, your application logic is broken.
        */
        inline bool Acquire() volatile   { return _Lock(Infinite); }
        /** Try to acquire the lock 
            @return true on successful acquisition (no timeout by default), false on error */
        inline bool TryAcquire(const TimeOut length) volatile { return _Lock(length); }
        /** Release the (acquired) lock or do nothing if unlocked */
        inline bool Release() volatile   { _Unlock(); return true; }
        /** Release the (acquired) lock from a interrupt service routine */
        inline bool Release(void * arg) volatile { _Unlock(arg); return true; }
    #endif
    #if (DEBUG==1)
        const char * getName() const volatile { return name; }
    #endif
    };

    /** Say we are using fast locks as locking primitive (it's not shared between processes) */
    typedef FastLock Lock;
    /** The classical scoped lock class */
    class ScopedLock
    {
    private: 
        volatile Lock & lock;

        /** Non mutable class */
        ScopedLock & operator = (const ScopedLock & other);
        /** Non mutable class */
        ScopedLock(const ScopedLock & other);
    public:
        /** Takes a reference on a lock to acquire it and release on scope's end */
        ScopedLock(volatile Lock & lock) : lock(lock) { lock.Acquire(); }
        ~ScopedLock() { lock.Release(); }
    };
    /** The classical scoped unlock class */
    class ScopedUnlock
    {
    private: 
        volatile Lock & lock;

        /** Non mutable class */
        ScopedUnlock & operator = (const ScopedUnlock & other);
        /** Non mutable class */
        ScopedUnlock(const ScopedUnlock & other);
    public:
        /** Takes a reference on a lock to release it and acquire on scope's end */
        ScopedUnlock(volatile Lock & lock) : lock(lock) { lock.Release(); }
        ~ScopedUnlock() { lock.Acquire(); }
    };
#if WantExtendedLock == 1
    /** The read write lock.
        This lock is used in case there is multiple reader but a single writer.
        When a reader read the protected structure, it takes the reader lock.
        Multiple reader can own the reader lock at the same time.
        However, when the writer want to modify the structure, it will prevent 
        the other readers from taking the locks, and thus ensure that no reader ever 
        read while a writing is in progress.

        The behaviour is made so that, as soon as a writer wants to enter a lock, no 
        more reader can take the reader lock. This prevents writer starvation.
    
        @warning The default maximum number of simultaneous reader is 2^32.
        @warning Upgrading and downgrading from writer lock can be non-atomic, and as 
                 such not thread safe. In most use, it's better to release your reader 
                 lock and take a writer lock.

        @sa ScopedReadLock, ScopedWriteLock, ScopedUpgradeLock.
    */
    class ReadWriteLock
    {
        // Members
    private:
        /** The lock to guard all other members */
        Lock        lock;
        /** The write event (auto reset) */
        Event *     write;
        /** The writer count */
        volatile int      writerCount;

        /** The read event */
        Event *      read;
        /** The current reader count */
        volatile int currentReaderCount;
        /** The waiting reader count */
        volatile int waitingReaderCount;
        
        // Construction and destruction
    public:
        /** Constructor */
	    ReadWriteLock() : write(0), writerCount(0), read(0), currentReaderCount(0), waitingReaderCount(0) {}
	    ~ReadWriteLock() 
	    { 
#ifdef MUTEX_DEBUG 
	        if (write || read)
	            MessageBoxA(NULL, "Destroying RW lock while taken!", sFile, 0);
#endif
        }

        /** Acquire the reader lock */
        bool acquireReader(const TimeOut timeout = Infinite) volatile;
        /** Release the reader lock */
        inline void releaseReader() volatile { ScopedLock scope(lock); readerRelease(); }
        /** Acquire the writer lock */
        bool acquireWriter(const TimeOut timeout = Infinite) volatile;
        /** Release the writer lock */
        void releaseWriter() volatile { ScopedLock scope(lock); writerRelease(false); }
        /** Downgrade from writer to reader lock.
            @warning this method is not always thread safe. */
        void downgradeFromWriter() volatile { ScopedLock scope(lock); writerRelease(true); }
        /** Upgrade reader lock to writer lock.
            @warning this method is not always thread safe. */
        bool upgradeToWriter(const TimeOut timeout = Infinite) volatile { lock.Acquire(); return upgradeToWriterLockAndLeaveCS(timeout); } 

        // Helpers 
    protected:
	    // Internal/Real implementation
	    bool readerWait(const TimeOut timeout) volatile;
	    void readerRelease() volatile;
	    bool writerWaitAndLeaveCSIfSuccess(const TimeOut timeout) volatile;
	    bool upgradeToWriterLockAndLeaveCS(const TimeOut timeout) volatile;
        void writerRelease(const bool downgrade) volatile;
    };

    /** The classical scoped lock class for read write lock */
    class ScopedReadLock
    {
    private: 
        volatile ReadWriteLock & lock;

        /** Non mutable class */
        ScopedReadLock & operator = (const ScopedReadLock & other);
        /** Non mutable class */
        ScopedReadLock(const ScopedReadLock & other);
    public:
        /** Takes a reference on a reader of a ReadWriteLock to acquire it and release on scope's end */
        ScopedReadLock(volatile ReadWriteLock & lock) : lock(lock) { lock.acquireReader(); }
        ~ScopedReadLock() { lock.releaseReader(); }
    };
    /** Unlock the read lock (used to unlock a ReadWriteLock for a small section of code) */
    class ScopedReadUnlock
    {
    private: 
        volatile ReadWriteLock & lock;
        /** Non mutable class */
        ScopedReadUnlock & operator = (const ScopedReadUnlock & other);
        /** Non mutable class */
        ScopedReadUnlock(const ScopedReadUnlock & other);
    public:
        /** Takes a reference on a reader of a ReadWriteLock to release it and acquire on scope's end */
        ScopedReadUnlock(volatile ReadWriteLock & lock) : lock(lock) { lock.releaseReader(); }
        ~ScopedReadUnlock() { lock.acquireReader(); }
    };
    /** The classical scoped lock class for write lock */
    class ScopedWriteLock
    {
    private: 
        volatile ReadWriteLock & lock;
        /** Non mutable class */
        ScopedWriteLock & operator = (const ScopedWriteLock & other);
        /** Non mutable class */
        ScopedWriteLock(const ScopedWriteLock & other);
    public:
        /** Takes a reference on a writer of a ReadWriteLock to acquire it and release on scope's end */
        ScopedWriteLock(volatile ReadWriteLock & lock) : lock(lock) { lock.acquireWriter(); }
        ~ScopedWriteLock() { lock.releaseWriter(); }
    };    
    /** This class release the read lock, and acquire a write lock.
        Releasing and acquiring aren't done atomically (so another writer can take 
        the writer lock while upgrading, and you'll wait until it's done) */
    class ScopedUpgradeLock
    {
    private: 
        volatile ReadWriteLock & lock;
        /** Non mutable class */
        ScopedUpgradeLock & operator = (const ScopedUpgradeLock & other);
        /** Non mutable class */
        ScopedUpgradeLock(const ScopedUpgradeLock & other);
    public:
        /** Takes a reference on a ReadWriteLock to release the reader lock, and acquire the Writer lock it and exchange back on scope's end */
        ScopedUpgradeLock(volatile ReadWriteLock & lock) : lock(lock) { lock.releaseReader(); lock.acquireWriter(); }
        ~ScopedUpgradeLock() { lock.releaseWriter(); lock.acquireReader(); }
    };

    /** A simple synchronization point for thread.
        This is typically used with ScopePP for safer thread synchronization.
        When one thread need to interrupt another thread, it has to signal it somehow.
        Then the other thread needs to tell when it's ready to be modified by the former thread.
        Once the former thread has finished its work, it just release the other thread to resume its loop.
        
        Please notice that this still requires a Lock for protecting the data to be modified.
        However, this is used when the data to be modified needs to be locked for long period of time by 
        the thread, and where a classical "ScopedLock scope(lock)" protection would not work, since the 
        external access will starve to get the lock. 
        
        @warning This only works when a single thread at a time needs to modify the other thread.
                 If you need concurrent multiple thread access, then you should use ReadWriteLock instead. */
    class PingPong
    {
        /** The required events */
        Event ping, pong, done;
        // Interface
    public:
        /** When you need to stop another thread, you have first to signal that you wait for it to pause its work */
        bool wantToDo(const TimeOut timeout) { ping.Set(); return pong.Wait(timeout); }
        /** If wantToDo returned true, you must release the other thread when you're done with your work */
        void doneWork() { ping.Reset(); pong.Reset(); done.Set(); }
        /** The other thread will need to call this at "synchronization points" where it's safe to get paused */
        void checkHasToDo() { if (ping.Wait(InstantCheck)) { pong.Set(); done.Wait(); } }

        /** Construction */
        PingPong(const char * name = NULL) : done(name, Event::AutoReset) {}
        /** Destruction */
        ~PingPong() { done.Set(); }
    };

    class WithStartMarker;
    /** This is used with a PingPong structure to synchronize thread for communication.
        Typical usage code is like this:
        @code
        struct Whatever : public Thread, public WithStartMarker
        {
            PingPong      sync;
            ImportantData data;
            Lock          lock;
     
            // Interface external to the thread
        public:
            void modifyData() 
            { 
                ScopedPP scope(lock, this, sync);
                data.changeSomething(); 
            }
            void start() { createThread(); waitUntilStarted(); }
            
            // Thread interface
        public:
            uint32 runThread() 
            { 
                started(); // Required by WithStartMarker
                while(isRunning())
                {
                    // Checkpoint here. Without this, it's almost impossible for the external access to get the lock
                    sync.checkHasToDo();
                    
                    // Then perform some work on the data
                    ScopedLock scope(lock);
                    data.process();
                }
            }
            
     
            Whatever() { }
            ~Whatever() { destroyThread(); }
        };
        
        Whatever a;
        a.modifyData(); // This works, even if the thread is not started yet.
        a.start();
        a.modifyData(); // This also works.
        @endcode */
    struct ScopedPP
    {
    private:
        /** The lock to remember */
        Lock & lock;
        /** The pingpong object for communication */
        PingPong & work;
        
        // Public interface
    public:
        ScopedPP(Lock & lock, const WithStartMarker * ts, PingPong & work);
        ~ScopedPP() { work.doneWork(); lock.Release(); }
    
        // Prevent copying
    private:
        ScopedPP & operator = (const ScopedPP & other);
    };
    #define HasExtendedLock 1
#endif

    /** This template wraps a volatile object and give access to it while this object is alive.
        This provides enormous advantages, as accessing the volatile object while 
        protected will fail at compile time (and not runtime), so the binary is guaranteed to be
        thread-access safe. 

        This object however doesn't ensure any deadlock protection, so the only way to avoid 
        deadlock is to lock the object is the same order in all threads.

        Typically, to write multithread safe classes, you'll write your class this way:
        @code
        class Example
        {
            // Add this to your members
            Lock    lock;

            // Your class interface 
            void myMethod() const 
            {
                // This section must be serialized (no 2 threads can call this at the same time)
            }
            // Another method 
            void otherMethod() const
            {
                // This section must be serialized too (no 2 threads can call this at the same time)
            }
            
            // The multithreading part you can write
            inline void myMethod() const volatile { LockingObjPtr<Example>(*this, lock)->myMethod(); }
        }

        // Let's see the classical approach:
        class Thread1 : public Thread
        {
            Example & obj;
            [...]
            void runThread() 
            {
                obj.myMethod(); // Will compile, calling "void myMethod() const;"
                obj.otherMethod(); // ditto
            }
        };
        class Thread2 : public Thread
        {
            Example & obj;
            [...]
            void runThread() 
            {
                obj.myMethod(); // Will compile, also calling "void myMethod() const" so will likely corrupt obj 
                obj.otherMethod(); // ditto
            }
        };

        // With LockingPtr:
        class Thread1 : public Thread
        {
            volatile Example & obj; // Notice that volatile is added
            [...]
            void runThread() 
            {
                obj.myMethod(); // Will compile, calling "void myMethod() const volatile;"
                obj.otherMethod();  // Compiler error here, telling you that you haven't 
                                    // written a code for multithreaded access (via volatile)
            }
        };
        class Thread2 : public Thread
        {
            volatile Example & obj;
            [...]
            void runThread() 
            {
                obj.myMethod(); // Will compile, calling "void myMethod() const volatile;", so will block 
                                // until Thread1 is done with the obj, so will never corrupt obj 
                obj.otherMethod();  // Compiler error here, telling you that you haven't 
                                    // written a code for multithreaded access (via volatile)    
            }
        };
        @endcode

        As you see, compiler check for unsafe access to your object (while you might miss it, compiler won't).
        If you don't want to write your interface twice, you could have written:
        @code
        class Example
        {
            // Add this to your members
            Lock    lock;

            // Your class interface 
            void myMethod() const;
            // Another method 
            void otherMethod() const;

            // No more dual interface for volatile
        };

        class Thread1 : public Thread
        {
            volatile Example & example; // Notice that volatile is added
            [...]
            void runThread() 
            {
                LockingObjPtr<Example> obj(example, example.lock);
                // While the obj is locked, no other thread can modify the object 
                obj.myMethod(); // Will compile, calling "void myMethod() const;"
                obj.otherMethod();  
            }
        };
        class Thread2 : public Thread
        {
            volatile Example & example;
            [...]
            void runThread() 
            {
                if (something)
                {
                    LockingObjPtr<Example> obj(example, example.lock);
                    obj.myMethod(); // Will compile, calling "void myMethod() const volatile;", so will block 
                                    // until Thread1 is done with the obj, so will never corrupt obj 
                }
                obj.otherMethod();  // Compiler error here, telling you that you haven't 
                                    // written a code for multithreaded access (via volatile)
                                    // This would have been unsafely compiled without volatile declaration
            }
        };        
        @endcode
    */
    template <class T>
    class LockingPtr
    {
        public:
            // Constructor
            LockingPtr(volatile T& obj, Lock& mtx) : pObj_(const_cast<T*>(&obj)), pMtx_(&mtx)  { mtx.Acquire(); }
            ~LockingPtr()           { pMtx_->Release(); }
            // Pointer behavior
            T& operator*()          { return *pObj_; }
            T* operator->()         { return pObj_;  }
        
        private:
            // The protected object pointer   
            T*              pObj_;
            // The lock itself
            Lock*           pMtx_;

            // Don't allow copying the object
            LockingPtr(const LockingPtr&);
            LockingPtr& operator=(const LockingPtr&);
    };

    /** This template wraps a volatile object and give access to it while this object is alive.
        This provides enormous advantages, as accessing the volatile object while 
        protected will fail at compile time (and not runtime), so the binary is guaranteed to be
        thread-access safe. 

        This object however doesn't ensure any deadlock protection, so the only way to avoid 
        deadlock is to lock the object is the same order in all threads.

        @sa LockingPtr
    */
    template <class T>
    class LockingObjPtr
    {
        public:
            // Constructor
            LockingObjPtr(volatile T& obj, volatile Lock& mtx) : pObj_(const_cast<T*>(&obj)), pMtx_(const_cast<Lock*>(&mtx))  { pMtx_->Acquire(); }
            ~LockingObjPtr()            { pMtx_->Release(); }
            // Pointer behavior
            T& operator*()          { return *pObj_; }
            T* operator->()         { return pObj_;  }
        
        private:
            // The protected object pointer   
            T*              pObj_;
            // The lock itself
            Lock*           pMtx_;

            // Don't allow copying the object
            LockingObjPtr(const LockingObjPtr&);
            LockingObjPtr& operator=(const LockingObjPtr&);
    };


    /** This template wraps a volatile object and give access to it while this object is alive.
        This provides enormous advantages, as accessing the volatile object while 
        protected will fail at compile time (and not runtime), so the binary is guaranteed to be
        thread-access safe. 

        This object however doesn't ensure any deadlock protection, so the only way to avoid 
        deadlock is to lock the object is the same order in all threads.

        @sa LockingPtr
    */
    template <class T>
    class LockingConstObjPtr
    {
        public:
            // Constructor
            LockingConstObjPtr(const volatile  T& obj, 
                const volatile Lock& mtx) 
                : pObj_(const_cast<const T*>(&obj)), 
                pMtx_(const_cast<Lock*>(&mtx))  
            { pMtx_->Acquire(); }
            ~LockingConstObjPtr()           { pMtx_->Release(); }
            // Pointer behavior
            const T& operator*()        { return *pObj_; }
            const T* operator->()       { return pObj_;  }
        
        private:
            // The protected object pointer   
            const T*                pObj_;
            // The lock itself
            Lock*                   pMtx_;

            // Don't allow copying the object
            LockingConstObjPtr(const LockingConstObjPtr&);
            LockingConstObjPtr& operator=(const LockingConstObjPtr&);
    };

    /** Simply warp a Plain Old Data type in a class so that compiler based 
        volatile protection works on them too.
        On POD types, volatile simply ensure that no reading nor writing 
        optimization takes place : 
        @code
            int a = 0; // global variable
            while (!a) sleep();  // The compiler will likely optimize this code to while(1) sleep();
            printf("Could only get here if it was volatile\n"); 
            // "a" can be set to a hardware register (at link time), or modified
            // by another thread
            // So the compiler doesn't prevent another thread to modify "a"
        @endcode

        However replacing "int" in the previous code with ThreadProtected<int>
        will stop compilation on "a" reading and writing, so the code will only 
        compile iif modified to :
        @code
            Lock sharedLock;
            volatile ThreadProtectedLong a = 0; 
            // while (!a) sleep(); Compile error here, can't read a while unlocked

            // This is correct
            while (!LockingObjPtr<ThreadProtectedLong>(a, sharedLock)) sleep(); 
            printf("Can only get here if volatile\n"); 
            // Similarly, writing should be done like this (with scope change):
            { LockingObjPtr<ThreadProtectedLong> pA(a, sharedLock); *pA = 3; }
        @endcode
    */
    template <typename T>
    struct ThreadProtected
    {
        T   object;

        operator T()    { return object; }
        ThreadProtected(const T obj ); // : object(obj) {}
    };
    template <typename T>
        ThreadProtected<T>::ThreadProtected(const T obj)
            : object(obj) {}

    /** Those are just wrapper on uint32 to except same behavior from compiler when volatile */
    typedef ThreadProtected<uint32>         ThreadProtectedULong;
    /** Those are just wrapper on tLong to except same behavior from compiler when volatile */
    typedef ThreadProtected<int>            ThreadProtectedLong;
    /** Those are just wrapper on tChar to except same behavior from compiler when volatile */
    typedef ThreadProtected<char>           ThreadProtectedChar;
    /** Those are just wrapper on tByte to except same behavior from compiler when volatile */
    typedef ThreadProtected<uint8>          ThreadProtectedByte;

    // Lock free structure if there is only one writer and multiple reader
    template <typename T>
    class SharedData
    {
    // No instancing allowed
    protected:
        volatile T & shared;

        ~SharedData() { } 
        SharedData(T & rData) : shared(rData) {}
    };

    template <>
    class SharedData<uint32>
    {
    private:
        SharedData<uint32>(const SharedData<uint32> & other);
        SharedData<uint32> operator = (const SharedData<uint32> & other);  
    // No instancing allowed
    protected:
        volatile uint32 & shared;
#if defined(_POSIX) && (HAS_ATOMIC_BUILTIN != 1)
        static pthread_mutex_t    sxMutex;
#endif
        SharedData(uint32 & rData) : shared(rData) 
        {}
        ~SharedData() { } 
    };

    /** Atomic write to the given reference.
        Use like a plain old integer.
        @code
        // One thread must define the object 
        uint32 sharedInt = 0;
        // Other thread simply declare a SharedDataWriter instead of "uint32 &"
        SharedDataWriter sdw(sharedInt);
        // Writing is exactly like an integer, but this one is atomic
        sdw = 3;
        @endcode
    */
    class SharedDataWriter : public SharedData<uint32>
    {
    private:
        SharedDataWriter(const SharedDataWriter & other);
        SharedDataWriter operator = (const SharedDataWriter & other);  
    public:
        SharedDataWriter(uint32 & rData) : SharedData<uint32>(rData) {}
        SharedDataWriter(volatile uint32 & rData) : SharedData<uint32>(const_cast<uint32&>(rData)) {}

        // Uses atomic store to set the value
        inline SharedDataWriter & operator = (const uint32 data) 
#ifdef _WIN32
        { InterlockedExchange((LONG*)&shared, (LONG)data); return *this; }
#elif defined(_POSIX)
    #if (HAS_ATOMIC_BUILTIN == 1)
        #define _enter __sync_synchronize();
        #define _leave __sync_synchronize();
    #else
        #define _enter pthread_cleanup_push((void(*)(void*))pthread_mutex_unlock, &sxMutex); int retVal = pthread_mutex_lock(&sxMutex)
        #define _leave pthread_cleanup_pop(retVal == 0)
    #endif
        { _enter; shared = data; _leave; return *this; }
#else
        #define _enter volatile tBoolean intsOff = MAP_IntMasterDisable(); 
        #define _leave if(!intsOff) MAP_IntMasterEnable()

        { _enter; shared = data; _leave; return *this; }
#endif
    };


    /** Atomic read the given reference.
        Use like a plain old integer.
        @code
        // One thread must define the object 
        uint32 sharedInt = 0;
        // Other thread simply declare a SharedDataReader instead of "uint32 &"
        SharedDataReader sdr(sharedInt);
        // Reading is exactly like an integer, but this one is atomic
        if (sdr == 6) doSomething();
        @endcode
    */
    class SharedDataReader : public SharedData<uint32>
    {
    private:
        SharedDataReader(const SharedDataReader & other);
        SharedDataReader operator = (const SharedDataReader & other);  
    public:
        SharedDataReader(uint32 & rData) : SharedData<uint32>(rData) {}
        SharedDataReader(volatile uint32 & rData) : SharedData<uint32>(const_cast<uint32&>(rData)) {}

#ifdef _WIN32
        // Uses atomic read to get the value
        inline operator uint32 () { return shared; }
#else
        // Uses atomic read to get the value
        inline operator uint32 () { uint32 lVal = 0; _enter; lVal = shared; _leave; return lVal; }
#endif
    };

    /** Atomic read or write the given reference.
        Use like a plain old integer.
        @code
        // One thread must define the object 
        uint32 sharedInt = 0;
        // Other thread simply declare a SharedDataReader instead of "uint32 &"
        SharedDataReaderWriter sdrw(sharedInt);
        // Reading and Writing is exactly like an integer, but this one is atomic
        if (sdrw == 6) doSomething();
        sdrw = 32; // Atomic write
        sdrw++; // Atomic increment
        @endcode
    */
    class SharedDataReaderWriter : public SharedData<uint32>
    {
    private:
        SharedDataReaderWriter(const SharedDataReaderWriter & other);
        SharedDataReaderWriter operator = (const SharedDataReaderWriter & other);  
    public:
        SharedDataReaderWriter(uint32 & rData) : SharedData<uint32>(rData) {}
        SharedDataReaderWriter(volatile uint32 & rData) : SharedData<uint32>(const_cast<uint32&>(rData)) {}

#ifdef _WIN32
        // Uses atomic store to set the value
        inline SharedDataReaderWriter & operator = (const uint32 data) 
        { InterlockedExchange((LONG*)&shared, (LONG)data); return *this; }
        // Uses atomic read to read the value
        inline operator uint32 () { return shared; }
        // Uses atomic read and write to increment this value
        inline SharedDataReaderWriter & operator ++()       { InterlockedIncrement((LONG*)&shared); return *this; }
        // Uses atomic read and write to decrement this value
        inline SharedDataReaderWriter & operator --()       { InterlockedDecrement((LONG*)&shared); return *this; }
        // Uses atomic read and write to increment this value
        inline SharedDataReaderWriter & operator ++(int)    { InterlockedIncrement((LONG*)&shared); return *this; }
        // Uses atomic read and write to decrement this value
        inline SharedDataReaderWriter & operator --(int)    { InterlockedDecrement((LONG*)&shared); return *this; }
#else
        // Uses atomic store to set the value
        inline SharedDataReaderWriter & operator = (const uint32 data) 
        { _enter; shared = data; _leave; return *this; }
        // Uses atomic read to get the value
        inline operator uint32 () {  uint32 lVal = 0; _enter; lVal = shared; _leave; return lVal; }
#if (HAS_ATOMIC_BUILTIN == 1)
        // Uses atomic read and write to increment this value
        inline SharedDataReaderWriter & operator ++()       { __sync_fetch_and_add(&shared, 1); return *this; }
        // Uses atomic read and write to decrement this value
        inline SharedDataReaderWriter & operator --()       { __sync_fetch_and_sub(&shared, 1); return *this; }
        // Uses atomic read and write to increment this value
        inline SharedDataReaderWriter & operator ++(int)    { __sync_fetch_and_add(&shared, 1); return *this; }
        // Uses atomic read and write to decrement this value
        inline SharedDataReaderWriter & operator --(int)    { __sync_fetch_and_sub(&shared, 1); return *this; }
#else
        // Uses atomic read and write to increment this value
        inline SharedDataReaderWriter & operator ++()       { _enter; shared++; _leave; return *this; }
        // Uses atomic read and write to decrement this value
        inline SharedDataReaderWriter & operator --()       { _enter; shared--; _leave; return *this; }
        // Uses atomic read and write to increment this value
        inline SharedDataReaderWriter & operator ++(int)    { _enter; ++shared; _leave; return *this; }
        // Uses atomic read and write to decrement this value
        inline SharedDataReaderWriter & operator --(int)    { _enter; --shared; _leave; return *this; }
#endif
#undef _enter
#undef _leave
#endif
    };

#if (WantAtomicClass == 1)
#if HAS_STD_ATOMIC == 1
    /** When you expect a value to be accessed or modified atomically, you can either use the SharedDataReader, SharedDataWriter or SharedDataReaderWriter on the value, if it's uint32.
        Else, if you intend to use any other type, you can simply declare an Atomic<Type> variable.
        This will not compile for type that can't be accessed atomically.
        The warped type must support being constructed with 0. */
    template <typename T>
    class Atomic
    {
        // Members
    private:
        /** The object to manipulate */
        std::atomic<T> obj;

        // Interface
    public:
        /** Get direct access to the object.
            This is unsafe, since there is no memory barrier involved in accessing the object */
        inline volatile T & unsafeAccess() { return obj.load(std::memory_order_relaxed); }

        /** Read the value atomically (and return a copy). */
        inline T read() const { return obj.load(std::memory_order_consume); }

        /** Save a new value atomically. */
        inline void save(T newValue) { obj.store(newValue, std::memory_order_release); }

        /** Swap the current value with the one given, returning the previous one, atomically.
            Perform this operation atomically:
            @code
                T oldValue = obj;
                obj = newValue;
                return oldValue;
            @endcode */
        inline T swap(const T & value) { return obj.exchange(value, std::memory_order_seq_cst); }

        /** Increments this value atomically
            @return ++obj */
        inline T operator++() { return obj.fetch_add(1, std::memory_order_acq_rel) + 1; }

        /** Atomically decrements this value, returning the new value. */
        inline T operator--() { return obj.fetch_sub(1, std::memory_order_acq_rel) - 1; }

        /** Atomically adds the given amount
            @return obj + amountToAdd */
        inline T operator +=(const T & amountToAdd) { return obj.fetch_add(amountToAdd, std::memory_order_acq_rel) + amountToAdd; }

        /** Atomically substracts the given amount
            @return obj - amountToSubstract */
        inline T operator -=(const T & amountToSubstract) { return obj.fetch_sub(amountToSubstract, std::memory_order_acq_rel) - amountToSubstract; }

        /** The expected CAS (Compare And Set) method.
            Perform this operation atomically:
            @code
                if (read() == comparand)
                {
                    save(newValue);
                    return true;
                }
                return false;
            @endcode
            
            It's typically used like this: 
            @code
                while (!atomic.compareAndSet(newValue, comparand)) comparand = atomic.read();
            @endcode
            @returns true on successful comparison and value change, else value is not modified.
            @warning This method actually requires you loop on the CAS (it's based on the weak form)
            @warning To avoid ABA problem, you should prefer the reference based version
        */
        inline bool compareAndSet(const T & newValue, T comparand, const bool weak) { return obj.compare_exchange_weak(comparand, newValue); }
        /** CAS with update of comparand if it fails.
            This is used to avoid ABA problem. 
            Perform this operation atomically:
            @code
                if (read() == comparand)
                {
                    save(newValue);
                    return true;
                }
                comparand = read();
                return false;
            @endcode
            
            It's typically used like this:
            @code
                while (!atomic.compareAndSet(newValue, comparand));
            @endcode
            @warning This form is unlikely to be generally useable
            */
        inline bool compareAndSet(const T & newValue, T & comparand, const bool weak) { return obj.compare_exchange_weak(comparand, newValue); }
        /** The expected CAS (Compare And Set) method.
            Perform this operation atomically:
            @code
                if (read() == comparand)
                {
                    save(newValue);
                    return true;
                }
                return false;
            @endcode
            
            It's typically used like this: 
            @code
                while (!atomic.compareAndSet(newValue, comparand)) comparand = atomic.read();
            @endcode
            @returns true on successful comparison and value change, else value is not modified.
            @warning To avoid ABA problem, you should prefer the reference based version
        */
        inline bool compareAndSet(const T & newValue, T comparand) { return obj.compare_exchange_strong(comparand, newValue); }
        /** CAS with update of comparand if it fails.
            This is used to avoid ABA problem. 
            Perform this operation atomically:
            @code
                if (read() == comparand)
                {
                    save(newValue);
                    return true;
                }
                comparand = read();
                return false;
            @endcode
            */
        inline bool compareAndSet(const T & newValue, T & comparand) { return obj.compare_exchange_strong(comparand, newValue); }

        // Construction and destruction
    public:
        /** Constructor */
        inline Atomic(const T value = 0) : obj(value) 
        {
        }
        /** Atomic copy constructor */
        inline Atomic (const Atomic& other) : obj(other.obj) {}
        /** Atomic copy operator. */
        inline Atomic& operator= (const Atomic& other) { save(other.read()); return *this; }
        /** Atomic copy operator. */
        inline Atomic& operator= (const T & newValue)    { save(newValue); return *this; }

        // Helpers
    private:
        /** Helper to perform a basic 32 bits to T conversion */
        inline static T from(uint32 value) { return *(T*)&value; }
        /** Helper to perform a basic 64 bits to T conversion */
        inline static T from(uint64 value) { return *(T*)&value; }

        // Prevent some operations
    private:
        // Not defined as it has no sense to use post increment and atomic variables
        T operator-- (int);
        // Not defined as it has no sense to use post increment and atomic variables
        T operator++ (int); 
    };
#else
    /** @internal The internal virtual table function that's selected in the atomic class to avoid runtime cost based on choosing the right function */
    template <int size>
    struct FuncTable
    {
        typedef uint32 Type;
        typedef int32 SignedType;
        inline static uint32 read(volatile uint32 & obj)
        {
#ifdef _MSC_VER
            return (uint32)_InterlockedExchangeAdd((LONG volatile *)&obj, 0UL);
#elif (HAS_ATOMIC_BUILTIN == 1)
            return (uint32)__sync_add_and_fetch (&obj, 0);
#else
            return obj;
#endif
        }
        inline static uint32 swap(volatile uint32 & obj, uint32 value)
        {
#ifdef _MSC_VER
            return (uint32)_InterlockedExchange((LONG volatile*)&obj, (LONG)value);
#elif (HAS_ATOMIC_BUILTIN == 1)
            uint32 currentVal = obj;
            while (! compareAndSet(obj, value, currentVal)) { currentVal = obj; }
            return currentVal;
#else
            return __atomic_swap(value, (volatile int*)obj);
#endif
        }

        inline static bool compareAndSet(volatile uint32 & obj, const uint32 value, const uint32 comparand)
        {
#ifdef _MSC_VER
            return _InterlockedCompareExchange((LONG volatile*)&obj, (LONG)value, (LONG)comparand) == (long)comparand;
#elif (HAS_ATOMIC_BUILTIN == 1)
            return __sync_bool_compare_and_swap((volatile uint32*) &obj, comparand, value);
#else
            return __atomic_cmpxchg(comparand, value, (volatile int*)&obj);
#endif
        }

        inline static uint32 add(volatile uint32 & obj, const int32 amount)
        {
#ifdef _MSC_VER
            return (uint32)_InterlockedExchangeAdd((LONG volatile *)&obj, (LONG)amount);
#elif (HAS_ATOMIC_BUILTIN == 1)
            return (uint32)__sync_add_and_fetch (&obj, amount);
#else
            uint32 oldVal, expectedVal;
            do
            {
                oldVal = obj;
                expectedVal = oldVal + amount;
            } while (!compareAndSet(obj, expectedVal, oldVal))
            return expectedVal;
#endif
        }

        inline static uint32 inc(volatile uint32 & obj)
        {
#ifdef _MSC_VER
            return (uint32)_InterlockedIncrement((LONG volatile *)&obj);
#elif (HAS_ATOMIC_BUILTIN == 1)
            return (uint32)__sync_add_and_fetch (&obj, 1);
#else
            return (uint32)__atomic_inc((volatile int *)&obj) + 1;
#endif
        }
        inline static uint32 dec(volatile uint32 & obj)
        {
#ifdef _MSC_VER
            return (uint32)_InterlockedDecrement((LONG volatile *)&obj);
#elif (HAS_ATOMIC_BUILTIN == 1)
            return (uint32)__sync_add_and_fetch (&obj, -1);
#else
            return (uint32)__atomic_dec((volatile int *)&obj) - 1;
#endif
        }
    };

#if defined(NO_ATOMIC_BUILTIN64)
    #if (_POSIX == 1)
        extern pthread_mutex_t    sxMutex;
        #define _enter pthread_cleanup_push((void(*)(void*))pthread_mutex_unlock, &sxMutex); pthread_mutex_lock(&sxMutex)
        #define _leave pthread_cleanup_pop(1)
    #elif defined(_WIN32)
        extern CRITICAL_SECTION sxMutex;
        #define _enter EnterCriticalSection(&sxMutex);
        #define _leave LeaveCriticalSection(&sxMutex);
    #else
        #error You can not build Atomic code without atomic primitives!
    #endif
#endif
    template <>
    struct FuncTable<8>
    {
        typedef uint64 Type;
        typedef int64 SignedType;

        inline static uint64 read(volatile uint64 & obj)
        {
#if(_MSC_VER && (HAS_ATOMIC_BUILTIN64 == 1))
            return (uint64)PTRInterlockedExchangeAdd64(&obj, 0ULL);
#elif (HAS_ATOMIC_BUILTIN64 == 1)
            return (uint64)__sync_add_and_fetch (&obj, 0);
#else
            return obj;
#endif
        }
        inline static uint64 swap(volatile uint64 & obj, uint64 value)
        {
#if(_MSC_VER && (HAS_ATOMIC_BUILTIN64 == 1))
            return (uint64)PTRInterlockedExchange64(&obj, value);
#elif (HAS_ATOMIC_BUILTIN64 == 1)
            uint64 currentVal = obj;
            while (! compareAndSet(obj, value, currentVal)) { currentVal = obj; }
            return currentVal;
#elif defined(NO_ATOMIC_BUILTIN64)
            uint64 currentVal = 0; _enter; currentVal = obj; obj = value; _leave; return currentVal;
#else
            return __atomic_swap(value, (volatile long long*)obj);
#endif
        }

        static inline bool compareAndSet(volatile uint64 & obj, const uint64 value, const uint64 comparand)
        {
#if(_MSC_VER && (HAS_ATOMIC_BUILTIN64 == 1))
            return PTRInterlockedCompareExchange64(&obj, value, comparand) == comparand;
#elif (HAS_ATOMIC_BUILTIN64 == 1)
            return __sync_bool_compare_and_swap((volatile uint64*) &obj, comparand, value);
#elif (NO_ATOMIC_BUILTIN64 == 1)
            bool ret = false; _enter; if (obj == comparand) { obj = value; ret = true; }  _leave; return ret;
#else
            return __atomic_cmpxchg(comparand, value, (volatile long long*)&obj);
#endif
        }
        static inline uint64 add(volatile uint64 & obj, const int64 amount)
        {
#if(_MSC_VER && (HAS_ATOMIC_BUILTIN64 == 1))
            return (uint64)PTRInterlockedExchangeAdd64(&obj, amount);
#elif (HAS_ATOMIC_BUILTIN64 == 1)
            return (uint64)__sync_add_and_fetch (&obj, amount);
#elif (NO_ATOMIC_BUILTIN64 == 1)
            uint64 ret; _enter; obj += amount; ret = obj; _leave; return ret;
#else
            uint64 oldVal, expectedVal;
            do
            {
                oldVal = obj;
                expectedVal = oldVal + amount;
            } while (!compareAndSet(obj, expectedVal, oldVal))
            return expectedVal;
#endif
        }
        inline static uint64 inc(volatile uint64 & obj)
        {
#if(_MSC_VER && (HAS_ATOMIC_BUILTIN64 == 1))
            return (uint64)PTRInterlockedIncrement64(&obj);
#elif (HAS_ATOMIC_BUILTIN64 == 1)
            return (uint64)__sync_add_and_fetch (&obj, 1);
#elif (NO_ATOMIC_BUILTIN64 == 1)
            uint64 ret; _enter; obj += 1; ret = obj; _leave; return ret;
#else
            return (uint64)__atomic_inc((volatile long long *)&obj) + 1;
#endif
        }
        inline static uint64 dec(volatile uint64 & obj)
        {
#if(_MSC_VER && (HAS_ATOMIC_BUILTIN64 == 1))
            return (uint64)PTRInterlockedDecrement64(&obj);
#elif (HAS_ATOMIC_BUILTIN64 == 1)
            return (uint64)__sync_add_and_fetch (&obj, -1);
#elif (NO_ATOMIC_BUILTIN64 == 1)
            uint64 ret; _enter; obj -= 1; ret = obj; _leave; return ret;
#else
            return (uint64)__atomic_dec((volatile long long *)&obj) - 1;
#endif
        }
    };

#if (NO_ATOMIC_BUILTIN64 == 1)
        #undef _enter
        #undef _leave
#endif

    /** When you expect a value to be accessed or modified atomically, you can either use the SharedDataReader, SharedDataWriter or SharedDataReaderWriter on the value, if it's uint32.
        Else, if you intend to use any other type, you can simply declare an Atomic<Type> variable.
        This will not compile for type that can't be accessed atomically.
        The warped type must support being constructed with 0. */
    template <typename T>
    class Atomic
    {
        // Members
    private:
        /** The object to manipulate */
        Aligned(8) volatile T obj;

        // Interface
    public:
        /** Get direct access to the object.
            This is unsafe, since there is no memory barrier involved in accessing the object */
        inline volatile T & unsafeAccess() { return obj; }

        /** Read the value atomically (and return a copy). */
        inline T read() const { return from(FuncTable<sizeof(T)>::read((volatile typename FuncTable<sizeof(T)>::Type &)obj)); }

        /** Save a new value atomically. */
        inline void save(T newValue) { (void)swap(newValue); }

        /** Swap the current value with the one given, returning the previous one, atomically.
            Perform this operation atomically:
            @code
                T oldValue = obj;
                obj = newValue;
                return oldValue;
            @endcode */
        inline T swap(const T & value) { return from(FuncTable<sizeof(T)>::swap((volatile typename FuncTable<sizeof(T)>::Type &)obj, *(typename FuncTable<sizeof(T)>::Type *)&value)); }


        /** Increments this value atomically
            @return ++obj */
        inline T operator++() { return from(FuncTable<sizeof(T)>::inc((volatile typename FuncTable<sizeof(T)>::Type&)obj)); }

        /** Atomically decrements this value, returning the new value. */
        inline T operator--() { return from(FuncTable<sizeof(T)>::dec((volatile typename FuncTable<sizeof(T)>::Type&)obj)); }

        /** Atomically adds the given amount
            @return obj + amountToAdd */
        inline T operator +=(const T & amountToAdd) { return from(FuncTable<sizeof(T)>::add((volatile typename FuncTable<sizeof(T)>::Type&)obj, *(typename FuncTable<sizeof(T)>::SignedType *)&amountToAdd)); }

        /** Atomically substracts the given amount
            @return obj - amountToSubstract */
        inline T operator -=(const T & amountToSubstract) { return from(FuncTable<sizeof(T)>::add((volatile typename FuncTable<sizeof(T)>::Type&)obj, -*(typename FuncTable<sizeof(T)>::SignedType*)&amountToSubstract)); }

        /** The expected CAS (Compare And Set) method.
            Perform this operation atomically:
            @code
                if (read() == comparand)
                {
                    save(newValue);
                    return true;
                }
                return false;
            @endcode
            
            It's typically used like this: 
            @code
                while (!atomic.compareAndSet(newValue, comparand)) comparand = atomic.read();
            @endcode
            @returns true on successful comparison and value change, else value is not modified.
        */
        inline bool compareAndSet(const T & newValue, const T & comparand) { return FuncTable<sizeof(T)>::compareAndSet((volatile typename FuncTable<sizeof(T)>::Type&)obj, *(typename FuncTable<sizeof(T)>::Type *)&newValue, *(typename FuncTable<sizeof(T)>::Type *)&comparand); }

        // Construction and destruction
    public:
        /** Constructor */
        inline Atomic(const T value = 0) : obj(value) 
        {
            // Atomic operations don't work on object smaller than 32 bits, and not larger than 64 bits (at least currently)
            CompileTimeAssert(sizeof(obj) == 4 || sizeof(obj) == 8);
        }
        /** Atomic copy constructor */
        inline Atomic (const Atomic& other) : obj(other.read()) {}
        /** Atomic copy operator. */
        inline Atomic& operator= (const Atomic& other) { swap(other.read()); return *this; }
        /** Atomic copy operator. */
        inline Atomic& operator= (const T & newValue)    { swap(newValue); return *this; }

        // Helpers
    private:
        /** Helper to perform a basic 32 bits to T conversion */
        inline static T from(uint32 value) { return *(T*)&value; }
        /** Helper to perform a basic 64 bits to T conversion */
        inline static T from(uint64 value) { return *(T*)&value; }

        // Prevent some operations
    private:
        // Not defined as it has no sense to use post increment and atomic variables
        T operator-- (int);
        // Not defined as it has no sense to use post increment and atomic variables
        T operator++ (int); 
    };
#endif

#define HasAtomicClass 1
#endif

}

#endif
