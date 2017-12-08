// We need thread code
#include "../../include/Threading/Threads.hpp"
// We need our declaration
#include "../../include/Crypto/OpenSSLWrap.hpp"

#if DebugSSLVersion
  #define StrIFy(X)   #X
  #define StrY(X) StrIFy(X)
  #pragma message "Built with OpenSSL version: " StrY(OPENSSL_VERSION_NUMBER)
#endif

#define OPENSSL_THREAD_DEFINES
#include <openssl/opensslconf.h>
#if !defined(OPENSSL_THREADS)
#error You must configure OpenSSL with threads support to use this library
#endif

// For safety reasons, we don't define anymore the SSLv2 crypto scheme. Only TLSv1 is supported for now
#define OPENSSL_NO_SSL2

namespace Crypto
{
#if (OPENSSL_VERSION_NUMBER < 0x10100000) || defined(LIBRESSL_VERSION_NUMBER)
    #ifdef _WIN32
        static HANDLE * lock_cs = 0;
    #else
        static pthread_mutex_t * lock_cs = 0;
        static long * lock_count = 0;
    #endif

    #define TLS_method TLSv1_method

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

#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)
        SSL_library_init();
#else
        OPENSSL_init_ssl(0, NULL);
#endif
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
                            (protocol == TLSv1 ? TLS_method() :
  #ifndef OPENSSL_NO_SSL2
                            (protocol == SSLv3 ? SSLv3_method() : SSLv2_method()))))
  #else
                            SSLv3_method())))
  #endif
#else
                            TLS_method()))
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
                                (protocol == TLSv1 ? TLS_method() :
    #ifndef OPENSSL_NO_SSL2
                                (protocol == SSLv3 ? SSLv3_method() : SSLv2_method()))))
    #else
                                SSLv3_method())))
    #endif
#else
                            TLS_method()))
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

// This glue code is required to support both OpenSSL version 1.1 and OpenSSL v1.0
#if (OPENSSL_VERSION_NUMBER < 0x10100000L) ||  defined(LIBRESSL_VERSION_NUMBER)
extern "C" {
 static void *OPENSSL_zalloc(size_t num)
 {
    void *ret = OPENSSL_malloc(num);

    if (ret != NULL)
        memset(ret, 0, num);
    return ret;
 }

 int RSA_set0_key(RSA *r, BIGNUM *n, BIGNUM *e, BIGNUM *d)
 {
    /* If the fields n and e in r are NULL, the corresponding input
     * parameters MUST be non-NULL for n and e.  d may be
     * left NULL (in case only the public key is used).
     */
    if ((r->n == NULL && n == NULL)
        || (r->e == NULL && e == NULL))
        return 0;

    if (n != NULL) {
        BN_free(r->n);
        r->n = n;
    }
    if (e != NULL) {
        BN_free(r->e);
        r->e = e;
    }
    if (d != NULL) {
        BN_free(r->d);
        r->d = d;
    }

    return 1;
 }

 int RSA_set0_factors(RSA *r, BIGNUM *p, BIGNUM *q)
 {
    /* If the fields p and q in r are NULL, the corresponding input
     * parameters MUST be non-NULL.
     */
    if ((r->p == NULL && p == NULL)
        || (r->q == NULL && q == NULL))
        return 0;

    if (p != NULL) {
        BN_free(r->p);
        r->p = p;
    }
    if (q != NULL) {
        BN_free(r->q);
        r->q = q;
    }

    return 1;
 }

 int RSA_set0_crt_params(RSA *r, BIGNUM *dmp1, BIGNUM *dmq1, BIGNUM *iqmp)
 {
    /* If the fields dmp1, dmq1 and iqmp in r are NULL, the corresponding input
     * parameters MUST be non-NULL.
     */
    if ((r->dmp1 == NULL && dmp1 == NULL)
        || (r->dmq1 == NULL && dmq1 == NULL)
        || (r->iqmp == NULL && iqmp == NULL))
        return 0;

    if (dmp1 != NULL) {
        BN_free(r->dmp1);
        r->dmp1 = dmp1;
    }
    if (dmq1 != NULL) {
        BN_free(r->dmq1);
        r->dmq1 = dmq1;
    }
    if (iqmp != NULL) {
        BN_free(r->iqmp);
        r->iqmp = iqmp;
    }

    return 1;
 }

 void RSA_get0_key(const RSA *r,
                  const BIGNUM **n, const BIGNUM **e, const BIGNUM **d)
 {
    if (n != NULL)
        *n = r->n;
    if (e != NULL)
        *e = r->e;
    if (d != NULL)
        *d = r->d;
 }

 void RSA_get0_factors(const RSA *r, const BIGNUM **p, const BIGNUM **q)
 {
    if (p != NULL)
        *p = r->p;
    if (q != NULL)
        *q = r->q;
 }

 void RSA_get0_crt_params(const RSA *r,
                         const BIGNUM **dmp1, const BIGNUM **dmq1,
                         const BIGNUM **iqmp)
 {
    if (dmp1 != NULL)
        *dmp1 = r->dmp1;
    if (dmq1 != NULL)
        *dmq1 = r->dmq1;
    if (iqmp != NULL)
        *iqmp = r->iqmp;
 }

 void DSA_get0_pqg(const DSA *d,
                  const BIGNUM **p, const BIGNUM **q, const BIGNUM **g)
 {
    if (p != NULL)
        *p = d->p;
    if (q != NULL)
        *q = d->q;
    if (g != NULL)
        *g = d->g;
 }

 int DSA_set0_pqg(DSA *d, BIGNUM *p, BIGNUM *q, BIGNUM *g)
 {
    /* If the fields p, q and g in d are NULL, the corresponding input
     * parameters MUST be non-NULL.
     */
    if ((d->p == NULL && p == NULL)
        || (d->q == NULL && q == NULL)
        || (d->g == NULL && g == NULL))
        return 0;

    if (p != NULL) {
        BN_free(d->p);
        d->p = p;
    }
    if (q != NULL) {
        BN_free(d->q);
        d->q = q;
    }
    if (g != NULL) {
        BN_free(d->g);
        d->g = g;
    }

    return 1;
 }

 void DSA_get0_key(const DSA *d,
                  const BIGNUM **pub_key, const BIGNUM **priv_key)
 {
    if (pub_key != NULL)
        *pub_key = d->pub_key;
    if (priv_key != NULL)
        *priv_key = d->priv_key;
 }

 int DSA_set0_key(DSA *d, BIGNUM *pub_key, BIGNUM *priv_key)
 {
    /* If the field pub_key in d is NULL, the corresponding input
     * parameters MUST be non-NULL.  The priv_key field may
     * be left NULL.
     */
    if (d->pub_key == NULL && pub_key == NULL)
        return 0;

    if (pub_key != NULL) {
        BN_free(d->pub_key);
        d->pub_key = pub_key;
    }
    if (priv_key != NULL) {
        BN_free(d->priv_key);
        d->priv_key = priv_key;
    }

    return 1;
 }

 void DSA_SIG_get0(const DSA_SIG *sig, const BIGNUM **pr, const BIGNUM **ps)
 {
    if (pr != NULL)
        *pr = sig->r;
    if (ps != NULL)
        *ps = sig->s;
 }

 int DSA_SIG_set0(DSA_SIG *sig, BIGNUM *r, BIGNUM *s)
 {
    if (r == NULL || s == NULL)
        return 0;
    BN_clear_free(sig->r);
    BN_clear_free(sig->s);
    sig->r = r;
    sig->s = s;
    return 1;
 }

 void ECDSA_SIG_get0(const ECDSA_SIG *sig, const BIGNUM **pr, const BIGNUM **ps)
 {
    if (pr != NULL)
        *pr = sig->r;
    if (ps != NULL)
        *ps = sig->s;
 }

 int ECDSA_SIG_set0(ECDSA_SIG *sig, BIGNUM *r, BIGNUM *s)
 {
    if (r == NULL || s == NULL)
        return 0;
    BN_clear_free(sig->r);
    BN_clear_free(sig->s);
    sig->r = r;
    sig->s = s;
    return 1;
 }

 void DH_get0_pqg(const DH *dh,
                 const BIGNUM **p, const BIGNUM **q, const BIGNUM **g)
 {
    if (p != NULL)
        *p = dh->p;
    if (q != NULL)
        *q = dh->q;
    if (g != NULL)
        *g = dh->g;
 }

 int DH_set0_pqg(DH *dh, BIGNUM *p, BIGNUM *q, BIGNUM *g)
 {
    /* If the fields p and g in d are NULL, the corresponding input
     * parameters MUST be non-NULL.  q may remain NULL.
     */
    if ((dh->p == NULL && p == NULL)
        || (dh->g == NULL && g == NULL))
        return 0;

    if (p != NULL) {
        BN_free(dh->p);
        dh->p = p;
    }
    if (q != NULL) {
        BN_free(dh->q);
        dh->q = q;
    }
    if (g != NULL) {
        BN_free(dh->g);
        dh->g = g;
    }

    if (q != NULL) {
        dh->length = BN_num_bits(q);
    }

    return 1;
 }

 void DH_get0_key(const DH *dh, const BIGNUM **pub_key, const BIGNUM **priv_key)
 {
    if (pub_key != NULL)
        *pub_key = dh->pub_key;
    if (priv_key != NULL)
        *priv_key = dh->priv_key;
 }

 int DH_set0_key(DH *dh, BIGNUM *pub_key, BIGNUM *priv_key)
 {
    /* If the field pub_key in dh is NULL, the corresponding input
     * parameters MUST be non-NULL.  The priv_key field may
     * be left NULL.
     */
    if (dh->pub_key == NULL && pub_key == NULL)
        return 0;

    if (pub_key != NULL) {
        BN_free(dh->pub_key);
        dh->pub_key = pub_key;
    }
    if (priv_key != NULL) {
        BN_free(dh->priv_key);
        dh->priv_key = priv_key;
    }

    return 1;
 }

 int DH_set_length(DH *dh, long length)
 {
    dh->length = length;
    return 1;
 }

 const unsigned char *EVP_CIPHER_CTX_iv(const EVP_CIPHER_CTX *ctx)
 {
    return ctx->iv;
 }

 unsigned char *EVP_CIPHER_CTX_iv_noconst(EVP_CIPHER_CTX *ctx)
 {
    return ctx->iv;
 }

 EVP_MD_CTX *EVP_MD_CTX_new(void)
 {
    return (EVP_MD_CTX *)OPENSSL_zalloc(sizeof(EVP_MD_CTX));
 }

 void EVP_MD_CTX_free(EVP_MD_CTX *ctx)
 {
    EVP_MD_CTX_cleanup(ctx);
    OPENSSL_free(ctx);
 }

 RSA_METHOD *RSA_meth_dup(const RSA_METHOD *meth)
 {
    RSA_METHOD *ret;

    ret = (RSA_METHOD*)OPENSSL_malloc(sizeof(RSA_METHOD));

    if (ret != NULL) {
        memcpy(ret, meth, sizeof(*meth));
        ret->name = OPENSSL_strdup(meth->name);
        if (ret->name == NULL) {
            OPENSSL_free(ret);
            return NULL;
        }
    }

    return ret;
 }

 int RSA_meth_set1_name(RSA_METHOD *meth, const char *name)
 {
    char *tmpname;

    tmpname = OPENSSL_strdup(name);
    if (tmpname == NULL) {
        return 0;
    }

    OPENSSL_free((char *)meth->name);
    meth->name = tmpname;

    return 1;
 }

 int RSA_meth_set_priv_enc(RSA_METHOD *meth,
                          int (*priv_enc) (int flen, const unsigned char *from,
                                           unsigned char *to, RSA *rsa,
                                           int padding))
 {
    meth->rsa_priv_enc = priv_enc;
    return 1;
 }

 int RSA_meth_set_priv_dec(RSA_METHOD *meth,
                          int (*priv_dec) (int flen, const unsigned char *from,
                                           unsigned char *to, RSA *rsa,
                                           int padding))
 {
    meth->rsa_priv_dec = priv_dec;
    return 1;
 }

 int RSA_meth_set_finish(RSA_METHOD *meth, int (*finish) (RSA *rsa))
 {
    meth->finish = finish;
    return 1;
 }

 void RSA_meth_free(RSA_METHOD *meth)
 {
    if (meth != NULL) {
        OPENSSL_free((char *)meth->name);
        OPENSSL_free(meth);
    }
 }

 int RSA_bits(const RSA *r)
 {
    return (BN_num_bits(r->n));
 }

 RSA *EVP_PKEY_get0_RSA(EVP_PKEY *pkey)
 {
    if (pkey->type != EVP_PKEY_RSA) {
        return NULL;
    }
    return pkey->pkey.rsa;
 }
}
#endif
