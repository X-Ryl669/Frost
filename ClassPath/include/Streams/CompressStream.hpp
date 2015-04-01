#ifndef hpp_CompressStream_hpp
#define hpp_CompressStream_hpp

// We need stream declarations
#include "Streams.hpp"

#if (WantCompression == 1)

// We need base compressor declaration too
#include "../Compress/BaseCompress.hpp"
// We need ZLib implementation too for default usage
#include "../Compress/ZLib.hpp"


namespace Stream
{
    /** An input stream that's decompressing on-the-fly while being read.
        This is a pseudo stream, in the sense that a real stream is used
        underneath to refill the compressed buffer when necessary.
        It's not possible to seek this stream. */
    class DecompressInputStream : public LineSplitStream<Strings::FastString>
    {
        // Members
    private:
        /** The input stream to read from */
        Utils::OwnPtr<const InputStream> stream;
        /** The compressor to use */
        Utils::ScopePtr<Compression::BaseCompressor> compressor;
        /** The current accumulator of output bytes */
        mutable uint64 position;
        /** The decompressed size (if known at construction time) */
        const uint64 decompressedSize;

        // Interface
    public:
        /** Get the compressor used */
        inline const Compression::BaseCompressor * getCompressor() const { return compressor; }
        /** Try to read the given amount of data to the specified buffer
            @return the number of byte truly read (this method doesn't throw) */
        inline uint64 read(void * const outBuffer, const uint64 _size) const throw()
        {
            uint32 size = (uint32)min((uint64)0xFFFFFFFE, _size);
            MemoryBlockOutStream outStr((uint8*)outBuffer, size);
            if (compressor->decompressStream(outStr, *stream, size))
            {
                position += (uint64)outStr.currentPosition();
                return (uint64)outStr.currentPosition();
            }
            return (uint64)-1;
        }
        /** Move the stream position forward of the given amount
            This should give the same results as setPosition(currentPosition() + value),
            but implementation can be faster for non-seek-able stream. */
        inline bool goForward(const uint64 size) { return read(0, size) == size; }
        /** This method returns the stream length in byte, if known
            For stream where the length is not known, this method will return (uint64)-1.
            Returning -1 is safer than returning the compressed stream size */
        inline uint64 fullSize() const { return decompressedSize; }
        /** This method returns true if the end of stream is reached */
        inline bool endReached() const { return stream->currentPosition() == stream->fullSize(); }
        /** This method returns the position of the next byte that could be read from this stream
            @warning This is false (as it returns the position in the compressed stream), but can nonetheless
                     can be used to estimate the current amount of data processed */
        inline uint64 currentPosition() const { return position; }
        /** Try to seek to the given absolute position (return false if not supported) */
        inline bool setPosition(const uint64) { return false; }

        // Construction and destruction
    public:
        /** Construct a DecompressInputStream object from the given stream with the given compressor (it's owned).
            @param stream       The actual stream to read compressed data from
            @param compressor   A pointer on a new allocated compressor that's own by this stream. If 0, a GZip is built and used. */
        DecompressInputStream(const InputStream & stream, Compression::BaseCompressor * compressor = 0, const uint64 decompressedSize = -1)
            : stream(stream), compressor(compressor), position(0), decompressedSize(decompressedSize)
        {
            if (!compressor)
                this->compressor = new Compression::GZip;
        }
        /** Construct a DecompressInputStream object from the given stream with the given compressor (it's owned).
            @param stream       The actual stream to read compressed data from (it's owned)
            @param compressor   A pointer on a new allocated compressor that's own by this stream. If 0, a GZip is built and used. */
        DecompressInputStream(const InputStream * stream, Compression::BaseCompressor * compressor = 0, const uint64 decompressedSize = -1)
            : stream(stream), compressor(compressor), position(0), decompressedSize(decompressedSize)
        {
            if (!compressor)
                this->compressor = new Compression::GZip;
        }

        ~DecompressInputStream()
        {
            // Before actually destructing the compressor, we have to read nothing out of the decompressor.
            // Some compression engine might require doing cleanup stuff and this is the signal for it.
            Strings::FastString outString;
            OutputStringStream outStream(outString);
            Strings::FastString inString;
            InputStringStream inStream(inString); // Empty input stream to empty any pending buffer of the compressor
            compressor->decompressStream(outStream, inStream); // We don't care about the result.
        }
    };

    /** An output stream that's compressing on-the-fly while being written into.
        This is a pseudo stream, in the sense that a real stream is used
        underneath to fill a compressed buffer and flush to the output stream when necessary.
        It's not possible to seek this stream.
        Beware that the output will only be correct with the object is deleted
        (some compressor require writing footer informations to the stream)  */
    class CompressOutputStream : public OutputStream
    {
        // Members
    private:
        /** The stream to write to */
        Utils::OwnPtr<OutputStream> stream;
        /** The amount written */
        uint64  amount;
        /** The compressor to use */
        Utils::ScopePtr<Compression::BaseCompressor> compressor;

        // Interface
    public:
        /** Get the compressor used */
        inline const Compression::BaseCompressor * getCompressor() const { return compressor; }
        /** This method returns the stream length in byte, if known
            If the length is equal or higher than 2^32 - 1, the returned value is 0xfffffffe */
        virtual uint64 fullSize() const  { return amount; }
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
        virtual uint64 write(const void * const buffer, const uint64 size) throw()
        {   // Don't flush the stream
            if (compressor->compressStream(*stream, MemoryBlockStream((const uint8*)buffer, size), 0, false))
            {
                amount += size;
                return size;
            }
            return (uint64)-1;
        }
        /** Try to write the given amount of data to the specified buffer.
            Unlike the former version, this version also tells if the stream needs to be flushed */
        virtual uint64 write(const void * const buffer, const uint64 size, const bool flush) throw()
        {
            if (compressor->compressStream(*stream, MemoryBlockStream((const uint8*)buffer, size), 0, flush))
            {
                amount += size;
                return size;
            }
            return (uint64)-1;
        }


        // Construction and destruction
    public:
        /** Construct a DecompressInputStream object from the given stream with the given compressor (it's owned).
            @param stream       The actual stream to read compressed data to (must exist after this object is deleted)
            @param compressor   A pointer on a new allocated compressor that's own by this stream. If 0, a GZip is built and used. */
        CompressOutputStream(OutputStream & stream, Compression::BaseCompressor * compressor = 0)
            : stream(stream), compressor(compressor)
        {
            if (!compressor)
                compressor = new Compression::GZip;
        }
        /** Construct a DecompressInputStream object from the given stream with the given compressor (it's owned).
            @param stream       A pointer on an output stream that's owned by this object
            @param compressor   A pointer on a new allocated compressor that's own by this stream. If 0, a GZip is built and used. */
        CompressOutputStream(OutputStream * stream, Compression::BaseCompressor * compressor = 0)
            : stream(stream), compressor(compressor)
        {
            if (!compressor)
                compressor = new Compression::GZip;
        }

        ~CompressOutputStream()
        {
            // Before actually destructing the compressor, we have to write nothing out of the compressor.
            // Some compression engine might require doing cleanup stuff and this is the signal for it.
            compressor->compressStream(*stream, MemoryBlockStream(0, 0), 0, true); // We don't care about the result, we force flushing
        }
    };

    /** An output stream based on a memory buffer for the first one,
        and a general purpose stream for the second one.
        In general, it's used when you need to write a header and then the data,
        and have to go back to the header to rewrite part of it.
        In order to do so, this stream check the functionalities of the given output streams,
        and if the output stream doesn't support seeking, it has to buffer the whole output to memory in order to work.
        In that case, the destructor will copy the output stream (so it must still exists by that time).
        You might be interested in absolutePosition() method. */
    class HeaderBodyStream : public OutputStream
    {
        // Members
    private:
        /** The header stream (its size must be known at construction) */
        MemoryBlockOutStream header;
        /** The data stream to write too */
        OutputStream & dataStream;
        /** The initial output stream position */
        uint64 initialOutPosition;
        /** The memory data stream if the former is not seekable */
        Utils::ScopePtr<OutputMemStream> bufferStream;
        /** The current global position */
        uint64 position;


        // Interface
    public:
        /** This method returns the stream length in byte, if known
            If the length is equal or higher than 2^32 - 1, the returned value is 0xfffffffe
            For stream where the length is not known, this method will return 0xffffffff. */
        virtual uint64 fullSize() const { return (bufferStream ? header.fullSize() + bufferStream->fullSize() : dataStream.fullSize() - initialOutPosition); }
        /** This method returns true if the end of stream is reached */
        virtual bool endReached() const { return position == fullSize(); }
        /** This method returns the position of the next byte that could be read from this stream */
        virtual uint64 currentPosition() const { return position; }
        /** Returns the absolute position.
            Since there are multiple streams, with possibly buffered output, the currentPosition() above
            will return the position for the current stream.
            On the other side, this method will return the (theoretical) position of the final output stream
            (so it includes any offset in the output stream and un-buffering computation) */
        inline uint64 absolutePosition() const { return bufferStream ? (header.fullSize() + dataStream.currentPosition() + bufferStream->fullSize()) : header.fullSize() + dataStream.currentPosition(); }
        /** Try to seek to the given absolute position (return false if not supported) */
        virtual bool setPosition(const uint64 newPos)
        {
            position = newPos;
            if (bufferStream)
            {
                if (newPos > header.fullSize())
                    return bufferStream->setPosition(newPos - header.fullSize());
            }
            else return dataStream.setPosition(initialOutPosition + newPos);
            return true;
        }
        /** Try to write the given amount of data to the specified buffer
            @return the number of byte truly written (this method doesn't throw) */
        virtual uint64 write(const void * const buffer, const uint64 size) throw()
        {
            uint64 done = 0;
            if (position < header.fullSize())
            {
                uint64 amount = min(header.fullSize() - position, size);
                if (buffer) memcpy(header.getBuffer() + position, buffer, (size_t)amount);
                position += amount;
                done += amount;
            }
            // Need to write to the second stream ?
            if (done < size)
            {
                OutputStream * out = bufferStream ? bufferStream : &dataStream;
                uint64 res = out->write((uint8*)buffer + done, size - done);
                if (res == (uint64)-1) return res;
                done += res;
                position += res;
            }
            return done;
        }

        /** Build an header body stream.
            @param outStream    The output stream to write too
            @param headerSize   The header size */
        HeaderBodyStream(OutputStream & outStream, const uint32 headerSize)
            : header(new uint8[headerSize], headerSize), dataStream(outStream), initialOutPosition(dataStream.currentPosition()),
              bufferStream(dataStream.setPosition(dataStream.currentPosition()) ? 0 : new OutputMemStream()), position(0)
        {
            if (!bufferStream) dataStream.setPosition(headerSize + initialOutPosition); // Since we don't always start from 0
        }

        ~HeaderBodyStream()
        {
            if (!bufferStream)
                dataStream.setPosition(initialOutPosition);
            // Need to copy the final operation to the output stream
            dataStream.write(header.getBuffer(), header.fullSize());
            if (bufferStream)
                dataStream.write(bufferStream->getBuffer(), bufferStream->fullSize()); // Not checking the result
            // Seek back to the end of stream (might fail, but we don't care)
            dataStream.setPosition(dataStream.fullSize());
            // Now wipe the header
            delete[] header.getBuffer();
        }
    };

}
#endif

#endif
