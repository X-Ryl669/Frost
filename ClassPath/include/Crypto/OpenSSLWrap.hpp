#ifndef hpp_CPP_OpenSSLWrap_CPP_hpp
#define hpp_CPP_OpenSSLWrap_CPP_hpp

// We need random too
#include "Random.hpp"
// We need BaseSign
#include "BaseSign.hpp"
// We need BaseAsymEncrypt
#include "BaseAsymCrypt.hpp"
// We need BaseSymEncrypt
#include "BaseSymCrypt.hpp"
// We need hashing too
#include "../Hashing/BaseHash.hpp"
// We need safe cleaning
#include "SafeMemclean.hpp"
// We need BaseSecret
#include "BaseSecret.hpp"
// We need the scope guards too
#include "../Utils/ScopeGuard.hpp"
// We need OpenSSL
#if defined(_MAC)
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
#include <openssl/bn.h>
#include <openssl/ssl.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/bio.h>
#include <openssl/sha.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/engine.h>


namespace Crypto
{
    struct MultiThreadProtection;
    MultiThreadProtection & getMultiThreadProtection();
    struct InitOpenSSL
    {
        InitOpenSSL();
        ~InitOpenSSL();
    };

    static InitOpenSSL autoregisterOpenSSL;
    
    /** SSL requires a context to handle the used SSL protocol, and certificates and keys */
    class SSLContext
    {
        SSL_CTX * context;
        
        // Type definition and enumeration
    public:
        /** The allowed protocols for SSL */
        enum Protocol
        {
            SSLv2     = 0, 
            SSLv3     = 1,
            TLSv1     = 2,   //!< This is also known as SSLv3.1
            Any       = 3,   //!< This is the default, to allow any type of protocol
        };
        
    private:
        /** Load one or multiple certificates from the given path.
            Certificate must be in PEM format, that means that the content of the pointed file should show at least one:
            @verbatim
            -----BEGIN CERTIFICATE-----
            ... (CA certificate in base64 encoding) ...
            -----END CERTIFICATE-----
            @endverbatim
            
            @param fullPath     The filesystem fullpath to the certificate file to load (in UTF-8)
            @return true if the certificate is valid and was loaded correctly */
        bool loadCertificate(const char * fullPath);
        
    public:
        /** @internal */
        inline operator SSL_CTX * () { return context; }
        /** Construct a SSL context, with the specified protocol.
            By default, a bundle of root certificate extracted from Mozilla's root store are loaded.
            You need the file "ca-bundle.crt" to exist in the source folder, as such file will be converted 
            in the process to a binary data added to the final binary executable. 
            @param protocol   Any of SSLv2, SSLv3, TLSv1 or Any (default) */
        SSLContext(const Protocol protocol = Any);
        /** Construct a SSL context, with the specified protocol, and the root bundle certificate
            to load. 
            @param rootCertificateBundlePath   A path on the filesystem to the root bundle certificate to load from (in UTF-8).
            @param protocol                    Any of SSLv2, SSLv3, TLSv1 or Any (default) */
        SSLContext(const char* rootCertificateBundlePath, const Protocol protocol = Any);
        ~SSLContext();
    };
    
    /** Get the default context using any protocol method, and the compile-time build root certificate store 
        extracted from Mozilla's source code. */
    SSLContext & getDefaultSSLContext();

    /** Inject the SSL structure in our namespace */
    typedef ::SSL SSL;

    /** SHA1 algorithm using OpenSSL */
    struct OSSL_SHA1 : public Hashing::Hasher
    {
    private:
        ::SHA_CTX c;

    public:
        /** The public size */
        enum { BlockSize = 64, DigestSize = 20 };

    public:
        /** Start the hashing */
        virtual void    Start() { ::SHA1_Init(&c); }
        /** Hash the given buffer */
        virtual void    Hash(const uint8 * buffer, uint32 size) { ::SHA1_Update(&c, buffer, size); }
        /** Finalize the hashing  and store the result */
        virtual void    Finalize(uint8 * outBuffer) { ::SHA1_Final((unsigned char*)outBuffer, &c); }
        /** Get the default hash size in byte */
        virtual uint32  hashSize() const throw() { return 20; }

    };

#if 0 // Considered unsafe now
    /** MD5 algorithm using OpenSSL */
    struct OSSL_MD5 : public Hashing::Hasher
    {
    private:
        ::MD5_CTX c;

    public:
        /** The public size */
        enum { BlockSize = 64, DigestSize = 16 };

    public:
        /** Start the hashing */
        virtual void    Start() { ::MD5_Init(&c); }
        /** Hash the given buffer */
        virtual void    Hash(const uint8 * buffer, uint32 size) { ::MD5_Update(&c, buffer, size); }
        /** Finalize the hashing  and store the result */
        virtual void    Finalize(uint8 * outBuffer) { ::MD5_Final((unsigned char*)outBuffer, &c); }
        /** Get the default hash size in byte */
        virtual uint32  hashSize() const throw() { return 16; }

        // This is expressively private, as MD5 is considered insecure (use SHA1 or 256 instead). If you really need to
        // use MD5, uncomment the following line
        // public:
        OSSL_MD5() {}

    };
#endif

    /** SHA256 algorithm using OpenSSL */
    struct OSSL_SHA256 : public Hashing::Hasher
    {
    private:
        ::SHA256_CTX c;

    public:
        /** The public size */
        enum { BlockSize = 64, DigestSize = 32 };

    public:
        /** Start the hashing */
        virtual void    Start() { ::SHA256_Init(&c); }
        /** Hash the given buffer */
        virtual void    Hash(const uint8 * buffer, uint32 size) { ::SHA256_Update(&c, buffer, size); }
        /** Finalize the hashing  and store the result */
        virtual void    Finalize(uint8 * outBuffer) { ::SHA256_Final((unsigned char*)outBuffer, &c); }
        /** Get the default hash size in byte */
        virtual uint32  hashSize() const throw() { return 32; }
    };

    /** AES algorithm using OpenSSL */
    struct OSSL_AES : public Crypto::BaseSymCrypt
    {
    private:
        EVP_CIPHER_CTX *        context;
        BlockSize               blockSize;
        uint8                   key[MaxBlockSize];
        uint8                   iv[MaxBlockSize];
        OperationMode           prevOpMode;
        bool                    previousEncrypt;


    public:
        /** Get the configured block size */
        virtual BlockSize getBlockSize() const { return blockSize; }
        /** Set the key from the given buffer
            @param key  The 128/192/256-bit user-key to use.
            @param keyLength    The length of the key in uint8
            @param chain        The initial chain block for CBC and CFB modes.
            @param blockSize    The expected block size in uint8s (16, 24 or 32 uint8s). */
        virtual void setKey(const uint8 * key, const BlockSize keyLength, const uint8 * chain, const BlockSize blockSize)
        {
            this->blockSize = blockSize;
            memset(this->key, 0, sizeof(this->key));
            memcpy(this->key, key, keyLength);
            memset(this->iv, 0, sizeof(this->iv));
            if (chain) memcpy(this->iv, chain, keyLength);
            Destroy();
        }

        /** Encrypt the given buffer
            @param in       The message of size n
            @param n        The message size in uint8 (should be a multiple of BlockSize)
            @param result   The cyphered message of size n (at least)
            @param mode     The operation mode
            @return false if the key isn't set up or if the input doesn't fit a block size */
        virtual bool Encrypt(const uint8* in, uint8* result, size_t n, const OperationMode & mode = ECB)
        {
            if (prevOpMode != mode || !previousEncrypt || !context)
            {
                Destroy();
                context = EVP_CIPHER_CTX_new();
                previousEncrypt = true;
                // Need to reset the context
                switch(blockSize)
                {
                case DefaultBlockSize:
                    // 128 bits
                    if (!::EVP_CipherInit_ex(context, mode == ECB ? ::EVP_aes_128_ecb() : (mode == CBC ? ::EVP_aes_128_cbc() : EVP_aes_128_cfb()), NULL, key, iv, previousEncrypt ? 1:0)) return false;
                    break;
                case MediumBlockSize:
                    // 192 bits
                    if (!::EVP_CipherInit_ex(context, mode == ECB ? ::EVP_aes_192_ecb() : (mode == CBC ? ::EVP_aes_192_cbc() : EVP_aes_192_cfb()), NULL, key, iv, previousEncrypt ? 1:0)) return false;
                    break;
                case MaxBlockSize:
                    // 256 bits
                    if (!::EVP_CipherInit_ex(context, mode == ECB ? ::EVP_aes_256_ecb() : (mode == CBC ? ::EVP_aes_256_cbc() : EVP_aes_256_cfb()), NULL, key, iv, previousEncrypt ? 1:0)) return false;
                    break;
                default:
                    return false;
                }
            }

            return EVP_Cipher(context, result, in, (unsigned int)n) != 0;
        }

        /** Decrypt the given buffer
            @param in The cyphered message of size n
            @param n The message size in uint8 (should be a multiple of BlockSize)
            @param result The clear message of size n (at least)
            @param mode The operation mode
            @return false if the key isn't set up or if the input doesn't fit a block size */
        virtual bool Decrypt(const uint8* in, uint8* result, size_t n, const OperationMode & mode = ECB)
        {
            if (prevOpMode != mode || previousEncrypt || !context)
            {
                Destroy();
                context = EVP_CIPHER_CTX_new();
                previousEncrypt = false;
                // Need to reset the context
                switch(blockSize)
                {
                case DefaultBlockSize:
                    // 128 bits
                    if (!::EVP_CipherInit_ex(context, mode == ECB ? ::EVP_aes_128_ecb() : (mode == CBC ? ::EVP_aes_128_cbc() : EVP_aes_128_cfb()), NULL, key, iv, previousEncrypt ? 1:0)) return false;
                    break;
                case MediumBlockSize:
                    // 192 bits
                    if (!::EVP_CipherInit_ex(context, mode == ECB ? ::EVP_aes_192_ecb() : (mode == CBC ? ::EVP_aes_192_cbc() : EVP_aes_192_cfb()), NULL, key, iv, previousEncrypt ? 1:0)) return false;
                    break;
                case MaxBlockSize:
                    // 256 bits
                    if (!::EVP_CipherInit_ex(context, mode == ECB ? ::EVP_aes_256_ecb() : (mode == CBC ? ::EVP_aes_256_cbc() : EVP_aes_256_cfb()), NULL, key, iv, previousEncrypt ? 1:0)) return false;
                    break;
                default:
                    return false;
                }
            }
            return EVP_Cipher(context, result, in, (unsigned int)n) != 0;
        }

        void Destroy() { if (context) ::EVP_CIPHER_CTX_free(context); context = 0; }
        OSSL_AES() : context(0), prevOpMode((OperationMode)-1), previousEncrypt(true) {}
        ~OSSL_AES() { Destroy(); }
    };

    /** RSA algorithm using OpenSSL */
    template <int size>
    struct OSSL_RSA : public BaseAsymCrypt
    {
    public:
        struct PrivateKey;
        /** The public key implement this interface */
        struct PublicKey : public BaseAsymCrypt::Key
        {
        private:
            /** The context */
            ::RSA * context;
        public:
            /** Import the private key from a uint8 array (for loading it) */
            virtual bool Import (const uint8 * array, const uint32 arrayLen, const Key * publicKey)
            {
                if (!array || arrayLen < getRequiredArraySize()) return false;
                Destroy();
                context = ::RSA_new();
                if (!context) return false;
                context->n = ::BN_bin2bn(&array[0 * size * 4], size * 4, NULL);
                context->e = ::BN_bin2bn(&array[1 * size * 4], size * 4, NULL);
                return true;
            }
            /** Export the private key as uint8 array (for storing it) */
            virtual bool Export (uint8 * array, const uint32 arrayLen) const
            {
                if (!array || arrayLen < getRequiredArraySize()) return false;
                if (!context) return false;
                if (BN_num_bytes(context->n) > size * 4) return false;
                if (BN_num_bytes(context->e) > size * 4) return false;
                memset(array, 0, arrayLen);
                ::BN_bn2bin(context->n, &array[1 * size * 4 - BN_num_bytes(context->n)]);
                // Find out the required size for the number
                ::BN_bn2bin(context->e, &array[2 * size * 4 - BN_num_bytes(context->e)]);
                return true;
            }

            /** Get the required array size for a given mask */
            virtual uint32 getRequiredArraySize() const
            {
                return size * 8;
            }
            /** Destroy the key safely */
            virtual void Destroy() { if (context) ::RSA_free(context); context = 0; }


            PublicKey() : context(0) {}
            ~PublicKey() { Destroy(); }

            /** Allow main object to access our internal data */
            friend struct OSSL_RSA<size>;
            friend struct PrivateKey;
        };

        /** The private and public key implement this interface */
        struct PrivateKey : public BaseAsymCrypt::Key
        {
        private:
            /** The context */
            ::RSA * context;
        public:
            /** Import the private key from a uint8 array (for loading it) */
            virtual bool Import (const uint8 * array, const uint32 arrayLen, const Key * publicKey)
            {
                if (!array || arrayLen < getRequiredArraySize() || !publicKey) return false;
                Destroy();

                context = ::RSAPublicKey_dup(((PublicKey*)publicKey)->context);
                if (!context) return false;

                context->d = ::BN_bin2bn(&array[0 * size * 4], size * 4, NULL);
                context->p = ::BN_bin2bn(&array[1 * size * 4], size * 2, NULL);
                context->q = ::BN_bin2bn(&array[1 * size * 4 + size * 2], size * 2, NULL);

                // Compute those value once for all
                context->dmp1 = ::BN_new();
                context->dmq1 = ::BN_new();
                context->iqmp = ::BN_new();
                BIGNUM * tmp = ::BN_new();
                BN_CTX * ctx = ::BN_CTX_new();
                ::BN_sub(tmp, context->p, ::BN_value_one());
                ::BN_mod(context->dmp1, context->d, tmp, ctx);

                ::BN_sub(tmp, context->q, ::BN_value_one());
                ::BN_mod(context->dmq1, context->d, tmp, ctx);

                ::BN_mod_inverse(context->iqmp, context->q, context->p, ctx);

                ::BN_free(tmp);
                ::BN_CTX_free(ctx);

                return ::RSA_check_key(context) == 1;
            }
            /** Export the private key as uint8 array (for storing it) */
            virtual bool Export (uint8 * array, const uint32 arrayLen) const
            {
                if (!array || arrayLen < getRequiredArraySize()) return false;
                if (!context) return false;
                if (BN_num_bytes(context->d) > size * 4) return false;
                if (BN_num_bytes(context->p) > size * 2) return false;
                if (BN_num_bytes(context->q) > size * 2) return false;
                memset(array, 0, arrayLen);
                ::BN_bn2bin(context->d, &array[1 * size * 4 - BN_num_bytes(context->d)]);
                ::BN_bn2bin(context->p, &array[1 * size * 4 + size * 2 - BN_num_bytes(context->p)]);
                ::BN_bn2bin(context->q, &array[2 * size * 4 - BN_num_bytes(context->q)]);
                return true;
            }

            /** Get the required array size for a given mask */
            virtual uint32 getRequiredArraySize() const { return size * 8; }
            /** Destroy the key safely */
            virtual void Destroy() { if (context) ::RSA_free(context); context = 0; }

            PrivateKey() : context(0) {}
            ~PrivateKey() { Destroy(); }

            friend struct OSSL_RSA<size>;
        };

    private:
        /** The public key object */
        PublicKey key;
    public:
        /** Decrypt a ciphered message */
        virtual bool Decrypt(const uint8 * cipheredMessage, const uint32 cipheredLen, uint8 * message, const uint32 messageLen, const Key & privateKey) const
        {
            // Can only decrypt a multiple of the internal number size
            if (cipheredLen % (size * 4)) return false;
            if (messageLen % (size * 4)) return false;
            uint32 pos = 0;
            PrivateKey & pkey = const_cast<PrivateKey &>((const PrivateKey&)privateKey);

            while (pos < cipheredLen)
            {
                if (::RSA_private_decrypt(size * 4, (const unsigned char*)&cipheredMessage[pos], (unsigned char *)&message[pos], pkey.context, RSA_NO_PADDING) < size * 4) return false;
                pos += size * 4;
            }
            return true;
        }
        /** Encrypt a message */
        virtual bool Encrypt(const uint8 * message, const uint32 messageLen, uint8 * cipheredMessage, const uint32 cipheredLen) const
        {
            if (cipheredLen % (size * 4)) return false;
            if (messageLen % (size * 4)) return false;
            uint32 pos = 0;
            while (pos < cipheredLen)
            {
                if (::RSA_public_encrypt(size * 4, (const unsigned char*)&message[pos], (unsigned char *)&cipheredMessage[pos], key.context, RSA_NO_PADDING) < size * 4) return false;
                pos += size * 4;
            }
            return true;
        }

        /** Generate a key pair. */
        virtual bool Generate(Key & privateKey)
        {
            PrivateKey & privKey = (PrivateKey&)privateKey;
            privKey.Destroy();
            privKey.context = ::RSA_generate_key(size * 32, 65537, NULL, NULL);
            if (!privKey.context) return false;

            key.Destroy();
            key.context = ::RSAPublicKey_dup(privKey.context);

            return ::RSA_check_key(privKey.context) == 1;
        }

        /** Get the message length in byte */
        virtual uint32 getMessageLength() const { return size * 4; }
        /** Get the public key */
        virtual const Key & getPublicKey() const { return key; }
        /** Set the public key */
        virtual void setPublicKey(const Key & publicKey)
        {
            const PublicKey & pubKey = (const PublicKey &)publicKey;
            key.Destroy();
            key.context = ::RSAPublicKey_dup(pubKey.context);
        }
    };

    /** RSA algorithm using OpenSSL for signature
        @param size The size in 32bits unit (32 for 1024bits, etc...) */
    template <int size>
    struct OSSL_RSASign : public BaseSign
    {
    public:
        /** The private key implement this interface */
        struct PrivateKey : public BaseSign::Key
        {
        private:
            ::RSA * context;

        public:
            /** Import the private key from a uint8 array (for loading it) */
            virtual bool Import (const uint8 * array, const uint32 arrayLen, const uint32 mask = 0xFFFFFFFF)
            {
                if (!array || arrayLen < getRequiredArraySize()) return false;
                Destroy();

                context = ::RSA_new();
                if (!context) return false;

                context->n = ::BN_bin2bn(&array[0 * size * 4], size * 4, NULL);
                context->e = ::BN_bin2bn(&array[1 * size * 4], size * 4, NULL);
                context->d = ::BN_bin2bn(&array[2 * size * 4], size * 4, NULL);
                context->p = ::BN_bin2bn(&array[3 * size * 4], size * 2, NULL);
                context->q = ::BN_bin2bn(&array[3 * size * 4 + size * 2], size * 2, NULL);

                // Compute those value once for all
                context->dmp1 = ::BN_new();
                context->dmq1 = ::BN_new();
                context->iqmp = ::BN_new();
                BIGNUM * tmp = ::BN_new();
                BN_CTX * ctx = ::BN_CTX_new();
                ::BN_sub(tmp, context->p, ::BN_value_one());
                ::BN_mod(context->dmp1, context->d, tmp, ctx);

                ::BN_sub(tmp, context->q, ::BN_value_one());
                ::BN_mod(context->dmq1, context->d, tmp, ctx);

                ::BN_mod_inverse(context->iqmp, context->q, context->p, ctx);

                ::BN_free(tmp);
                ::BN_CTX_free(ctx);

                return ::RSA_check_key(context) == 1;
            }
            /** Export the private key as uint8 array (for storing it) */
            virtual bool Export (uint8 * array, const uint32 arrayLen, const uint32 mask = 0xFFFFFFFF) const
            {
                if (!array || arrayLen < getRequiredArraySize()) return false;
                if (!context) return false;
                if (BN_num_bytes(context->n) > size * 4) return false;
                if (BN_num_bytes(context->e) > size * 4) return false;
                if (BN_num_bytes(context->d) > size * 4) return false;
                if (BN_num_bytes(context->p) > size * 2) return false;
                if (BN_num_bytes(context->q) > size * 2) return false;
                memset(array, 0, arrayLen);
                ::BN_bn2bin(context->n, &array[1 * size * 4 - BN_num_bytes(context->n)]);
                ::BN_bn2bin(context->e, &array[2 * size * 4 - BN_num_bytes(context->e)]);
                ::BN_bn2bin(context->d, &array[3 * size * 4 - BN_num_bytes(context->d)]);
                ::BN_bn2bin(context->p, &array[3 * size * 4 + size * 2 - BN_num_bytes(context->p)]);
                ::BN_bn2bin(context->q, &array[4 * size * 4 - BN_num_bytes(context->q)]);
                return true;
            }

            /** Get the required array size for a given mask */
            virtual uint32 getRequiredArraySize(const uint32 mask = 0xFFFFFFFF) const { return size * 16; }
            /** Destroy the key safely */
            virtual void Destroy() { if (context) ::RSA_free(context); context = 0; }

            PrivateKey() : context(0) {}
            ~PrivateKey() { Destroy(); }

            friend struct OSSL_RSASign<size>;
        };
        /** The public key implement this interface */
        struct PublicKey : public BaseSign::Key
        {
        private:
            ::RSA * context;

        public:
            /** Import the private key from a uint8 array (for loading it) */
            virtual bool Import (const uint8 * array, const uint32 arrayLen, const uint32 mask = 0xFFFFFFFF)
            {
                if (!array || arrayLen < getRequiredArraySize()) return false;
                Destroy();
                context = ::RSA_new();
                if (!context) return false;
                context->n = ::BN_bin2bn(&array[0 * size * 4], size * 4, NULL);
                context->e = ::BN_bin2bn(&array[1 * size * 4], size * 4, NULL);
                return true;
            }
            /** Export the private key as uint8 array (for storing it) */
            virtual bool Export (uint8 * array, const uint32 arrayLen, const uint32 mask = 0xFFFFFFFF) const
            {
                if (!array || arrayLen < getRequiredArraySize()) return false;
                if (!context) return false;
                if (BN_num_bytes(context->n) > size * 4) return false;
                if (BN_num_bytes(context->e) > size * 4) return false;
                memset(array, 0, arrayLen);
                ::BN_bn2bin(context->n, &array[1 * size * 4 - BN_num_bytes(context->n)]);
                // Find out the required size for the number
                ::BN_bn2bin(context->e, &array[2 * size * 4 - BN_num_bytes(context->e)]);
                return true;
            }

            /** Get the required array size for a given mask */
            virtual uint32 getRequiredArraySize(const uint32 mask = 0xFFFFFFFF) const { return size * 8; }
            /** Destroy the key safely */
            virtual void Destroy() { if (context) ::RSA_free(context); context = 0; }

            PublicKey() : context(0) {}
            ~PublicKey() { Destroy(); }

            friend struct OSSL_RSASign<size>;
        };

    private:
        /** The public key object */
        PublicKey key;
    public:
        /** Verify a signed message */
        virtual bool Verify(const uint8 * message, const uint32 len, const uint8 * signedMessage, const uint32 signedLen) const
        {
            if (!message || !signedMessage) return false;
            // Need to digest the message first
            uint8 digest[32] = {0};
            OSSL_SHA1 sha1;
            sha1.Start();
            sha1.Hash(message, len);
            sha1.Finalize(digest);

            ::RSA * context = const_cast<PublicKey&>(key).context;
            return ::RSA_verify(NID_sha1, digest, sha1.hashSize(), const_cast<uint8*>(signedMessage), signedLen, context) == 1;
        }
        /** Sign a message with the given private key. */
        virtual bool Sign(const uint8 * message, const uint32 len, uint8 * signedMessage, const uint32 signedLen, const Key & privateKey) const
        {
            const PrivateKey & privKey = (const PrivateKey &)privateKey;
            if (!message || !signedMessage) return false;
            // Need to digest the message first
            uint8 digest[32] = {0};
            OSSL_SHA1 sha1;
            sha1.Start();
            sha1.Hash(message, len);
            sha1.Finalize(digest);

            unsigned int length = signedLen;
            ::RSA * context = const_cast<PrivateKey&>(privKey).context;
            return ::RSA_sign(NID_sha1, digest, sha1.hashSize(), signedMessage, &length, context) == 1 && length <= signedLen;
        }

        /** Generate a key pair. */
        virtual bool Generate(Key & privateKey)
        {
            PrivateKey & privKey = (PrivateKey&)privateKey;
            privKey.Destroy();
            privKey.context = ::RSA_generate_key(size * 32, 65537, NULL, NULL);
            if (!privKey.context) return false;

            key.Destroy();
            key.context = ::RSAPublicKey_dup(privKey.context);
            return ::RSA_check_key(privKey.context) == 1;
        }

        /** Get the signature length in byte */
        virtual uint32 getSignatureLength() const { return size * 4; }
        /** Get the public key */
        virtual const Key & getPublicKey() const  { return key; }
        /** Set the public key */
        virtual void setPublicKey(const Key & publicKey)
        {
            const PublicKey & pubKey = (const PublicKey &)publicKey;
            key.Destroy();
            key.context = ::RSAPublicKey_dup(pubKey.context);
        }
    };

    // If compilation breaks here, it's because you're not using a supported Elliptic Curve group
    // Please use either: NID_secp160r2, NID_secp192k1, NID_secp224k1, NID_secp256k1, NID_secp384r1
    template <int group>
        struct EC_KeySize { };

    template <> struct EC_KeySize<NID_secp160r2> { enum { Size = 20, IsPrime = 1 }; };
    template <> struct EC_KeySize<NID_secp192k1> { enum { Size = 24, IsPrime = 1 }; };
    template <> struct EC_KeySize<NID_secp224k1> { enum { Size = 28, IsPrime = 1 }; };
    template <> struct EC_KeySize<NID_secp224r1> { enum { Size = 28, IsPrime = 1 }; };
    template <> struct EC_KeySize<NID_secp256k1> { enum { Size = 32, IsPrime = 1 }; };
    template <> struct EC_KeySize<NID_secp384r1> { enum { Size = 48, IsPrime = 1 }; };


    // If compilation breaks here, it's because you must use a supported symmetric crypto size
    // Please use either: 128, 192, 256
    template <int baseSizeInBits>
        struct MatchingHash {};

    template <> struct MatchingHash<128> { typedef OSSL_SHA1 HashT; };
    template <> struct MatchingHash<192> { typedef OSSL_SHA256 HashT; };
    template <> struct MatchingHash<256> { typedef OSSL_SHA256 HashT; };

    /** Wrapper for OpenSSL ECDSA signing
        @param group Any of NID_secp160r2, NID_secp192k1, NID_secp224k1 */
    template <int group = NID_secp224k1>
    struct OSSL_ECDSA : public BaseSign
    {
    public:
        /** The private key implement this interface */
        struct PrivateKey : public BaseSign::Key
        {
        private:
            ::EC_KEY    * context;

        public:
            /** Import the private key from a uint8 array (for loading it) */
            virtual bool Import (const uint8 * array, const uint32 arrayLen, const uint32 mask = 0xFFFFFFFF)
            {
                if (!array || arrayLen < getRequiredArraySize()) return false;
                Destroy();

                context = ::EC_KEY_new_by_curve_name(group);
                if (!context) return false;

                BIGNUM * kx = 0, * ky = 0, * sec = 0;
                const EC_GROUP * usedGroup = ::EC_KEY_get0_group(context);
                EC_POINT * kxy = ::EC_POINT_new(usedGroup); CleanUpOnScopeExit(::EC_POINT_free, kxy);
                if (!kxy) { return false; }

                kx = ::BN_new(); CleanUpOnScopeExit(::BN_free, kx);
                if (!kx) { return false; }
                ky = ::BN_new(); CleanUpOnScopeExit(::BN_free, ky);
                if (!ky) { return false; }
                sec = ::BN_new(); CleanUpOnScopeExit(::BN_free, sec);
                if (!sec) { return false; }


                kx = ::BN_bin2bn(&array[0 * EC_KeySize<group>::Size], EC_KeySize<group>::Size, kx);
                ky = ::BN_bin2bn(&array[1 * EC_KeySize<group>::Size], EC_KeySize<group>::Size, ky);

                sec = ::BN_bin2bn(&array[2 * EC_KeySize<group>::Size], EC_KeySize<group>::Size, sec);
                if (!::EC_KEY_set_private_key(context, sec)) return false;

                if (EC_KeySize<group>::IsPrime)
                {
                    if (!::EC_POINT_set_affine_coordinates_GFp(usedGroup, kxy, kx, ky, 0)) return false;
                } else
                {
                    if (!::EC_POINT_set_affine_coordinates_GF2m(usedGroup, kxy, kx, ky, 0)) return false;
                }
                if (!::EC_KEY_set_public_key(context, kxy)) return false;

                return ::EC_KEY_check_key(context) == 1;
            }
            /** Export the private key as uint8 array (for storing it) */
            virtual bool Export (uint8 * array, const uint32 arrayLen, const uint32 mask = 0xFFFFFFFF) const
            {
                if (!array || arrayLen < getRequiredArraySize()) return false;
                if (!context) return false;
                memset(array, 0, arrayLen);

                BIGNUM * kx = 0, * ky = 0;
                const EC_GROUP * usedGroup = ::EC_KEY_get0_group(context);
                if (BN_num_bytes(::EC_KEY_get0_private_key(context)) > EC_KeySize<group>::Size) return false;

                kx = ::BN_new(); CleanUpOnScopeExit(::BN_free, kx);
                if (!kx) { return false; }
                ky = ::BN_new(); CleanUpOnScopeExit(::BN_free, ky);
                if (!ky) { return false; }

                if (EC_KeySize<group>::IsPrime)
                {
                    if (!::EC_POINT_get_affine_coordinates_GFp(usedGroup, ::EC_KEY_get0_public_key(context), kx, ky, 0)) return false;
                } else
                {
                    if (!::EC_POINT_get_affine_coordinates_GF2m(usedGroup, ::EC_KEY_get0_public_key(context), kx, ky, 0)) return false;
                }
                if (BN_num_bytes(kx) > EC_KeySize<group>::Size || BN_num_bytes(ky) > EC_KeySize<group>::Size) return false;

                ::BN_bn2bin(kx, &array[1*EC_KeySize<group>::Size - BN_num_bytes(kx)]);
                ::BN_bn2bin(ky, &array[2*EC_KeySize<group>::Size - BN_num_bytes(ky)]);
                ::BN_bn2bin(::EC_KEY_get0_private_key(context), &array[3*EC_KeySize<group>::Size - BN_num_bytes(::EC_KEY_get0_private_key(context))]);

                return true;
            }

            /** Get the required array size for a given mask */
            virtual uint32 getRequiredArraySize(const uint32 mask = 0xFFFFFFFF) const { return EC_KeySize<group>::Size * 3; }
            /** Destroy the key safely */
            virtual void Destroy() { if (context) ::EC_KEY_free(context); context = 0; }

            PrivateKey() : context(0) {}
            ~PrivateKey() { Destroy(); }

            friend struct OSSL_ECDSA<group>;
        };
        /** The public key implement this interface */
        struct PublicKey : public BaseSign::Key
        {
        private:
            ::EC_KEY    * context;

        public:
            /** Import the private key from a uint8 array (for loading it) */
            virtual bool Import (const uint8 * array, const uint32 arrayLen, const uint32 mask = 0xFFFFFFFF)
            {
                if (!array || arrayLen < getRequiredArraySize()) return false;
                Destroy();

                context = ::EC_KEY_new_by_curve_name(group);
                if (!context) return false;

                BIGNUM * kx = 0, * ky = 0;
                const EC_GROUP * usedGroup = ::EC_KEY_get0_group(context);
                EC_POINT * kxy = ::EC_POINT_new(usedGroup); CleanUpOnScopeExit(::EC_POINT_free, kxy);
                if (!kxy) { return false; }

                kx = ::BN_new(); CleanUpOnScopeExit(::BN_free, kx);
                if (!kx) { return false; }
                ky = ::BN_new(); CleanUpOnScopeExit(::BN_free, ky);
                if (!ky) { return false; }

                kx = ::BN_bin2bn(&array[0 * EC_KeySize<group>::Size], EC_KeySize<group>::Size, kx);
                ky = ::BN_bin2bn(&array[1 * EC_KeySize<group>::Size], EC_KeySize<group>::Size, ky);

                if (EC_KeySize<group>::IsPrime)
                {
                    if (!::EC_POINT_set_affine_coordinates_GFp(usedGroup, kxy, kx, ky, 0)) return false;
                } else
                {
                    if (!::EC_POINT_set_affine_coordinates_GF2m(usedGroup, kxy, kx, ky, 0)) return false;
                }
                if (!::EC_KEY_set_public_key(context, kxy)) return false;

                return ::EC_KEY_check_key(context) == 1;
            }
            /** Export the private key as uint8 array (for storing it) */
            virtual bool Export (uint8 * array, const uint32 arrayLen, const uint32 mask = 0xFFFFFFFF) const
            {
                if (!array || arrayLen < getRequiredArraySize()) return false;
                if (!context) return false;
                memset(array, 0, arrayLen);

                BIGNUM * kx = 0, * ky = 0;
                const EC_GROUP * usedGroup = ::EC_KEY_get0_group(context);

                kx = ::BN_new(); CleanUpOnScopeExit(::BN_free, kx);
                ky = ::BN_new(); CleanUpOnScopeExit(::BN_free, ky);

                if (EC_KeySize<group>::IsPrime)
                {
                    if (!::EC_POINT_get_affine_coordinates_GFp(usedGroup, ::EC_KEY_get0_public_key(context), kx, ky, 0)) return false;
                } else
                {
                    if (!::EC_POINT_get_affine_coordinates_GF2m(usedGroup, ::EC_KEY_get0_public_key(context), kx, ky, 0)) return false;
                }
                if (BN_num_bytes(kx) > EC_KeySize<group>::Size || BN_num_bytes(ky) > EC_KeySize<group>::Size) return false;

                ::BN_bn2bin(kx, &array[EC_KeySize<group>::Size - BN_num_bytes(kx)]);
                ::BN_bn2bin(ky, &array[2*EC_KeySize<group>::Size - BN_num_bytes(ky)]);

                return true;
            }

            /** Get the required array size for a given mask
                If compilation breaks here, you must use a supported group */
            virtual uint32 getRequiredArraySize(const uint32 mask = 0xFFFFFFFF) const { return EC_KeySize<group>::Size * 2; }
            /** Destroy the key safely */
            virtual void Destroy() { if (context) ::EC_KEY_free(context); context = 0; }

            PublicKey() : context(0) {}
            ~PublicKey() { Destroy(); }

            friend struct OSSL_ECDSA<group>;
        };

    private:
        /** The public key object */
        PublicKey   key;

    public:
        /** Verify a signed message */
        virtual bool Verify(const uint8 * message, const uint32 len, const uint8 * signedMessage, const uint32 signedLen) const
        {
            if (!message || !signedMessage || signedLen < EC_KeySize<group>::Size * 2 || !const_cast<PublicKey&>(key).context) return false;
            // Need to digest the message first
            uint8 digest[32] = {0};
            OSSL_SHA1 sha1;
            sha1.Start();
            sha1.Hash(message, len);
            sha1.Finalize(digest);

            ::ECDSA_SIG * sig = ::ECDSA_SIG_new();
            CleanUpOnScopeExit(::ECDSA_SIG_free, sig);
            sig->r = ::BN_bin2bn(&signedMessage[0 * EC_KeySize<group>::Size], EC_KeySize<group>::Size, sig->r);
            sig->s = ::BN_bin2bn(&signedMessage[1 * EC_KeySize<group>::Size], EC_KeySize<group>::Size, sig->s);

            return ::ECDSA_do_verify(digest, sha1.hashSize(), sig, const_cast<PublicKey&>(key).context) == 1;
        }
        /** Sign a message with the given private key. */
        virtual bool Sign(const uint8 * message, const uint32 len, uint8 * signedMessage, const uint32 signedLen, const Key & privateKey) const
        {
            const PrivateKey & privKey = (const PrivateKey &)privateKey;
            if (!message || !signedMessage || signedLen < EC_KeySize<group>::Size * 2) return false;
            // Need to digest the message first
            uint8 digest[32] = {0};
            OSSL_SHA1 sha1;
            sha1.Start();
            sha1.Hash(message, len);
            sha1.Finalize(digest);

            ::ECDSA_SIG * sig = ::ECDSA_do_sign(digest, sha1.hashSize(), const_cast<PrivateKey&>(privKey).context);
            if (sig == NULL) return false;
            // Need to copy the numbers in the signed message
            memset(signedMessage, 0, signedLen);
            ::BN_bn2bin(sig->r, &signedMessage[EC_KeySize<group>::Size - BN_num_bytes(sig->r)]);
            ::BN_bn2bin(sig->s, &signedMessage[2*EC_KeySize<group>::Size - BN_num_bytes(sig->s)]);
            ::ECDSA_SIG_free(sig);
            return true;
        }

        /** Generate a key pair. */
        virtual bool Generate(Key & privateKey)
        {
            PrivateKey & privKey = (PrivateKey&)privateKey;
            privKey.Destroy();

            privKey.context = ::EC_KEY_new_by_curve_name(group);
            if (!privKey.context) return false;
            if (::EC_KEY_generate_key(privKey.context) != 1) return false;

            key.Destroy();
            key.context = ::EC_KEY_new_by_curve_name(group);
            if (!key.context) return false;
            if (!::EC_KEY_set_public_key(key.context, ::EC_KEY_get0_public_key(privKey.context))) return false;
            return ::EC_KEY_check_key(privKey.context) == 1;
        }
        /** Get the signature length in byte
            If compilation breaks here, it's because you've used an unsupported group space */
        virtual uint32 getSignatureLength() const { return EC_KeySize<group>::Size * 2; }
        /** Get the public key */
        virtual const Key & getPublicKey() const  { return key; }
        /** Set the public key */
        virtual void setPublicKey(const Key & publicKey)
        {
            const PublicKey & pubKey = (const PublicKey &)publicKey;
            key.Destroy();
            key.context = ::EC_KEY_dup(pubKey.context);
        }
    };

    /** The OpenSSL Elliptic curve Diffie Hellman algorithm */
    template <int group = NID_secp224k1>
    struct OSSL_ECDH : public BaseSecret
    {
        /** The private key implement this interface */
        struct PrivateKey : public BaseSecret::Key
        {
        private:
            ::EC_KEY    * context;

        public:
            /** Import the private key from a uint8 array (for loading it) */
            virtual bool Import (const uint8 * array, const uint32 arrayLen)
            {
                if (!array || arrayLen < getRequiredArraySize()) return false;
                Destroy();

                context = ::EC_KEY_new_by_curve_name(group);
                if (!context) return false;

                BIGNUM * kx = 0, * ky = 0, * sec = 0;
                const EC_GROUP * usedGroup = ::EC_KEY_get0_group(context);
                EC_POINT * kxy = ::EC_POINT_new(usedGroup); CleanUpOnScopeExit(::EC_POINT_free, kxy);
                if (!kxy) { return false; }

                kx = ::BN_new(); CleanUpOnScopeExit(::BN_free, kx);
                if (!kx) { return false; }
                ky = ::BN_new(); CleanUpOnScopeExit(::BN_free, ky);
                if (!ky) { return false; }
                sec = ::BN_new(); CleanUpOnScopeExit(::BN_free, sec);
                if (!sec) { return false; }



                kx = ::BN_bin2bn(&array[0 * EC_KeySize<group>::Size], EC_KeySize<group>::Size, kx);
                ky = ::BN_bin2bn(&array[1 * EC_KeySize<group>::Size], EC_KeySize<group>::Size, ky);

                sec = ::BN_bin2bn(&array[2 * EC_KeySize<group>::Size], EC_KeySize<group>::Size, sec);
                if (!::EC_KEY_set_private_key(context, sec)) return false;

                if (EC_KeySize<group>::IsPrime)
                {
                    if (!::EC_POINT_set_affine_coordinates_GFp(usedGroup, kxy, kx, ky, 0)) return false;
                } else
                {
                    if (!::EC_POINT_set_affine_coordinates_GF2m(usedGroup, kxy, kx, ky, 0)) return false;
                }
                if (!::EC_KEY_set_public_key(context, kxy)) return false;

                return ::EC_KEY_check_key(context) == 1;
            }
            /** Export the private key as uint8 array (for storing it) */
            virtual bool Export (uint8 * array, const uint32 arrayLen) const
            {
                if (!array || arrayLen < getRequiredArraySize()) return false;
                if (!context) return false;
                memset(array, 0, arrayLen);

                BIGNUM * kx = 0, * ky = 0;
                const EC_GROUP * usedGroup = ::EC_KEY_get0_group(context);
                if (BN_num_bytes(::EC_KEY_get0_private_key(context)) > EC_KeySize<group>::Size) return false;

                kx = ::BN_new(); CleanUpOnScopeExit(::BN_free, kx);
                if (!kx) { return false; }
                ky = ::BN_new(); CleanUpOnScopeExit(::BN_free, ky);
                if (!ky) { return false; }

                if (EC_KeySize<group>::IsPrime)
                {
                    if (!::EC_POINT_get_affine_coordinates_GFp(usedGroup, ::EC_KEY_get0_public_key(context), kx, ky, 0)) return false;
                } else
                {
                    if (!::EC_POINT_get_affine_coordinates_GF2m(usedGroup, ::EC_KEY_get0_public_key(context), kx, ky, 0)) return false;
                }
                if (BN_num_bytes(kx) > EC_KeySize<group>::Size || BN_num_bytes(ky) > EC_KeySize<group>::Size) return false;

                ::BN_bn2bin(kx, &array[1*EC_KeySize<group>::Size - BN_num_bytes(kx)]);
                ::BN_bn2bin(ky, &array[2*EC_KeySize<group>::Size - BN_num_bytes(ky)]);
                ::BN_bn2bin(::EC_KEY_get0_private_key(context), &array[3*EC_KeySize<group>::Size - BN_num_bytes(::EC_KEY_get0_private_key(context))]);

                return true;
            }

            /** Get the required array size for a given mask */
            virtual uint32 getRequiredArraySize() const { return EC_KeySize<group>::Size * 3; }
            /** Destroy the key safely */
            virtual void Destroy() { if (context) ::EC_KEY_free(context); context = 0; }

            PrivateKey() : context(0) {}
            ~PrivateKey() { Destroy(); }

            friend struct OSSL_ECDH<group>;
        };
        /** The public key implement this interface */
        struct PublicKey : public BaseSecret::Key
        {
        private:
            ::EC_KEY    * context;

        public:
            /** Import the public key from a uint8 array (for loading it) */
            virtual bool Import (const uint8 * array, const uint32 arrayLen)
            {
                if (!array || arrayLen < getRequiredArraySize()) return false;
                Destroy();

                context = ::EC_KEY_new_by_curve_name(group);
                if (!context) return false;

                BIGNUM * kx = 0, * ky = 0;
                const EC_GROUP * usedGroup = ::EC_KEY_get0_group(context);
                EC_POINT * kxy = ::EC_POINT_new(usedGroup); CleanUpOnScopeExit(::EC_POINT_free, kxy);
                if (!kxy) { return false; }

                kx = ::BN_new(); CleanUpOnScopeExit(::BN_free, kx);
                if (!kx) { return false; }
                ky = ::BN_new(); CleanUpOnScopeExit(::BN_free, ky);
                if (!ky) { return false; }

                kx = ::BN_bin2bn(&array[0 * EC_KeySize<group>::Size], EC_KeySize<group>::Size, kx);
                ky = ::BN_bin2bn(&array[1 * EC_KeySize<group>::Size], EC_KeySize<group>::Size, ky);

                if (EC_KeySize<group>::IsPrime)
                {
                    if (!::EC_POINT_set_affine_coordinates_GFp(usedGroup, kxy, kx, ky, 0)) return false;
                } else
                {
                    if (!::EC_POINT_set_affine_coordinates_GF2m(usedGroup, kxy, kx, ky, 0)) return false;
                }
                if (!::EC_KEY_set_public_key(context, kxy)) return false;

                return ::EC_KEY_check_key(context) == 1;
            }
            /** Export the public key as uint8 array (for storing it) */
            virtual bool Export (uint8 * array, const uint32 arrayLen) const
            {
                if (!array || arrayLen < getRequiredArraySize()) return false;
                if (!context) return false;
                memset(array, 0, arrayLen);

                BIGNUM * kx = 0, * ky = 0;
                const EC_GROUP * usedGroup = ::EC_KEY_get0_group(context);

                kx = ::BN_new(); CleanUpOnScopeExit(::BN_free, kx);
                ky = ::BN_new(); CleanUpOnScopeExit(::BN_free, ky);

                if (EC_KeySize<group>::IsPrime)
                {
                    if (!::EC_POINT_get_affine_coordinates_GFp(usedGroup, ::EC_KEY_get0_public_key(context), kx, ky, 0)) return false;
                } else
                {
                    if (!::EC_POINT_get_affine_coordinates_GF2m(usedGroup, ::EC_KEY_get0_public_key(context), kx, ky, 0)) return false;
                }
                if (BN_num_bytes(kx) > EC_KeySize<group>::Size || BN_num_bytes(ky) > EC_KeySize<group>::Size) return false;

                ::BN_bn2bin(kx, &array[1*EC_KeySize<group>::Size - BN_num_bytes(kx)]);
                ::BN_bn2bin(ky, &array[2*EC_KeySize<group>::Size - BN_num_bytes(ky)]);

                return true;
            }

            /** Get the required array size for a given mask
                If compilation breaks here, you must use a supported group */
            virtual uint32 getRequiredArraySize() const { return EC_KeySize<group>::Size * 2; }
            /** Destroy the key safely */
            virtual void Destroy() { if (context) ::EC_KEY_free(context); context = 0; }

            PublicKey() : context(0) {}
            ~PublicKey() { Destroy(); }

            friend struct OSSL_ECDH<group>;
        };

    private:
        /** The public key object */
        PublicKey   key;
    public:
        /** Establish the DH session.
            @param privateKey       The private key used to generate the message
            @param publicInfo       The public information that should have been agreed before committing
            @param publicInfoLen    The public info length (in bytes)
            @param message          The message that can be sent to the other party
            @param messageLen       The message length (in bytes)
            @param secret           The computed secret
            @param secretLen        The secret length (in bytes)
            @return true if match, false on error or doesn't match
            @warning Depending on algorithms, it's possible for the message to be empty. In that case, no transmission is required. */
        virtual bool EstablishSession(const Key & privateKey, const uint8 * publicInfo, const uint32 publicInfoLen, const uint8 * message, const uint32 messageLen, uint8 * secret, const uint32 secretLen) const
        {
            if (!message || messageLen < getMessageLength()) return false;
            // The idea with elliptic curve is to compute
            // Message = ECDH(privateKey, publicInfo)

            PublicKey pubKey;
            if (!pubKey.Import(message, messageLen)) return false;
            const PrivateKey & privKey = (const PrivateKey&)privateKey;

            return ::ECDH_compute_key(secret, secretLen, ::EC_KEY_get0_public_key(pubKey.context), const_cast<PrivateKey&>(privKey).context, ourKDF) > 0;
        }
        /** Start a DH session, the message can be sent on the wire, it can't be used to find
            our private key and is only useful by the other party.
            @param privateKey       The private key used to generate the message
            @param publicInfo       The public information that should have been agreed before committing
            @param publicInfoLen    The public info length (in bytes)
            @param message          The message that can be sent to the other party
            @param messageLen       The message length (in bytes)
            @return true on success
            @warning You should delete your key from memory as soon as it's not required anymore
            @warning Depending on algorithms, it's possible for the message to be empty. In that case, no transmission is required. */
        virtual bool StartSession(const Key & privateKey, const uint8 * publicInfo, const uint32 publicInfoLen, uint8 * message, const uint32 messageLen) const
        {
            if (!message || messageLen < getMessageLength() || !publicInfo || publicInfoLen < key.getRequiredArraySize()) return false;
            // The idea with elliptic curve is to compute
            // Message = ECDH(privateKey, publicInfo)

            PublicKey pubKey;
            if (!pubKey.Import(publicInfo, publicInfoLen)) return false;
            const PrivateKey & privKey = (const PrivateKey&)privateKey;

            return ::ECDH_compute_key(message, messageLen, ::EC_KEY_get0_public_key(pubKey.context), const_cast<PrivateKey&>(privKey).context, ourKDF) > 0;
        }

        // Required to derive our key
        static void * ourKDF(const void *in, size_t inlen, void *out, size_t *outlen)
        {
            if (*outlen < 32) return 0;
            else *outlen = 32;
            return ::SHA256((const uint8*)in, inlen, (uint8*)out);
        }


        /** Generate a key pair.
            Public key is stored in the object, use getPublicKey to retrieve it.
            @param privateKey where to store the private key
            @return true on success */
        virtual bool GenerateKeys(Key & privateKey)
        {
            PrivateKey & privKey = (PrivateKey&)privateKey;
            privKey.Destroy();

            privKey.context = ::EC_KEY_new_by_curve_name(group);
            if (!privKey.context) return false;
            if (::EC_KEY_generate_key(privKey.context) != 1) return false;

            key.Destroy();
            key.context = ::EC_KEY_new_by_curve_name(group);
            if (!key.context) return false;
            if (!::EC_KEY_set_public_key(key.context, ::EC_KEY_get0_public_key(privKey.context))) return false;
            return ::EC_KEY_check_key(privKey.context) == 1;
        }

        /** Get the signature length in byte */
        virtual uint32 getSecretLength() const { return 32; }
        /** Get the message length in byte */
        virtual uint32 getMessageLength() const { return 32; }
        /** Get the public key */
        virtual const Key & getPublicKey() const  { return key; }
        /** Set the public key */
        virtual void setPublicKey(const Key & publicKey)
        {
            const PublicKey & pubKey = (const PublicKey &)publicKey;
            key.Destroy();
            key.context = ::EC_KEY_dup(pubKey.context);
        }
    };

    /** Elliptic curve encryption system using OpenSSL.
        This is supposed to be safer than discrete logarithm based encryption (DH, RSA).
        This use ECDH internally to generate a secret between both participant,
        and AES to encrypt the data with this secret, and SHA-1 to ensure message integrity.

    */
    template <int group = NID_secp224k1, class SymCrypt = OSSL_AES, const int symSizeInBit = 128>
    struct OSSL_ECIES : public BaseAsymCrypt
    {
        /** The private key implement this interface */
        struct PrivateKey : public BaseAsymCrypt::Key
        {
        private:
            ::EC_KEY    * context;

        public:
            /** Import the private key from a uint8 array (for loading it) */
            virtual bool Import (const uint8 * array, const uint32 arrayLen, const Key * publicKey)
            {
                if (!array || arrayLen < getRequiredArraySize()) return false;
                Destroy();

                context = ::EC_KEY_new_by_curve_name(group);
                if (!context) return false;

                BIGNUM * kx = 0, * ky = 0, * sec = 0;
                const EC_GROUP * usedGroup = ::EC_KEY_get0_group(context);
                EC_POINT * kxy = ::EC_POINT_new(usedGroup); CleanUpOnScopeExit(::EC_POINT_free, kxy);
                if (!kxy) { return false; }

                kx = ::BN_new(); CleanUpOnScopeExit(::BN_free, kx);
                if (!kx) { return false; }
                ky = ::BN_new(); CleanUpOnScopeExit(::BN_free, ky);
                if (!ky) { return false; }
                sec = ::BN_new(); CleanUpOnScopeExit(::BN_free, sec);
                if (!sec) { return false; }



                kx = ::BN_bin2bn(&array[0 * EC_KeySize<group>::Size], EC_KeySize<group>::Size, kx);
                ky = ::BN_bin2bn(&array[1 * EC_KeySize<group>::Size], EC_KeySize<group>::Size, ky);

                sec = ::BN_bin2bn(&array[2 * EC_KeySize<group>::Size], EC_KeySize<group>::Size, sec);
                if (!::EC_KEY_set_private_key(context, sec)) return false;

                if (EC_KeySize<group>::IsPrime)
                {
                    if (!::EC_POINT_set_affine_coordinates_GFp(usedGroup, kxy, kx, ky, 0)) return false;
                } else
                {
                    if (!::EC_POINT_set_affine_coordinates_GF2m(usedGroup, kxy, kx, ky, 0)) return false;
                }
                if (!::EC_KEY_set_public_key(context, kxy)) return false;

                return ::EC_KEY_check_key(context) == 1;
            }
            /** Export the private key as uint8 array (for storing it) */
            virtual bool Export (uint8 * array, const uint32 arrayLen) const
            {
                if (!array || arrayLen < getRequiredArraySize()) return false;
                if (!context) return false;
                memset(array, 0, arrayLen);

                BIGNUM * kx = 0, * ky = 0;
                const EC_GROUP * usedGroup = ::EC_KEY_get0_group(context);
                if (BN_num_bytes(::EC_KEY_get0_private_key(context)) > EC_KeySize<group>::Size) return false;

                kx = ::BN_new(); CleanUpOnScopeExit(::BN_free, kx);
                if (!kx) { return false; }
                ky = ::BN_new(); CleanUpOnScopeExit(::BN_free, ky);
                if (!ky) { return false; }

                if (EC_KeySize<group>::IsPrime)
                {
                    if (!::EC_POINT_get_affine_coordinates_GFp(usedGroup, ::EC_KEY_get0_public_key(context), kx, ky, 0)) return false;
                } else
                {
                    if (!::EC_POINT_get_affine_coordinates_GF2m(usedGroup, ::EC_KEY_get0_public_key(context), kx, ky, 0)) return false;
                }
                if (BN_num_bytes(kx) > EC_KeySize<group>::Size || BN_num_bytes(ky) > EC_KeySize<group>::Size) return false;

                ::BN_bn2bin(kx, &array[1*EC_KeySize<group>::Size - BN_num_bytes(kx)]);
                ::BN_bn2bin(ky, &array[2*EC_KeySize<group>::Size - BN_num_bytes(ky)]);
                ::BN_bn2bin(::EC_KEY_get0_private_key(context), &array[3*EC_KeySize<group>::Size - BN_num_bytes(::EC_KEY_get0_private_key(context))]);

                return true;
            }

            /** Get the required array size for a given mask */
            virtual uint32 getRequiredArraySize() const { return EC_KeySize<group>::Size * 3; }
            /** Destroy the key safely */
            virtual void Destroy() { if (context) ::EC_KEY_free(context); context = 0; }

            PrivateKey() : context(0) {}
            ~PrivateKey() { Destroy(); }

            friend struct OSSL_ECIES<group>;
        };
        /** The public key implement this interface */
        struct PublicKey : public BaseAsymCrypt::Key
        {
        private:
            ::EC_KEY    * context;

        public:
            /** Import the public key from a uint8 array (for loading it) */
            virtual bool Import (const uint8 * array, const uint32 arrayLen, const Key * publicKey)
            {
                if (!array || arrayLen < getRequiredArraySize()) return false;
                Destroy();

                context = ::EC_KEY_new_by_curve_name(group);
                if (!context) return false;

                BIGNUM * kx = 0, * ky = 0;
                const EC_GROUP * usedGroup = ::EC_KEY_get0_group(context);
                EC_POINT * kxy = ::EC_POINT_new(usedGroup); CleanUpOnScopeExit(::EC_POINT_free, kxy);
                if (!kxy) { return false; }

                kx = ::BN_new(); CleanUpOnScopeExit(::BN_free, kx);
                if (!kx) { return false; }
                ky = ::BN_new(); CleanUpOnScopeExit(::BN_free, ky);
                if (!ky) { return false; }

                kx = ::BN_bin2bn(&array[0 * EC_KeySize<group>::Size], EC_KeySize<group>::Size, kx);
                ky = ::BN_bin2bn(&array[1 * EC_KeySize<group>::Size], EC_KeySize<group>::Size, ky);

                if (EC_KeySize<group>::IsPrime)
                {
                    if (!::EC_POINT_set_affine_coordinates_GFp(usedGroup, kxy, kx, ky, 0)) return false;
                } else
                {
                    if (!::EC_POINT_set_affine_coordinates_GF2m(usedGroup, kxy, kx, ky, 0)) return false;
                }
                if (!::EC_KEY_set_public_key(context, kxy)) return false;

                return ::EC_KEY_check_key(context) == 1;
            }
            /** Export the public key as uint8 array (for storing it) */
            virtual bool Export (uint8 * array, const uint32 arrayLen) const
            {
                if (!array || arrayLen < getRequiredArraySize()) return false;
                if (!context) return false;
                memset(array, 0, arrayLen);

                BIGNUM * kx = 0, * ky = 0;
                const EC_GROUP * usedGroup = ::EC_KEY_get0_group(context);

                kx = ::BN_new(); CleanUpOnScopeExit(::BN_free, kx);
                ky = ::BN_new(); CleanUpOnScopeExit(::BN_free, ky);

                if (EC_KeySize<group>::IsPrime)
                {
                    if (!::EC_POINT_get_affine_coordinates_GFp(usedGroup, ::EC_KEY_get0_public_key(context), kx, ky, 0)) return false;
                } else
                {
                    if (!::EC_POINT_get_affine_coordinates_GF2m(usedGroup, ::EC_KEY_get0_public_key(context), kx, ky, 0)) return false;
                }
                if (BN_num_bytes(kx) > EC_KeySize<group>::Size || BN_num_bytes(ky) > EC_KeySize<group>::Size) return false;

                ::BN_bn2bin(kx, &array[1*EC_KeySize<group>::Size - BN_num_bytes(kx)]);
                ::BN_bn2bin(ky, &array[2*EC_KeySize<group>::Size - BN_num_bytes(ky)]);

                return true;
            }

            /** Get the required array size for a given mask
                If compilation breaks here, you must use a supported group */
            virtual uint32 getRequiredArraySize() const { return EC_KeySize<group>::Size * 2; }
            /** Destroy the key safely */
            virtual void Destroy() { if (context) ::EC_KEY_free(context); context = 0; }

            PublicKey() : context(0) {}
            ~PublicKey() { Destroy(); }

            friend struct OSSL_ECIES<group>;
        };
    private:
        /** The public key object */
        PublicKey key;

        /** Symmetric encryption with arbitrary length */
        bool symCrypt(SymCrypt & crypt, const uint8 * message, const uint32 messageLen, uint8 * cipher) const
        {
            const uint32 symSize = symSizeInBit / 8;
            uint32 blockCount = (messageLen + symSize - 1) / symSize; // Ceil
            uint32 fullBlockCount = messageLen / symSize; // Floor
            uint32 i = 0;
            for (i = 0; i < fullBlockCount; i++)
            {
                if (!crypt.Encrypt(&message[i * symSize], &cipher[i * symSize], symSize, SymCrypt::CFB)) return false;
            }
            for (; i < blockCount; i++)
            {
                uint8 inputBuffer[symSize];
                memset(inputBuffer, 0, sizeof(inputBuffer));
                memcpy(inputBuffer, &message[i * symSize], messageLen - i * symSize);

                uint8 localBuffer[symSize];
                memset(localBuffer, 0, sizeof(inputBuffer));

                if (!crypt.Encrypt(inputBuffer, localBuffer, symSize, SymCrypt::CFB)) return false;
                memcpy(&cipher[i * symSize], localBuffer,  messageLen - i * symSize);
            }
            return true;
        }

        /** Symmetric decryption with arbitrary length */
        bool symDecrypt(SymCrypt & crypt, const uint8 * cipher, const uint32 cipherLen, uint8 * message) const
        {
            const uint32 symSize = symSizeInBit / 8;
            uint32 blockCount = (cipherLen + symSize - 1) / symSize; // Ceil
            uint32 fullBlockCount = cipherLen / symSize; // Floor
            uint32 i = 0;
            for (i = 0; i < fullBlockCount; i++)
            {
                if (!crypt.Decrypt(&cipher[i * symSize], &message[i * symSize], symSize, SymCrypt::CFB)) return false;
            }
            for (; i < blockCount; i++)
            {
                uint8 inputBuffer[symSize];
                memset(inputBuffer, 0, sizeof(inputBuffer));
                memcpy(inputBuffer, &cipher[i * symSize], cipherLen - i * symSize);

                uint8 localBuffer[symSize];
                memset(localBuffer, 0, sizeof(inputBuffer));

                if (!crypt.Decrypt(inputBuffer, localBuffer, symSize, SymCrypt::CFB)) return false;
                memcpy(&message[i * symSize], localBuffer,  cipherLen - i * symSize);
            }
            return true;
        }

    public:
        /** Decrypt a ciphered message */
        virtual bool Decrypt(const uint8 * cipheredMessage, const uint32 cipheredLen, uint8 * message, const uint32 messageLen, const Key & privateKey) const
        {
            uint32 messageSize = getMessageLength(cipheredLen);
            if (!cipheredMessage || !message  || messageLen < messageSize || cipheredLen < getMessageLength()) return false;

            // Extract the ephemeral key
            PublicKey ephemPub;
            if (!ephemPub.Import(cipheredMessage, ephemPub.getRequiredArraySize(), 0)) return false;

            // Then use our private key to extract the shared secret
            const PrivateKey & privKey = (const PrivateKey&)privateKey;
            typedef typename MatchingHash<symSizeInBit>::HashT HashT;
            const int hashDigest = HashT::DigestSize;
            const int symSize = symSizeInBit / 8;
            uint8 keyBuffer[hashDigest];
            if (::ECDH_compute_key(keyBuffer, sizeof(keyBuffer),
                                    ::EC_KEY_get0_public_key(ephemPub.context),
                                    const_cast<PrivateKey&>(privKey).context, ourKDF) != sizeof(keyBuffer)) return false;

            // Then try to decrypt now
            // Now that we have our secret, let's derive 2 keys for encrypting and MAC
            uint8 K[symSize * 2] = {0};
            // We use CBC mode for symmetric crypto
            uint8 chain[symSize] = {0};
            // Let's derive the key to fit the symmetric and HMAC required key size
            Hashing::KDF1<symSize * 16, hashDigest * 8, OSSL_SHA1> kdf;
            kdf.Start(); kdf.Hash(keyBuffer, hashDigest);
            kdf.Finalize(K); // K now contains Ke || Km
            // Clean the keyBuffer now
            SafeClean(keyBuffer);

            // Check the HMAC now
            uint8 Km[HashT::DigestSize] = {0};
            Hashing::HMAC<HashT> hmac(&K[symSize], symSize);
            hmac.Start();
            hmac.Hash(&cipheredMessage[ephemPub.getRequiredArraySize()], messageSize);
            hmac.Finalize(Km);
            // Does the MAC match ?
            if (memcmp(Km, &cipheredMessage[cipheredLen - HashT::DigestSize], sizeof(Km)) != 0) return false;
            // Clear the MAC now
            SafeClean(Km);

            // Ok, seems to work, let's decrypt now
            SymCrypt crypt;
            crypt.setKey(K, (typename SymCrypt::BlockSize)symSize, chain, (typename SymCrypt::BlockSize)symSize);
            return symDecrypt(crypt, &cipheredMessage[ephemPub.getRequiredArraySize()], messageSize, message);
        }

        /** Encrypt a message */
        virtual bool Encrypt(const uint8 * message, const uint32 messageLen, uint8 * cipheredMessage, const uint32 cipheredLen) const
        {
            uint32 cipherSize = getCiphertextLength(messageLen);
            if (!cipheredMessage || !message  || !messageLen || cipheredLen < cipherSize) return false;

            // Ok, let's create an ephemeral key (that is choose r in [0; n-1] at random)
            PrivateKey ephemKey;
            ephemKey.context = ::EC_KEY_new_by_curve_name(group);
            if (!ephemKey.context) return false;
            if (::EC_KEY_generate_key(ephemKey.context) != 1) return false;
            // We need to export the public key we've used (that is R = r * Elliptic_Group)
            PublicKey ephemPub;
            ephemPub.context = ::EC_KEY_new_by_curve_name(group);
            if (!ephemPub.context) return false;
            if (!::EC_KEY_set_public_key(ephemPub.context, ::EC_KEY_get0_public_key(ephemKey.context))) return false;

            // Write the first part: public ephem key
            if (!ephemPub.Export(cipheredMessage, ephemPub.getRequiredArraySize())) return false;

            // Ok, now, let's compute a shared secret with the recipient public key
            typedef typename MatchingHash<symSizeInBit>::HashT HashT;
            const int hashDigest = HashT::DigestSize;
            const int symSize = symSizeInBit / 8;
            uint8 keyBuffer[hashDigest];
            if (::ECDH_compute_key(keyBuffer, sizeof(keyBuffer),
                                    ::EC_KEY_get0_public_key(key.context),
                                    const_cast<PrivateKey&>(ephemKey).context, ourKDF) != sizeof(keyBuffer)) return false;

            // Clean the private key now (it's not safe)
            ephemKey.Destroy();

            // Now that we have our secret, let's derive 2 keys for encrypting and MAC
            uint8 K[symSize * 2] = {0};
            // We use CBC mode for symmetric crypto
            uint8 chain[symSize] = {0};
            // Let's derive the key to fit the symmetric and HMAC required key size
            Hashing::KDF1<symSize * 16, hashDigest * 8, OSSL_SHA1> kdf;
            kdf.Start(); kdf.Hash(keyBuffer, hashDigest);
            kdf.Finalize(K); // K now contains Ke || Km
            // Clean the keyBuffer now
            SafeClean(keyBuffer);

            // Encrypt the input message in Cm
            SymCrypt crypt;
            crypt.setKey(K, (typename SymCrypt::BlockSize)symSize, chain, (typename SymCrypt::BlockSize)symSize);
            if (!symCrypt(crypt, message, messageLen, &cipheredMessage[ephemPub.getRequiredArraySize()])) return false;

            // Compute the HMAC now
            Hashing::HMAC<HashT> hmac(&K[symSize], symSize);
            hmac.Start();
            hmac.Hash(&cipheredMessage[ephemPub.getRequiredArraySize()], messageLen);
            hmac.Finalize(&cipheredMessage[cipherSize - HashT::DigestSize]);

            // Ok, it's a success
            return true;
        }

        // Required to derive our key
        static void * ourKDF(const void *in, size_t inlen, void *out, size_t *outlen)
        {
            if (symSizeInBit == 128)
            {
                if (*outlen < 20) return 0;
                else *outlen = 20;
                return ::SHA1((const uint8*)in, inlen, (uint8*)out);
            }
            else
            {
                if (*outlen < 32) return 0;
                else *outlen = 32;
                return ::SHA256((const uint8*)in, inlen, (uint8*)out);
            }
        }

        /** Generate a key pair. */
        virtual bool Generate(Key & privateKey)
        {
            PrivateKey & privKey = (PrivateKey&)privateKey;
            privKey.Destroy();

            privKey.context = ::EC_KEY_new_by_curve_name(group);
            if (!privKey.context) return false;
            if (::EC_KEY_generate_key(privKey.context) != 1) return false;

            key.Destroy();
            key.context = ::EC_KEY_new_by_curve_name(group);
            if (!key.context) return false;
            if (!::EC_KEY_set_public_key(key.context, ::EC_KEY_get0_public_key(privKey.context))) return false;
            return ::EC_KEY_check_key(privKey.context) == 1;
        }

        /** Get the message length in byte
            @warning This has no sense for ECIES, so you better should call getCiphertextLength */
        virtual uint32 getMessageLength() const { return getCiphertextLength(0); }
        /** Get the message length from the ciphertext length
            @warning This isn't the same as the getMessageLength base method  */
        virtual uint32 getMessageLength(const uint32 ciphertextLength) const
        {
            typedef typename MatchingHash<symSizeInBit>::HashT HashT;
            if (ciphertextLength < key.getRequiredArraySize() + HashT::DigestSize) return 0;
            // Because we use CFB, we don't have to round up the message length size to the symmetric algorithm block size
            return ciphertextLength - (key.getRequiredArraySize() + HashT::DigestSize);
        }
        /** Get the expected ciphertext length for the given message length */
        virtual uint32 getCiphertextLength(const uint32 messageLength) const
        {
            typedef typename MatchingHash<symSizeInBit>::HashT HashT;
            // Because we use CFB, we don't have to round up the message length size to the symmetric algorithm block size
            return key.getRequiredArraySize() + messageLength + HashT::DigestSize;
        }

        /** Get the public key */
        virtual const Key & getPublicKey() const { return key; }
        /** Set the public key */
        virtual void setPublicKey(const Key & publicKey)
        {
            const PublicKey & pubKey = (const PublicKey &)publicKey;
            key.Destroy();
            key.context = ::EC_KEY_dup(pubKey.context);
        }
    };



}



#endif
