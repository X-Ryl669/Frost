#ifndef hpp_CPP_Encode_CPP_hpp
#define hpp_CPP_Encode_CPP_hpp

// We need type
#include "../Types.hpp"

#if (WantBaseEncoding == 1)

/** Method required to en(de)code binary data to/from text.
    You'll likely use a Utils::MemoryBlock to avoid bothering about memory allocation. */
namespace Encoding
{
    /** Encode the given input binary buffer to Base85 suitable for XML storing.
        Typically, XML only allow 95 different chars anywhere in the file, and this encoding use those char
        to present XML valid binary data.
        Compared to Base64, the required space is, at max,  25% more than the binary data size.
        The following characters are used:
        @verbatim
        0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxy!#$()*+,-./:;=?@^`{|}~z_
        @endverbatim

        @param input        A pointer on a byte array
        @param inputLen     The input buffer size in bytes
        @param output       If non zero, and suitable size, will be filled with Base85 encoding of input
        @param outputLen    On input, the output buffer size. On output, the required/used size.
        @return true on successful encoding. You can call this method with output set to 0, it will then compute the required size. */
    bool encodeBase85(const uint8 * input, const uint32 inputLen, uint8 * output, uint32 & outputLen);

    /** Decode the given Base85 buffer into a binary buffer.
        Typically, XML only allow 95 different chars anywhere in the file, and this encoding use those char
        to present XML valid binary data.
        Compared to Base64, the required space is, at max, 25% more than the binary data size.

        The following characters are used:
        @verbatim
        0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxy!#$()*+,-./:;=?@^`{|}~z_
        @endverbatim

        @param input        A pointer on an Base85 encoded text
        @param inputLen     The input buffer size in bytes
        @param output       If non zero, and suitable size, will be filled with binary value
        @param outputLen    On input, the output buffer size. On output, the required/used size.
        @return true on successful decoding. You can call this method with output set to 0, it will then compute the required size. */
    bool decodeBase85(const uint8 * input, uint32 inputLen, uint8 * output, uint32 & outputLen);


    /** Encode the given input binary buffer to Base64 suitable for MIME/ASCII storing.
        Compared to Base85, this is more commonly found on the internet, but the required space
        is 33% more than the binary data size.

        @param input        A pointer on a byte array
        @param inputLen     The input buffer size in bytes
        @param output       If non zero, and suitable size, will be filled with Base85 encoding of input
        @param outputLen    On input, the output buffer size. On output, the required/used size.
        @return true on successful encoding. You can call this method with output set to 0, it will then compute the required size. */
    bool encodeBase64(const uint8 * input, const uint32 inputLen, uint8 * output, uint32 & outputLen);

    /** Decode the given Base64 buffer into a binary buffer.
        Compared to Base85, this is more commonly found on the internet, but the required space
        is 33% more than the binary data size (while Base85 only use 25% overhead).

        @param input        A pointer on an Base85 encoded text
        @param inputLen     The input buffer size in bytes
        @param output       If non zero, and suitable size, will be filled with binary value
        @param outputLen    On input, the output buffer size. On output, the required/used size.
        @return true on successful decoding. You can call this method with output set to 0, it will then compute the required size. */
    bool decodeBase64(const uint8 * input, uint32 inputLen, uint8 * output, uint32 & outputLen);


    /** Encode the given input binary buffer to Base16 suitable for MIME/ASCII storing.
        Base16 is also called hexadecimal.
        Compared to Base64, this is more commonly found on the internet, but the required space
        is 100% more than the binary data size.

        @param input        A pointer on a byte array
        @param inputLen     The input buffer size in bytes
        @param output       If non zero, and suitable size, will be filled with Base16 encoding of input
        @param outputLen    On input, the output buffer size. On output, the required/used size.
        @return true on successful encoding. You can call this method with output set to 0, it will then compute the required size. */
    bool encodeBase16(const uint8 * input, const uint32 inputLen, uint8 * output, uint32 & outputLen);

    /** Decode the given Base16 buffer into a binary buffer.
        Base16 is also called hexadecimal.
        Compared to Base64, this is more commonly found on the internet, but the required space
        is 100% more than the binary data size (while Base85 only use 25% overhead).

        @param input        A pointer on an Base16 encoded text
        @param inputLen     The input buffer size in bytes
        @param output       If non zero, and suitable size, will be filled with binary value
        @param outputLen    On input, the output buffer size. On output, the required/used size.
        @return true on successful decoding. You can call this method with output set to 0, it will then compute the required size. */
    bool decodeBase16(const uint8 * input, uint32 inputLen, uint8 * output, uint32 & outputLen);

}

#define HasBaseEncoding 1
#endif

#endif
