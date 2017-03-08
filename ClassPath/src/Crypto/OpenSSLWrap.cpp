// We need thread code
#include "../../include/Threading/Threads.hpp"
// We need our declaration
#include "../../include/Crypto/OpenSSLWrap.hpp"

#define OPENSSL_THREAD_DEFINES
#include <openssl/opensslconf.h>
#if !defined(OPENSSL_THREADS)
#error You must configure OpenSSL with threads support to use this library
#endif

// For safety reasons, we don't define anymore the SSLv2 crypto scheme. Only TLSv1 is supported for now
#define OPENSSL_NO_SSL2

namespace Crypto
{
#if (OPENSSL_VERSION_NUMBER < 0x10000000) 
    #ifdef _WIN32
        static HANDLE * lock_cs = 0;
    #else
        static pthread_mutex_t * lock_cs = 0;
        static long * lock_count = 0;
    #endif



    struct MultiThreadProtection
    {
        MultiThreadProtection()
        {
    #ifdef _WIN32
	        lock_cs = (HANDLE*)OPENSSL_malloc(CRYPTO_num_locks() * sizeof(HANDLE));
	        for (int i = 0; i < CRYPTO_num_locks(); i++)
		        lock_cs[i] = CreateMutex(NULL, FALSE, NULL);

	        CRYPTO_set_locking_callback((void (*)(int,int, const char *,int))win32_locking_callback);
    #else

            lock_cs = (pthread_mutex_t*)OPENSSL_malloc(CRYPTO_num_locks() * sizeof(pthread_mutex_t));
            lock_count = (long*)OPENSSL_malloc(CRYPTO_num_locks() * sizeof(long));
            for (int i = 0; i < CRYPTO_num_locks(); i++)
	        {
	            lock_count[i] = 0;
	            pthread_mutex_init(&(lock_cs[i]),NULL);
	        }

            CRYPTO_set_id_callback((unsigned long (*)())pthreads_thread_id);
            CRYPTO_set_locking_callback((void (*)(int, int, const char*, int))pthreads_locking_callback);
    #endif
        }
#ifdef _WIN32
        static void win32_locking_callback(int mode, int type, char *file, int line)
        {
            if (mode & CRYPTO_LOCK) WaitForSingleObject(lock_cs[type],INFINITE);
            else   	                ReleaseMutex(lock_cs[type]);
        }
#else
        static void pthreads_locking_callback(int mode, int type, char *file, int line)
        {
    	    if (mode & CRYPTO_LOCK)
		    {
		        pthread_mutex_lock(&(lock_cs[type]));
		        lock_count[type]++;
		    }
	        else
		    {
		        pthread_mutex_unlock(&(lock_cs[type]));
		    }
	    }
        static unsigned long pthreads_thread_id(void)
	    {
	        unsigned long ret;

	        ret = (unsigned long)pthread_self();
	        return(ret);
	    }

#endif
        ~MultiThreadProtection()
        {
#ifdef _WIN32
	        CRYPTO_set_locking_callback(NULL);
	        for (int i = 0; i < CRYPTO_num_locks(); i++)
		        CloseHandle(lock_cs[i]);
	        OPENSSL_free(lock_cs);
#else
	        CRYPTO_set_locking_callback(NULL);
	        for (int i = 0; i < CRYPTO_num_locks(); i++)
		        pthread_mutex_destroy(&(lock_cs[i]));
	        OPENSSL_free(lock_cs);
	        OPENSSL_free(lock_count);
#endif
        }
    };
#else
    #ifdef _WIN32
        static CRITICAL_SECTION * lock_cs = 0;
    #else
        static pthread_mutex_t * lock_cs = 0;
        static long * lock_count = 0;
    #endif



    struct MultiThreadProtection
    {
        MultiThreadProtection()
        {
    #ifdef _WIN32
	        lock_cs = (CRITICAL_SECTION*)OPENSSL_malloc(CRYPTO_num_locks() * sizeof(CRITICAL_SECTION));
	        for (int i = 0; i < CRYPTO_num_locks(); i++)
	        {
		        InitializeCriticalSection(&lock_cs[i]);
            }

    #else

            lock_cs = (pthread_mutex_t*)OPENSSL_malloc(CRYPTO_num_locks() * sizeof(pthread_mutex_t));
            lock_count = (long*)OPENSSL_malloc(CRYPTO_num_locks() * sizeof(long));
            for (int i = 0; i < CRYPTO_num_locks(); i++)
	        {
	            lock_count[i] = 0;
	            pthread_mutex_init(&(lock_cs[i]),NULL);
	        }
    #endif
	        CRYPTO_set_locking_callback((void (*)(int,int, const char *,int))locking_callback);
	        // Not required, the default implementation should work
            // CRYPTO_THREADID_set_callback(identifyThread);
        }
#ifdef _WIN32
        static void locking_callback(int mode, int type, char *file, int line)
        {
            if (mode & CRYPTO_LOCK) EnterCriticalSection(&lock_cs[type]);
            else   	                LeaveCriticalSection(&lock_cs[type]);
        }
#else
        static void locking_callback(int mode, int type, char *file, int line)
        {
    	    if (mode & CRYPTO_LOCK)
		    {
		        pthread_mutex_lock(&(lock_cs[type]));
		        lock_count[type]++;
		    }
	        else
		    {
		        pthread_mutex_unlock(&(lock_cs[type]));
		    }
	    }
#endif
        ~MultiThreadProtection()
        {
#ifdef _WIN32
	        CRYPTO_set_locking_callback(NULL);
	        for (int i = 0; i < CRYPTO_num_locks(); i++)
		        DeleteCriticalSection(&lock_cs[i]);
	        OPENSSL_free(lock_cs);
#else
	        CRYPTO_set_locking_callback(NULL);
	        for (int i = 0; i < CRYPTO_num_locks(); i++)
		        pthread_mutex_destroy(&(lock_cs[i]));
	        OPENSSL_free(lock_cs);
	        OPENSSL_free(lock_count);
#endif
        }
    };

#endif

    MultiThreadProtection & getMultiThreadProtection()
    {
        static MultiThreadProtection prot;
        return prot;
    }


    InitOpenSSL::InitOpenSSL()
    {
        char buf[32];
        Random::getDefaultGenerator().collectEntropy((uint8*)buf, sizeof(buf));
#ifdef _WIN32
//#pragma comment(lib, "cryptoeay32-0.9.8.lib")
#pragma comment(lib, "libeay32.lib")
#pragma comment(lib, "ssleay32.lib")

#endif

        OpenSSL_add_all_algorithms();
        ENGINE_register_all_complete();
        ERR_load_crypto_strings();

        SSL_library_init();
        SSL_load_error_strings();

//            ::RAND_seed(buf, sizeof(buf));
        (void)getMultiThreadProtection();
    }


    InitOpenSSL::~InitOpenSSL()
    {
        // Thread safe clean up
        ENGINE_cleanup();
        // Thread unsafe clean up
        ERR_free_strings();
        EVP_cleanup();
        // This is not clear if we need to call this or not (OpenSSL is soooo well documented).
        // So, as this is crashing under Windows, let's remove it
		// CRYPTO_cleanup_all_ex_data();
    }


    bool SSLContext::loadCertificate(const char* fullPath)
    {
        return SSL_CTX_load_verify_locations(context, fullPath, NULL) == 1;
    }

    namespace Private
    {
    #include "ca-bundle-asn1.inc"
    }

    SSLContext::SSLContext(const Protocol protocol)
        :context(SSL_CTX_new(protocol == Any ? SSLv23_method() :
#ifndef OPENSSL_NO_SSL3_METHOD
                            (protocol == TLSv1 ? TLSv1_method() :
  #ifndef OPENSSL_NO_SSL2
                            (protocol == SSLv3 ? SSLv3_method() : SSLv2_method()))))
  #else
                            SSLv3_method())))
  #endif
#else
                            TLSv1_method()))
#endif
    {
        // Load the root certificate store from Mozilla's version.
        // The ca-bundle-asn1.cpp is automatically generated from genBinCert.sh script
        const unsigned char * certPtr = Private::certStore; int len = Private::certStore_size;
        X509 * cert = 0;

        X509_STORE * store = SSL_CTX_get_cert_store(context);

        do
        {
            X509_free(cert); cert = NULL;

            cert = d2i_X509(&cert, &certPtr, len);
            if (!cert) break;
            len = Private::certStore_size - (int)(certPtr - Private::certStore);


            if (store)
            {
                X509_STORE_add_cert(store, cert);
                continue;
            }
//            if (!SSL_CTX_add_extra_chain_cert(context, cert))
//                continue; // Ignore this certificate and goes on to the next one

              // Might worth this for server mode context, but for now it's not used
//            if (!SSL_CTX_add_client_CA(context, cert))
//                continue;

        } while (cert != 0);

        X509_free(cert);
    }
    SSLContext::SSLContext(const char * rootCertificateBundlePath, const Protocol protocol)
        : context ( ::SSL_CTX_new( protocol == Any ? SSLv23_method() :
#ifndef OPENSSL_NO_SSL3_METHOD
                                (protocol == TLSv1 ? TLSv1_method() :
    #ifndef OPENSSL_NO_SSL2
                                (protocol == SSLv3 ? SSLv3_method() : SSLv2_method()))))
    #else
                                SSLv3_method())))
    #endif
#else
                            TLSv1_method()))
#endif
    {
        loadCertificate(rootCertificateBundlePath);
    }

    SSLContext::~SSLContext()
    {
        ::SSL_CTX_free(context); context = 0;
    }

    // Get the default context using any protocol method
    SSLContext & getDefaultSSLContext()
    {
        static SSLContext context;
        return context;
    }

}

