#ifndef hpp_CPP_BaseAsyncCrypt_CPP_hpp
#define hpp_CPP_BaseAsyncCrypt_CPP_hpp

namespace Crypto
{
    /** Base interface for signature and verification.
        All signature algorithm implements this interface. */
    struct BaseAsymCrypt
    {
        /** The private and public key implement this interface */
        struct Key
        {
            /** Import the private key from a uint8 array (for loading it)
                @param array        The uint8 array of size (depends on selected number : All is 404 bytes)
                @param arrayLen     The required array size in bytes
                @param publicKey    The public key to use for loading (can be 0 when importing a public key)
                @return true on success */
            virtual bool Import (const uint8 * array, const uint32 arrayLen, const Key * publicKey) = 0;
            /** Export the private key as uint8 array (for storing it)
                @param array        The uint8 array of size (depends on selected number : All is 404 bytes)
                @param arrayLen     The required array size in bytes
                @return true on success */
            virtual bool Export (uint8 * array, const uint32 arrayLen) const = 0;

            /** Get the required array size for a given mask */
            virtual uint32 getRequiredArraySize() const = 0;
            /** Destroy the key safely */
            virtual void Destroy() = 0;

            virtual ~Key() {}
        };


        /** Decrypt a ciphered message
            @param cipheredMessage  The message to decrypt
            @param cipheredLen      The message length in bytes
            @param message          On output, where to store the clear message
            @param messageLen       The message buffer size in bytes
            @param privateKey       The key used to decrypt the message
            @return true on success
            @warning You should delete your private key from memory as soon as it's not required anymore */
        virtual bool Decrypt(const uint8 * cipheredMessage, const uint32 cipheredLen, uint8 * message, const uint32 messageLen, const Key & privateKey) const = 0;
        /** Encrypt a message
            @param message          The clear message
            @param messageLen       The message length
            @param cipheredMessage  On output, the ciphered message
            @param cipheredLen      The ciphered message size in bytes
            @return true on success */
        virtual bool Encrypt(const uint8 * message, const uint32 messageLen, uint8 * cipheredMessage, const uint32 cipheredLen) const = 0;

        /** Generate a key pair.
            Public key is stored in the object, use getPublicKey to retrieve it.
            @param privateKey where to store the private key
            @return true on success */
        virtual bool Generate(Key & privateKey) = 0;

        /** Get the message length in byte */
        virtual uint32 getMessageLength() const = 0;
        /** Get the public key */
        virtual const Key & getPublicKey() const = 0;
        /** Set the public key */
        virtual void setPublicKey(const Key & publicKey) = 0;

        /** Required destructor */
        virtual ~BaseAsymCrypt() {}
    };


}



#endif
