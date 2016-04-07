#ifndef hpp_CPP_File_CPP_hpp
#define hpp_CPP_File_CPP_hpp

// We need basic type declaration
#include "../Types.hpp"
// We need platform specific structure
#include "../Platform/Platform.hpp"
// We need strings
#include "../Strings/Strings.hpp"
// And container
#include "../Container/Container.hpp"
// We need time
#include "../Time/Time.hpp"
// We need timeout too
#include "../Time/Timeout.hpp"
// We need locks and events
#include "../Threading/Lock.hpp"

#if defined(_POSIX)
#include <aio.h>
#endif

/** All utility for manipulating files */
namespace File
{
    /** The string class we are using */
    typedef Strings::FastString String;

    /** The stream interface.

        Once you've retrieved file informations (Info), you can get a stream to
        manipulate file content.

        The interface is limited to basic stream operations
    */
    struct BaseStream
    {
        /** The end of line enumeration */
        typedef Platform::EndOfLine EndOfLine;
        /** Read the stream of the given amount of bytes.
            Depending on the stream blocking behaviour, the stream pointer position,
            and the memory, this method can fail to retrieve the queried amount of data.
            @param buffer   The buffer to read into
            @param length   The data length to read
            @return 0 on end of file, -1 on error, or the actual length read else */
        virtual int read(char * buffer, int length) = 0;
        /** Read a line from the stream.
            @param buffer   The buffer to read into (buffer must be at least length bytes)
            @param length   The length of the data to read into buffer
            @param EOL      The allowed end of line marker (@sa EndOfLine) */
        virtual int readLine(char * buffer, int length, const EndOfLine EOL = Platform::Default) = 0;
        /** Write the given amount of bytes to the stream.
            Depending on the stream blocking behaviour, the stream pointer position,
            and the memory, this method can fail to write the queried amount of data.
            @param buffer   The buffer to read into
            @param length   The data length to read
            @return 0 on end of file, -1 on error, or the actual length read else */
        virtual int write(const char * buffer, int length) = 0;

        /** Flush the stream (this is a no op in most implementation). */
        virtual void flush() = 0;

        /** Get the stream size (if known by advance) */
        virtual uint64 getSize() const = 0;
        /** Get the current pointer position in the stream. */
        virtual uint64 getPosition() const = 0;
        /** Set the current pointer position in the stream */
        virtual bool setPosition(const uint64 offset) = 0;

        /** Set the stream size.
            This only work for specific stream, like files */
        virtual bool setSize(const uint64 offset) = 0;
        /** Check if the stream is finished. */
        virtual bool endOfStream() const = 0;

        /** Get the private field.
            This is used to store an unknown data per socket.
            It is set to 0 at construction.

            @return a reference to a pointer you can set to whatever struct you want.
            @warning you must manage this field allocation and destruction. */
        virtual void * & getPrivateField() = 0;

        /** Required destructor */
        virtual ~BaseStream() {}
    };


    /** The file info contains meta information about files */
    struct Info
    {
        // Type definition and enumeration
    public:
        /** The file permission */
        enum Permission
        {
            OwnerRead       =   0400,
            OwnerWrite      =   0200,
            OwnerExecute    =   0100,
            OwnerMask       =   0700,

            GroupRead       =   0040,
            GroupWrite      =   0020,
            GroupExecute    =   0010,
            GroupMask       =   0070,

            OtherRead       =   0004,
            OtherWrite      =   0002,
            OtherExecute    =   0001,
            OtherMask       =   0007,

            OwnerSUID       =   04000,
            GroupSUID       =   02000,
            StickyBit       =   01000,
        };

        /** The permission to check */
        enum PermissionType
        {
            Reading     =   0,
            Writing     =   1,
            Execution   =   2,
        };

        /** The different file type */
        enum Type
        {
            Regular     = 0x001,
            Link        = 0x100,
            Directory   = 0x002,
            FIFO        = 0x004,
            Pipe        = 0x008,
            Device      = 0x010,
            Socket      = 0x020,
        };

        /** Compare in metadata */
        enum Comparand
        {
            All                     =   0,
            AllButAccessTime        =   1,
            AllButTimes             =   2,
        };

        /** The possible mode for setContent */
        enum ContentMode
       	{
            AtomicReplace = 0,   //<! Generate a temporary file and atomically move it over the file to replace
            Append        = 1,   //<! Append to the existing file instead of replacing the content
       	    Overwrite     = 2,   //<! Overwrite the file, but this is not atomic
        };

        /** The set content additional mode */
        struct SetContentMode
        {
            /** The possible mode type */
            ContentMode type;

            SetContentMode(const ContentMode type) : type(type) {}
            SetContentMode(const bool append = false) : type(append ? Append : AtomicReplace) {}
        };

        // Members
    public:
        /** The file name */
        String      name;
        /** The full file path without file name */
        String      path;
        /** The file size */
        uint64      size;
        /** The file creation time (in second since epoch) */
        double      creation;
        /** The file modification time (in second since epoch) */
        double      modification;
        /** The file last access time (in second since epoch) */
        double      lastAccess;

        /** If applicable, the file owner */
        uint32      owner;
        /** If applicable, the file group */
        uint32      group;

        /** The file permission */
        uint32      permission;
        /** The file POSIX type */
        Type        type;


        // Interface
    public:
        /** Get the full path for this file */
        inline String getFullPath() const { return path.getLength() ? path + PathSeparator + name : name; }
        /** Check the file permission.
            @warning This method doesn't check ACL directly
            @param type     The permission type to check
            @param userID   When set, check the permission for the given user, else use current user
            @param groupID  When set, check the permission for the given group, else use current group
            @return true on success */
        bool checkPermission(const PermissionType type, const uint32 userID = (uint32)-1, const uint32 groupID = (uint32)-1) const;
        /** Get a stream from this file.
            @param blocking         When true, return a blocking stream, else return an asynchronous stream
            @param forceReadOnly    If true, the stream is read only (even if you have write permission to it), otherwise the stream is open with the current permission
            @param forceOverwrite   If true, the stream overwrite the previous file content (if any)
            @return a pointer to a new allocated base stream you must delete, or 0 on error */
        BaseStream * getStream(const bool blocking = true, const bool forceReadOnly = false, const bool forceOverwrite = false) const;

        /** Copy the file to the given path.
            This methods use the most efficient implementation available.
            The destination should point to a file.
            If the destination exists, it's overwritten (provided it's possible to do so).
            If the destination is a directory, a file with the same name as the source file is created in it.
            If the destination does not exist all path segment but to the last will be used to create the destination hierarchy, the last path segment
            will be the destination name (for example, specifying '/tmp/app/loc.txt' will create "/tmp/app" folder, and copy the source file to a file
            called "loc.txt" in this folder).
            @param destination      The destination path. */
        bool copyTo(const String & destination) const;
        /** Move the file to the given path.
            This methods use the most efficient implementation available
            @param destination      The destination path. */
        bool moveTo(const String & destination);
        /** Remove the file.
            This methods use the most efficient implementation available */
        bool remove();
        /** Create as a link to an existing file.
            Depending on the platform, it may fail to work if it's not supported.
            On Windows, making Hardlink is not supported.
            @param destination   The link destination (where the link points to)
            @param hardLink      If true a hard link is created, else a symbolic link is created */
        bool createAsLinkTo(const String & destination, const bool hardLink = false);


        /** Make the directory for this (invalid) file.
            @note With POSIX system, a default permission of 0755 is attempted for the directory.
                  This is actually masked with umask and the supported bits. For example, under linux,
                  the final permission will be (0755 & ~umask & 01777) (this is a mkdir limitation)
            @param recursive    If true, will create missing directory up to the given path */
        bool makeDir(const bool recursive = true);

        /** Change the file modification time.
            @param  newTime     In second since Epoch */
        bool setModifiedTime(const double newTime);

        /** Check if the file or directory exists */
        bool doesExist() const;
        /** Check if the pointed value is a file */
        inline bool isFile() const { return (type & Regular) > 0; }
        /** Check if the pointed value is a directory */
        inline bool isDir() const { return (type & Directory) > 0; }
        /** Check if the pointed value is a link */
        inline bool isLink() const { return (type & Link) > 0; }
        /** Check if the pointed value is a device */
        inline bool isDevice() const { return (type & Device) > 0; }
        /** Get the last 16 bytes of the file.
            This is only used for file fingerprint matching, file have usually same start bytes, but rarely same end bytes.
            It's only used as a fast (and likely not exact) discrimination method */
        String last16Bytes() const;

        // Helpers
    public:
        /** Build the path and filename from the given full path */
        void buildNameAndPath(const String & fullPath);
        /** Reget the file information */
        bool restatFile();
        /** Get the metadata information as an opaque buffer.
            This buffer is system dependent, and contains everything needed to backup the file's metadata.
            @sa setMetaData
            @return an opaque buffer that's system specific. */
        String getMetaData() const;
        /** Get a compressed version of the metadata informations.
            This buffer is system dependent, and contains everything needed to backup the file's metadata.
            Unlike the getMetaData() call above, this one outputs a binary version of the metadata that can not be saved
            in a text only archive. The size is not fixed, as compression is used to maximize compactness.
            You'll expand this block with the expandMetaData call to reuse other metadata related functions.
            @sa getMetaData expandMetaData
            @param buffer   A pointer to a buffer where to store the metadata information. Can be 0 to query for required size
            @param len      The length of the buffer in bytes (can be 0 to query the required size)
            @return The used buffer size in bytes (or 0 upon error)
            @warning Please notice that this method does not save the filesystem specific information like the inode number and device id for standard file.
                     (but it's useless information anyway). When an hardlink is detected, the required information to restore it is saved */
        uint32 getMetaDataEx(uint8 * buffer, const size_t len) const;
        /** Expand the compressed metadata buffer received from call to getMetaDataEx.
            @param buffer   A pointer to a buffer containing the compressed version of the metadata
            @param len      The length of the buffer in bytes
            @return A string describing the metadata that you can use with setMetaData, analyseMetaData and other methods */
        static String expandMetaData(const uint8 * buffer, const size_t len);
        /** Set the metadata information from an opaque buffer.
            This buffer is system dependent, and contains everything needed to restore the file's metadata.
            @sa getMetaData
            @param metadata    The metadata to update
            @return true on successful metadata update. */
        bool setMetaData(String metadata);
        /** Analyse the metadata string and set the stat value based on it.
            This is useful if you don't intend to apply the metadata on the file system immediately and want
            to decide whether to do so or not at a later stage.
            @sa getMetaData and setMetaData
            @param metadata    The metadata to analyze
            @return true if the parsing of the metadata succeeded */
        bool analyzeMetaData(String metadata);
        /** Check if the given metadata match the current file.
            @param metadata    The metadata to compare with
            @param checkMask   The field to check against in the metadata
            @param override    If provided, this is used instead of the current file metadata.
            @return true if both metadata match */
        bool hasSimilarMetadata(String metadata, const Comparand checkMask, const String * override = 0) const;
        /** Get a human understandable version of the metadata.
            This typically output "type_mode_octal owner:group size modification_time_iso8601" (like ls -l does on POSIX).
            @param metadata    The metadata to analyze
            @return a string for the given metadata in the specified format */
        static String printMetaData(String metadata);

        /** Get the complete content as a String.
            @warning Beware that the whole content is read in memory so big file will exhaust your memory */
        String getContent() const;
        /** Replace the file content with the given string.
            The replacement is atomic (so the file contains either the previous content or the new one).
            If the file doesn't exist, it's created.
            @param content       The new file's content.
            @param mode          Depending on the mode chose, either the file is replaced atomically, either it's appended,
                                 either it's not replaced atomically. @sa ContentMode
            @return false if it was not possible to set the content (either disk full, either permissions issues) */
        bool setContent(const String & content, const SetContentMode mode = AtomicReplace);
        /** Get the parent folder (if any applicable).
            This method actually resolves any symbolic link, path substitution and stack abuse */
        String getParentFolder() const;
        /** Get the real path for this file.
            This method actually resolves any symbolic link, path substitution and stack abuse */
        String getRealFullPath() const;
        /** Get the file permission (if applicable).
            On platform with no permission per file, this returns a fake mode where both user, group and other are based on the current user permission.
            @return A union of permission flags @sa Permission */
        inline uint32 getPermission() const { return permission; }
        /** Set the file permission (if applicable).
            On platform with no permission per file, this is completely ignored.
            @warning If the file is a symbolic link, the linked file permission is changed, not the link itself (as this is not supported in all unices).
            @param permission A union of permission flags @sa Permission
            @return true if the file permission was set correctly */
        bool setPermission(const uint32 permission);
        /** Set the user and group owner.
            On platform with no owner per file, this is completely ignored.
            @param userID        The user ID to own the file (ignored if -1)
            @param groupID       The group ID to own the file (ignored if -1)
            @param followSymLink If true and the the file is a symbolic link, the linked file is changed, else, the symbolic link is changed
            @return true on success */
        bool setOwner(const uint32 userID = (uint32)-1, const uint32 groupID = (uint32)-1, const bool followSymlink = true);
        /** Get the file's user ID.
            On platform with no owner per file, this returns a fake owner. */
        inline uint32 getOwnerUser() const { return owner; }
        /** Get the file's group ID.
            On platform with no owner per file, this returns a fake owner. */
        inline uint32 getOwnerGroup() const { return group; }
        /** Get the number of contained files/items inside this item.
            For most file type, this will be 1, but for directories (or "pseudo-archive")
            this will returns the number of files inside the directory.
            @note If the current item is a symbolic link, it's not followed (so it returns 1)
            @param extension   If provided, only file with the given extension are counted. Expected value with leading dot like ".jpg" */
        uint32 getEntriesCount(const String & extension = "") const;



        // Construction / Destruction
    public:
        /** Build an info object from a file path (will query the info by itself) */
        Info(const String & fullPath);
        /** Build an info object from a file path (will query the info by itself).
            This method fix any environment variable inside the file name, so that "~/someFile" will expand to "/home/user/someFile" for example. */
        Info(const String & fullPath, const bool fixEnvVariable);
        /** Default constructor */
        Info() : size(0), creation(0), modification(0), lastAccess(0), owner(0), group(0), permission(0), type(Regular) {}
    };

    /** A file item hold the minimal required informations to identify a file uniquely.
        The file fingerprint is based on its size, its last modification time, and the last 16 bytes (might or might not match).
        <br>
        <br>
        The fingerprint algorithm follows the Byped's synchronization patent (refer to the description in the patent text) */
    struct FileItem
    {
        /** The file name (including full path relative to the mount point entry (so if the mount point changes it, this name says the same) */
        const String            name;
        /** The current file level. File with level 0 are on root of the drive, and it increase while going down in the hierarchy */
        const uint32            level;
        /** The file type */
        const Info::Type        type;

        /** The file size. */
        const uint64            size;
        /** The last modification time (in seconds since Epoch) */
        const uint32            lastModif;
        /** The last 16 bytes */

        FileItem(const String & path = "", const uint32 level = 0, const File::Info::Type type = File::Info::Regular, const uint64 size = 0, const uint32 lastModif = 0)
            : name(path), level(level), type(type), size(size), lastModif(lastModif) {}
    };
    /** An array of file items */
    typedef Container::NotConstructible<FileItem>::IndexList     FileItemArray;

    /** A directory iterator */
    struct DirectoryIterator
    {
        // Type definition and enumeration
    public:
        /** The file array (if retrieving all files at once) */
        typedef Container::WithCopyConstructor<Info>::Array     InfoArray;
        /** The file name array (if retrieving all files at once) */
        typedef Container::WithCopyConstructor<String>::Array   NameArray;

        // Members
    private:
#ifdef _WIN32
        mutable HANDLE              finder;
        mutable WIN32_FIND_DATAW    data;
#elif defined(_POSIX)
        mutable DIR *               finder;
//        mutable struct dirent       data;
#elif defined(NEXIO)
		void *               finder;
#endif
        /** The search path */
        String          path;

        // Interface
    public:
        /** Retrieve all the files information at once.
            This method modify the iterator, so you'll only get the entries that
            aren't iterated yet. The iterator is reset after the method.
            This method is a shortcut to "while(getNextFile())"

            @param array    A reference on the array to retrieve */
        bool getAllFilesAtOnce(InfoArray & array) const;
        /** Get a minimal file listing from this iterator.
            This method doesn't modify the iterator, so you will not loose your
            current position in iteration with getNextFile.

            @param array    A reference on the array to retrieve
            @param withPath When set, the array is filled with full file path */
        bool getAllFilesAtOnce(NameArray & array, const bool withPath = false) const;
        /** Get the next file iteratively
            @param info     The file info to retrieve
            @return false when iteration should stop */
        bool getNextFile(Info & info) const;
        /** Get the next file iteratively.
            The path member of the info object is not filled.
            @param info     The file info to retrieve
            @return false when iteration should stop */
        bool getNextFilePath(Info & info) const;

        /** Only allow General to build us */
        friend struct General;

        // Construction and destruction
    private:
        /** Build an iterator */
        DirectoryIterator(const String & path);

    public:
        DirectoryIterator(const DirectoryIterator & dir);
        ~DirectoryIterator();

    };

    /** The general class allows to list / rename / bulk-copy / move / delete files in directories.
        It doesn't actually look into file content, only manipulate pointers to files */
    struct General
    {
        /** The special folder to retrieve */
        enum SpecialFolder
        {
            Home        =   0x00000001, //!< The current user home directory
            Root        =   0x00000002, //!< The root directory (under Windows, this is C:\\ )
            Programs    =   0x00000003, //!< The program directory
            Temporary   =   0x00000004, //!< The temporary directory
            Current     =   0x00000005, //!< The current directory
        };

        /** Enumerate directory at the given path */
        static DirectoryIterator listFilesIn(const String & path);
        /** Normalize a path.
            You can put whatever path on input (even most complex path like "../../C:/.././a/b/.../")
            When using this method, the input path must come from a logical value, illogical value is silently removed, see example output below:
            @verbatim
                "../../C:/.././a/b/.../" => "a/b/.../" on Posix, "C:/a/b/.../" on windows
                "/some/strange/path" => "/some/strange/path/"
                "/some/../a/strange/path/" => "/a/strange/path/"
                "C:/windows" => "C:/windows/"
                "/some/bad/trick/./.." => "/some/bad/"
                "./././././././././././././" => "/"
            @endverbatim
            @param strangePath    A non relative path, the path must starts from something tangible
            @warning This does not interpret the path.
                     If you give for example "../../" the normalization will not figure out the current path, and strip the last
                     two segments, but instead will return an empty string.
            @return a normalized path ending with PathSeparator */
        static String normalizePath(String strangePath);
        /** Get the special folder
            @param folder   Any of SpecialFolder enumeration
            @return The absolute path to the special folder or empty string on error */
        static String getSpecialPath(const SpecialFolder folder);
        /** Get the total and free space for the given mount point path
            @param path         The mount point path to analyze free space with
            @param totalBytes   On output, contains the total drive space
            @param freeBytes    On output, contains the total free space on the drive
            @return true on successful drive space query */
        static bool getDriveUsage(const String & path, uint64 & totalBytes, uint64 & freeBytes);
		/** Get the list of mount points (or drives) in the system.
		    @param paths        On output, contains the minimal path to the drive/mount point
		    @param remoteNames  On output, if provided, contains the path to the volume for this drive
			@return true on successful enumeration */
		static bool findMountPoints(Strings::StringArray & paths, Strings::StringArray * remoteNames = 0);
    };


    /** The blocking, classic file stream. */
    class Stream : public BaseStream
    {
        // Member
    private:
        /** The private field */
        void    *       priv;
        /** The stream pointer */
        FILE    *       file;

        // Interface
    public:
        /** Read the stream of the given amount of bytes.
            Depending on the stream blocking behaviour, the stream pointer position,
            and the memory, this method can fail to retrieve the queried amount of data.
            @param buffer   The buffer to read into
            @param length   The data length to read
            @return 0 on end of file, -1 on error, or the actual length read else */
        virtual int read(char * buffer, int length);
        /** Read a line from the stream.
            @param buffer   The buffer to read into (buffer must be at least length bytes)
            @param length   The length of the data to read into buffer
            @param EOL      The allowed end of line marker (@sa EndOfLine) */
        virtual int readLine(char * buffer, int length, const EndOfLine EOL = Platform::Default);
        /** Write the given amount of bytes to the stream .
            Depending on the stream blocking behaviour, the stream pointer position,
            and the memory, this method can fail to write the queried amount of data.
            @param buffer   The buffer to read into
            @param length   The data length to read
            @return 0 on end of file, -1 on error, or the actual length read else */
        virtual int write(const char * buffer, int length);
        /** Flush the stream (this is a no op in most implementation). */
        virtual void flush();

        /** Get the stream size (if known by advance) */
        virtual uint64 getSize() const;
        /** Get the current pointer position in the stream. */
        virtual uint64 getPosition() const;
        /** Set the current pointer position in the stream */
        virtual bool setPosition(const uint64 offset);
        /** Check if the stream is finished. */
        virtual bool endOfStream() const;

        /** Set the stream size.
            This only work for specific stream, like files */
        virtual bool setSize(const uint64 offset);


        /** Get the private field.
            This is used to store an unknown data per socket.
            It is set to 0 at construction.

            @return a reference to a pointer you can set to whatever struct you want.
            @warning you must manage this field allocation and destruction. */
        virtual void * & getPrivateField() { return priv; }

        /** Default constructor */
        Stream(const String & fullPath = "", const String & mode = "r+b");
        ~Stream();

    private:
        /** Prevent copying */
        Stream(const Stream &);
    };


#if WantAsyncFile == 1
    /** The asynchronous file stream.

        All IO operations in this stream will likely return Asynchronous error.
        This is the expected behaviour.
        Using asynchronous file stream is a bit more difficult than classic stream,
        as you'll have to handle asynchronous operations.

        Using this class is interesting when you're reading or writing multiple
        different files and you don't want to write them sequentially (adding the latency),
        but as fast as possible (max of biggest latency).

        Typically using this class is done like this:
        @code
            function read(AsyncStream & stream, char * buffer, int length)
            {
                // Start the reading operation
                int ret = stream.read(buffer, length);
                int len = 0;
                // You should wait until the read operation is possible
                while (ret == Asynchronous && stream.isReadPossible(DefaultTimeout))
                {   // Then read
                    ret = stream.read(&buffer[len], length - len);
                    if (ret > 0 && ret < (length - len))
                    {
                        len += ret;
                        ret == Asynchronous;
                    }
                }
            }
        @endcode

        @warning You must not increase the read / write size between call. Doing so could cause unexpected behaviour.
        */
    class AsyncStream : public BaseStream
    {
        // Type definition and enumeration
    public:
        /** The opening mode */
        enum OpenMode
        {
            Read        =   1,
            Write       =   2,
            ReadWrite   =   3,
        };
        /** The async error */
        enum { Asynchronous = -2 };

        // Member
    private:
        /** The private field */
        void    *       priv;
        /** The stream pointer */
#ifdef _WIN32
        HANDLE                  file;
        mutable OVERLAPPED      over;
        char *                  tmpBuffer;
#elif defined(_POSIX)
        int                     file;
        mutable struct aiocb    over;
        void *                  monitored;
#endif
        uint64                  currentPos;
        uint64                  readPos;
        uint64                  asyncSize;

        // Interface
    public:
        /** Read the stream of the given amount of bytes.
            Depending on the stream blocking behaviour, the stream pointer position,
            and the memory, this method can fail to retrieve the queried amount of data.
            @param buffer   The buffer to read into
            @param length   The data length to read
            @return 0 on end of file, -1 on error, or the actual length read else */
        virtual int read(char * buffer, int length);
        /** Read a line from the stream.
            @param buffer   The buffer to read into (buffer must be at least length bytes)
            @param length   The length of the data to read into buffer
            @param EOL      The allowed end of line marker (@sa EndOfLine) */
        virtual int readLine(char * buffer, int length, const EndOfLine EOL = Platform::CRLF);
        /** Write the given amount of bytes to the stream .
            Depending on the stream blocking behaviour, the stream pointer position,
            and the memory, this method can fail to write the queried amount of data.
            @param buffer   The buffer to read into
            @param length   The data length to read
            @return 0 on end of file, -1 on error, or the actual length read else */
        virtual int write(const char * buffer, int length);
        /** Flush the stream (this is a no op in most implementation). */
        virtual void flush() {}

        /** Get the stream size (if known by advance) */
        virtual uint64 getSize() const;
        /** Get the current pointer position in the stream. */
        virtual uint64 getPosition() const;
        /** Set the current pointer position in the stream */
        virtual bool setPosition(const uint64 offset);
        /** Check if the stream is finished. */
        virtual bool endOfStream() const;


        /** Set the stream size.
            This only work for specific stream, like files */
        virtual bool setSize(const uint64 offset);

        /** Get the private field.
            This is used to store an unknown data per socket.
            It is set to 0 at construction.

            @return a reference to a pointer you can set to whatever struct you want.
            @warning you must manage this field allocation and destruction. */
        virtual void * & getPrivateField() { return priv; }

        /** Default constructor */
        AsyncStream(const String & fullPath = "", const OpenMode mode = ReadWrite);
        ~AsyncStream();

        // Our interface
    public:
        /** Check if it's possible to read from this stream
            @param timeout  The time to wait for a state change. */
        bool isReadPossible(const Time::TimeOut & timeout = Time::DefaultTimeOut) const;
        /** Check if it's possible to write to this stream
            @param timeout  The time to wait for a state change. */
        bool isWritePossible(const Time::TimeOut & timeout = Time::DefaultTimeOut) const;

        // The required access to the monitoring pool
    private:
        /** Get the internal object */
        void * getInternal() const;

    private:
        /** Prevent copying */
        AsyncStream(const AsyncStream &);

    public:
        /** Allow access to the monitoring pool */
        friend struct MonitoringPool;
    };


    /** The monitoring pool base interface.

        Typically, using a monitoring pool, is as simple as appending asynchronous stream to monitor to the pool,
        then calling the monitoring method, and then get the results.
        @code
            // Construct a pool
            MonitoringPool & pool = ...;
            // Ok, let's append some stream
            pool.appendStream(info.getStream(false));

            // Then monitor when there is data ready for the client
            if (pool.isReadPossible())
            {
                int index = pool.getNextReadyStream();
                while (index >= 0)
                {
                    // Your reading method is here
                    pool.getReadyAt(index)->receive(buffer, bufferSize);
                    // Parse input, and fill output
                    pool.getReadyAt(index)->send(outBuffer, outBufferSize);
                    index = pool.getNextReadyStream(index);
                }
            }
        @endcode
        @warning A pool never owns the stream passed in unless you specify it in its constructor (see specialization) */
    struct MonitoringPool
    {
        // Type definition and enumeration
    public:
#ifdef _WIN32
        /** The maximum number of socket in the pool */
        enum { MaxQueueLen = MAXIMUM_WAIT_OBJECTS };
#else
        /** The maximum number of socket in the pool */
        enum { MaxQueueLen = 16384 };

        /** This is the hidden struct that is given to the async call when they complete */
        struct AsyncCompleted
        {
            uint32           index;
            bool             completed;
            MonitoringPool & pool;
            void wasCompleted()
            {   // Simply append a index to the ready pool
                pool.indexLock.Acquire();
                pool.indexPool[pool.triggerCount++] = (uint16)index;
                completed = true;
                pool.indexLock.Release();
                // And wake up any sleeping thread
                pool.eventReady.Set();
            }
            void restartedOperation()
            {
                completed = false;
            }

            AsyncCompleted(MonitoringPool & pool, uint16 index) : index(index), completed(false), pool(pool)  {}
        };
        friend struct AsyncCompleted;
#endif

        // Members
    private:
        /** The socket pool */
        AsyncStream **      pool;
        /** The pool size */
        uint32              size;
#ifdef _WIN32
        HANDLE  *           readSet;
        HANDLE  *           writeSet;
        HANDLE  *           bothSet;
        mutable HANDLE  *           waitSet;
        mutable DWORD               triggerCount;
#else
        /** The index pool */
        uint16 *            indexPool;
        /** The index pool lock */
        mutable Threading::Lock     indexLock;
        /** The event ready */
        mutable Threading::Event    eventReady;
        /** The array of callback handler */
        Container::NotConstructible<AsyncCompleted>::IndexList  asyncCB;
        /** The last request triggered event's count */
        mutable int         triggerCount;
#endif
        /** Are we owning the sockets ? */
        bool                own;

        // Monitoring pool interface
    public:
        /** Append a socket to this pool */
        virtual bool    appendStream(AsyncStream * socket);
        /** Remove a socket from the pool */
        virtual bool    removeStream(AsyncStream * socket);
        /** Get the pool size */
        virtual uint32  getSize() const;

        /** Select the pool for at least an element that is ready
            @param reading When true, the select return true as soon as the socket has read data available
            @param writing When true, the select return true as soon as the socket is ready to be written to
            @param timeout The timeout in millisecond to wait for before returning (negative for infinite time)
            @return false on timeout or error, or true if at least one socket in the pool is ready */
        virtual bool select(const bool reading, const bool writing, const Time::TimeOut & timeout = Time::DefaultTimeOut) const;

        /** Check if at least a socket in the pool is ready for reading */
        virtual bool isReadPossible(const Time::TimeOut & timeout = Time::DefaultTimeOut) const;
        /** Check if at least a socket in the pool is ready for writing */
        virtual bool isWritePossible(const Time::TimeOut & timeout = Time::DefaultTimeOut) const;

        /** Check which socket was ready in the given pool
            @param index    Start by this index when searching (start by -1)
            @return index of the next ready socket (use getReadyStreamAt() to get the socket), or -1 if none are ready */
        virtual int getNextReadyStream(const int index = -1) const;
        /** Get the socket at the given position */
        virtual AsyncStream * operator[] (const int index);
        /** Get the ready socket at the given position */
        virtual AsyncStream * getReadyAt(const int index);

        // Construction and destruction
    public:
        /** Build a pool using epoll or kqueue when available.
            @param own  When set to true, the poll own the socket passed in. */
        MonitoringPool(const bool own = false);
        ~MonitoringPool();
    };

#endif

}


#endif
