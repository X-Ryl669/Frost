#include "File.hpp"
#include "../Container/FIFO.hpp"
#include "../Strings/Strings.hpp"


namespace File
{
    /** This is useful to scan a directory recursively (or not), applying a filter on the file names to select */
    class Scanner
    {
    public:
        /** The string class we use */
        typedef Strings::FastString String;

        /** A file filter check if the given file name match the filter set
            This example here, simply match the file extension. */
        class FileFilter
        {
            // Members
        protected:
            /** The file pattern to match */
            const String      filePattern;
            /** The found count */
            mutable uint32            found;

            // Interface
        public:
            void Reset() { found = 0; }
            /** Check if the file extension match our filter */
            virtual bool matchFile(const String & fileName) const
            {
                bool match = (filePattern == fileName.midString(fileName.getLength() - filePattern.getLength(), filePattern.getLength()));
                found += match;
                return match;
            }
            FileFilter() : filePattern(""), found(0) {}
            FileFilter(const String & pattern) : filePattern(pattern), found(0) {}
        };

        /** The file filter array */
        typedef Container::WithCopyConstructor<FileFilter>::Array   FileFilters;

        /** When using the low-level system, you can provide your own EntryFound 
            structure that'll be called each time a new entry is found in the folder 
            scanning process.
            @sa DefaultEntryIterator */
        struct EntryIterator
        {
            /** Called to extract the next file in the given directory.
                @return false when no more file of interest in the given directory. */
            virtual bool getNextFile(File::DirectoryIterator & dir, File::Info & file, const String & name) = 0;
            virtual ~EntryIterator() {}
            
            /** The entry iterator */
            EntryIterator(const bool recursive) : recursive(recursive) {}
            /** Set to true in case the iterator should recurse into directories */
            const bool    recursive;
        };
        
        /** The default implementation:
            - ignores hidden files and parent directory
            - ignores symbolic link for directories (since there is no way to figure out if they lead out of the initial path)
            - applies file filter on the file name to select a file for inclusion in the final list */
        class DefaultEntryIterator : public EntryIterator
        {
            FileFilters & filters;
            
        public:
            /** Called to fetch information about files in directory */
            virtual bool getFileInfo(File::DirectoryIterator & dir, File::Info & file)
            {
                return dir.getNextFile(file);
            }
        
            /** Called to extract the next file in the given directory.
                @return false when no more file of interest in the given directory. */
            virtual bool getNextFile(File::DirectoryIterator & dir, File::Info & file, const String & name)
            {
                while (getFileInfo(dir, file))
                {
                    if (file.name[0] == '.') continue; // Ignore hidden files and parent/current directory
                    if (recursive && file.isDir() && !file.isLink()) return true;
                    for (uint32 i = 0; i < filters.getSize(); i++)
                    {
                        const FileFilter & filter = filters[i];
                        if (filter.matchFile(file.name)) return true;
                    }
                }
                return false;
            }
            /** We need the filters and recursive information to figure out what to do next */
            DefaultEntryIterator(FileFilters & filters, const bool recursive) : EntryIterator(recursive), filters(filters) {}
        };
        
        /** The filename only implementation:
            - ignores hidden files and parent directory
            - ignores symbolic link for directories (since there is no way to figure out if they lead out of the initial path)
            - applies file filter on the file name to select a file for inclusion in the final list */
        struct FileNameOnlyIterator : public DefaultEntryIterator
        {
        public:
            /** Called to fetch information about files in directory */
            bool getFileInfo(File::DirectoryIterator & dir, File::Info & file) { return dir.getNextFilePath(file); }
            /** We need the filters and recursive information to figure out what to do next */
            FileNameOnlyIterator(FileFilters & filters, const bool recursive) : DefaultEntryIterator(filters, recursive) {}
        };
        
        /** Event based iterator. 
            The given callback is called for each matching file.
            @warning If you use this class, the scanFolderGeneric function will always return false (since no files are matched anyway), 
                     and you must set onlyFile to true (the default) */
        class EventIterator : public EntryIterator
        {
            // Type definition and enumeration
        public:
            /** You need to implement this callback to be notified when a file is found */
            struct FileFoundCB
            {
                /** This is called each time a file is found.
                    @param info             The file that's currently iterated upon
                    @param strippedFilePath The file path that was stripped from the given mount point
                    @return false to stop iterating */
                virtual bool fileFound(File::Info & info, const String & strippedFilePath) = 0;
                
                virtual ~FileFoundCB() {}
            };
        private:
            /** Set to true when the iteration is finished */
            bool finished;
            /** The callback to call */
            FileFoundCB & callback;
            
        public:
            /** Called to extract the next file in the given directory.
                @return false when no more file of interest in the given directory. */
            virtual bool getNextFile(File::DirectoryIterator & dir, File::Info & file, const String & name)
            {
                if (finished) return false;
                while (dir.getNextFilePath(file))
                {
                    if (file.name == "." || file.name == "..") continue;
                    
                    if (!callback.fileFound(file, name + file.name)) { finished = true; return false; }
                    if (recursive && file.isDir() && !file.isLink())
                    {
                        // Let the scanner feed the directory in the stack of directory to scan
                        return true;
                    }
                }
                return false;
            }
            /** You need to provide a logical callback that's not owned */
            EventIterator(const bool recursive, FileFoundCB & callback) : EntryIterator(recursive), callback(callback), finished(false) {}
        };
        
        /** The basic engine.
            This scans the files hierarchy in a depth last manner, that is for a folder structure like this:
            @verbatim
              root
                |--- file1
                |--- subDir1
                |       |--- file2
                |--- file3
            @endverbatim 
            
            will be filled/called in the following order: 
            @verbatim
            root, file1, subDir1, file3, file2
            @endverbatim */
        static bool scanFolderGeneric(String mountPath, const String & path, File::FileItemArray & array, EntryIterator & iterator, const bool onlyFiles = true)
        {
            Stack::WithClone::FIFO<File::FileItem> dirs;
            array.Clear();

            File::FileItem * item = 0;
            dirs.Push(new File::FileItem(File::General::normalizePath(path), 0, File::Info::Directory));

            mountPath = mountPath.normalizedPath(Platform::Separator);

            bool foundOne = false;
            while ((item = dirs.Pop()))
            {
                const String & name = item->name;
                if (!onlyFiles) array.Append(new File::FileItem(name, item->level + 1, File::Info::Directory));

                File::DirectoryIterator dir = File::General::listFilesIn(mountPath + name);
                File::Info file;
                while (iterator.getNextFile(dir, file, name))
                {
                    // Ignore hidden files and directories
                    if (file.isDir())
                    {
                        if (iterator.recursive)
                            dirs.Push(new File::FileItem(name + file.name + (char)Platform::Separator, item->level + 1, File::Info::Directory));
                    }
                    else if (!file.isDir() || !onlyFiles)
                    {
                        foundOne = true;
                        array.Append(new File::FileItem(name + file.name, item->level + 1, file.type, file.size, (uint32)file.modification));
                    }
                }
                delete item;
            }
            return foundOne;
        }

        /** Analyze the files in the given path and store the path of the files matching the given filter in array
            @param mountPath The mount point path (the file scanned have their path stripped from this mount point path, so if the mount point path change, the path doesn't)
            @param path      The initial directory path to scan
            @param array     On output, contains the matching files
            @param filters   The file filters to match against
            @param recursive When true, the folder is search recursively
            @param onlyFiles When set, only the files are returned, and not the directories
            @return true if at least a file was found */
        static bool scanFolder(const String & mountPath, const String & path, File::FileItemArray & array, FileFilters & filters, const bool recursive = false, const bool onlyFiles = true)
        {
            DefaultEntryIterator iterator(filters, recursive);
            return scanFolderGeneric(mountPath, path, array, iterator, onlyFiles);
        }
        /** Analyze the files in the given path and store the path of the files matching the given filter in array
            @warning This version doesn't try to stat the files found (so it doesn't fill the file size and modifications time information),
                     but is preferable for folders with tens of thousands of files (it's faster in that case).
            @param mountPath The mount point path (the file scanned have their path stripped from this mount point path, so if the mount point path change, the path doesn't)
            @param path      The initial directory path to scan
            @param array     On output, contains the matching files
            @param filters   The file filters to match against
            @param recursive When true, the folder is search recursively
            @param onlyFiles When set, only the files are returned, and not the directories
            @return true if at least a file was found */
        static bool scanFolderFilename(const String & mountPath, const String & path, File::FileItemArray & array, FileFilters & filters, const bool recursive = false, const bool onlyFiles = true)
        {
            FileNameOnlyIterator iterator(filters, recursive);
            return scanFolderGeneric(mountPath, path, array, iterator, onlyFiles);
        }
    };
}