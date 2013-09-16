#ifndef hpp_CPP_BaseSecret_CPP_hpp
#define hpp_CPP_BaseSecret_CPP_hpp

namespace Crypto
{
    /** Base interface for transmitting a secret securely.
        All secret transmission algorithm implements this interface.
        The basic idea comes from Diffie Hellman.


        Usage is typically done this way:
           - Alice want to set up a secret with Bob
           - Alice knows Bob's public key
           - Alice generate a message computed from an ephemeral key pair and Bob's public key (please refer to StartSession)
           - Alice sends this message to Bob
           - Alice use the ephemeral public key as the secret
           - Bob, on his side use his private key to find out the secret too (please refer to EstablishSession)

        @warning Please notice that this is not safe in case of man-in-the-middle attack for Bob, as he can't assert Alice identity.
                 So don't rely on this secret transmission for full authentication, but instead also use a signature algorithm in Alice's side.
    */
    struct BaseSecret
    {
        /** The private key and "public" session key implement this interface */
        struct Key
        {
            /** Import the private key from a uint8 array (for loading it)
                @param array    The uint8 array of size (depends on selected number : All is 404 bytes)
                @param arrayLen The required array size in bytes
                @return true on success */
            virtual bool Import (const uint8 * array, const uint32 arrayLen) = 0;
            /** Export the private key as uint8 array (for storing it)
                @param array    The uint8 array of size (depends on selected number : All is 404 bytes)
                @param arrayLen The required array size in bytes
                @return true on success */
            virtual bool Export (uint8 * array, const uint32 arrayLen) const = 0;

            /** Get the required array size for a given mask */
            virtual uint32 getRequiredArraySize() const = 0;
            /** Destroy the key safely */
            virtual void Destroy() = 0;

            virtual ~Key() {}
        };


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
        virtual bool EstablishSession(const Key & privateKey, const uint8 * publicInfo, const uint32 publicInfoLen, const uint8 * message, const uint32 messageLen, uint8 * secret, const uint32 secretLen) const = 0;
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
        virtual bool StartSession(const Key & privateKey, const uint8 * publicInfo, const uint32 publicInfoLen, uint8 * message, const uint32 messageLen) const = 0;

        /** Generate a key pair.
            Public key is stored in the object, use getPublicKey to retrieve it.
            @param privateKey where to store the private key
            @return true on success */
        virtual bool GenerateKeys(Key & privateKey) = 0;

        /** Get the signature length in byte */
        virtual uint32 getSecretLength() const = 0;
        /** Get the message length in byte */
        virtual uint32 getMessageLength() const = 0;
        /** Get the public key */
        virtual const Key & getPublicKey() const = 0;
        /** Set the public key */
        virtual void setPublicKey(const Key & publicKey) = 0;

        /** Required destructor */
        virtual ~BaseSecret() {}
    };




}



#endif
