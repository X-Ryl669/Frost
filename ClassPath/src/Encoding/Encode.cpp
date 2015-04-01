// We need our declaration
#include "../../include/Encoding/Encode.hpp"

#if (HasBaseEncoding == 1)
namespace Encoding
{
    static const char enc85[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxy!#$()*+,-./:;=?@^`{|}~z_";
    static const char enc85Lead[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxy!#$()*+,-./:;=?@^`{|}~_z";
    static const uint8 dec85[256] =
    {
        0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,
        0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,
        0xFF,  0x3D,  0xFF,  0x3E,  0x3F,  0xFF,  0xFF,  0xFF,  0x40,  0x41,  0x42,  0x43,  0x44,  0x45,  0x46,  0x47,
        0x00,  0x01,  0x02,  0x03,  0x04,  0x05,  0x06,  0x07,  0x08,  0x09,  0x48,  0x49,  0xFF,  0x4A,  0xFF,  0x4B,
        0x4C,  0x0A,  0x0B,  0x0C,  0x0D,  0x0E,  0x0F,  0x10,  0x11,  0x12,  0x13,  0x14,  0x15,  0x16,  0x17,  0x18,
        0x19,  0x1A,  0x1B,  0x1C,  0x1D,  0x1E,  0x1F,  0x20,  0x21,  0x22,  0x23,  0xFF,  0xFF,  0xFF,  0x4D,  0x54,
        0x4E,  0x24,  0x25,  0x26,  0x27,  0x28,  0x29,  0x2A,  0x2B,  0x2C,  0x2D,  0x2E,  0x2F,  0x30,  0x31,  0x32,
        0x33,  0x34,  0x35,  0x36,  0x37,  0x38,  0x39,  0x3A,  0x3B,  0x3C,  0x53,  0x4F,  0x50,  0x51,  0x52,  0xFF,
        0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,
        0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,
        0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,
        0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,
        0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,
        0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,
        0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,
        0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,
    };
    static const uint8 dec85Lead[256] =
    {
        0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,
        0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,
        0xFF,  0x3D,  0xFF,  0x3E,  0x3F,  0xFF,  0xFF,  0xFF,  0x40,  0x41,  0x42,  0x43,  0x44,  0x45,  0x46,  0x47,
        0x00,  0x01,  0x02,  0x03,  0x04,  0x05,  0x06,  0x07,  0x08,  0x09,  0x48,  0x49,  0xFF,  0x4A,  0xFF,  0x4B,
        0x4C,  0x0A,  0x0B,  0x0C,  0x0D,  0x0E,  0x0F,  0x10,  0x11,  0x12,  0x13,  0x14,  0x15,  0x16,  0x17,  0x18,
        0x19,  0x1A,  0x1B,  0x1C,  0x1D,  0x1E,  0x1F,  0x20,  0x21,  0x22,  0x23,  0xFF,  0xFF,  0xFF,  0x4D,  0x53,
        0x4E,  0x24,  0x25,  0x26,  0x27,  0x28,  0x29,  0x2A,  0x2B,  0x2C,  0x2D,  0x2E,  0x2F,  0x30,  0x31,  0x32,
        0x33,  0x34,  0x35,  0x36,  0x37,  0x38,  0x39,  0x3A,  0x3B,  0x3C,  0x54,  0x4F,  0x50,  0x51,  0x52,  0xFF,
        0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,
        0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,
        0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,
        0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,
        0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,
        0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,
        0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,
        0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,  0xFF,
    };

    bool encodeBase85(const uint8 * input, const uint32 inputLen, uint8 * output, uint32 & outputLen)
    {
        if (!input || !inputLen) return false;
        if (!output)
        {
            outputLen = (inputLen * 5 + 3) / 4;
            return true;
        }
        // First pass, handle octet 4 by 4
        uint32 i = 0, j = 0, k = 0;
        uint32 remainders[5] = {0};
        uint32 current = 0, val = 0;
        for (; i < (inputLen / 4); i++)
        {
            val = htonl(*(uint32 *)&input[i * 4]);
            if (val == 0) { if (j + 1 > outputLen) return false; output[j++] = (uint8)'z'; continue; }

            remainders[0] = val % 84;
            current = val / 84;
            remainders[1] = current % 85;
            current /= 85;
            remainders[2] = current % 85;
            current /= 85;
            remainders[3] = current % 85;
            current /= 85;
            remainders[4] = current % 85;

            if (j + 5 > outputLen) return false;
            output[j++] = enc85Lead[remainders[4]];
            output[j++] = enc85[remainders[3]];
            output[j++] = enc85[remainders[2]];
            output[j++] = enc85[remainders[1]];
            output[j++] = enc85[remainders[0]];
        }
        // The last part, encode what remains
        switch(inputLen - i * 4)
        {
        case 0:
            // Done
            break;
        case 1:
            val = input[i * 4];
            current = val / 84;
            remainders[0] = val % 84;
            k = 1;
            remainders[k++] = current % 85;

            if (j + k > outputLen) return false;
            output[j++] = enc85Lead[remainders[k-- - 1]];
            while (k > 0) output[j++] = enc85[remainders[k-- - 1]];
            break;
        case 2:
            val = (input[i * 4] << 8) | input[i * 4 + 1];
            current = val / 84;
            remainders[0] = val % 84;
            k = 1;
            remainders[k++] = current % 85;
            current /= 85;
            remainders[k++] = current % 85;

            if (j + k > outputLen) return false;
            output[j++] = enc85Lead[remainders[k-- - 1]];
            while (k > 0) output[j++] = enc85[remainders[k-- - 1]];
            break;
        case 3:
            val = (input[i * 4] << 16) | (input[i * 4 + 1] << 8) | input[i * 4 + 2];
            remainders[0] = val % 84;
            current = val / 84;
            k = 1;
            remainders[k++] = current % 85;
            current /= 85;
            remainders[k++] = current % 85;
            current /= 85;
            remainders[k++] = current % 85;

            if (j + k > outputLen) return false;
            output[j++] = enc85Lead[remainders[k-- - 1]];
            while (k > 0) output[j++] = enc85[remainders[k-- - 1]];
//            output[j++] = enc85Lead[remainders[0]];
            break;
        }
        outputLen = j;
        return true;
    }

    bool decodeBase85(const uint8 * input, uint32 inputLen, uint8 * output, uint32 & outputLen)
    {
        if (!input || !inputLen) return false;

        // Trim padding
        while (input[inputLen - 1] == '_') inputLen--;

        if (!output)
        {
            // Count the output length required
            outputLen = 0;
            for (uint32 i = 0; i < inputLen; i++)
            {
                uint8 val = input[i];
                // Bad format so let's exit
                if (dec85Lead[val] == 0xFF) { outputLen = 0; return false; }
                if (val == 'z') { outputLen += 4; continue; }
                // Then add a output byte for each valid next char
                uint32 count = 4, j = 1;
                while (count && (i+j) < inputLen)
                {
                    val = input[i+j];
                    if (dec85[val] == 0xFF) { j++; continue; }
                    count--;
                    j++;
                }
                i += (j - 1);
                // Plain Base85 5 digits encoding
                /* if (!count) */ outputLen += 4;
                // The last bytes will decode to a given required buffer size
                // (we don't decode here, simply tell the size, so if the application is asking the size, we might be ask 3 bytes more than required)
            }
            return true;
        }


        // First pass, handle octet 4 by 4
        uint32 i = 0, j = 0;
        uint8 remainders[5] = {0};
        uint32 current = 0;
        for (; i < inputLen;)
        {
            uint8 val = input[i];
            if (dec85Lead[val] == 0xFF) { i++; continue; }
            if (val == 'z') { if (j + 4 > outputLen) return false; if (output) *(uint32*)&output[j] = 0; j+= 4; i++; continue; }
            remainders[0] = dec85Lead[val];

            uint32 count = 4, k = 1;
            while (count && (i+k) < inputLen)
            {
                val = input[i+k];
                if (dec85[val] == 0xFF) { k++; continue; }
                remainders[k] = dec85[val];
                count--;
                k++;
            }
            if (!count)
            {
                // Complete encoding
                if (j + 4 > outputLen) return false;
                *(uint32*)&output[j] = ntohl((((remainders[0] * 85 + remainders[1]) * 85 + remainders[2]) * 85 + remainders[3]) * 84 + remainders[4]);
                i += k;
                j += 4;
                continue;
            }
            else
            {
                if (j + (4 - count) > outputLen) return false;
                switch(count)
                {
                case 1:
                    current = ((remainders[0] * 85 + remainders[1]) * 85 + remainders[2]) * 84 + remainders[3];
                    if (current > 16777215) // Check for overflow
                        output[j++] = current >> 24;
//                    if (current > 65535)
                        output[j++] = (current & 0xFF0000) >> 16;
//                    if (current > 255)
                        output[j++] = (current & 0xFF00) >> 8;
                    output[j++] = current & 0xFF;
                    break;
                case 2:
                    current = (remainders[0] * 85 + remainders[1]) * 84 + remainders[2];
                    if (current > 65535) // Check for overflow
                        output[j++] = current >> 16;
//                    if (current > 255)
                        output[j++] = (current & 0xFF00) >> 8;
                    output[j++] = current & 0xFF;
                    break;
                case 3:
                    current = remainders[0] * 84 + remainders[1];
                    if (current > 255) // Check for overflow
                        output[j++] = current >> 8;
                    output[j++] = current & 0xFF;
                    break;
                case 4:
                    output[j++] = remainders[0];
                    break;
                }
                i += k;
            }
        }
        outputLen = j;
        return true;
    }


    bool encodeBase64(const uint8 * input, const uint32 inputLen, uint8 * output, uint32 & outputLen)
    {
        if (!input) return false;

        static const char b64table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        unsigned char initial[3];
        const size_t dataLen = (size_t)(!inputLen ? strlen((const char*)input) : inputLen);

        if (!output)
        {
            outputLen = (uint32)((dataLen%3) != 0 ? (dataLen / 3 + 1) * 4 : (dataLen / 3) * 4);
            return true;
        }

        const size_t trail = dataLen % 3;

        size_t k = 0, o = 0;

        while ( k + 3 <= dataLen)
        {
            initial[0] = input[k++];
            initial[1] = input[k++];
            initial[2] = input[k++];

            output[o+0] = b64table[(initial[0] & 0xfc) >> 2];
            output[o+1] = b64table[((initial[0] & 0x03) << 4) + ((initial[1] & 0xf0) >> 4)];
            output[o+2] = b64table[((initial[1] & 0x0f) << 2) + ((initial[2] & 0xc0) >> 6)];
            output[o+3] = b64table[initial[2] & 0x3f];

            o+= 4;
        }

        if (trail)
        {
            initial[0] = input[k++];
            initial[1] = trail == 1 ? 0 : input[k++];

            output[o+0] = b64table[(initial[0] & 0xfc) >> 2];
            output[o+1] = b64table[((initial[0] & 0x03) << 4) + ((initial[1] & 0xf0) >> 4)];
            output[o+2] = trail == 1 ? '=' : b64table[((initial[1] & 0x0f) << 2)];
            output[o+3] = '=';
            o+= 4;
        }
        outputLen = (uint32)o;
        return true;
    }

    static const uint8 base64DecMap[256] =
    {
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 64, 63,
        52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64,
        64,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
        15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64,
        64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
        41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64
    };

    bool decodeBase64(const uint8 * input, uint32 inputLen, uint8 * output, uint32 & outputLen)
    {
        if (!input) return false;
        // Count the valid char on input (check input, and fix the inputLen if not specified)
        uint32 i = 0;
        while ((inputLen ? i < inputLen : 1) && base64DecMap[input[i]] <= 63) i++;
        if (!inputLen || i < inputLen ) inputLen = i;

        // Check for null output
        if (!output)
        {
            outputLen = (inputLen * 3 / 4) + 1;
            return true;
        }

        // Sorry the required size is, well, required...
        if (outputLen < (inputLen * 3) / 4 + 1) return false;
        uint8 * outBuf = output;

        i = 0;
        while (inputLen > 4)
        {
            *(outBuf++) = (uint8) (base64DecMap[input[i + 0]] << 2 | base64DecMap[input[i + 1]] >> 4);
            *(outBuf++) = (uint8) (base64DecMap[input[i + 1]] << 4 | base64DecMap[input[i + 2]] >> 2);
            *(outBuf++) = (uint8) (base64DecMap[input[i + 2]] << 6 | base64DecMap[input[i + 3]]);
            i += 4;
            inputLen -= 4;
        }

        if (inputLen > 1)
            *(outBuf++) = (uint8) (base64DecMap[input[i + 0]] << 2 | base64DecMap[input[i + 1]] >> 4);
        if (inputLen > 2)
            *(outBuf++) = (uint8) (base64DecMap[input[i + 1]] << 4 | base64DecMap[input[i + 2]] >> 2);
        if (inputLen > 3)
            *(outBuf++) = (uint8) (base64DecMap[input[i + 2]] << 6 | base64DecMap[input[i + 3]]);

        outputLen = (uint32)(outBuf - output);
        return true;
    }

    static const uint8 base16EncMap[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
    bool encodeBase16(const uint8 * input, const uint32 inputLen, uint8 * output, uint32 & outputLen)
    {
        if (!input) return false;

        const size_t dataLen = (size_t)(!inputLen ? strlen((const char*)input) : inputLen);

        if (!output)
        {
            outputLen = (uint32)(2 * dataLen);
            return true;
        }
        if (outputLen < 2 * dataLen) return false;

        for (size_t i = 0; i < dataLen; i++)
        {
            output[i * 2 + 0] = base16EncMap[input[i] >> 4];
            output[i * 2 + 1] = base16EncMap[input[i] & 0xF];
        }
        outputLen = (uint32)dataLen * 2;
        return true;
    }

    bool decodeBase16(const uint8 * input, uint32 inputLen, uint8 * output, uint32 & outputLen)
    {
        if (!input) return false;
        // Count the valid char on input (check input, and fix the inputLen if not specified)
        if (!inputLen) inputLen = (uint32)strlen((const char*)input);

        // Check for null output
        if (!output)
        {
            outputLen = (inputLen+1) / 2;
            return true;
        }

        // Sorry the required size is, well, required...
        if (outputLen < (inputLen+1) / 2) return false;
        uint32 i = 0;
        for (i = 0; i < inputLen / 2; i++)
        {
            uint8 chHigh = input[i * 2 + 0];
            uint8 chLow = input[i * 2 + 1];

            if (chLow < '0' || (chLow > '9' && chLow < 'A') || (chLow > 'f') || (chLow > 'F' && chLow < 'a')) return false;
            if (chHigh < '0' || (chHigh > '9' && chHigh < 'A') || (chHigh > 'f') || (chHigh > 'F' && chHigh < 'a')) return false;
            output[i] = (chHigh <= '9') ? (chHigh - '0') : (chHigh <= 'F' ? (chHigh - 'A' + 10) : chHigh - 'a' + 10);
            output[i] <<= 4;
            output[i] |= (chLow <= '9') ? (chLow - '0') : (chLow <= 'F' ? (chLow - 'A' + 10) : chLow - 'a' + 10);
        }
        // Last byte if the number isn't pair
        if (i*2 < inputLen)
        {
            uint8 chLow = input[i * 2];

            if (chLow < '0' || (chLow > '9' && chLow < 'A') || (chLow > 'f') || (chLow > 'F' && chLow < 'a')) return false;
            output[i] = (chLow <= '9') ? (chLow - '0') : (chLow <= 'F' ? (chLow - 'A' + 10) : chLow - 'a' + 10);
            i++;
        }

        outputLen = i;
        return true;
    }
}

#endif

