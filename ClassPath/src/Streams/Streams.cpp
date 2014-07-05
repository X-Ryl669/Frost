// We need our base configuration
#include "../../include/Types.hpp"

#if (WantBaseEncoding == 1)
#include "../../include/Utils/MemoryBlock.hpp"
#include "../../include/Utils/ScopePtr.hpp"
#endif
#if (WantAES == 1)
#include "../../include/Crypto/AES.hpp"
#endif

// We need our declarations
#include "../../include/Streams/Streams.hpp"
// We need file declaration
#include "../../include/File/File.hpp"
// We need assert too
#include "../../include/Utils/Assert.hpp"

namespace Stream
{
    InputFileStream::InputFileStream(const String & name) : fileName(name), stream(0), fileSize((uint64)BadStreamSize)
    {
        if (fileName.getLength())
        {
            File::Info info(fileName);
            stream = info.getStream(true, true);
            if (stream)
                fileSize = info.size;
        }
    }
    InputFileStream::InputFileStream(const InputFileStream & other) : fileName(other.fileName), stream(0), fileSize(other.fileSize)
    {
        if (fileName.getLength())
        {
            File::Info info(fileName);
            stream = info.getStream(true, true);
            if (stream)
            {
                fileSize = info.size;
                setPosition(other.currentPosition());
            }
        }
    }


    InputFileStream::~InputFileStream()
    {
        fileSize = 0; delete (File::BaseStream*)stream;
    }

    uint64 InputFileStream::fullSize() const { return fileSize; }
    bool InputFileStream::endReached() const { return stream ? ((File::BaseStream*)stream)->endOfStream() : true; }
    uint64 InputFileStream::currentPosition() const { return stream ? ((File::BaseStream*)stream)->getPosition() : 0; }
    bool InputFileStream::setPosition(const uint64 newPos)
    {
        return stream ? ((File::BaseStream*)stream)->setPosition(newPos) : false;
    }
    bool InputFileStream::goForward(const uint64 skipAmount)
    {
        return stream ? ((File::BaseStream*)stream)->setPosition(((File::BaseStream*)stream)->getPosition() + skipAmount) : false;
    }
    uint64 InputFileStream::read(void * const buffer, const uint64 size) const throw()
    {
        if (!buffer || !stream || !size) return 0;

        return ((File::BaseStream*)stream)->read((char*)buffer, (int)min((uint64)INT_MAX, size));
    }


    InputStringStream::InputStringStream(const String & _content) : content(_content), position(0) {}
    void InputStringStream::resetStream(const String & _content) { content = _content; position = 0; }
    uint64 InputStringStream::fullSize() const { return content.getLength(); }
    bool InputStringStream::endReached() const { return position >= (uint64)content.getLength(); }
    uint64 InputStringStream::currentPosition() const { return position; }
    bool InputStringStream::setPosition(const uint64 newPos)
    {
        if (newPos <= (uint64)content.getLength()) { position = newPos; return true; }
        return false;
    }
    bool InputStringStream::goForward(const uint64 skipAmount)
    {
        return setPosition(currentPosition() + skipAmount);
    }
    uint64 InputStringStream::read(void * const buffer, const uint64 size) const throw()
    {
        if (!buffer || !size) return 0;

        uint32 readSize = (uint32)min((uint64)0xFFFFFFFF, (size + position) < (uint64)content.getLength() ? size : ((uint64)content.getLength() - position));

        memcpy(buffer, &((const char*)content)[position], (size_t)readSize);
        position += readSize;
        return readSize;
    }

#if defined(HasBaseEncoding)
    uint64 Base64InputStream::read(void * const buffer, uint64 _size) const throw()
    {
        if (!buffer) return (uint64)-1;
        // Check for very large read
        if (_size > 0xFFFFFFFF) 
        {
            uint64 totalSize = 0;
            while (_size > 0xFFFFFFFF)
            {
                uint64 result = read((uint8*)buffer + totalSize, 0xFFFFFFFF);
                if (result == (uint64)-1) return (uint64)-1;
                totalSize += result;
                if (result <= 0xFFFFFFFF) return totalSize;
                _size -= result;
            }
            return totalSize;
        }
        // First empty the current buffer
        uint32 size = min(memoryBlock.getSize(), (uint32)_size);
        if (!memoryBlock.Extract((uint8*)buffer, size)) return (uint64)-1;
        if (size >= _size) return size;
        uint64 outSize = size;
        
        // Now we need to refill the memory block while reading it
        uint8 * newBuffer = new uint8[blockSize];
        while (outSize < _size)
        {
            uint32 realBlock = (uint32)inputStream.read(newBuffer, blockSize);
            if (realBlock == (uint32)-1) { delete[] newBuffer; return (uint64)-1; }
        
            if (!memoryBlock.rebuildFromBase64(newBuffer, realBlock)) { delete[] newBuffer; return (uint64)-1; }
            size = min((uint32)(_size - outSize), memoryBlock.getSize());
            memoryBlock.Extract((uint8*)buffer + outSize, size);
            outSize += size;
            if (realBlock < blockSize) break;
        }
        delete[] newBuffer;
        return outSize;        
    }
    
    // Move the stream position forward of the given amount
    bool Base64InputStream::goForward(const uint64 skipAmount)
    {
        if (skipAmount < (uint64)memoryBlock.getSize())
        {
            // Simply extract the buffer of the skipAmount
            uint8 * buffer = new uint8[(size_t)skipAmount];
            memoryBlock.Extract(buffer, (uint32)skipAmount);
            delete[] buffer;
            return true;
        }
        
        uint64 amountToSkipInBase64EncodedData = convertSize(skipAmount);
        if (!inputStream.goForward(amountToSkipInBase64EncodedData)) return false;
        // Need to empty the memory block, and skip the input stream accordingly to the position we want
        memoryBlock.stripTo(0);
        return true;
    }
    
    uint64 Base64OutputStream::write(const void * const _buffer, const uint64 _size) throw()
    {
        if (memoryBlock.getSize() + _size < (uint64)blockSize)
        {
            // There's not enough data to convert to base64, so let's buffer this now
            memoryBlock.Append((uint8*)_buffer, (uint32)_size);
            return _size;
        }
        
        // Let's process the encoding in blocks
        uint64 processedSize = 0;
        while (processedSize < _size)
        {
            uint32 spaceLeftInBlock = blockSize - memoryBlock.getSize();
            uint32 currentBlockSize = (uint32)min((uint64)spaceLeftInBlock, _size - processedSize);
            if (!memoryBlock.Append((uint8*)_buffer + processedSize, currentBlockSize)) return (uint64)-1;
            if (currentBlockSize < spaceLeftInBlock) return _size;
            // Let's convert and write it
            Utils::ScopePtr<Utils::MemoryBlock> convertedBlock = memoryBlock.toBase64();
            if (!convertedBlock) return (uint64)-1;

            uint64 outputSize = outputStream.write(convertedBlock->getBuffer(), convertedBlock->getSize());
            if (outputSize == (uint64)-1) 
            {
                memoryBlock.stripTo(0);
                return outputSize;
            }
            if (outputSize != convertedBlock->getSize()) 
            {
                // Need to extract the current memory block consumed size
                uint64 consumedSize = unconvertSize(outputSize);
                memoryBlock.Extract(convertedBlock->getBuffer(), (uint32)consumedSize);
                return consumedSize;
            }
            memoryBlock.stripTo(0);
            processedSize += currentBlockSize;   
        }
        return (uint64)processedSize;
    }
    bool Base64OutputStream::Flush() throw()
    {
        Utils::ScopePtr<Utils::MemoryBlock> convertedBlock = memoryBlock.toBase64();
        if (!convertedBlock) return false;
        // We don't care about the result here since we can't do any thing if it fails
        return outputStream.write(convertedBlock->getBuffer(), convertedBlock->getSize()) == convertedBlock->getSize();  
    }
#endif


#if defined(HasAES)
    AESInputStream::AESInputStream(const InputStream & is, const String & keyInHex, const String & IVInHex) : inputStream(is), tempPos(0), keySize(0)
    {
        memset(buffer, 0, sizeof(buffer));

        if (keyInHex.getLength() != IVInHex.getLength()) return;
        if (is.fullSize() < 0xfffffffe)
        {
            // Build the key
            uint8 key[32] = {0}; uint8 iv[32] = {0};
            if ((keyInHex.getLength() & 0xF) == 0)
            {
                const_cast<uint16 &>(keySize) = (uint16)(keyInHex.getLength() / 2);
                tempPos = keySize;
                for (int i = 0; i < keyInHex.getLength(); i+=2)
                {
                    uint8 hi = keyInHex[i];
                    uint8 lo = keyInHex[i+1];
                    key[i/2] = hi >= 'a' ? (hi + 10 - 'a') : (hi >= 'A' ? (hi + 10 - 'A') : (hi - '0'));
                    key[i/2] <<= 4;
                    key[i/2] |= lo >= 'a' ? (lo + 10 - 'a') : (lo >= 'A' ? (lo + 10 - 'A') : (lo - '0'));
                    hi = IVInHex[i];
                    lo = IVInHex[i+1];
                    iv[i/2] = hi >= 'a' ? (hi + 10 - 'a') : (hi >= 'A' ? (hi + 10 - 'A') : (hi - '0'));
                    iv[i/2] <<= 4;
                    iv[i/2] |= lo >= 'a' ? (lo + 10 - 'a') : (lo >= 'A' ? (lo + 10 - 'A') : (lo - '0'));
                }

                // Then set the key
                crypto.setKey(key, (Crypto::AES::BlockSize)keySize, iv, (Crypto::AES::BlockSize)(IVInHex.getLength() / 2));
            }
        }
    }
    AESInputStream::AESInputStream(const InputStream & is, const uint8 * key, const uint32 _keySize, const uint8* iv, const uint32 _ivSize)
        : inputStream(is), tempPos(0), keySize(0)
    {
        memset(buffer, 0, sizeof(buffer));

        if (_keySize != _ivSize) return;
        if (is.fullSize() < 0xfffffffe)
        {
            // Build the key
            if ((_keySize & 0xF) == 0)
            {
                const_cast<uint16 &>(keySize) = (uint16)_keySize;
                tempPos = keySize;
                // Then set the key
                crypto.setKey(key, (Crypto::AES::BlockSize)_keySize, iv, (Crypto::AES::BlockSize)_ivSize);
            }
        }
    }

    bool AESInputStream::goForward(const uint64 skipAmount)
    {
        // Need to read the skip amount multiple time until we're done.
        if (skipAmount <= (uint64)(keySize - tempPos))
        {
            tempPos += (uint16)min(skipAmount, (uint64)-1);
            return true;
        }
        // Ok, then advance of the given amount
        uint64 skipLeft = skipAmount - (keySize - tempPos);
        tempPos = 0;
        uint8 localBuffer[32];
        while (skipLeft)
            skipLeft -= read(localBuffer, min(skipLeft, (uint64)32));

        return skipLeft == 0;
    }

    uint64 AESInputStream::read(void * const _buffer, const uint64 size) const throw()
    {
        if (!_buffer || !size) return 0;
        // Need to return the already decoded buffer
        uint8 * const outBuf = (uint8 * const)_buffer;
        uint32 pos = 0;
        memcpy(&outBuf[pos], &buffer[tempPos], (int)min((uint64)keySize - tempPos, size));
        pos = (int)min((uint64)keySize - tempPos, size);
        if (pos == size) { tempPos += (uint16)pos; return pos; }

        // Need to read the next block out of the input stream
        // Optimize the algorithm a little by read a bunch at a time
        while ((size - pos) >= keySize)
        {
            // Big amount first
            if ((size - pos) >= 1024)
            {
                uint8 localBuffer[1024];
                uint64 readSize = inputStream.read(localBuffer, 1024);

                if (readSize < 1024)
                {   // Probable end of stream
                    // Need to pad with 0 to be able to decrypt
                    memset(&localBuffer[readSize], 0, (size_t)(1024 - readSize));
                    // Decrypt now
                    uint8 localOutput[1024] = {0};
                    crypto.Decrypt(localBuffer, localOutput, 1024, Crypto::AES::CFB);
                    memcpy(&outBuf[pos], localOutput, (size_t)readSize);
                    pos += (uint32)min(readSize, (uint64)-1);
                    // Virtually set end of stream
                    tempPos = keySize;
                    return pos;
                }

                // Decrypt now
                crypto.Decrypt(localBuffer, &outBuf[pos], 1024, Crypto::AES::CFB);
                pos += 1024;
                continue;
            }
            // Finish by small amounts
            uint8 localBuffer[32];
            uint64 readSize = inputStream.read(localBuffer, keySize);
            if (readSize < keySize)
            {   // Probable end of stream
                // Need to pad with 0 to be able to decrypt
                memset(&localBuffer[readSize], 0, (size_t)(keySize - readSize));
                // Decrypt now
                uint8 localOutput[32] = {0};
                crypto.Decrypt(localBuffer, localOutput, keySize, Crypto::AES::CFB);
                memcpy(&outBuf[pos], localOutput, (size_t)readSize);
                pos += (uint32)min(readSize, (uint64)-1);
                // Virtually set end of stream
                tempPos = keySize;
                return pos;
            }

            // Decrypt now
            crypto.Decrypt(localBuffer, &outBuf[pos], keySize, Crypto::AES::CFB);
            pos += keySize;
        }
        // Don't read ahead unless required
        if (size == pos) return pos;

        uint8 localBuffer[32];
        uint64 readSize = inputStream.read(localBuffer, keySize);
        if (readSize < keySize)
        {
            // Need to pad with 0 to be able to decrypt
            memset(&localBuffer[readSize], 0, (size_t)(keySize - readSize));
            // Decrypt now
            crypto.Decrypt(localBuffer, buffer, keySize, Crypto::AES::CFB);
            memcpy(&outBuf[pos], buffer, (size_t)min(readSize, size - pos));
            // Virtually set end of stream if required
            if (readSize > (size - pos))
            {
                uint32 remain = (uint32)min(readSize - (size - pos), (uint64)-1);
                memcpy(&buffer[keySize - remain], &buffer[size - pos], (size_t)remain);
                tempPos = (uint16)(keySize - remain);
            }
            pos += (uint32)min(readSize, size - pos);
            return pos;
        }
        crypto.Decrypt(localBuffer, buffer, keySize, Crypto::AES::CFB);
        memcpy(&outBuf[pos], buffer, (size_t)(size - pos));
        // Virtually set end of stream if required
        tempPos = (uint16)(keySize - (size - pos));
        pos = (uint16)size;
        return pos;
    }
    
    AESOutputStream::AESOutputStream(OutputStream & os, const String & keyInHex, const String & IVInHex) : outputStream(os), tempPos(0), keySize(0)
    {
        memset(buffer, 0, sizeof(buffer));

        if (keyInHex.getLength() != IVInHex.getLength()) return;
        // Build the key
        uint8 key[32] = {0}; uint8 iv[32] = {0};
        if ((keyInHex.getLength() & 0xF) == 0)
        {
            const_cast<uint16 &>(keySize) = (uint16)(keyInHex.getLength() / 2);
            for (int i = 0; i < keyInHex.getLength(); i+=2)
            {
                uint8 hi = keyInHex[i];
                uint8 lo = keyInHex[i+1];
                key[i/2] = hi >= 'a' ? (hi + 10 - 'a') : (hi >= 'A' ? (hi + 10 - 'A') : (hi - '0'));
                key[i/2] <<= 4;
                key[i/2] |= lo >= 'a' ? (lo + 10 - 'a') : (lo >= 'A' ? (lo + 10 - 'A') : (lo - '0'));
                hi = IVInHex[i];
                lo = IVInHex[i+1];
                iv[i/2] = hi >= 'a' ? (hi + 10 - 'a') : (hi >= 'A' ? (hi + 10 - 'A') : (hi - '0'));
                iv[i/2] <<= 4;
                iv[i/2] |= lo >= 'a' ? (lo + 10 - 'a') : (lo >= 'A' ? (lo + 10 - 'A') : (lo - '0'));
            }

            // Then set the key
            crypto.setKey(key, (Crypto::AES::BlockSize)keySize, iv, (Crypto::AES::BlockSize)(IVInHex.getLength() / 2));
        }
    }
    AESOutputStream::AESOutputStream(OutputStream & os, const uint8 * key, const uint32 _keySize, const uint8* iv, const uint32 _ivSize)
        : outputStream(os), tempPos(0), keySize(0)
    {
        memset(buffer, 0, sizeof(buffer));

        if (_keySize != _ivSize) return;
        // Build the key
        if ((_keySize & 0xF) == 0)
        {
            const_cast<uint16 &>(keySize) = (uint16)_keySize;
            // Then set the key
            crypto.setKey(key, (Crypto::AES::BlockSize)keySize, iv, (Crypto::AES::BlockSize)_ivSize);
        }
    }

    uint64 AESOutputStream::write(const void * const _buffer, const uint64 size) throw()
    {
        if (!_buffer || !size) return 0;
        // Need to return the already decoded buffer
        uint8 * const inBuf = (uint8 * const)_buffer;
        uint32 pos = 0;
        memcpy(&buffer[tempPos], &inBuf[pos], (int)min((uint64)(keySize - tempPos), size));
        pos = (uint32)min((uint64)(keySize - tempPos), size);
        if (pos == size) { tempPos += (uint16)pos; return pos; }

        tempPos = 0;
        uint8 localBuffer[32] = {0};
        crypto.Encrypt(buffer, localBuffer, keySize, Crypto::AES::CFB);
        // Then write to the output stream
        uint64 writeSize = outputStream.write(localBuffer, keySize);
        if (writeSize < keySize) return pos + writeSize;


        // Need to read the next block out of the input stream
        while ((size - pos) >= keySize)
        {
            if ((size - pos) >= 1024)
            {
                // Proceed to a faster encryption
                uint8 localBuffer[1024];
                crypto.Encrypt(&inBuf[pos], localBuffer, 1024, Crypto::AES::CFB);
                // Then write to the output stream
                uint64 writeSize = outputStream.write(localBuffer, 1024);
                if (writeSize < 1024) return pos + writeSize;
                pos += 1024;
                continue;
            }
            crypto.Encrypt(&inBuf[pos], localBuffer, keySize, Crypto::AES::CFB);
            // Then write to the output stream
            uint64 writeSize = outputStream.write(localBuffer, keySize);
            if (writeSize < keySize) return pos + writeSize;
            pos += keySize;
        }
        // Don't write ahead unless required
        if (size == pos) return pos;

        // Copy the remaining data to the buffer
        memcpy(buffer, &inBuf[pos], (size_t)(size - pos));
        tempPos = (uint16)(size - pos);
        return size;
    }

    AESOutputStream::~AESOutputStream()
    {
        // Need to copy the last bytes in the file
        if (tempPos && tempPos < keySize)
        {
            uint8 localBuffer[32] = {0};
            memset(localBuffer, 0, sizeof(localBuffer));
            memcpy(localBuffer, buffer, keySize);
            crypto.Encrypt(localBuffer, buffer, keySize, Crypto::AES::CFB);
            // Then write the final stream back to the output stream
            outputStream.write(buffer, tempPos); // Please note that we have cut the last part, here
        }
    }
    
#endif

    OutputStringStream::OutputStringStream(String & _content) : content(_content), position(0) {}

    uint64 OutputStringStream::fullSize() const { return content.getLength(); }
    bool OutputStringStream::endReached() const { return position >= (uint64)content.getLength(); }
    uint64 OutputStringStream::currentPosition() const { return position; }
    bool OutputStringStream::setPosition(const uint64 newPos)
    {
        if (newPos <= (uint64)content.getLength()) { position = newPos; return true; }
        return false;
    }
    bool OutputStringStream::goForward(const uint64 skipAmount)
    {
        return setPosition(currentPosition() + skipAmount);
    }
    uint64 OutputStringStream::write(const void * const buffer, const uint64 _size) throw()
    {
        if (!buffer || !_size) return 0;

        uint32 size = (uint32)min((uint64)0xFFFFFFFF, _size);
        if ((size + position) > (uint64)content.getLength())
        {
            // Optimization here
            if (position == (uint64)content.getLength())
            {
                // Directly append the given buffer
                content += String(buffer, (int)size);
            } else
            {
                // Append the required data size first, and then fill it here
                int addSize = (int)(size + position) - content.getLength();
                content += String(' ', addSize);
                memcpy((void *)(&((const char*)content)[position]), buffer, size);
            }
        }
        else
        {
            memcpy((void *)(&((const char*)content)[position]), buffer, size);
        }
        position += size;
        return size;
    }


    OutputFileStream::OutputFileStream(const String & name, bool delayedOpening) : fileName(name), stream(0), fileSize(0)
    {
        if (!delayedOpening) openFile();
    }

    OutputFileStream::~OutputFileStream()
    {
        if (stream) delete (File::BaseStream*)stream;
        fileSize = 0;
    }

    bool OutputFileStream::openFile()
    {
        File::Info info(fileName);
        stream = info.getStream(true, false, true);
        if (stream)
        {
            fileSize = 0;
            ((File::BaseStream*)stream)->setPosition(0);
        }
        return (stream != 0);
    }

    uint64 OutputFileStream::fullSize() const { return fileSize; }
    bool OutputFileStream::endReached() const { return stream ? ((File::BaseStream*)stream)->getPosition() == fileSize : true; }
    uint64 OutputFileStream::currentPosition() const { return stream ? ((File::BaseStream*)stream)->getPosition() : 0; }

    bool OutputFileStream::setPosition(const uint64 newPos)
    {
        if (newPos > fileSize && stream)
        {
            if (((File::BaseStream*)stream)->setSize(newPos)) { fileSize = newPos; return true; }
            return false;
        }
        return stream ? ((File::BaseStream*)stream)->setPosition(newPos) : 0;
    }

    uint64 OutputFileStream::write(const void * const buffer, const uint64 size) throw()
    {
        // Open the file if we have been delayed
        if (!stream) openFile();
        if (!buffer || !stream) return 0;
        if (!size) return 0;
        uint64 curPos = ((File::BaseStream*)stream)->getPosition();
        int written = ((File::BaseStream*)stream)->write((const char*)buffer, (int)min((uint64)INT_MAX, size));
        if (written < 0) return (uint64)written;
        ((File::BaseStream*)stream)->flush();
        fileSize = max(curPos + written, fileSize); 
        return written;
    }



    // The copy stream function
    bool copyStream(const InputStream & is, OutputStream & os, const uint64 forcedSize)
    {
        uint64 total = forcedSize ? forcedSize : is.fullSize();

        const MappableStream * ms = is.getMappable();
        if (ms) return os.write(ms->getBuffer(), total) == total;
        
        uint8 buffer[4096];
        uint64 data = is.read(buffer, min(total, (uint64)4096)); // This works because if fullSize() returns -1 (size not known), it's seens as (uint64)-1, ie the biggest number possible
        while(data == 4096)
        {
            if (os.write(buffer, data, false) != data) return false;
            total -= data;
            data = is.read(buffer, min(total, (uint64)4096));
        }
        return data < 4096 && os.write(buffer, data, true) == data;
    }
    // The copy stream function
    bool copyStream(const InputStream & is, OutputStream & os, CopyCallback & callback, const uint64 forceOutputSize)
    {
        uint64 total = forceOutputSize ? forceOutputSize : is.fullSize(), current = 0;
        const MappableStream * ms = is.getMappable();
        if (ms)
        {
            // Cut the amount in 100 so we can call the callback at regular interval
            const uint64 step = total / 100;
            for (int i = 0; i < 100; i++)
            {
                if (os.write(&ms->getBuffer()[current], step, false) != step) return false;
                current += step;
                if (!callback.copiedData(current, total)) return false;
            }
            if (os.write(&ms->getBuffer()[current], total - current, true) != (total - current)) return false;
            return callback.copiedData(total, total);
        }
        
        uint8 buffer[4096];
        uint64 data = is.read(buffer, min(total, (uint64)4096));
        while(data == 4096)
        {
            if (os.write(buffer, data, false) != data) return false;
            current += 4096;
            if (!callback.copiedData(current, total)) return false;
            data = is.read(buffer, min(total - current, (uint64)4096));
        }
        if (data < 4096 && os.write(buffer, data, true) != data) return false;
        return callback.copiedData(current + data, total);
    }

    // The clone stream function
    InputStream * cloneStream(const InputStream & is)    
    {
        // If you try to clone a large input stream, it's not the method to use, 
        // since this function buffers the whole stream. You likely have to redesign the code logic so it 
        // doesn't involve creating such large memory buffers.
        Assert(is.fullSize() < 64000000);
        uint64 currentPos = is.currentPosition();
        uint8 * buffer = new uint8[(size_t)is.fullSize()];
        if (!buffer) return 0;
        if (is.read(buffer, is.fullSize()) != is.fullSize()) { delete[] buffer; return 0; }
        // Restore position (if it fails, we ignore the error)
        const_cast<InputStream &>(is).setPosition(currentPos);
        return new Stream::MemoryBlockStream(buffer, is.fullSize(), true);
    }
    
    /** @internal */
    inline int asHex(char ch) { return ch >= '0' && ch <= '9' ? (ch - '0') : (ch >= 'a' && ch <= 'f' ? (ch - 'a' + 10) : (ch >= 'A' && ch <= 'F' ? (ch - 'A' + 10) : 0)); }

    Strings::FastString readString(const InputStream & is, const Strings::FastString & stop) 
    {
        char ch = 0; 
        Strings::FastString out; 
        if (!stop) while (is.read(&ch, 1) == 1 && ch) out += ch;
        else while (is.read(&ch, 1) == 1 && stop.Find(ch) == -1) out += ch;
        return out;
    }
    
    Strings::FastString readHexNumber(const InputStream & is, const Strings::FastString & stop)
    {
        char ch[3] = { 0, 0, 0 }; 
        Strings::FastString out; 
        if (!stop) 
        {
            while (1)
            {
                switch (is.read(ch, 2))
                {
                case 2: out += (char)((asHex(ch[0]) << 4) | asHex(ch[1])); break;
                case 1: out += (char)(asHex(ch[0]) << 4); return out;
                default: return out;
                }
            }
        }
        else
        { 
            while (1)
            {
                if (is.read(&ch[0], 1) != 1) return out;
                if (stop.Find(ch[0]) != -1) return out; // Stop character found 
                if (is.read(&ch[1], 1) != 1 || stop.Find(ch[1]) != -1) 
                {
                    out += (char)(asHex(ch[0]) << 4);
                    return out;
                }
                out += (char)((asHex(ch[0]) << 4) | asHex(ch[1]));
            }
        }
    }
    
}
