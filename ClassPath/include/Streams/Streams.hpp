#ifndef hpp_CPP_Streams_CPP_hpp
#define hpp_CPP_Streams_CPP_hpp

// We need the memory interface
#include <stdlib.h>
// We need types
#include "../Types.hpp"
// We need strings
#include "../Strings/Strings.hpp"
// We need SafeRealloc too
#include "../Platform/Platform.hpp"
// If specified, we have to declare Base64 based streams
#include "../Utils/MemoryBlock.hpp"
// We need scoped pointers too
#include "../Utils/ScopePtr.hpp"

#if (WantAES == 1)
// If specified, we have to declare AES based streams
#include "../Crypto/AES.hpp"
#endif



/** Manipulating data from different medium is often easier when seen as a generic stream.
    You'll use an input stream (any of InputFileStream, InputStringStream, MemoryBlockStream,
    MemoryBufferedInputStream, AESInputStream), or the equivalent output stream (any of
    OutputFileStream, OutputStringStream, MemoryBufferedOutputStream, AESOutputStream).

    Using the AES stream, you'll get transparent and fast encryption / decryption.
    Using Base64 stream, you'll get transparent and fast Base64 encoding / decoding.
    Similarly, you have compression specialized stream, socket based stream. 
    
    For a more complete list of input streams:
     Class                        | Description
     -----------------------------|------------------------------------------------------------
     ForwardInputStream           | Used for wrapper streams class
     RangeInputStream             | A range limiter input stream
     StdInStream                  | A stream based on the standard input file descriptor (STDIN)
     LineBasedInputStream         | A wrapper that allow reading an input stream line by line
     InputFileStream              | An input stream whose source is a file
     InputStringStream            | An input stream whose source is a string
     MemoryBlockStream            | An input stream made from a pre-allocated memory buffer
     MemoryBufferedInputStream    | An input stream wrapper that's completely reading the given input stream in a memory buffer for caching
     SuccessiveStream             | An input stream that's reading from 2 input streams successively
     Base64InputStream            | An input stream that's decoding Base64 data on the fly
     AESInputStream               | An input stream that's decoding AES encrypted data on the fly
     SocketInputStream            | An input stream that's reading from a socket
     BufferedInputStream          | An input stream that's reading the given input stream block by block
     DecompressInputStream        | An input stream that's decompressing data on the fly
    
    For a more complete list of output streams:
     Class                        | Description
     -----------------------------|------------------------------------------------------------
     OutputFileStream             | An output stream that writes to a file
     OutputStringStream           | An output stream that's filling a string
     OutputMemStream              | An output stream that's filling a MemoryBlock
     MemoryBlockOutStream         | An output stream that's filling a pre-allocated memory buffer
     MemoryBufferedOutputStream   | An output stream wrapper that's filling a memory buffer and then write to the given output stream
     NullOutputStream             | An output stream that just keeps track of the amount written, but does not write anything
     TeeStream                    | An output stream that duplicate its action on two output streams
     Base64OutputStream           | An output stream that's encoding in Base64 on the fly
     AESOutputStream              | An output stream that's encoding AES encrypted data on the fly
     HashStream                   | An output stream that's computing a Hash on the fly while still writing to an output stream
     SocketOutputStream           | An output stream that's writing to a socket
     CompressOutputStream         | An output stream that's compressing data on the fly
     HeaderBodyStream             | An output stream based on a memory buffer for an fixed size header and a output stream for data, that allow out-of time write to the header after data is written.
*/
namespace Stream
{
    /** All the fullSize() method in each stream can return this value if the stream is not opened correctly, or broken */
    enum { BadStreamSize = (uint64)-1 };

    /** The Stream interface that must be supported in child classes */
    class BaseStream
    {
        // Constructors
    protected:
        /** The default constructor is only available to child classes */
        BaseStream() {}

    public:
        /** The only destructor */
        virtual ~BaseStream() {}

        // Interface
    public:
        /** This method returns the stream length in byte, if known
            If the length is equal or higher than 2^32 - 1, the returned value is (uint64)-2
            For stream where the length is not known, this method will return (uint64)-1. */
        virtual uint64 fullSize() const = 0;
        /** This method returns true if the end of stream is reached */
        virtual bool endReached() const = 0;
        /** This method returns the position of the next byte that could be read from this stream */
        virtual uint64 currentPosition() const = 0;
        /** Try to seek to the given absolute position (return false if not supported) */
        virtual bool setPosition(const uint64 newPos) = 0;
    };

    /** The mappable stream interface.
        Only streams backed up by memory buffer can implement this interface.
        Use BaseStream::getMappable() to figure out if you can access this interface. */
    struct MappableStream
    {
        /** Get the current buffer from the stream */
        virtual const uint8 * getBuffer() const = 0;
        /** Get the current buffer from the stream */
        virtual uint8 * getBuffer() { return 0; }
        
        virtual ~MappableStream() {}
    };

    /** The base input stream interface */
    class InputStream : public BaseStream
    {
    protected:
        /** The default constructor is only available to child classes */
        InputStream() {}

        // The interface
    public:
        /** Try to read the given amount of data to the specified buffer
            @return the number of byte truly read (this method doesn't throw) */
        virtual uint64 read(void * const buffer, const uint64 size) const throw() = 0;
        /** Move the stream position forward of the given amount
            This should give the same results as setPosition(currentPosition() + value),
            but implementation can be faster for non-seek-able stream. */
        virtual bool goForward(const uint64 skipAmount) = 0;
        /** Check if this stream is map-able (seen as a memory buffer).
            This can lead to significant optimizations if we can work directly on the 
            memory buffer, and avoid the copy of the buffer to a temporary buffer.
            @return A pointer to a "MappableStream" if convertible, 0 else */
        virtual MappableStream * getMappable() { return 0; }
        /** Check if this stream is map-able (seen as a memory buffer).
            This can lead to significant optimizations if we can work directly on the 
            memory buffer, and avoid the copy of the buffer to a temporary buffer.
            @return A pointer to a "MappableStream" if convertible, 0 else */
        virtual const MappableStream * getMappable() const { return 0; }
        
        /** A useful helper when what you read is typed and you don't react on errors, just bailing out. 
            @param val  The value to read into
            @return true when val was completely read out of the stream */
        template <typename T>
        bool read(T & val) const throw() { return read(&val, (uint64)sizeof(val)) == (uint64)sizeof(val); }
        /** A useful helper when what you read is typed and you don't react on errors, just bailing out.
            @param val  The value to read into
            @return true when val was completely read out of the stream */
        template <typename T, size_t N>
        bool read(T (&val)[N]) const throw() { return read(&val[0], (uint64)(sizeof(val[0]) * N)) == (uint64)(sizeof(val[0]) * N); }
    };
    

    
    /** This is a useful wrapper class if you intend to forward most calls to an existing input stream */
    class ForwardInputStream : public InputStream
    {
    protected:
        /** The reference stream */
        InputStream & ref;
        
    public:
        /** This method returns the stream length in byte, if known
            If the length is equal or higher than 2^32 - 1, the returned value is (uint64)-2
            For stream where the length is not known, this method will return (uint64)-1. */
        virtual uint64 fullSize() const { return ref.fullSize(); }
        /** This method returns true if the end of stream is reached */
        virtual bool endReached() const { return ref.endReached(); }
        /** This method returns the position of the next byte that could be read from this stream */
        virtual uint64 currentPosition() const { return ref.currentPosition(); }
        /** Try to seek to the given absolute position (return false if not supported) */
        virtual bool setPosition(const uint64 newPos) { return ref.setPosition(newPos); } 
        
        ForwardInputStream(InputStream & ref) : ref(ref) {}
    };
    
    /** A range limited input stream.
        This stream takes an input stream of some size and appear like another input stream 
        where the start and end position are limited. 
        This is useful for example when your input stream contains multiple different 
        parts at different position, you can create as many range input stream as the number of parts */
    class RangeInputStream : public InputStream
    {
        // Members
    private:
        /** The reference input stream */
        InputStream & ref;
        /** The absolute start position in the input stream (that could be reached) */
        uint64 start;
        /** The absolute end position in the input stream (that could be reached) */
        uint64 stop;
        
        // Interface 
    public:
        /** Try to read the given amount of data to the specified buffer
            @return the number of byte truly read (this method doesn't throw) */
        inline uint64 read(void * const buffer, const uint64 size) const throw() { return ref.read(buffer, min(stop - ref.currentPosition(), size)); } 
        /** Move the stream position forward of the given amount
            This should give the same results as setPosition(currentPosition() + value),
            but implementation can be faster for non-seek-able stream. */
        inline bool goForward(const uint64 skipAmount) { if (skipAmount + ref.currentPosition() > stop) return false; return ref.goForward(skipAmount); }
        /** This method returns the stream length in byte, if known
            If the length is equal or higher than 2^32 - 1, the returned value is (uint64)-2
            For stream where the length is not known, this method will return (uint64)-1. */
        inline uint64 fullSize() const { if (start > ref.fullSize()) return 0; return min(stop, ref.fullSize()) - start; }
        /** This method returns true if the end of stream is reached */
        inline bool endReached() const { return ref.endReached() || ref.currentPosition() < stop; }
        /** This method returns the position of the next byte that could be read from this stream */
        inline uint64 currentPosition() const { return min(stop, ref.currentPosition() - start); }
        /** Try to seek to the given absolute position (return false if not supported) */
        inline bool setPosition(const uint64 newPos) { return (start + newPos) < stop && (start + newPos) < ref.fullSize() ? ref.setPosition(start + newPos) : false; }
        
        // Construction and destruction
    public:
        /** Construct an input stream that's only a small part of the given stream.
            @param ref      The (larger) input stream to use as support
            @param start    The start position (included) to start using the stream from 
            @param stop     The ending position (included) to stop using the stream from */
        RangeInputStream(InputStream & ref, const uint64 start = 0, const uint64 stop = (uint64)-1) : ref(ref), start(start), stop(stop) { ref.setPosition(start); }
    };

    /** An input stream that read's process input stream (if it exists).
        This is a wrapper around stdin, so the same limitation applies to stdin.
        You can not instantiate it (because only one exists), so you have to use
        the getInstance() method to get a reference on the only instance. */
    class StdInStream : public InputStream
    {
        // Members
    private:
        /** Keep track of state like the position */
        mutable uint64 position;

    public:
        /** This method returns the stream length in byte, if known
            If the length is equal or higher than 2^32 - 1, the returned value is (uint64)-2
            For stream where the length is not known, this method will return (uint64)-1. */
        inline uint64 fullSize() const { return (uint64)-1; }
        /** This method returns true if the end of stream is reached */
        inline bool endReached() const { return feof(stdin) != 0; }
        /** This method returns the position of the next byte that could be read from this stream */
        virtual uint64 currentPosition() const { return position; }
        /** Try to seek to the given absolute position (return false if not supported) */
        virtual bool setPosition(const uint64 newPos) { return false; }
        /** Try to read the given amount of data to the specified buffer
            @return the number of byte truly read (this method doesn't throw) */
        virtual uint64 read(void * const buffer, const uint64 size) const throw()
        {
            uint64 ret = (uint64)fread(const_cast<void*>(buffer), 1, (size_t)size, stdin);
            position += (uint64)ret; return ret;
        }
        /** Move the stream position forward of the given amount
            This should give the same results as setPosition(currentPosition() + value),
            but implementation can be faster for non-seek-able stream. */
        virtual bool goForward(const uint64 skipAmount)
        {
            char a;
            uint64 skip = skipAmount;
            while (skip-- != 0)
                if (fread(&a, 1, 1, stdin) < 1) return false; else position++;

            return true;
        }
        /** Get the current instance of this stream */
        static StdInStream & getInstance() { static StdInStream stream; return stream; }

        // Construction
    private:
        /** The default constructor is private */
        StdInStream() : position(0) {}
    };

    /** The base output stream interface */
    class OutputStream : public BaseStream
    {
    protected:
        /** The default constructor is only available to child classes */
        OutputStream() {}

        // The interface
    public:
        /** Try to write the given amount of data to the specified buffer
            @return the number of byte truly written (this method doesn't throw) */
        virtual uint64 write(const void * const buffer, const uint64 size) throw() = 0;
        /** Try to write the given amount of data to the specified buffer.
            Unlike the former version, this version also tells if the stream needs to be flushed */
        virtual uint64 write(const void * const buffer, const uint64 size, const bool flush) throw() { return write(buffer, size); }

        /** A useful helper when what you write is typed and you don't react on errors, just bailing out.
            @param val  The value to write onto
            @return true when val was completely written into the stream */
        template <typename T>
        bool write(const T & val) throw() { return write(&val, (const uint64)sizeof(val)) == (uint64)sizeof(val); }
        /** A useful helper when what you write is typed and you don't react on errors, just bailing out.
            @param val  The value to write onto
            @return true when val was completely written into the stream */
        template <typename T, size_t N>
        bool write(const T (&val)[N]) throw() { return write(&val[0], (const uint64)(sizeof(val[0]) * N)) == (uint64)(sizeof(val[0]) * N); }
        /** A useful helper for writing strings to this stream.
            @param val  The string to write to the stream.
            @return true when val was completely written into the stream */
        bool write(const Strings::FastString & val) throw() { return write((const unsigned char*)val, (const uint64)val.getLength()) == (uint64)val.getLength(); }
        /** A useful helper to write a memory block to this stream.
            @param val  The memory block to write to the stream.
            @return true when val was completely written into the stream */
        bool write(const Utils::MemoryBlock & val) throw() { return write(val.getConstBuffer(), (const uint64)val.getSize()) == (uint64)val.getSize(); }
    };

    namespace Private
    {
        /** @internal */
        template <class String>
        String readNextLine(const InputStream & is, const bool allowCRAtEOL, String *) throw()
        {
            Strings::FastString s; char buffer[256], ch = 0;
            int positionInBuffer = 0;

            while (!is.endReached())
            {
                if (is.read(&ch, sizeof(ch)) != sizeof(ch)) break;
                if (ch == '\n') break;

                buffer[positionInBuffer++] = ch;
                if (positionInBuffer == 256) { s += String(buffer, positionInBuffer); positionInBuffer = 0; }
            }
            if (positionInBuffer) s += String(buffer, buffer[positionInBuffer - 1] == '\r' && !allowCRAtEOL ? positionInBuffer - 1 : positionInBuffer);
            return s;
        }
    }

    /** An interface version that can split the input in lines 
        @param String  The string class that's used. It must support this interface:
                       String() and  String(const char * buffer, int len) and operator += (const String & other)*/
    template <class String>
    class LineSplitStream : public InputStream
    {
    protected:
        /** The default constructor is only available to child classes */
        LineSplitStream() {}

        // The interface
    public:
        /** Read the next line
            @return The String containing the next line (should end by LF or CRLF) */
        virtual String readNextLine() const throw() { return Private::readNextLine(*this, true, (String*)0); }
    };
    
    /** Transform an input stream to a line split stream */
    class LineBasedInputStream
    {
        const InputStream & is;
    public:
        /** Read the next line from the input stream. 
            @param allowCRAtEOL   If true, allow \r at end of line (so line ended by \r\n will be returned ended by \r only). 
                                  Else, the line is trimmed from both \r and \n */
        virtual Strings::FastString readNextLine(const bool allowCRAtEOL = true) const throw() { return Private::readNextLine(is, allowCRAtEOL, (Strings::FastString*)0); }
        /** Default constructor */
        LineBasedInputStream(const InputStream & is) : is(is) {}
    };

    /** A File-based input stream */
    class InputFileStream : public LineSplitStream<Strings::FastString>
    {
    protected:
        /** The string implementation we are using */
        typedef Strings::FastString String;

        // Members
    private:
        /** The filename */
        String                  fileName;
        /** The file handle */
        void             *      stream;
        /** The file size */
        uint64                  fileSize;

        // The interface
    public:
        /** This method returns the stream length in byte, if known
            If the length is equal or higher than 2^32 - 1, the returned value is 0xfffffffe
        */
        virtual uint64 fullSize() const;
        /** This method returns true if the end of stream is reached */
        virtual bool endReached() const;
        /** This method returns the position of the next byte that could be read from this stream */
        virtual uint64 currentPosition() const;
        /** Try to seek to the given absolute position (return false if not supported) */
        virtual bool setPosition(const uint64 newPos);
        /** Move the stream position forward of the given amount
            This should give the same results as setPosition(currentPosition() + value),
            but implementation can be faster for non-seek-able stream. */
        virtual bool goForward(const uint64 skipAmount);
        /** Try to read the given amount of data to the specified buffer
            @return the number of byte truly read (this method doesn't throw) */
        virtual uint64 read(void * const buffer, const uint64 size) const throw();

        // Construction
    public:
        /** The only allowed constructor */
        InputFileStream(const String & name);
        /** The only allowed destructor */
        ~InputFileStream();

    public:
        /** Allow copying */
        InputFileStream(const InputFileStream & other);
    };


    /** A File-based output stream */
    class OutputFileStream : public OutputStream
    {
    protected:
        /** The string implementation we are using */
        typedef Strings::FastString String;

        // Members
    private:
        /** The filename */
        String              fileName;
        /** The file stream */
        void             *  stream;
        /** The file size */
        uint64              fileSize;

        // Helpers
    private:
        /** Open the file to write to */
        bool openFile();
        // The interface
    public:
        /** This method returns the stream length in byte, if known
            If the length is equal or higher than 2^32 - 1, the returned value is 0xfffffffe
        */
        virtual uint64 fullSize() const;
        /** This method returns true if the end of stream is reached */
        virtual bool endReached() const;
        /** This method returns the position of the next byte that could be read from this stream */
        virtual uint64 currentPosition() const;
        /** Try to seek to the given absolute position (return false if not supported) */
        virtual bool setPosition(const uint64 newPos);
        /** Try to write the given amount of data to the specified buffer
            @return the number of byte truly write (this method doesn't throw)
        */
        virtual uint64 write(const void * const buffer, const uint64 size) throw();


        // Construction
    public:
        /** The only allowed constructor
            @param name             The file name to write to
            @param delayedOpening   Will the file opening be delayed until first write ? */
        OutputFileStream(const String & name, bool delayedOpening = false);
        /** The only allowed destructor */
        ~OutputFileStream();

    private:
        /** Deny copying */
        OutputFileStream(const OutputFileStream &);
    };

    /** A string-based input stream */
    class InputStringStream : public LineSplitStream<Strings::FastString>
    {
    protected:
        /** The string implementation we are using */
        typedef Strings::FastString String;

        // Members
    private:
        /** The content */
        String          content;
        /** The current position */
        mutable uint64  position;

        // The interface
    public:
        /** This method returns the stream length in byte, if known
            If the length is equal or higher than 2^32 - 1, the returned value is 0xfffffffe */
        virtual uint64 fullSize() const;
        /** This method returns true if the end of stream is reached */
        virtual bool endReached() const;
        /** This method returns the position of the next byte that could be read from this stream */
        virtual uint64 currentPosition() const;
        /** Try to seek to the given absolute position (return false if not supported) */
        virtual bool setPosition(const uint64 newPos);
        /** Move the stream position forward of the given amount
            This should give the same results as setPosition(currentPosition() + value),
            but implementation can be faster for non-seek-able stream. */
        virtual bool goForward(const uint64 skipAmount);
        /** Try to read the given amount of data to the specified buffer
            @return the number of byte truly read (this method doesn't throw) */
        virtual uint64 read(void * const buffer, const uint64 size) const throw();

        // Our specific interface
    public:
        /** Reset the content string and position */
        void resetStream(const String & content);

        // Construction
    public:
        /** The only allowed constructor */
        InputStringStream(const String & content);

    public:
        /** Allow copying */
        InputStringStream(const InputStringStream & other) : content(other.content), position(other.position) {}
    };


    /** A string-based output stream */
    class OutputStringStream : public OutputStream
    {
    protected:
        /** The string implementation we are using */
        typedef Strings::FastString String;

        // Members
    private:
        /** The reference on the content */
        String  &       content;
        /** The current position */
        mutable uint64  position;

        // The interface
    public:
        /** This method returns the stream length in byte, if known
            If the length is equal or higher than 2^32 - 1, the returned value is 0xfffffffe
        */
        virtual uint64 fullSize() const;
        /** This method returns true if the end of stream is reached */
        virtual bool endReached() const;
        /** This method returns the position of the next byte that could be read from this stream */
        virtual uint64 currentPosition() const;
        /** Try to seek to the given absolute position (return false if not supported) */
        virtual bool setPosition(const uint64 newPos);
        /** Move the stream position forward of the given amount
            This should give the same results as setPosition(currentPosition() + value),
            but implementation can be faster for non-seek-able stream. */
        virtual bool goForward(const uint64 skipAmount);
        /** Try to write the given amount of data to the specified buffer
            @return the number of byte truly write (this method doesn't throw) */
        virtual uint64 write(const void * const buffer, const uint64 size) throw();


        // Construction
    public:
        /** The only allowed constructor */
        OutputStringStream(String & content);

    private:
        /** Deny copying */
        OutputStringStream(const OutputStringStream &);
    };
    
    /** An output stream that's using a resizable memory buffer underneath. */
    class OutputMemStream : public OutputStream, public MappableStream
    {
        // Members
    private:
        /** The reference on the content */
        Utils::MemoryBlock content;
        /** The current position */
        mutable uint64     position;

        // The interface
    public:
        /** This method returns the stream length in byte, if known
            If the length is equal or higher than 2^32 - 1, the returned value is 0xfffffffe */
        virtual uint64 fullSize() const { return content.getSize(); }
        /** This method returns true if the end of stream is reached */
        virtual bool endReached() const { return position == content.getSize(); }
        /** This method returns the position of the next byte that could be read from this stream */
        virtual uint64 currentPosition() const { return position; }
        /** Try to seek to the given absolute position (return false if not supported) */
        virtual bool setPosition(const uint64 newPos) { if (newPos > content.getSize()) return false; position = newPos; return true;}
        /** Move the stream position forward of the given amount
            This should give the same results as setPosition(currentPosition() + value),
            but implementation can be faster for non-seek-able stream. */
        virtual bool goForward(const uint64 skipAmount) { return setPosition(currentPosition() + skipAmount); } 
        /** Try to write the given amount of data to the specified buffer
            @return the number of byte truly write (this method doesn't throw) */
        virtual uint64 write(const void * const buffer, const uint64 size) throw()
        {
            // Never fill the 4GB anyway
            if (position + size > 0xFFFFFFFF) return 0;
            // Might need to enlarge the buffer, so let's do it now
            if (size + position > content.getSize()
                && !content.Append(0, (uint32)(size + position) - content.getSize()))
                return (uint64)-1;

            if (buffer) memcpy(content.getBuffer() + position, buffer, (size_t)size);
            position += size;
            return size;
        }
        /** Get the buffer directly */
        virtual const uint8 * getBuffer() const { return content.getConstBuffer(); }
        /** Get the mappable interface */
        virtual MappableStream * getMappable() { return (MappableStream*)this; }
        /** Get the mappable interface */
        virtual const MappableStream * getMappable() const { return (const MappableStream*)this; }

        // Construction
    public:
        /** The only allowed constructor */
        OutputMemStream(const uint32 startSize = 0) : content(startSize), position(0) {}

    private:
        /** Deny copying */
        OutputMemStream(const OutputMemStream &);
    };


    /** A buffered input stream based on a existing memory block */
    class MemoryBlockStream : public InputStream, public MappableStream
    {
        // Members
    private:
        /** The buffer */
        const uint8 *       buffer;
        /** The buffer length */
        uint64              length;
        /** The current position in block */
        mutable uint64      position;
        /** Delete the block on exit */
        bool                own;

        // The interface
    public:
        /** This method returns the stream length in byte, if known
            If the length is equal or higher than 2^32 - 1, the returned value is 0xfffffffe
        */
        virtual uint64 fullSize() const  { return length;}
        /** This method returns true if the end of stream is reached */
        virtual bool endReached() const  { return position >= length; }
        /** This method returns the position of the next byte that could be read from this stream */
        virtual uint64 currentPosition() const { return position; }
        /** Try to seek to the given absolute position (return false if not supported) */
        virtual bool setPosition(const uint64 newPos) { if (newPos < length) { position = newPos; return true; } return false; }
        /** Move the stream position forward of the given amount
            This should give the same results as setPosition(currentPosition() + value),
            but implementation can be faster for non-seek-able stream. */
        virtual bool goForward(const uint64 skipAmount) { if (skipAmount + position > length) return false; position += skipAmount; return true; }
        /** Try to read the given amount of data to the specified buffer
            @return the number of byte truly read (this method doesn't throw) */
        virtual uint64 read(void * const _buffer, const uint64 size) const throw()
        {
            uint32 amount = (uint32)min((uint64)0xFFFFFFFF,    (length - position) < size ? (length - position) : size);
            memcpy(_buffer, &buffer[position], amount);
            position += amount;
            return amount;
        }

        /** Get the buffer */
        virtual const uint8 * getBuffer() const { return buffer; }
        /** Get the mappable interface */
        virtual const MappableStream * getMappable() const { return (const MappableStream*)this; }

        // Construction
    public:
        /** Construction from memory block (it's not copied) */
        MemoryBlockStream(const uint8 * data, const uint64 len, const bool own = false) 
            : buffer(data), length(len), position(0), own(own)    {}
        /** The only allowed destructor */
        ~MemoryBlockStream()     { if (own) delete[] buffer; buffer = 0; length = 0; position = 0;}

    public:
        /** Allow copying 
            @warning This method logic is transitive. If the MemoryBlockStream to copy owns the buffer, 
                     then it's changed not to own it anymore, and instead transfer ownership to this object */
        MemoryBlockStream(const MemoryBlockStream & other)
            : buffer(other.buffer), length(other.length), position(other.position), own(other.own)
        {
            if (other.own) const_cast<bool &>(other.own) = false;
        }
    };
    /** A buffered output stream based on a existing memory block */
    class MemoryBlockOutStream : public OutputStream, public MappableStream
    {
        // Members
    private:
        /** The buffer */
        uint8 *             buffer;
        /** The buffer length */
        uint64              length;
        /** The current position in block */
        mutable uint64      position;

        // The interface
    public:
        /** This method returns the stream length in byte, if known
            If the length is equal or higher than 2^32 - 1, the returned value is 0xfffffffe
        */
        virtual uint64 fullSize() const  { return length;}
        /** This method returns true if the end of stream is reached */
        virtual bool endReached() const  { return position == length; }
        /** This method returns the position of the next byte that could be read from this stream */
        virtual uint64 currentPosition() const { return position; }
        /** Try to seek to the given absolute position (return false if not supported) */
        virtual bool setPosition(const uint64 newPos) { if (newPos <= length) { position = newPos; return true; } return false; }
        /** Move the stream position forward of the given amount
            This should give the same results as setPosition(currentPosition() + value),
            but implementation can be faster for non-seek-able stream.
        */
        virtual bool goForward(const uint64 skipAmount) { if (position + skipAmount <= length) { position += skipAmount; return true; } return false; }
        /** Try to write the given amount of data to the specified buffer
            @return the number of byte truly written (this method doesn't throw)
        */
        virtual uint64 write(const void * const _buffer, const uint64 _size) throw()
        {
            if (!_size) return 0;
            if (!buffer) return (uint64)-1;

            size_t size = (size_t)min(length - position, _size);
            memcpy((uint8*)&buffer[position], _buffer, size);
            position = position + (uint64)size;
            return size;
        }

        /** Get the buffer */
        virtual uint8 * getBuffer() { return buffer; }
        /** Get the buffer */
        virtual const uint8 * getBuffer() const { return buffer; }
        /** Get the mappable interface */
        virtual MappableStream * getMappable() { return (MappableStream*)this; }
        /** Get the mappable interface */
        virtual const MappableStream * getMappable() const { return (const MappableStream*)this; }

        // Construction
    public:
        /** Construction from memory block (it's not owned) */
        MemoryBlockOutStream(uint8 * data, const uint64 len) : buffer(data), length(len), position(0)
        {
        }
        /** The only allowed destructor */
        ~MemoryBlockOutStream()     { buffer = 0; length = 0; position = 0;}

    private:
        /** Deny copying */
        MemoryBlockOutStream(const MemoryBlockOutStream &);
    };


    /** A buffered memory-based input stream.
        This class reads the given input stream completely in a memory buffer. */
    class MemoryBufferedInputStream : public InputStream, public MappableStream
    {
        // Members
    private:
        /** The given input stream as reference */
        const InputStream & inputStream;
        /** The buffer */
        uint8       *       buffer;
        /** The current position in the (pseudo read) stream */
        mutable uint64      currentPos;

        // The interface
    public:
        /** This method returns the stream length in byte, if known
            If the length is equal or higher than 2^32 - 1, the returned value is 0xfffffffe
        */
        virtual uint64 fullSize() const  { return inputStream.fullSize();}
        /** This method returns true if the end of stream is reached */
        virtual bool endReached() const  { return currentPos == inputStream.fullSize(); }
        /** This method returns the position of the next byte that could be read from this stream */
        virtual uint64 currentPosition() const { return currentPos; }
        /** Try to seek to the given absolute position (return false if not supported) */
        virtual bool setPosition(const uint64 newPos) { if (newPos >= inputStream.fullSize()) return false; currentPos = newPos; return true; }
        /** Move the stream position forward of the given amount
            This should give the same results as setPosition(currentPosition() + value),
            but implementation can be faster for non-seek-able stream. */
        virtual bool goForward(const uint64 skipAmount) { if (skipAmount + currentPos >= inputStream.fullSize()) return false; currentPos += skipAmount; return true; }
        /** Try to read the given amount of data to the specified buffer
            @return the number of byte truly read (this method doesn't throw) */
        virtual uint64 read(void * const _buffer, const uint64 size) const throw() 
        {
            uint64 readSize = (uint64)min((int64)(inputStream.fullSize() - currentPos), (int64)size);
            if (readSize > 0xFFFFFFFF) readSize = 0xFFFFFFFF;
            memcpy(_buffer, &buffer[currentPos], (size_t)readSize);
            currentPos += readSize;
            return readSize; 
        }

        /** Get the buffer */
        virtual const uint8 * getBuffer() const { return buffer; }
        /** Get the mappable interface */
        virtual const MappableStream * getMappable() const { return (const MappableStream*)this; }

        // Construction
    public:
        /** The only allowed constructor */
        MemoryBufferedInputStream(const InputStream & is, const bool zeroTerminated = true)
            : inputStream(is), buffer(0), currentPos(0)
        {
            if (is.fullSize() < 0xfffffffe)
            {
                buffer = new uint8[(uint32)is.fullSize() + (zeroTerminated ? 1 : 0)];
                if (!buffer) return;
                if (is.read(buffer, is.fullSize()) != is.fullSize())
                {
                    delete[] buffer;
                    buffer = 0;
                }
                // Last char is set to 0 so it can be interpreted as text
                if (zeroTerminated && buffer) buffer[(uint32)is.fullSize()] = 0;
            }
        }
        /** The only allowed destructor */
        ~MemoryBufferedInputStream()     { if (buffer) delete[] buffer; buffer = 0; }

    private:
        /** Deny copying */
        MemoryBufferedInputStream(const MemoryBufferedInputStream &);
    };

    /** A buffered input stream that reads in block on the given stream.
        If only read, the underlying inputstream must support returning a monotonic currentPos.
        If it's needed to seek in the given input stream, please make sure the feature is supported for the underlying stream. */
    class BufferedInputStream : public InputStream
    {
        // Members
    private:
        /** The given input stream as reference */
        Utils::OwnPtr<InputStream> inputStream;
        /** The buffer */
        mutable uint8       *        buffer;
        /** The current size */
        mutable uint32               bufferSize;
        /** The construction size */
        const uint32                 bufferInitialSize;
        /** The current position in the (pseudo read) stream */
        mutable uint64               currentPos;

        // The interface
    public:
        /** Get the buffer size */
        uint32 getBufferSize() const { return bufferSize; }
        /** This method returns the stream length in byte, if known
            If the length is equal or higher than 2^32 - 1, the returned value is 0xfffffffe */
        virtual uint64 fullSize() const  { return inputStream->fullSize();}
        /** This method returns true if the end of stream is reached */
        virtual bool endReached() const  { return buffer && currentPos == inputStream->fullSize(); }
        /** This method returns the position of the next byte that could be read from this stream */
        virtual uint64 currentPosition() const { return currentPos; }
        /** Try to seek to the given absolute position (return false if not supported) */
        virtual bool setPosition(const uint64 newPos) 
        { 
            if (currentPos == newPos) return true;
            if (newPos >= inputStream->fullSize()) return false;
            // Check if seeking is actually required on the input stream
            uint64 hiPos = inputStream->currentPosition();
            uint64 lowPos = hiPos - bufferSize;
            // Is buffer refilling required ?
            if (newPos >= lowPos && newPos < hiPos)
            {
                // No, it's not, let change the buffer positions
                currentPos = newPos;
                return true;
            }
            // Optimization to avoid using setPosition that might not be accessible
            uint64 basePos = (newPos / bufferInitialSize) * bufferInitialSize;
            if (newPos > hiPos)
            {
                if (!inputStream->goForward(basePos - hiPos)) return false;
            } else
            {
                if (!inputStream->setPosition(basePos)) return false;
            }
            if (!refillBuffer()) return false;
            currentPos = newPos; 
            return true; 
        }
        /** Move the stream position forward of the given amount
            This should give the same results as setPosition(currentPosition() + value),
            but implementation can be faster for non-seek-able stream. */
        virtual bool goForward(const uint64 skipAmount) 
        { 
            if (skipAmount + currentPos >= inputStream->fullSize()) return false; 
            return setPosition(currentPos + skipAmount); 
        }
        /** Try to read the given amount of data to the specified buffer
            @return the number of byte truly read (this method doesn't throw) */
        virtual uint64 read(void * const _buffer, const uint64 size) const throw() 
        {
            // Need to compute the positions and check the buffer
            if (!bufferSize) return (uint64)-1;
            uint64 done = 0;
            while (done < size)
            {
                // Check how many bytes are left in the buffer
                uint32 bytesInBuffer = (uint32)(inputStream->currentPosition() - currentPos);
                // Let's empty the buffer first
                uint32 amount = (uint32)min(size - done, (uint64)bytesInBuffer);
                if (_buffer) memcpy(&((uint8*)_buffer)[done], &buffer[bufferSize - bytesInBuffer], amount);
                currentPos += amount;
                done += amount;
                if (done == size) break;
                if (inputStream->endReached()) return done;
                if (!refillBuffer()) return (uint64)-1;
            }
            return done; 
        }

        /** Refill the buffer */
        inline bool refillBuffer() const
        { 
            if (!buffer) return false;
            bufferSize = (uint32)inputStream->read(buffer, bufferInitialSize);
            if (bufferSize == (uint32)-1)
            {
                bufferSize = 0;
                return false;
            }
            return true;
        }

        // Construction
    public:
        /** Construct a buffered input stream.
            @param is           A reference on a stream that should exists for the whole lifetime of this object
            @param bufferSize   The buffer size to use while reading the stream */
        BufferedInputStream(InputStream & is, const uint32 bufferSize = 32768) 
            : inputStream(is), buffer(0), bufferSize(bufferSize), bufferInitialSize(bufferSize), currentPos(0)
        {
            buffer = new uint8[bufferSize];
            refillBuffer();
        }
        /** Construct a buffered input stream.
            @param is           A pointer of a input stream that's owned by the object
            @param bufferSize   The buffer size to use while reading the stream */
        BufferedInputStream(InputStream * is, const uint32 bufferSize = 32768) 
            : inputStream(is), buffer(0), bufferSize(bufferSize), bufferInitialSize(bufferSize), currentPos(0)
        {
            buffer = new uint8[bufferSize];
            refillBuffer();
        }       
        /** The only allowed destructor */
        ~BufferedInputStream()     { delete[] buffer; buffer = 0; }

    private:
        /** Deny copying */
        BufferedInputStream(const BufferedInputStream &);
    };
    
    
    /** A buffered memory-based input stream */
    class MemoryBufferedOutputStream : public OutputStream
    {
        // Members
    private:
        /** The given output stream as reference */
        OutputStream &      outputStream;
        /** The buffer */
        uint8       *       buffer;
        /** The buffer size */
        uint64              size;
        /** Is the buffer dirty */
        bool                isDirty;

        // The interface
    public:
        /** This method returns the stream length in byte, if known
            If the length is equal or higher than 2^32 - 1, the returned value is 0xfffffffe
        */
        virtual uint64 fullSize() const  { return outputStream.fullSize();}
        /** This method returns true if the end of stream is reached */
        virtual bool endReached() const  { return true; }
        /** This method returns the position of the next byte that could be read from this stream */
        virtual uint64 currentPosition() const { return outputStream.fullSize(); }
        /** Try to seek to the given absolute position (return false if not supported) */
        virtual bool setPosition(const uint64) { return false; }
        /** Move the stream position forward of the given amount
            This should give the same results as setPosition(currentPosition() + value),
            but implementation can be faster for non-seek-able stream.
        */
        virtual bool goForward(const uint64) { return false; }
        /** Try to write the given amount of data to the specified buffer
            @return the number of byte truly written (this method doesn't throw)
        */
        virtual uint64 write(const void * const, const uint64) throw() { return 0; }

        /** Get the buffer
            @param newSize  The minimal required size
            @return A pointer on the allocated buffer */
        inline uint8 * getBufferOfSize(const uint32 newSize) { isDirty = true; return size >= newSize ? buffer : (buffer = (uint8*)Platform::safeRealloc(buffer, (size_t)(size = newSize))); }
        /** Write the buffer to the output stream
            @return true on success */
        inline bool deliverBuffer() { isDirty = false; return outputStream.write(buffer, size) == size; }

        // Construction
    public:
        /** The only allowed constructor */
        MemoryBufferedOutputStream(OutputStream & os) : outputStream(os), buffer(0), size(0), isDirty(false)  { }
        /** The only allowed destructor */
        ~MemoryBufferedOutputStream()     { if (buffer && isDirty) deliverBuffer(); Platform::safeRealloc(buffer, 0); buffer = 0; size = 0; isDirty = false; }

    private:
        /** Deny copying */
        MemoryBufferedOutputStream(const MemoryBufferedOutputStream &);
    };

    /** A stream that doesn't output anything.
        This is useful for testing, or computing a hash.
        It does track the amount of written data however. */
    class NullOutputStream : public OutputStream
    {
        uint64 size;
    public:
        /** This method returns the stream length in byte, if known
            If the length is equal or higher than 2^32 - 1, the returned value is 0xfffffffe
            For stream where the length is not known, this method will return 0xffffffff.
        */
        virtual uint64 fullSize() const { return size; }
        /** This method returns true if the end of stream is reached */
        virtual bool endReached() const { return false; }
        /** This method returns the position of the next byte that could be read from this stream */
        virtual uint64 currentPosition() const { return size; }
        /** Try to seek to the given absolute position (return false if not supported) */
        virtual bool setPosition(const uint64 newPos) { size = newPos; return true; }
        /** Try to write the given amount of data to the specified buffer
            @return the number of byte truly written (this method doesn't throw) */
        virtual uint64 write(const void * const, const uint64 size) throw() { this->size += size; return size; }

        NullOutputStream() : size(0) {}
    };
    
    /** A stream that duplicate its operations on both of its output stream */
    class TeeStream : public OutputStream
    {
        OutputStream & one;
        OutputStream & two;
    public:
        /** This method returns the stream length in byte, if known
            If the length is equal or higher than 2^32 - 1, the returned value is 0xfffffffe
            For stream where the length is not known, this method will return 0xffffffff.
        */
        virtual uint64 fullSize() const { return min(one.fullSize(), two.fullSize()); }
        /** This method returns true if the end of stream is reached */
        virtual bool endReached() const { return one.endReached() || two.endReached(); }
        /** This method returns the position of the next byte that could be read from this stream */
        virtual uint64 currentPosition() const { return one.currentPosition(); }
        /** Try to seek to the given absolute position (return false if not supported) */
        virtual bool setPosition(const uint64 newPos) 
        { 
            if (!one.setPosition(newPos)) return false; 
            if (!two.setPosition(newPos)) return one.setPosition(two.currentPosition()) && false;  
            return true; 
        }
        /** Try to write the given amount of data to the specified buffer
            @return the number of byte truly written (this method doesn't throw) */
        virtual uint64 write(const void * const buffer, const uint64 size) throw() 
        { 
            uint64 out = one.write(buffer, size), out2 = two.write(buffer, out);
            if (out2 != out) one.setPosition(one.currentPosition() - (out - out2)); 
            return out2; 
        }
        
        /** Construct the stream with the two output stream.
            @param one  The first (and master) stream (if you have one stream that doesn't track its position don't use it here)
            @param two  The second (and slave) stream (if you have one stream that doesn't track its position use it here) */
        TeeStream(OutputStream & one, OutputStream & two) : one(one), two(two) {}
    };

    /** An input stream using two input streams read successively, but that appears like a single stream when read.
        The first stream is read fully before the second stream is read in turn. 
        The two initial input streams can be of different type. */
    class SuccessiveStream : public InputStream
    {
        InputStream & one, & two;
        mutable uint64 pos; 
    public:
        /** Try to read the given amount of data to the specified buffer
            @return the number of byte truly read (this method doesn't throw) */
        inline uint64 read(void * const buffer, const uint64 size) const throw() 
        {
            uint64 readFromFirst = max((int64)min(size, (one.fullSize() - pos)), (int64)0);
            uint64 readFromSecond = size - readFromFirst;
            uint64 firstRead = readFromFirst ? one.read(buffer, readFromFirst) : 0;
            pos += firstRead; 
            if (firstRead != readFromFirst) return firstRead; 
            uint64 secondRead = readFromSecond ? two.read((char* const)buffer + firstRead, readFromSecond) : 0;
            return secondRead + firstRead; 
        } 
        /** Move the stream position forward of the given amount
            This should give the same results as setPosition(currentPosition() + value),
            but implementation can be faster for non-seek-able stream. */
        inline bool goForward(const uint64 skipAmount) 
        { 
            if (skipAmount + pos > (one.fullSize() + two.fullSize())) return false; 
            uint64 skipFromFirst = max((int64)min(skipAmount, (one.fullSize() - pos)), (int64)0);
            if (skipFromFirst && !one.goForward(skipFromFirst)) return false;
            pos += skipFromFirst;
            return two.goForward(skipAmount - skipFromFirst);
        }
        /** This method returns the stream length in byte, if known
            If the length is equal or higher than 2^32 - 1, the returned value is (uint64)-2
            For stream where the length is not known, this method will return (uint64)-1. */
        inline uint64 fullSize() const { return one.fullSize() + two.fullSize(); }
        /** This method returns true if the end of stream is reached */
        inline bool endReached() const { return pos < one.fullSize() ? false : two.endReached(); }
        /** This method returns the position of the next byte that could be read from this stream */
        inline uint64 currentPosition() const { return pos < one.fullSize() ? pos : (pos + two.currentPosition()); }
        /** Try to seek to the given absolute position (return false if not supported) */
        inline bool setPosition(const uint64 newPos) 
        { 
            if (newPos < one.fullSize()) { pos = newPos; return one.setPosition(newPos); } 
            pos = one.fullSize(); return two.setPosition(newPos - one.fullSize()); 
        }
        /** Get the first stream (you usually don't need this) */
        inline InputStream & getFirstStream() { return one; }
        /** Get the second stream (you usually don't need this) */
        inline InputStream & getSecondStream() { return two; }

        /** Basic construction */
        SuccessiveStream(InputStream & one, InputStream & two) : one(one), two(two), pos(0) {}
    };
    

#if defined(HasBaseEncoding)
    /** A encoding based stream decoding 
        This process on-the-fly decoding while reading, so it's more convenient than buffer the whole input stream first 
        and decode it. */
    class Base64InputStream : public InputStream
    {
        // Members
    private:
        /** The given input stream as reference */
        InputStream & inputStream;
        /** The memory block used for input */
        mutable Utils::MemoryBlock  memoryBlock;
        /** The block size to deal upon */
        const uint32        blockSize;
        
        // Helpers
    private:
        /** Convert a size from Base64 to the real decoded size */
        uint64 convertSize(uint64 inSize) const { return ((inSize % 3) != 0 ? (inSize / 3 + 1) * 4 : (inSize / 3) * 4); }
       
        // The interface
    public:
        /** This method returns the stream length in byte, if known
            If the length is equal or higher than 2^32 - 1, the returned value is 0xfffffffe */
        virtual uint64 fullSize() const  { return convertSize(inputStream.fullSize()); }
        /** This method returns true if the end of stream is reached */
        virtual bool endReached() const  { return inputStream.endReached() && memoryBlock.getSize(); }
        /** This method returns the position of the next byte that could be read from this stream */
        virtual uint64 currentPosition() const { return convertSize(inputStream.currentPosition()) - memoryBlock.getSize(); }
        /** Try to seek to the given absolute position (return false if not supported) */
        virtual bool setPosition(const uint64 newPos)
        {   // Can't seek backward, in CFB mode it's not possible without decoding the whole stream again.
            if (newPos > currentPosition()) return goForward(newPos - currentPosition());
            return false;
        }
        /** Move the stream position forward of the given amount
            This should give the same results as setPosition(currentPosition() + value),
            but implementation can be faster for non-seek-able stream. */
        virtual bool goForward(const uint64 skipAmount);
        /** Try to read the given amount of data to the specified buffer
            @return the number of byte truly read (this method doesn't throw) */
        virtual uint64 read(void * const buffer, const uint64 size) const throw();
    
    public:
        /** Build the input stream  */
        Base64InputStream(InputStream & is, const uint32 blockSize = 4096) 
            : inputStream(is), blockSize((blockSize + 3) & ~3) {}
    };
    
    /** A encoding based stream encoding 
        This process on-the-fly encoding while writing, so it's more convenient than 
        buffer the whole output stream first and decode it. */
    class Base64OutputStream : public OutputStream
    {
        // Members
    private:
        /** The given input stream as reference */
        OutputStream & outputStream;
        /** The memory block used for input */
        Utils::MemoryBlock  memoryBlock;
        /** The block size to deal upon */
        const uint32        blockSize;
        
        // Helpers
    private:
        /** Convert a size from Base64 to the real decoded size */
        uint64 unconvertSize(uint64 inSize) const { return (inSize * 3 / 4) + 1; }
       
        // The interface
    public:
        /** This method returns the stream length in byte, if known
            If the length is equal or higher than 2^32 - 1, the returned value is 0xfffffffe */
        virtual uint64 fullSize() const  { return outputStream.fullSize() + unconvertSize(memoryBlock.getSize()); }
        /** This method returns true if the end of stream is reached */
        virtual bool endReached() const  { return true; }
        /** This method returns the position of the next byte that could be read from this stream */
        virtual uint64 currentPosition() const { return fullSize(); }
        /** Try to seek to the given absolute position (return false if not supported) */
        virtual bool setPosition(const uint64) { return false; }
        /** Move the stream position forward of the given amount
            This should give the same results as setPosition(currentPosition() + value),
            but implementation can be faster for non-seek-able stream. */
        virtual bool goForward(const uint64) { return false; }
        /** Try to write the given amount of data to the specified buffer
            @return the number of byte truly written (this method doesn't throw) */
        virtual uint64 write(const void * const _buffer, const uint64 _size) throw();
        /** Flush the stream.
            Because Base64 works in blocks, it's required to actually flush the stream to have any pending data written.
            You can also destruct this object if you want to. */
        virtual bool Flush() throw();
    
    public:
        /** Build the output stream  */
        Base64OutputStream(OutputStream & os, const uint32 blockSize = 4096) 
            : outputStream(os), blockSize((blockSize * 4) / 3) {}
        /** Destroy the stream */
        ~Base64OutputStream() { Flush(); }            
    };    
#endif

#if defined(HasAES)
    /** A symmetric crypto using AES input stream.
        This method use CFB mode for AES.
        This is because the stream doesn't have to be a multiple of the ciphertext block size.
        This stream will probably try to read more data than expected on the input stream, this is because
        the decryption works in blocks.
        This is the on-the-fly decryption version. */
    class AESInputStream : public InputStream
    {
    public:
        /** The string implementation we are using */
        typedef Strings::FastString String;

        // Members
    private:
        /** The given input stream as reference */
        const InputStream & inputStream;
        /** The AES object */
        mutable Crypto::AES crypto;
        /** The temporary buffer (this is required for processing) */
        mutable uint8       buffer[32];
        /** The current position in the temporary buffer */
        mutable uint16      tempPos;
        /** The used key size in byte */
        const uint16        keySize;

        // The interface
    public:
        /** This method returns the stream length in byte, if known
            If the length is equal or higher than 2^32 - 1, the returned value is 0xfffffffe */
        virtual uint64 fullSize() const  { return inputStream.fullSize();}
        /** This method returns true if the end of stream is reached */
        virtual bool endReached() const  { return inputStream.endReached() && tempPos == keySize; }
        /** This method returns the position of the next byte that could be read from this stream */
        virtual uint64 currentPosition() const { return inputStream.currentPosition() - (keySize - tempPos); }
        /** Try to seek to the given absolute position (return false if not supported) */
        virtual bool setPosition(const uint64 newPos)
        {   // Can't seek backward, in CFB mode it's not possible without decoding the whole stream again.
            if (newPos > currentPosition()) return goForward(newPos - currentPosition());
            return false;
        }
        /** Move the stream position forward of the given amount
            This should give the same results as setPosition(currentPosition() + value),
            but implementation can be faster for non-seek-able stream. */
        virtual bool goForward(const uint64 skipAmount);
        /** Try to read the given amount of data to the specified buffer
            @return the number of byte truly read (this method doesn't throw) */
        virtual uint64 read(void * const buffer, const uint64 size) const throw();



        // Construction
    public:
        /** Construct an input stream with the given decryption key and initialization vector */
        AESInputStream(const InputStream & is, const String & keyInHex, const String & IVInHex);
        /** Construct an input stream with the given decryption key and initialization vector */
        AESInputStream(const InputStream & is, const uint8 * key, const uint32 keySize, const uint8* iv, const uint32 ivSize);
        /** The only allowed destructor */
        ~AESInputStream()     {  }

    private:
        /** Deny copying */
        AESInputStream(const AESInputStream &);
    };

    /** The AES output stream.
        This is the on-the-fly encryption version.
        @sa AES input stream */
    class AESOutputStream : public OutputStream
    {
    public:
        /** The string implementation we are using */
        typedef Strings::FastString String;

        // Members
    private:
        /** The given output stream as reference */
        OutputStream &      outputStream;
        /** The AES object */
        mutable Crypto::AES crypto;
        /** The temporary buffer (this is required for processing) */
        mutable uint8       buffer[32];
        /** The current position in the temporary buffer */
        mutable uint16      tempPos;
        /** The used key size in byte */
        const uint16        keySize;

        // The interface
    public:
        /** This method returns the stream length in byte, if known
            If the length is equal or higher than 2^64 - 1, the returned value is -2 */
        virtual uint64 fullSize() const  { return outputStream.fullSize() + (keySize - tempPos); }
        /** This method returns true if the end of stream is reached */
        virtual bool endReached() const  { return true; }
        /** This method returns the position of the next byte that could be read from this stream */
        virtual uint64 currentPosition() const { return fullSize(); }
        /** Try to seek to the given absolute position (return false if not supported) */
        virtual bool setPosition(const uint64) { return false; }
        /** Move the stream position forward of the given amount
            This should give the same results as setPosition(currentPosition() + value),
            but implementation can be faster for non-seek-able stream.
        */
        virtual bool goForward(const uint64) { return false; }
        /** Try to write the given amount of data to the specified buffer
            @return the number of byte truly written (this method doesn't throw)
        */
        virtual uint64 write(const void * const _buffer, const uint64 _size) throw();

        // Construction
    public:
        /** The only allowed constructor */
        AESOutputStream(OutputStream & os, const String & keyInHex, const String & IVInHex);
        /** The only allowed constructor */
        AESOutputStream(OutputStream & os, const uint8 * key, const uint32 keySize, const uint8* iv, const uint32 ivSize);
        /** The only allowed destructor */
        ~AESOutputStream();

    private:
        /** Deny copying */
        AESOutputStream(const AESInputStream &);
    };
#endif

    /** The copy stream function.
        @param is               The input stream to copy from
        @param os               The output stream to copy to
        @param forceInputSize   If provided, this override the input stream size by this one. You can use this to limit the copy to a certain size. */
    bool copyStream(const InputStream & is, OutputStream & os, const uint64 forceInputSize = 0);

    /** The copy callback that is called while the copy is running. */
    struct CopyCallback
    {
        /** Overload this method to get informed about the progress of the stream copy
            @param  size    The currently copied size
            @param  total   The total stream size (if known beforehand else can be zero)
            @return If you return false from this method, the copying is aborted, and left in the current state */
        virtual bool copiedData(const uint64 size, const uint64 total) = 0;
        virtual ~CopyCallback() {}
    };
    /** The copy stream function with callback.
        @param is               The input stream to copy from
        @param os               The output stream to copy to
        @param callback         The callback object with a copiedData method that'll be called while copy is in progress
        @param forceInputSize   If provided, this override the input stream size by this one. You can use this to limit the copy to a certain size.
        @warning If you don't care about the copy progress, don't use a callback as it's much slower. */
    bool copyStream(const InputStream & is, OutputStream & os, CopyCallback & callback, const uint64 forceInputSize = 0);
    
    /** Clone a stream.
        This actually read all the data from an input stream to generate an equivalent stream.
        @warning Depending on the input stream, this could overflow the available memory
        @return A pointer on a new allocated input stream */
    InputStream * cloneStream(const InputStream & is);
    /** Read a C string out of an input stream.
        This stop on any of the given stop char (usually 0) 
        @param is   The input stream to read from
        @param stop The stop character(s) to look for 
        @return the maximum string found that's not including the stop character(s). 
        @warning The stream has been read until one of the stop character was consumed. */
    Strings::FastString readString(const InputStream & is, const Strings::FastString & stop = "");
    
    /** Read a Base16 (hexadecimal) C string out of an input stream.
        Character are converted on the fly. 
        Any missing pair is read as if a 0 is appended.
        So, this gives the same result:
        @code
            InputStringStream test("91AFC"), test2("91AFC0"); 
            String out = readHexNumber(test), out2 = readHexNumber(test2);
            out == out2; // true and = "\x91\xAF\xC0"
        @endcode
        
        This stop on any of the given stop char (usually 0) 
        @param is   The input stream to read from
        @param stop The stop character to look for 
        @return the maximum string found that's not including the stop character(s) 
        @warning The stream has been read until one of the stop character was consumed. */
    Strings::FastString readHexNumber(const InputStream & is, const Strings::FastString & stop = ""); 
    
}

#endif
