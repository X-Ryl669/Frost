#ifndef h_CPP_Thread_CPP_h
#define h_CPP_Thread_CPP_h

// Need types
#include "../Types.hpp"
// Need Locks
#include "Lock.hpp"
// Need Strings
#include "../Strings/Strings.hpp"

#ifndef __cplusplus
    #error "This file shouldn't be included in C code"
#endif

#ifdef _POSIX
    extern "C" void GetSigStack(int sig,  siginfo_t * siginfo, void * _ucontext);
    Strings::FastString GetMainThreadStack();
#endif

/** Classes that provides multithreading functionality.
    You'll find the abstract class Threading::Thread to implement a safe thread.
    There's also the Threading::WithStartMarker that let you wait until the thread is started.
    If you need to run some code asynchronously, you'll have to use Threading::AsyncExecution. 
    
    There's also the multiple lock system, like:
    - Threading::Lock (based on Futex or Critical section), 
    - Threading::Event (useful to signal a condition atomically)
    - Threading::ReadWriteLock (multiple reader can access a reader-locked section, but only one write is allowed to enter a writer locked section)
    - Threading::SharedDataWriter / Threading::SharedDataReader / Threading::SharedDataReaderWriter are used to implement atomic operations on integers 
    - Threading::LockingObjPtr et al, are used to automatically lock an object on access transparently (� la serialize in Java) */
namespace Threading
{
    class RunCondition
    {
        /** The thread run condition */
        bool        run;
        /** Disallow copying */
        RunCondition(const RunCondition &);
        /** Disallow copying */
        RunCondition & operator = (const RunCondition &);

    public:
        /** Thread should run */
        inline void start()             { run = true; }
        /** Is the thread running ? */
        inline bool isRunning() const   { return run; }
        /** Thread should stop */
        inline void stop()              { run = false; }
        /** Construction */
        RunCondition(const bool run) : run(run)   {}
    };

    /** This class implements the thread model in platform independence
        To run a thread in your software, simply derive from this class
        and overload the RunThread pure virtual method.


        It is not safe to call DestroyThread from the thread's RunThread 
        method. Doing so could result in deadlocking.
        
        The proper way to stop the thread from RunThread is to return from 
        the method (this also ensure correct stack and object destruction).

        It is safe to call isRunning from any thread, including inside
        RunThread method.
    */
    class Thread
    {
        // Type definition and enumeration
    public:
        /** The priority you can deal with */
        enum Priority
        {
            MinPriority = 0,        //!< The minimum priority
            DefaultPriority = 50,   //!< The default priority
            MaxPriority = 100,      //!< The maximum priority
        };

#if (WantThreadLocalStorage == 1)
        /** The thread local storage information */
        struct LocalVariable
        {
#ifdef _WIN32 
            typedef DWORD Key;
#else 
            typedef pthread_key_t Key;
#endif

            /** Typically construct a variable each time the thread is created */
            virtual void construct() = 0;
            /** Destruct the variable each time a thread is deleted */
            virtual void destruct() = 0;
            /** Get the variable name (used for listing the variables) */
            virtual Strings::FastString getName() const = 0;
            /** Get the current key */
            virtual const Key getKey() const = 0;
            
            LocalVariable * next;
            
            LocalVariable() : next(0) {}
            // Linked list destruction is not a good idea here
            virtual ~LocalVariable() { /* if (next) next->destruct(); next = 0; */ }
        };
        
        /** The templated version of such variable */
        template <class T>
        class LocalVariableImpl : public LocalVariable
        {
            /** The local key */
            Key  key;
            /** The construction function */
            typedef T * (*ConstructFunc) (Key key, T * currentGet);
            /** The destruction function */
            typedef void (*DestructFunc) (T * currentGet);
            ConstructFunc   constructFunc;
            DestructFunc    destructFunc;
            
            // Helper functions
            T * get() 
            { 
#ifdef _WIN32 
                return (T*)TlsGetValue(key);
#else 
                return (T*)pthread_getspecific(key);
#endif
            }
            
            void set(T * value)
            {
#ifdef _WIN32 
                TlsSetValue(key, (LPVOID)value);
#else 
                pthread_setspecific(key, (void *)value);
#endif
            }
            
            static void defaultDestructWithDelete(T * val) { delete val; }
            
        public:
            inline operator T*  () 
            { 
                T* ret = get(); 
                if (!ret) { construct(); return get(); }
                return ret;
            }
            
            LocalVariableImpl & operator = (T * other) 
            {
                if (other == this) return *this;
                
                delete get(); 
                set(other);
                return *this;
            }
            
            virtual void destruct() { (*destructFunc)(get()); set(0); }
            virtual void construct() { set((*constructFunc)(key, get())); }
            Strings::FastString getName() const
            {
                Strings::FastString templateType = Strings::FastString(
#ifdef _MSC_VER
                __FUNCTION__);
#else
                __PRETTY_FUNCTION__);
#endif
                // Try GCC type first, since it's the most specific : "LocalVariableImpl<T>::getName() [ with T = yourTypeHere ]"
                Strings::FastString finalType = templateType.fromFirst("=").upToFirst("]").Trimmed();
                // Else, try to use the templated argument itself: "LocalVariableImpl<yourTypeHere>::getName()"
                if (!finalType) finalType = templateType.fromFirst("<").upToFirst(">").Trimmed();

                return Strings::FastString::Print("Thread Local Variable of type %s and key " PF_LLD , (const char*)finalType, (int64)key);
            }
            const Key getKey() const { return key; }
            
            
            LocalVariableImpl(const Key key, ConstructFunc cons, DestructFunc des = &defaultDestructWithDelete) : key(key), constructFunc(cons), destructFunc(des) 
            {}
            LocalVariableImpl(const Key key, T * val, ConstructFunc cons, DestructFunc des = &defaultDestructWithDelete) : key(key), constructFunc(cons), destructFunc(des) 
            { set(val); }
            
            ~LocalVariableImpl() { destruct(); }
        };
        
        /** This is used to keep a list of the local variable that are used in our thread
            This is global to all threads.
            Beware that this is not an optimized list, as the number of TLS variable is limited anyway. */
        class LocalVariableList
        {
        private:
            /** The first element in the list */
            LocalVariable * first;
            /** The global lock */
            FastLock        lock;
            
            /** Get the last variable */
            inline LocalVariable * last() { LocalVariable * cur = first; while (cur && cur->next) cur = cur->next; return cur; }
            /** Count the number of variable */
            inline size_t getSize() { ScopedLock scope(lock); size_t size = 0; LocalVariable * cur = first; while (cur) { cur = cur->next; size ++; } return size; }
            /** Log the remaining local variable on the logger */
            static bool logExistingVariable(LocalVariable * var);
            
            // Interface
        public:
            /** Add a local variable to the list 
                @param localVariable  A local variable that's owned by our class 
                @return false if the number of thread local variable is too high for this system */
            bool addVariable(LocalVariable * localVariable);
            /** Add a local variable to the list 
                @param value    A pointer on a new allocated variable that will be used for this thread's local version once created (it's owned by this thread)
                @param consFunc A pointer to a function that will be called in other thread to construct such a variable when accessed for the first time
                @param desFunc  A pointer to a function that will be called in other thread to destruct such a variable when leaving the thread */
            template <class T>
            LocalVariable::Key addVariableWithFunc(T * value, T * (*consFunc)(LocalVariable::Key, T*), void (*desFunc)(T*))
            {
                // Need to allocate a TLS index
                LocalVariable::Key newKey;
#ifdef _WIN32
                newKey = TlsAlloc();
#else
                pthread_key_create(&newKey, NULL); // We don't use a destructor here as we are doing our own
#endif
                // Then append such a variable
                return addVariable(new LocalVariableImpl<T>(newKey, value, consFunc, desFunc)) ? newKey : 0;
            }
            
            /** Remove a variable from the list.
                Beware that all other thread must be stopped before doing this else it'll failed whenever a thread try to access the removed variable 
                @warning This has nothing to do with deleting the thread local version of the variable which is done automatically when thread exits */
            void removeVariable(LocalVariable::Key key);
            
            /** Enumerate the variables.  
                @param func     A pointer on a bool function(LocalVariable * var) function
                @warning You can't add/remove a variable in the same thread which is enumerating, since the list is locked during enumeration */
            inline bool enumerateVariables(bool (*func)(LocalVariable *)) { ScopedLock scope(lock); LocalVariable * cur = first; while (cur) { if (!(*func)(cur)) return false; cur = cur->next; } return true; }
            
            
            /** Find a local variable by key, and return it. 
                @return 0 on error or not found */
            LocalVariable * findByKey(LocalVariable::Key key) { ScopedLock scope(lock); LocalVariable * cur = first; while (cur) { if (cur->getKey() == key) return cur; cur = cur->next; } return 0; }
            
            LocalVariableList() : first(0) {}
            ~LocalVariableList() 
            { 
                // Called when the static object is destructed, expected after the main program is stopped.
                enumerateVariables(logExistingVariable);
                while (first) { removeVariable(first->getKey()); } 
            }
        };
#define GetLocalVariable(X, Y, key) X * Y = 0; { Threading::Thread::LocalVariableImpl<X>* tmpVar = dynamic_cast< Threading::Thread::LocalVariableImpl<X>* >(Threading::Thread::getLocalVariable(key)); Y = tmpVar ? tmpVar->operator X*() : 0; }
        
        
        static LocalVariableList & getLocalVariableList();
#endif

        // Type definition and enumeration
    public:
        /** Thread leaving callback */
        struct Leaving
        {
            /** This is called when the thread is leaving (at the very last moment). */
            virtual void threadLeaving(Thread * leavingThread) = 0;
            virtual ~Leaving() {}
        };
    
        // Members
    protected:
        // Those members here must not be used outside this object
        // If you need to do so, please mark them as volatile and
        // provide a locking mechanism to access them
        /** The thread handle */
        HTHREAD     thread;
#ifdef _WIN32
        /** The thread identifier */
        DWORD       threadID;
#endif
        /** The thread leaving callback, if provided */
        Leaving *               leaving;
#if (DEBUG==1)
        /** The thread name if provided at construction time */
        Strings::FastString *    threadName;
#endif
    
        // Give no access to child classes 
    private:
        /** The run condition */
        volatile RunCondition   run; 
        /** This thread lock */
        mutable  Lock           lock;
        

        // Interface
    public:
        /** Create the thread */
        bool createThread(const int stackSize = 0) volatile;
        /** Destroy the thread (stop it and close the handles)
            @param dontWait     Never, never set this to true (only in test vectors)
        */
        bool destroyThread(const bool & dontWait = false) volatile;
        /** The needed override */
        virtual uint32 runThread() = 0;
        /** Is the thread running ? (this is thread safe, as it locks the object) */
        bool isRunning() const;
        
#ifdef _WIN32
        /** The run thread hook */
        static DWORD RunThread(LPVOID pVoid);
#else 
        /** The run thread hook */
        static void* RunThread(void* pVoid);

    private:
        /** Is the thread created ? */
        bool                    isCreated;
    public:
#endif
#ifdef _POSIX
        /** Stop the thread, get its stack and resume (Linux only) */
        Strings::FastString getStack();
    
        /** Install the dump stack handler for main thread */
        static void installMainThreadHandler();
        
        /** @cond Private 
            Stack only stuff */
    private:
        /** Current stack */
        Strings::FastString     stack;
        /** The semaphore to wait on 
            (cannot wait with a lock or an event as they are not signal safe) */
        sig_sem                 semaphore;
        /** Static key to store "this" in thread specific data */
        static pthread_key_t    threadThisKey;
    public:
        /** Make sure the signal handler can access private stuff */
        friend void ::GetSigStack(int, siginfo_t *, void *);
        /** @endcond */
#endif      
        /** The platform independent Sleep method */
        static void Sleep(const uint32 milliseconds = 0);
        /** Interruptible sleep.
            Sleep the given amount of millisecond while still watching the isRunning flag at regular interval and exits if the thread should be stopped.
            @param milliseconds   The sleep time in milliseconds
            @return true if the sleep completed fully */
        inline bool interruptibleSleep(const uint32 milliseconds) const
        {
            uint32 sleepTime = 0;
            while (sleepTime < milliseconds && isRunning())
            {
                Sleep(100 < (milliseconds - sleepTime) ? 100 : (milliseconds - sleepTime)); sleepTime += 100;
            }
            return isRunning();
        }
        /** Get the current thread ID */
        static void * getCurrentThreadID();
        /** Get the thread ID */
        void * getThreadID() const;
        /** Set the thread leaving callback, that will be called upon thread termination.
            @param cb A pointer to a non-owned callback that'll be called when the thread is leaving. Set to 0 to remove the callback.
            @warning The thread does not own the callback pointer that should still exist when the thread is leaving. */
        inline void setLeavingCallback(Leaving * cb) { leaving = cb; }
        
#if (WantThreadLocalStorage == 1)
        /** Append a TLS variable */
        template <class T>
        inline static LocalVariable::Key appendLocalVariable(T * value, T * (*consFunc)(LocalVariable::Key, T*), void (*desFunc)(T*)) { return getLocalVariableList().addVariableWithFunc(value, consFunc, desFunc); }
        /** Get a local variable by its key */
        inline static LocalVariable * getLocalVariable(LocalVariable::Key key) { return getLocalVariableList().findByKey(key); }
        /** Remove a local variable from the list */
        inline static void removeLocalVariable(LocalVariable::Key key) { getLocalVariableList().removeVariable(key); }
        // Helpers functions
    private:
        /** Delete all local variable */
        static bool destructAllLocalVariables(LocalVariable * var) { if (var) var->destruct(); return true; }
    public:
#define HasThreadLocalStorage 1
#endif
        /** Change the current thread priority.
            @param priority  A value in range [ MinPriority ; MaxPriority ]. 
            @return true if the priority was changed */
        static bool setCurrentThreadPriority(const int priority);
        /** Set the affinity mask for the current thread. 
            @return true if the mask was changed */
        static bool setCurrentThreadOnProcessorMask(const uint64 mask);
        /** Get the number of core on this system */
        static int getCurrentCoreCount();


        // Constructor & Destructors
    public:
        /** Construct a thread.
            This version doesn't own the thread name if provided.
            @param name     A pointer on a static area containing the name.
            @warning The thread doesn't own the memory given, and it must survive until the thread is destructed. */
        Thread(const char * name = NULL);
        
        /** Construct a thread.
            This version own the thread name if provided.
            @param name     A pointer on a static area containing the name.
            @warning The thread doesn't own the memory given, and it must survive until the thread is destructed. */
        Thread(const Strings::FastString & name);

        // Destructor
        virtual ~Thread();
    };

    /** Simply add an event to make sure the thread is started before destroying it. 
        Usage is very simple:
        @code 
            // Add WithStartMarker to base class list 
            class MyClass : public Thread, public WithStartedMarker
            {
            [...]
                uint32 RunThread() 
                {   // Add Started() call to tell any waiter we have started
                    started()
                    // [...]
                }
            };

            // In the thread creator code
            bool bError = xMyClassInstance.CreateThread();
            // Add WaitUntilStarted() call
            if (!bError) xMyClassInstance.WaitUntilStarted();
            //  [...]
        @endcode
    */
    class WithStartMarker 
    {
        // The private member
    private:
        /** Is the thread started event ? */
        mutable Event   start;

        // Interface
    public:
        /** Wait until the thread is started */
        bool    waitUntilStarted() const volatile    { return start.Wait(); }
        /** Tell any waiter that the thread is started */
        bool    started() volatile                   { return start.Set(); }

        // Construction
    public:
        /** Default constructor */
        WithStartMarker(const char * name = NULL) : start(name, true) {}
    };

    /** A simple scheduling thread.
        This is used to trigger predefined actions when the given delay is elapsed 
        @warning An asynchronous thread is very hard to get right (deadlock / livelock), so please avoid using this. 
        @warning The callback is called in the asynchronous thread context. You can't cancel a scheduling inside the callback's code.
        @code
            // Using the class is straightforward
            // In your member:
            AsyncExecution  async;

            // In your code
            Constructor() : async(this) { [...] }
            async.schedule(delay);
            // You can also unschedule something
            async.cancelScheduling();
        @endcode 
        @sa Callback::delayExpired */
    class AsyncExecution : public Thread, public WithStartMarker
    {
        // Type definition and enumeration
    public:
        /** The callback to call when delay expires.
            @sa AsyncExecution */
        struct Callback
        {
            /** The method that is called back 
                @return true if you want to be scheduled again with the same delay or false for one shot 
                @warning The method is not volatile here. This means that the call shouldn't take any lock.
                
                If you can't insure that there there is no lock taken, then your code must call 
                cancelScheduling at the very beginning of your destructor, like this:
                @code
                    struct MyClass : public AsyncExecution::Callback
                    {
                        AsyncExecution             asyncObj;
                        Lock                       lock;
                    [...]
                        void delayExpired() 
                        { 
                            // Call whatever locking method 
                            lock.Acquire();
                            lock.Release();
                        }
                    
                        // Make sure that destruction code doesn't do any creation or locking whatsoever due 
                        // to async callback being triggered.
                        // This should be done before any other code is called in the destructor, 
                        // to prevent corrupted object state when calling the asynchronous method 
                        virtual ~MyClass() 
                        { 
                            // If the callback is being processed, the object is still valid here
                            asyncObj.cancelScheduling(); // This waits until the asynchronous callback is finished anyway
                            [...] // Here you can take your locks, this won't deadlock, or worst, crash.

                            // For example, this code is wrong:
                            // ScopedLock scope(lock);
                            // delete[] array;
                            // asyncObj.cancelScheduling(); // Deadlock here, as the we are waiting for the async callback

                            // For example, this code is also wrong:
                            // {
                            //     ScopedLock scope(lock);
                            //     delete[] array;
                            //     array = 0; // This is useless when multithreading
                            // } // The async callback now could use array here, and crash
                            // asyncObj.cancelScheduling(); 
                        }
                    };
                @endcode */
            virtual bool delayExpired() = 0;
            /** Useless virtual destructor */
            virtual ~Callback() {};
        };

        // Members
    private:
        /** The setup delay (in second) */
        uint32      delay;
        /** The callback object */
        Callback &  callback;

        // Interface
    public:    
        /** Schedule an asynchronous call. 
            @warning If there is already a call scheduled, it's canceled, and a new one is started */
        bool schedule(const uint32 millisecond)
        {
            SharedDataWriter writer(delay);
            writer = millisecond;
            // Stop the thread if it is already running
            if (isRunning()) { cancelScheduling(); }
            if (!createThread() || !waitUntilStarted()) return false;   
            return true;
        }
        /** Cancel a programmed schedule.
            @warning If the asynchronous callback is being processed, this call waits until it's finished. */
        inline void cancelScheduling() { destroyThread(); }

        // Thread interface
    private: 
        /** Run the thread */
        virtual uint32 runThread()
        {
            started();
            SharedDataReader reader(delay);
            while (isRunning())
            {
                // Allow fast closing 
                uint32 delay = reader;
                while (isRunning() && delay > 100)
                {
                    Thread::Sleep(100);
                    delay -= 100;
                }
                if (!isRunning()) return 0;
                if (!callback.delayExpired()) break;
            }
            return 0;
        }
        
        // Construction
    public:
        /** Default construction */
        AsyncExecution(Callback & callback) : Thread("AsyncExec"), WithStartMarker("AsyncExM"), delay(0), callback(callback) {}
    };
}

#endif
