// Need declaration
#include "../../include/Threading/Threads.hpp"

#if (DEBUG==1)
// Need logger for debugging purpose
#include "../../include/Logger/Logger.hpp"
// We need the Assert too
#include "../../include/Utils/Assert.hpp"
#endif

using namespace Threading;


#if defined(_POSIX)
  #include <dlfcn.h>
#endif

#if defined(_MAC)
  // We need them for the semaphore code, since unnamed POSIX semaphore are not supported.
  #include <mach/mach.h>
  #include <mach/task.h>
  #define SIGSTACK SIGUSR2
#endif

#if defined(_LINUX)
#include <link.h>
#include <ucontext.h>
#include <sched.h>

#include <sys/prctl.h>
  #if defined(REG_RIP)
    #define REG_PC REG_RIP
  #elif defined (REG_EIP)
    #define REG_PC REG_EIP
  #elif defined R15
    #define REG_PC R15
  #endif
  #define SIGSTACK SIGRTMIN
#endif // Linux

#if defined(_POSIX)
    namespace Strings { char* ulltoa(uint64 value, char* result, int base); }
  #ifndef STDERR_FILENO
    #define STDERR_FILENO 2
  #endif
    namespace Threading { int ErrorFD = STDERR_FILENO; } // You can override this in your own program if you need to

    struct StackFrameInfo
    {
        char   imagePath[256]; // Fixed size
        void * baseAddr;
        void * frameAddr;

        void writeFrame(int fd)
        {
            char buffer[] = "[0x] ", number[] = "0000000000000000", eol= '\n';
            // Export the values
            write(fd, buffer, 3);

            Strings::ulltoa(reinterpret_cast<uint64>(baseAddr), number, 16);
            write(fd, number, strlen(number));
            write(fd, buffer+4, 1);
            write(fd, buffer+1, 2);

            Strings::ulltoa(reinterpret_cast<uint64>(frameAddr), number, 16);
            write(fd, number, strlen(number));
            write(fd, buffer+3, 2);
            write(fd, imagePath, strlen(imagePath));
            write(fd, &eol, 1);
        }

        // Beware this method allocate memory
        Strings::FastString getFrame()
        {
            return Strings::FastString("[") + Strings::FastString::getHexOf(reinterpret_cast<uint64>(baseAddr)) + " "
                    + Strings::FastString::getHexOf(reinterpret_cast<uint64>(frameAddr)) + "] " + Strings::FastString(imagePath) + "\n";
        }

        StackFrameInfo() : baseAddr(0), frameAddr(0) { memset(imagePath, 0, ArrSz(imagePath)); }
    };


  #if LINUX_USING_LINKER // Using dladdr which seems more standard
    static int saveSymbols(struct dl_phdr_info *info, size_t size, void *data)
    {
        StackFrameInfo * match = (StackFrameInfo*)data;

        const ElfW(Phdr) *phdr;
        ElfW(Addr) load_base = info->dlpi_addr;
        phdr = info->dlpi_phdr;
        for (long n = info->dlpi_phnum; --n >= 0; phdr++)
        {
            if (phdr->p_type == PT_LOAD)
            {
                ElfW(Addr) vaddr = phdr->p_vaddr + load_base;
                if (match->frameAddress >= (void*)vaddr && match->frameAddress < (void*)(vaddr + phdr->p_memsz))
                {   // Ok, the address was found
                    size_t reqNameLen = strlen(info->dlpi_name);
                    const char * binaryName = reqNameLen >= ArrSz(match->imagePath) ? (info->dlpi_name + reqNameLen - ArrSz(match->imagePath) - 1) : info->dlpi_name;
                    strncpy(match->imagePath, binaryName, min(reqNameLen, ArrSz(match->imagePath)));
                    match->baseAddr = (void*)info->dlpi_addr;
                }
            }
        }
        return 0;
    }
  #endif

    void dumpStackFrames(StackFrameInfo * frames, size_t size, void * array[])
    {
        Dl_info info;
        for (size_t i = 0; i < size; i++)
        {
  #if LINUX_USING_LINKER
            frames[i].frameAddr = array[i];
            // Then walk the current list of symbols
            dl_iterate_phdr(saveSymbols, &frames);
  #else // This is supported between linux and mac
            if (array[i] && dladdr(array[i], &info))
            {
                size_t reqNameLen = strlen(info.dli_fname);
                const char * binaryName = reqNameLen >= ArrSz(frames[i].imagePath) ? (info.dli_fname + reqNameLen - ArrSz(frames[i].imagePath) - 1) : info.dli_fname;
                strncpy(frames[i].imagePath, binaryName, min(reqNameLen, ArrSz(frames[i].imagePath)));
                frames[i].baseAddr = (void*)info.dli_fbase;
                frames[i].frameAddr = array[i];
            }
  #endif
        }
    }

    extern "C" void dumpCallstack(void * context)
    {
        ucontext_t * ucontext = (ucontext_t*)context;
        // 30 stack frame should be enough
        void *  array[30];
        size_t size = backtrace(array, 30);
  #if defined(_MAC)
        array[1] = (void *) ucontext->uc_mcontext->__ss.__rip;
  #elif defined(__ARMEL__)
        array[1] = (void *) ucontext->uc_mcontext.arm_pc;
  #else
        array[1] = (void *) ucontext->uc_mcontext.gregs [REG_PC];
  #endif

        // Prepare the backtrace using our own method
        StackFrameInfo frames[30];
        dumpStackFrames(frames, size, array);

        for (size_t i = 0; i < size; i++)
        {
            // And dump them to stderr
            frames[i].writeFrame(Threading::ErrorFD);
        }
    }
#endif


#if defined(_POSIX)
    pthread_key_t Thread::threadThisKey;
    Strings::FastString sStack;
    sig_sem  xSemaphore;
    pthread_t mainThreadT = {0};
    extern "C" void GetSigStack(int sig,  siginfo_t * siginfo, void * _ucontext)
    {
        // Need to dump the stack here
        if (sig == SIGUSR1 || sig == SIGSTACK)
        {
            ucontext_t * ucontext = (ucontext_t*)_ucontext;
            // 30 stack frame should be enough
            void *  array[30];
            size_t size = backtrace(array, 30);
  #ifdef __ARMEL__
            array[1] = (void *) ucontext->uc_mcontext.arm_pc;
  #elif defined(_LINUX)
            array[1] = (void *) ucontext->uc_mcontext.gregs [REG_PC];
  #elif defined(_MAC)
            array[1] = (void *) ucontext->uc_mcontext->__ss.__rip;
  #endif

            // Prepare the backtrace using our own method
            StackFrameInfo frames[30];
            dumpStackFrames(frames, size, array);

            if (size)
            {
                // Get the this pointer in the TLS
                Thread * pThis = (Thread*)pthread_getspecific(Thread::threadThisKey);
                if (sig != SIGSTACK && pThis != NULL)
                {
                    pThis->stack = "";
                    for (size_t i = 0; i < size; i++)
                        pThis->stack += frames[i].getFrame();
                    // And set the semaphore
                    SemPost(pThis->semaphore);
                } else
                {
                    // Probably the main thread, so handle the stack now with global variables
                    sStack = "";
                    for (size_t i = 0; i < size; i++)
                        sStack += frames[i].getFrame();
                    // And set the semaphore
                    SemPost(xSemaphore);
                }
            }
        }
    }

    bool isTIDValid(HTHREAD * hThr)
    {
        static HTHREAD shTr = { 0 };
        return memcmp(hThr, &shTr, sizeof(HTHREAD));
    }
#endif

#if (HasThreadLocalStorage == 1)
namespace Threading
{
    static bool isLocalVariableUsed = false;
    // Global declaration here, should be first
    Thread::LocalVariableList & Thread::getLocalVariableList() { static LocalVariableList tlsList; return tlsList; }
}


bool Thread::LocalVariableList::addVariable(Thread::LocalVariable * localVariable)
{
    ScopedLock scope(lock);
    isLocalVariableUsed = true;
    LocalVariable * cur = last();
    if (!cur) first = localVariable;
    else cur->next = localVariable;
    return true;
}


void Thread::LocalVariableList::removeVariable(LocalVariable::Key key)
{
    ScopedLock scope(lock);
    LocalVariable * cur = first;
    if (first && first->getKey() == key)
    {
        LocalVariable * found = first->next;
        delete first;
        first = found;

#ifdef _WIN32
        TlsFree(key);
#else
        pthread_key_delete(key);
#endif
        isLocalVariableUsed = first != 0;
        return;
    }
    while (cur)
    {
        if (cur->next && cur->next->getKey() == key)
        {   // Found
            // Delete the next variable
            LocalVariable * found = cur->next->next;
            delete cur->next;
            cur->next = found;
#ifdef _WIN32
            TlsFree(key);
#else
            pthread_key_delete(key);
#endif
            return;
        }
        cur = cur->next;
    }
}

bool Thread::LocalVariableList::logExistingVariable(Thread::LocalVariable * var)
{
#if (DEBUG==1)
    if (var) Logger::log(Logger::Warning, "Remaining thread local variable found before leaving: [%s]", (const char*)var->getName());
#endif
    return true;
}
#endif

Thread::~Thread()
{
#if (DEBUG==1)
    delete0(threadName);
    // You MUST destroy the thread in your own destructor, as when the
    // execution hits here, your members are already destructed, but your
    // thread is still running using them!!!
    Assert (!isRunning());
#endif
    destroyThread();
#if defined(_POSIX)
    SemDestroy(semaphore);
#endif
}

Thread::Thread(const char * name) :
#if (DEBUG==1)
    threadName(0),
#endif
#ifdef _WIN32
    thread(NULL), threadID(0), leaving(0), run(false), _lock(name) {}
#else
    leaving(0), run(false), _lock(name), isCreated(false)
{
    memset(&thread, 0, sizeof(thread));
#ifdef _POSIX
    SemInit(semaphore, 0);
#endif
}
#endif


Thread::Thread(const Strings::FastString & name) :
#if (DEBUG==1)
    threadName(new Strings::FastString(name)),
#endif
#ifdef _WIN32
    thread(NULL), threadID(0), leaving(0), run(false), _lock(name) {}
#else
    leaving(0), run(false), _lock(name), isCreated(false)
{
    memset(&thread, 0, sizeof(thread));
#ifdef _POSIX
    SemInit(semaphore, 0);
#endif
}
#endif



#if defined(_WIN32) && (DEBUG==1)
void setThreadName(const char * name, DWORD threadID)
{
    struct ThreadInfo
    {
        DWORD dwType;
        LPCSTR szName;
        DWORD dwThreadID;
        DWORD dwFlags;
    } info;

    info.dwType = 0x1000;
    info.szName = name;
    info.dwThreadID = threadID;
    info.dwFlags = 0;

    __try { RaiseException (0x406d1388, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info); } __except (EXCEPTION_CONTINUE_EXECUTION) {}

}
#endif

bool Thread::createThread(const int stackSize) volatile
{
    LockingObjPtr<RunCondition> pRun(run, _lock);
    if (pRun->isRunning())
    {
        // Need to release the RunCondition object lock
        _lock.Release();
        if (!destroyThread()) return 0;
        _lock.Acquire();
    }

    pRun->start();
#ifdef _WIN32
    thread = ::CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)Thread::RunThread, (LPVOID)this, 0, (DWORD*)&threadID);
    if (thread == INVALID_HANDLE_VALUE)
    {   pRun->stop();   thread = NULL; threadID = 0; return false; }

    #if (DEBUG==1)
        setThreadName(threadName ? (const char*)*threadName : _lock.getName(), threadID);
    #endif
    return true;
#else
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    if (stackSize)
        pthread_attr_setstacksize(&attr, stackSize);

#if defined(_POSIX) && (DEBUG==1)
    // This needs to be done before the thread
    if (!isTIDValid((HTHREAD*)&mainThreadT)) installMainThreadHandler();
#endif
    if (pthread_create((HTHREAD*)&thread, &attr, Thread::RunThread, (void*)this) != 0)
    {
        pRun->stop();   memset((void*)&thread, 0, sizeof(thread)); return false;
    }
    // Make sure the main thread also have an handler
    isCreated = true;
    return true;
#endif
}

void * Thread::getThreadID() const
{
    return const_cast<void*>((const void*)thread);
}

#ifdef _WIN32
DWORD Thread::RunThread(LPVOID pVoid)
#else
void * Thread::RunThread(void * pVoid)
#endif
{
    if (pVoid != NULL)
    {
        Thread * pThread = (Thread*)pVoid;
#ifdef _WIN32
        DWORD dw = pThread->runThread();
#else

#ifdef _POSIX
        // Install the signal handler for SIGUSR1 if not set already
        // Check if the signal handler as already been set
        struct sigaction pOld;
        if (sigaction(SIGUSR1, NULL, &pOld) == 0)
        {
            if (pOld.sa_sigaction == ::GetSigStack)
            {   // Already set up
                // Use thread local storage to store this pointer
                pthread_setspecific(threadThisKey, (void*)pThread);
                // Set the mask to block SIGUSR2 (only the main thread can get it)
                sigset_t mask;
                sigemptyset(&mask);
                sigaddset(&mask, SIGUSR2);
                pthread_sigmask(SIG_SETMASK, &mask, NULL);
            } else
            {
                struct sigaction sa;
                memset(&sa, 0, sizeof(sa));
                sa.sa_sigaction = ::GetSigStack;
                sa.sa_flags = SA_SIGINFO;
                sigemptyset(&sa.sa_mask);
                // Use thread local storage to store this pointer
                pthread_setspecific(threadThisKey, (void*)pThread);
                sigaction(SIGUSR1, &sa, NULL);
                // Set the mask to block SIGUSR2 (only the main thread can get it)
                sigset_t mask;
                sigemptyset(&mask);
                sigaddset(&mask, SIGUSR2);
                pthread_sigmask(SIG_SETMASK, &mask, NULL);
            }
        }
#if defined(_LINUX) && (DEBUG==1)
        if (pThread->threadName || pThread->_lock.getName()) prctl (PR_SET_NAME, pThread->threadName ? (const char*)*pThread->threadName : pThread->_lock.getName(), 0, 0, 0);
#elif defined(_MAC) && (DEBUG==1)
        if (pThread->threadName || pThread->_lock.getName()) pthread_setname_np(pThread->threadName ? (const char*)*pThread->threadName : pThread->_lock.getName());
#endif

#endif
        void * dw = (void*)(long int)pThread->runThread();
#endif

        // Stop the run condition anyway
        LockingPtr<RunCondition> pRun(pThread->run, pThread->_lock);
        pRun->stop();

#if (HasThreadLocalStorage == 1)
        // Then delete all the TLS variable, if any used
        if (Threading::isLocalVariableUsed)
            getLocalVariableList().enumerateVariables(destructAllLocalVariables);
#endif

        return dw;
    }
    return 0;
}

// Check if the thread is running
bool Thread::isRunning() const
{
    LockingConstObjPtr<RunCondition> pRun(run, _lock);
    return pRun->isRunning();
}




bool Thread::destroyThread(const bool & rcbDontWait) volatile
{

    // Force calling the locking destructor
    {
        // Access volatile boolean
        LockingObjPtr<RunCondition> pRun(run, _lock);
        // Check if the thread is running
        if (!pRun->isRunning())
        {
            if (leaving) leaving->threadLeaving(const_cast<Thread*>(this));
#ifdef _WIN32
            // Close the handles if needed
            if (thread != NULL)
            {
                HTHREAD hThread = thread;
                thread = NULL;
                threadID = 0;
                CloseHandle(hThread);
            }
#elif defined(_POSIX)
            pthread_setspecific(threadThisKey, NULL);
            if (isCreated)
            {
                void * ret;
                if (rcbDontWait)
                    pthread_cancel(thread);
                pthread_join(thread, &ret);
                memset((void*)&thread, 0, sizeof(thread));
                isCreated = false;
            }
            if (isTIDValid((HTHREAD*)&thread))
            {
                memset((void*)&thread, 0, sizeof(thread));
            }
            SemDestroy(semaphore);
            SemInit(semaphore, 0);
#endif
            return true;
        }
        // Stop the thread
        pRun->stop();
    }

    // Check if we are not suiciding
    if (isOurThread()) return false;
#ifdef _WIN32
    // Close the handles if needed
    if (thread != NULL)
    {
        // Wait until it has finished
        DWORD dwRet = ::WaitForSingleObject(thread, rcbDontWait ? 1000 : INFINITE);
        if (leaving) leaving->threadLeaving(const_cast<Thread*>(this));
        if (rcbDontWait && dwRet != WAIT_OBJECT_0) TerminateThread(thread, 0);

        HTHREAD hThread = thread;
        thread = NULL;
        threadID = 0;
        CloseHandle(hThread);
    }
#elif defined(_POSIX)
    // The only possible errors here mean thread is stopped or invalid
    // so it is safe to continue
    if (isTIDValid((HTHREAD*)&thread))
    {
        void * ret;
        if (leaving) leaving->threadLeaving(const_cast<Thread*>(this));
        // Undo the signal handling here
        pthread_setspecific(threadThisKey, NULL);
        if (rcbDontWait)
            pthread_cancel(thread);
        else
        {
            pthread_join(thread, &ret);
        }
        memset((void*)&thread, 0, sizeof(thread));
        SemDestroy(semaphore);
        SemInit(semaphore, 0);
    }
#endif
#ifndef _WIN32
    isCreated = false;
#endif
    return true;
}

void Thread::Sleep(const uint32 lMilliseconds, const bool hard)
{
#ifdef _WIN32
    ::Sleep((DWORD)lMilliseconds);
#else
    if (lMilliseconds == 0) sched_yield();
    else
    {
#ifdef _POSIX
        struct timespec req = { (time_t)(lMilliseconds / 1000), (long)((lMilliseconds % 1000) * 1000000) };
        while (nanosleep(&req, &req) < 0 && hard);
#else
        portTickType xDelay = lMilliseconds / portTICK_RATE_MS;
        portTickType xLastWakeTime = xTaskGetTickCount();
        vTaskDelayUntil( &xLastWakeTime, xDelay );
#endif
    }
#endif
}
void * Thread::getCurrentThreadID()
{
#ifdef _WIN32
    return (void*)(uint64)::GetCurrentThreadId();
#else
    return (void*)pthread_self();
#endif
}
bool Thread::isOurThread() const volatile
{
#ifdef _WIN32
    return (GetCurrentThreadId() == threadID);
#elif defined(_POSIX)
    HTHREAD curThread = pthread_self();
    return (memcmp((void*)&curThread, (void*)&thread, sizeof(thread)) == 0);
#endif
}

#ifdef _POSIX
Strings::FastString Thread::getStack()
{
    // Send a get-stack signal
    pthread_kill(thread, SIGUSR1);
    // And wait until it is available
    SemWait(semaphore);
    return stack;
}

Strings::FastString GetMainThreadStack()
{
    // Initialize the signal handler for the main thread
    // Send a get-stack signal
    pthread_kill(mainThreadT, SIGUSR1);
    // And wait until it is available
    SemWait(xSemaphore);
    return sStack;
}

/** Get the current thread's stack (Linux only) */
Strings::FastString Thread::getCurrentThreadStack()
{
    pthread_kill(pthread_self(), SIGSTACK);
    SemWait(xSemaphore);
    return sStack;
}


void Thread::installMainThreadHandler()
{
    if (!isTIDValid(&mainThreadT))
    {
        // We are called for the first time, so remember the main thread ID
        mainThreadT = pthread_self();
        // And now, install the signal handler for SIGUSR1
        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_sigaction = ::GetSigStack;
        sa.sa_flags = SA_SIGINFO;
        sigemptyset(&sa.sa_mask);
        sigaction(SIGUSR1, &sa, NULL);
        sigaction(SIGSTACK, &sa, NULL);
        // Use thread local storage to store this pointer
        pthread_key_create(&threadThisKey, NULL);
        pthread_setspecific(threadThisKey, (void*)NULL);

/*
        struct sigaction sa = { 0 };
        sa.sa_handler = ::GetSigStack;
        sigemptyset(&sa.sa_mask);
        sigaction(SIGUSR1, &sa, NULL);
        // Use thread local storage to store this pointer
        if (pthread_setspecific(threadThisKey, (void*)NULL) == EINVAL)
            pthread_key_create(&threadThisKey, NULL);
*/
    }
}
#endif

bool Thread::setCurrentThreadPriority(const int priority)
{
#ifdef _WIN32
    return SetThreadPriority(GetCurrentThread(), (priority - MinPriority) * (THREAD_PRIORITY_TIME_CRITICAL - THREAD_PRIORITY_IDLE) / (MaxPriority - MinPriority) + THREAD_PRIORITY_IDLE) != FALSE;
#elif defined(_POSIX)
    int policy = 0;
    struct sched_param param = {0};

    // Need to figure out the current scheduler parameters
    if (pthread_getschedparam (pthread_self(), &policy, &param) != 0)
        return false;

    policy = priority == MinPriority ? SCHED_OTHER : SCHED_RR;

    const int minPriority = sched_get_priority_min(policy);
    const int maxPriority = sched_get_priority_max(policy);

    // Change priority now
    param.sched_priority = (priority - MinPriority) * (maxPriority - minPriority) / (MaxPriority - MinPriority) + minPriority;
    return pthread_setschedparam (pthread_self(), policy, &param) == 0;
#else
    return false;
#endif
}
bool Thread::setCurrentThreadOnProcessorMask(const uint64 mask)
{
#ifdef _WIN32
    return SetThreadAffinityMask(GetCurrentThread(), (DWORD_PTR)mask) != 0;
#elif defined(_LINUX) // Really linux, doesn't exists on Mac
    cpu_set_t Mask;
    CPU_ZERO (&Mask);

    for (size_t i = 0; i < sizeof(mask); ++i)
        if ((mask & (1 << i))) CPU_SET (i, &Mask);

    // If this doesn't compile, you need to update your glibc library
    // Quote from man page:
    // "The CPU affinity system calls were introduced in Linux kernel 2.5.8. The library interfaces were introduced in glibc 2.3.
    // Initially, the glibc interfaces included a cpusetsize argument. In glibc 2.3.2, the cpusetsize argument was removed,
    // but this argument was restored in glibc 2.3.4. "
    sched_setaffinity (0, sizeof(Mask), &Mask);
    sched_yield();
    return true;
#else
    return false;
#endif
}
int Thread::getCurrentCoreCount()
{
#ifdef _WIN32
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return info.dwNumberOfProcessors;
#elif defined(_POSIX)
    return sysconf(_SC_NPROCESSORS_CONF);
#else
    return 1;
#endif

/*
    struct sysinfo info;
    if (sysinfo(&info) == 0) return info.procs;
    return 0;
*/
}
