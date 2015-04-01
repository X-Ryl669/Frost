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
           - Alice generate a message computed from an ephemeral key pair and Bob's public key (please refer to startSession)
           - Alice sends this message to Bob
           - Alice use the generated secret from startSession
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
            Please check the startSession method for example code.

            @param privateKey       The private key used to decode the message
            @param message          The message that's received from the other party
            @param messageLen       The message length (in bytes)
            @param secret           The computed secret
            @param secretLen        The secret length (in bytes)
            @return true if match, false on error or doesn't match
            @sa startSession
            @warning Depending on algorithms, it's possible for the message to be empty. In that case, no transmission is required. */
        virtual bool establishSession(const Key & ourPrivateKey, const uint8 * message, const uint32 messageLen, uint8 * secret, const uint32 secretLen) const = 0;

        /** Start a DH session, the public info can be sent on the wire, it can't be used to find
            our private key and is only useful by the other party.
            You must load the other party's public key first and use it like this:
            @code
                BaseSecret & dh = ...;
                BaseSecret::Key & otherKey = ...get other side's public key...;
                dh.setPublicKey(otherKey);
                BaseSecretChild::PrivateKey ephemeralPrivKey; // Likely ECDH::PrivateKey
                Utils::MemoryBlock message(dh.getMessageLength());
                Utils::MemoryBlock secret(dh.getSecretLength());
                if (!dh.startSession(ephemeralPrivKey, message.getBuffer(), message.getSize(), secret.getBuffer(), secret.getSize())) return false;

                // Now you can use the secret on this side too
                // If you don't need the private key anymore, destroy it
                ephemeralPrivKey.Destroy();
                // Send the message to the other side
            @endcode
            On the other side, you'll do something like this:
            @code
                BaseSecret & dh = ...;
                BaseSecret::Key & privKey = ... load private key ...;
                Utils::MemoryBlock message = ... received from other side ...;
                Utils::MemoryBlock secret(dh.getSecretLength());
                if (!dh.establishSession(privKey, message.getConstBuffer(), message.getSize(), secret.getBuffer(), secret.getSize())) return false;

                // Now you can use the secret on this side too
                // If you don't need the private key anymore, destroy it
                privKey.Destroy();
            @endcode

            @param privateKey       On output, contains the ephemeral privateKey used to generate the message. You unlikely need this key anymore.
            @param message          The public information that can be send on the wire after starting the session.
            @param messageLen       The public info length (in bytes)
            @param secret           The secret that's generated from both party
            @param secretLen        The secret length (in bytes)
            @return true on success
            @warning You should delete your key from memory as soon as it's not required anymore
            @warning Depending on algorithms, it's possible for the message to be empty. In that case, no transmission is required. */
        virtual bool startSession(Key & privateKey, uint8 * message, const uint32 messageLen, uint8 * secret, const uint32 secretLen) const = 0;


        /** Generate a key pair.
            Public key is stored in the object, use getPublicKey to retrieve it.
            @param privateKey where to store the private key
            @return true on success */
        virtual bool generateKeys(Key & privateKey) = 0;

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
