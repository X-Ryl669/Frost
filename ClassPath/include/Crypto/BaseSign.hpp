#ifndef hpp_CPP_BaseSign_CPP_hpp
#define hpp_CPP_BaseSign_CPP_hpp

namespace Crypto
{
    /** Base interface for signature and verification.
        All signature algorithm implements this interface. */
    struct BaseSign
    {
        /** The private and public key implement this interface */
        struct Key
        {
            /** Import the private key from a uint8 array (for loading it)
                @param array    The uint8 array of size (depends on selected number : All is 404 bytes)
                @param arrayLen The required array size in bytes
                @param mask     Which number to load (signature system dependent)
                @return true on success */
            virtual bool Import (const uint8 * array, const uint32 arrayLen, const uint32 mask = 0xFFFFFFFF) = 0;
            /** Export the private key as uint8 array (for storing it)
                @param array    The uint8 array of size (depends on selected number : All is 404 bytes)
                @param arrayLen The required array size in bytes
                @param mask     Which number to load (signature system dependent)
                @return true on success */
            virtual bool Export (uint8 * array, const uint32 arrayLen, const uint32 mask = 0xFFFFFFFF) const = 0;

            /** Get the required array size for a given mask */
            virtual uint32 getRequiredArraySize(const uint32 mask = 0xFFFFFFFF) const = 0;
            /** Destroy the key safely */
            virtual void Destroy() = 0;

            virtual ~Key() {}
        };


        /** Verify a signed message
            @param message          The message to verify
            @param len              The message length in bytes
            @param signedMessage    A pointer to a valid x bytes buffer
            @param signedLen        The signature length in bytes
            @return true if match, false on error or doesn't match */
        virtual bool Verify(const uint8 * message, const uint32 len, const uint8 * signedMessage, const uint32 signedLen) const = 0;
        /** Sign a message with the given private key.
            @param message          Message to sign
            @param len              Message size (in bytes)
            @param signedMessage    A pointer to a valid bytes buffer
            @param signedLen        The signature length in bytes
            @param privateKey       The private key used to sign the message
            @return true on success
            @warning You should delete your key from memory as soon as it's not required anymore */
        virtual bool Sign(const uint8 * message, const uint32 len, uint8 * signedMessage, const uint32 signedLen, const Key & privateKey) const = 0;

        /** Generate a key pair.
            Public key is stored in the object, use getPublicKey to retrieve it.
            @param privateKey where to store the private key
            @return true on success */
        virtual bool Generate(Key & privateKey) = 0;

        /** Get the signature length in byte */
        virtual uint32 getSignatureLength() const = 0;
        /** Get the public key */
        virtual const Key & getPublicKey() const = 0;
        /** Set the public key */
        virtual void setPublicKey(const Key & publicKey) = 0;

        /** Required destructor */
        virtual ~BaseSign() {}
    };




}



#endif
