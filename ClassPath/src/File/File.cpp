// We need our declaration
#include "../../include/File/File.hpp"
// We need time declaration
#include "../../include/Time/Time.hpp"

#ifdef _WIN32
#include <stdio.h>
#include <io.h>
#include <share.h>
#include <shlobj.h>
#include <sys/utime.h>

#if _MSC_VER > 1300
  #define chsize _chsize_s
#else
  #define chsize _chsize
#endif

#if _MSC_VER <= 1300
    extern "C" int __cdecl _fseeki64(FILE *, __int64, int);
    extern "C" __int64 __cdecl _ftelli64(FILE *);
#endif

#elif defined(_POSIX)
#include <sys/types.h>
#include <grp.h>
#include <pwd.h>
#include <aio.h>
#include <sys/mman.h>
#include <utime.h>
#include <sys/statvfs.h>
#include <wordexp.h>
#include <sys/stat.h>
#include <dirent.h>
#endif

#if defined(_MAC)
#include <sys/param.h>
#include <sys/ucred.h>
#include <sys/mount.h>
#include <dirent.h>
#endif

namespace File
{
    bool Info::checkPermission(const PermissionType type, const uint32 userID, const uint32 groupID) const
    {
        if (userID == groupID && groupID == (uint32)-1)
        {
#ifdef _WIN32
            Strings::ReadOnlyUnicodeString fileName = Strings::convert(getFullPath());
            if (type == Execution)
            {
                // Poor man executable testing
                if (_waccess(fileName.getData(), 4) == 0 && (name.Find(".exe") != -1 || name.Find(".com") != -1 || name.Find(".bat") != -1 || name.Find(".pif") != -1))
                    return true;
                return false;
            }
            return _waccess(fileName.getData(), type == Reading ? 4 : 2) == 0;
#elif defined(_POSIX)
            return access(getFullPath(), type == Reading ? R_OK : type == Writing ? W_OK : X_OK) == 0;
#endif
        }

#if defined(_POSIX)
        // We don't impersonate under windows
        struct group grp, *gr;
        char grbuf[32768];
        // Get user ID name
        struct passwd pwd, *ppwd;
        char pwbuf[32768];

        switch (type)
        {
        case Reading:
            // Check user
            if (userID == owner) return (permission & OwnerRead) > 0;
            if (getgrgid_r(group, &grp, grbuf, sizeof(grbuf), &gr)) return false;
            if (getpwuid_r(userID, &pwd, pwbuf, sizeof(pwbuf), &ppwd)) return false;

            // Check group
            for (uint32 i = 0; grp.gr_mem[i]; i++)
            {
                if (strcmp(pwd.pw_name, grp.gr_mem[i]) == 0) return (permission & GroupRead) > 0;
            }
            // Then check other
            if (permission & OtherRead) return true;
            break;
        case Writing:
            // Check user
            if (userID == owner) return (permission & OwnerWrite) > 0;
            if (getgrgid_r(group, &grp, grbuf, sizeof(grbuf), &gr)) return false;
            if (getpwuid_r(userID, &pwd, pwbuf, sizeof(pwbuf), &ppwd)) return false;

            // Check group
            for (uint32 i = 0; grp.gr_mem[i]; i++)
            {
                if (strcmp(pwd.pw_name, grp.gr_mem[i]) == 0) return (permission & GroupWrite) > 0;
            }
            // Then check other
            if (permission & OtherWrite) return true;
            break;
        case Execution:
            // Check user
            if (userID == owner) return (permission & OwnerExecute) > 0;
            if (getgrgid_r(group, &grp, grbuf, sizeof(grbuf), &gr)) return false;
            if (getpwuid_r(userID, &pwd, pwbuf, sizeof(pwbuf), &ppwd)) return false;

            // Check group
            for (uint32 i = 0; grp.gr_mem[i]; i++)
            {
                if (strcmp(pwd.pw_name, grp.gr_mem[i]) == 0) return (permission & GroupExecute) > 0;
            }
            // Then check other
            if (permission & OtherExecute) return true;
            break;
        }
#endif
        return false;
    }

    // Make the directory for this (invalid) file
    bool Info::makeDir(const bool recursive)
    {
        if (!recursive)
        {
#ifdef _WIN32
            Strings::ReadOnlyUnicodeString from = Strings::convert(getFullPath());
            return _wmkdir(from.getData()) == 0;
#elif defined(_POSIX)
            return mkdir(getFullPath(), 0755) == 0;
#endif
        }
        // Need to split the directory to make them recursively
        String fullPath = getFullPath();
        fullPath.rightTrim(PathSeparator); // Remove any trailing slash
        char * pt = (char*)(const char*)fullPath;
        while ((pt = strchr(pt+1, Platform::Separator)))
        {
            *pt = '\0';
            if (!Info((const char*)fullPath).isDir())
            {
#ifdef _WIN32
                Strings::ReadOnlyUnicodeString from = Strings::convert(fullPath);
                if (_wmkdir(from.getData())) return false;
#elif defined(_POSIX)
                if (mkdir((const char*)fullPath, 0755)) return false;
#endif
            }
            *pt = Platform::Separator;
        }
        restatFile();
        if (!isDir())
        {
#ifdef _WIN32
            Strings::ReadOnlyUnicodeString from = Strings::convert(getFullPath());
            if (_wmkdir(from.getData())) return false;
#elif defined(_POSIX)
            if (mkdir(getFullPath(), 0755)) return false;
#endif
        }
        return restatFile();
    }

    // Change the file modification time.
    bool Info::setModifiedTime(const double newTime)
    {
#ifdef _WIN32
        Strings::ReadOnlyUnicodeString from = Strings::convert(getFullPath());
        struct _utimbuf utim;
        utim.actime = (time_t)newTime; utim.modtime = (time_t)newTime;
        return _wutime(from.getData(), &utim) == 0;
#elif defined(_POSIX)
        struct utimbuf utim;
        utim.actime = (time_t)newTime; utim.modtime = (time_t)newTime;

        return utime(getFullPath(), &utim) == 0;
#endif
    }

    // Copy the file to the given path.
    bool Info::copyTo(const String & destination) const
    {
#ifdef _WIN32
        Strings::ReadOnlyUnicodeString from = Strings::convert(getFullPath());
        Info dest(destination);
        if (dest.isDir())
        {
            Strings::ReadOnlyUnicodeString to = Strings::convert(destination + PathSeparator + name);
            return CopyFileW(from.getData(), to.getData(), FALSE) == TRUE;
        }
        // Check if the destination's parent folder existence
        Info destParent(dest.getParentFolder());
        if (destParent.doesExist() || destParent.isDir())
        {
            Strings::ReadOnlyUnicodeString to = Strings::convert(destination);
            return CopyFileW(from.getData(), to.getData(), FALSE) == TRUE;
        }
        // Create the parent directory now
        if (!destParent.makeDir(true)) return false;
        Strings::ReadOnlyUnicodeString to = Strings::convert(destination);
        return CopyFileW(from.getData(), to.getData(), FALSE) == TRUE;
#elif defined(_POSIX)
#define MAXMAPSIZE      (1024*1024*8)-(1024*16)
#define SMALLFILESIZE   (32*1024)
#define min(a, b)       ((a) < (b) ? (a): (b))
        void *  map = 0;
        size_t  mapsize = 0;
        size_t  munmapsize = 0;
        off_t   offset = 0;
        struct stat status;
        Platform::FileIndexWrapper srcFd(open(getFullPath(), O_RDONLY, 0));
        if (srcFd < 0) return false;
        if (fstat(srcFd, &status) != 0) return false;
#if defined(_MAC)
        struct radvisory radv;
        radv.ra_offset = 0;
        radv.ra_count = size;
        fcntl(srcFd, F_RDADVISE, &radv);

        // Just tell that the source file is not going to be read multiple time and not seeked.
        fcntl( srcFd, F_NOCACHE, 1 );
        fcntl( srcFd, F_RDAHEAD, 1 );
#else
        posix_fadvise(srcFd, 0, size, POSIX_FADV_SEQUENTIAL);
#endif

        Platform::FileIndexWrapper destFd(open(destination, O_WRONLY | O_TRUNC | O_CREAT, status.st_mode));
        if (destFd < 0)
        {
            // Either the destination is a directory, either it's at a path that's not created yet.
            // Let's figure out the best behaviour
            Info dest(destination);
            if (dest.isDir())
            {   // It's a directory, let's create a file with the same name in that directory
                destFd.Mutate(open(destination + PathSeparator + name, O_WRONLY | O_TRUNC | O_CREAT, status.st_mode));
                // If it fails, it's probably a right issue, so let's fail too
                if (destFd < 0) return false;
            }
            else
            {
                // Either the file exists, but it's not allowed to be opened (let's fail in that case too)
                if (dest.doesExist()) return false;
                // Either it does not and we'll need to create the complete path to the destination
                Info destParent(dest.getParentFolder());
                if (destParent.doesExist() || destParent.isDir()) return false; // Right issue
                // Create the parent directory now
                if (!destParent.makeDir(true)) return false;
                // Finally try again to open the destination
                destFd.Mutate(open(destination, O_WRONLY | O_TRUNC | O_CREAT, status.st_mode));
                if (destFd < 0) return false;
            }
        }

        if (type == Regular && (size > SMALLFILESIZE))
        {
            munmapsize = mapsize = min(size, MAXMAPSIZE);

            // Start mapping
            map = mmap(NULL, mapsize, PROT_READ, (MAP_SHARED), srcFd, (off_t)0);
            if (map == MAP_FAILED) mapsize = 0;
            // Tell the kernel that we are reading the file sequentially
            madvise(map, mapsize, MADV_SEQUENTIAL);
        }

        if (mapsize == 0)
        {   // Read / write loop required
            char    buf[SMALLFILESIZE];
            int     pagesize = getpagesize();
            size_t  blocksize = min(SMALLFILESIZE, pagesize);

            while (1)
            {
                ssize_t n;
                n = read(srcFd, buf, blocksize);
                if (n == 0)
                {   // Success
                    struct utimbuf  times; times.actime = status.st_atime; times.modtime = status.st_mtime;
                    utime(destination, &times);
                    return true;
                }
                else if (n < 0) return false;

                if (write(destFd, buf, (size_t)n) != n) return false;
            }
        }

        off_t filesize = size;
        while (filesize)
        {   // MMap loop
            // Write the first map back to destination
            ssize_t nbytes = write(destFd, map, mapsize);

            if ((nbytes >= 0) && (nbytes !=(ssize_t)mapsize))
            {   // Retry writing the missing bytes
                size_t  remains = mapsize - nbytes;
                while (remains > 0)
                {
                    nbytes = write(destFd, ((uint8*)map + mapsize - remains), remains);
                    if (nbytes >= 0)
                    {
                        remains -= nbytes;
                        if (remains == 0)
                            nbytes = mapsize;
                        continue;
                    }

                    (void) munmap(map, munmapsize);
                    return false;
                }
            }

            if (nbytes < 0)
            {
                (void) munmap(map, munmapsize);
                return false;
            }

            filesize -= nbytes;
            if (filesize == 0) break;

            offset += nbytes;
            if ((size_t)filesize < mapsize) mapsize = (size_t)filesize;

            // Map next segment over existing mapping
            map = mmap(map, mapsize, PROT_READ, (MAP_SHARED | MAP_FIXED), srcFd, offset);
            if (map == MAP_FAILED)
            {
                (void) munmap(map, munmapsize);
                return false;
            }
            // Tell the kernel that we are reading the file sequentially
            madvise(map, mapsize, MADV_SEQUENTIAL);
        }

        (void) munmap(map, munmapsize);
        struct utimbuf  times; times.actime = status.st_atime; times.modtime = status.st_mtime;
        utime(destination, &times);
        return true;
#undef MAXMAPSIZE
#undef SMALLFILESIZE
#undef min

#endif
    }
    // Move the file to the given path.
    bool Info::moveTo(const String & destination)
    {
        // Need to check if we are allowed to move the file to the destination
#ifdef _WIN32
        Strings::ReadOnlyUnicodeString from = Strings::convert(getFullPath());
        Strings::ReadOnlyUnicodeString to = Strings::convert(destination);
        if (_wrename(from.getData(), to.getData()) == 0)
#else
        if (rename(getFullPath(), destination) == 0)
#endif
        {
            buildNameAndPath(destination);
            return true;
        }
        return false;
    }

#ifdef _WIN32
    // Win32 doesn't have a recursive remove folder function, so we have to wrap our on
    static BOOL RemoveFolderW(LPCWSTR pFolder)
    {
        WCHAR szFullPathFileName[MAX_PATH];
        WCHAR szFilename[MAX_PATH];

        if (!::RemoveDirectoryW(pFolder))
        {
            DWORD err = GetLastError();
            if (err != ERROR_DIR_NOT_EMPTY)
                return FALSE;
        }

        // remove sub folders and files.
        WIN32_FIND_DATAW FileData = {0};
        BOOL bFinished = FALSE;
        DWORD dwSize = 0;

        lstrcpyW(szFullPathFileName, pFolder);
        lstrcatW(szFullPathFileName, L"\\*.*");
        HANDLE hSearch = FindFirstFileW(szFullPathFileName, &FileData);
        if (hSearch == INVALID_HANDLE_VALUE)
            return 0;

        while (!bFinished)
        {
            lstrcpyW(szFilename, pFolder);
            lstrcatW(szFilename, L"\\");
            lstrcatW(szFilename, FileData.cFileName);
            if (FileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                if (lstrcmpW(FileData.cFileName, L".") && lstrcmpW(FileData.cFileName, L".."))
                {
                    RemoveFolderW(szFilename);
                    RemoveDirectoryW(szFilename);
                }
            }
            else
            {
                if (FileData.dwFileAttributes & FILE_ATTRIBUTE_READONLY)
                    SetFileAttributesW(szFilename, FILE_ATTRIBUTE_NORMAL);

                if (!::DeleteFileW(szFilename))
                {
                    FindClose(hSearch);
                    return FALSE;
                }
            }
            if (!FindNextFileW(hSearch, &FileData))
            {
                if (GetLastError() == ERROR_NO_MORE_FILES)
                    bFinished = TRUE;
            }
        }
        FindClose(hSearch);

        // Here the directory is empty.
        ::RemoveDirectoryW(pFolder);
        return TRUE;
    }
#elif defined(_POSIX)
    static int remove_file(const char *path)
    {
        struct stat path_stat;
        if (lstat(path, &path_stat) < 0)
            return errno != ENOENT ? -1 : 0;

        if (S_ISDIR(path_stat.st_mode))
        {
            DIR * dp;
            struct dirent * d;
            int status = 0;

            if ((dp = opendir(path)) == NULL)  return -1;
            while ((d = readdir(dp)) != NULL)
            {
                char * new_path;

                if (strcmp(d->d_name, ".") == 0 || strcmp(d->d_name, "..") == 0)
                    continue;

                new_path = (char*)malloc(strlen(d->d_name) + strlen(path) + 2);
                if (new_path == NULL)
                {
                    closedir(dp);
                    return -1;
                }

                strcpy(new_path, path);
                if (path[strlen(path)-1] != '/')  strcat(new_path, "/");
                strcat(new_path, d->d_name);
                if (remove_file(new_path) < 0)
                    status = -1;

                free(new_path);
            }

            if (closedir(dp) < 0)               return -1;
            if (rmdir(path) < 0)                return -1;
            return status;
        }
        return unlink(path) < 0 ? -1 : 0;
    }
#endif

    // Remove the file.
    bool Info::remove()
    {
        if (!checkPermission(Writing)) return false;
#ifdef _WIN32
        Strings::ReadOnlyUnicodeString fileName = Strings::convert(getFullPath());
        if (isDir()) return RemoveFolderW(fileName.getData()) == TRUE;

        if (_wunlink(fileName.getData()) == 0)
#elif defined(_POSIX)
        if (remove_file(getFullPath()) == 0)
#endif
        {
            size = 0; owner = 0; group = 0; creation = 0; modification = 0; lastAccess = 0; permission = 0; type = Regular;
            return true;
        }
        return false;
    }


    // Check if the file exists
    bool Info::doesExist() const
    {
        return checkPermission(Reading);
    }

    // Get the last 16 bytes of the file.
    String Info::last16Bytes() const
    {
    	FILE * pFile = 0;
#ifdef _WIN32
        Strings::ReadOnlyUnicodeString fileName = Strings::convert(getFullPath());
        pFile = _wfopen(fileName.getData(), L"rb");
#elif defined(_POSIX)
        pFile = fopen(getFullPath(), "rb");
#endif
        if (pFile == NULL) return "";
        fseek(pFile, 16, SEEK_END);
        uint8 buffer[16];
        if (fread(buffer, 1, 16, pFile) != 16) { fclose(pFile); return ""; }

        fclose(pFile);
        return String(buffer, 16);
    }


    // Build the path and filename from the given full path
    void Info::buildNameAndPath(const String & _fullPath)
    {
#ifdef _WIN32
        String fullPath = _fullPath;
        // Under windows, '/' is not allowed in paths, anyway, so let's parse it like if it was '\'
        fullPath.replaceAllTokens('/', '\\');
#else
        const String & fullPath = _fullPath;
#endif
        int pos = fullPath.reverseFind(Platform::Separator, fullPath.getLength());
        if (pos == -1)
        {
            path = "";
            name = fullPath;
        } else
        {
            path =
#ifdef _WIN32
#else
                pos == 0 ? "/" :
#endif
                fullPath.midString(0, pos);
            name = fullPath.midString(pos+1, fullPath.getLength());
        }
    }

#ifdef _WIN32
    static void convertAttributes(const DWORD winAccess, uint32 & flags, Info::Type & type, uint32 & owner, uint32 & group)
    {
        flags     = Info::OwnerRead | Info::OwnerWrite | Info::OwnerExecute |
                    Info::GroupRead | Info::GroupWrite | Info::GroupExecute |
                    Info::OtherRead | Info::OtherWrite | Info::OtherExecute;
        flags &= ~(winAccess & FILE_ATTRIBUTE_HIDDEN ? (Info::OwnerRead | Info::GroupRead | Info::OtherRead) : 0);
        flags &= ~(winAccess & FILE_ATTRIBUTE_READONLY ? (Info::OwnerWrite | Info::GroupWrite | Info::OtherWrite) : 0);
        flags &= ~(winAccess & FILE_ATTRIBUTE_SYSTEM ? (Info::OtherWrite | Info::OtherExecute) : 0);
        if (winAccess & FILE_ATTRIBUTE_SYSTEM) owner = group = 0;
        if (winAccess & FILE_ATTRIBUTE_DIRECTORY)
            type = Info::Directory;
    }
#endif

    // Reget the file information
    bool Info::restatFile()
    {
#ifdef _WIN32
        WIN32_FILE_ATTRIBUTE_DATA data = {0};
        Strings::ReadOnlyUnicodeString fileName = Strings::convert(getFullPath());
        if (GetFileAttributesExW(fileName.getData(), GetFileExInfoStandard, &data) == FALSE) return false;

        size = ((uint64)data.nFileSizeHigh << 32) | data.nFileSizeLow;
        creation = Time::convert(data.ftCreationTime);
        lastAccess = Time::convert(data.ftLastAccessTime);
        modification = Time::convert(data.ftLastWriteTime);
        type = Info::Regular;
        convertAttributes(data.dwFileAttributes, permission, type, owner, group);
        return true;
#elif defined(_POSIX)
        struct stat status = {0};
        if (stat(getFullPath(), &status) != 0) return false;
        owner = (uint32)status.st_uid;
        group = (uint32)status.st_gid;
        permission = (uint32)status.st_mode & 0777;
        size = (uint64)status.st_size;
        modification = (double)status.st_mtime;
        creation = (double)status.st_ctime;
        lastAccess = (double)status.st_atime;
        if (S_ISREG(status.st_mode)) type = Regular;
        if (S_ISDIR(status.st_mode)) type = Directory;
        if (S_ISCHR(status.st_mode)) type = Device;
        if (S_ISBLK(status.st_mode)) type = Device;
        if (S_ISFIFO(status.st_mode)) type = FIFO;
        if (S_ISLNK(status.st_mode)) type = Link;
        if (S_ISSOCK(status.st_mode)) type = Socket;

        // Check if the object is a link
        if (lstat(getFullPath(), &status) != 0) return false;
        if (S_ISLNK(status.st_mode)) type = Link;

        return true;
#endif
    }

    // Get the number of contained files/items inside this item.
    uint32 Info::getEntriesCount(const String & extension) const
    {
        if (!isDir() || isLink()) return 1; // We don't follow link in this function

#ifdef _WIN32
        // On directories, under windows, it's not possible to get the file count without iterating it, so let's do it
        WIN32_FIND_DATAW data = {0};
        Strings::ReadOnlyUnicodeString fileName = Strings::convert(getFullPath() + "\\*" + extension);

        HANDLE hFind = FindFirstFileW(fileName.getData(), &data);
        if (hFind == INVALID_HANDLE_VALUE) return 0;

        uint32 count = 1;
        while (FindNextFileW(hFind, &data) != FALSE && GetLastError() != ERROR_NO_MORE_FILES)
        {
            count++;
        }
        FindClose(hFind);
        return count < 2 ? 0 : count - 2;
#elif defined(_POSIX)
        DIR * finder = opendir(getFullPath());
        if (finder == 0) return 0;

        uint32 count = 0;
        struct dirent * ent = 0;
        // Then read the directory
        if (extension) while ((ent = readdir(finder)) != NULL) count+= String(ent->d_name).fromLast(".", true) == extension;
        else           while (readdir(finder) != NULL) count++;
        closedir(finder);
        /*
        struct stat status = {0};
        if (stat(getFullPath(), &status) != 0) return 0;
        return status.st_nlink - 2;
        */
        return count - 2;
#endif
    }


    // Get the complete content as a String.
    String Info::getContent() const
    {
        if (size > 0x80000000) return ""; // Too big
        BaseStream * stream = getStream(true, true, false);
        if (!stream) return "";
        String ret;
        if (size)
            ret.releaseLock(stream->read(ret.Alloc((int)size), (int)size));
        else
        {   // 0 bytes files can be special files, so try to read them anyway
            char buffer[512]; int bufSize = 0;
            while ((bufSize = stream->read(buffer, 512)) == 512) ret += String((void*)buffer, bufSize);
            ret += String((void*)buffer, bufSize);
        }
        delete stream;
        return ret;
    }
    // Replace the file content with the given string.
    bool Info::setContent(const String & content, const Info::SetContentMode mode)
    {
        if (mode.type != AtomicReplace)
        {
#ifdef _WIN32
            Strings::ReadOnlyUnicodeString fileName = Strings::convert(getFullPath());
            FILE * file = _wfopen(fileName.getData(), mode.type == Overwrite ? L"wb" : L"a+b");
#else
            FILE * file = fopen(getFullPath(), mode.type == Overwrite ? "wb" : "a+b");
#endif
            if (!file) return false;
            size_t ret = fwrite((const char *)content, 1, content.getLength(), file);
            fclose(file);
            return ret == (size_t)content.getLength();
        }

        // Create a temporary file and save in it
        String tmpName = (path ? (path + PathSeparator) : String("")) + String::Print(".%s_tmp_%d.tmp", (const char*)name, (int)clock());
        while (Info(tmpName).doesExist())
            tmpName += "_";

        BaseStream * stream = Info(tmpName).getStream(true, false, true);
        bool ret = stream->write((const char*)content, content.getLength()) == content.getLength();
        delete stream;

        // Then move the file over the expected one.
#ifdef _WIN32
        Strings::ReadOnlyUnicodeString from = Strings::convert(tmpName);
        Strings::ReadOnlyUnicodeString to = Strings::convert(getFullPath());
        if (ret && MoveFileExW(from.getData(), to.getData(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0)
#else
        if (ret && rename(tmpName, getFullPath()) == 0)
#endif
        {
            restatFile();
            return true;
        }
        return false;
    }
    // Create as a link to an existing file.
    bool Info::createAsLinkTo(const String & destination, const bool hardLink)
    {
        if (doesExist()) return false;
#ifdef _WIN32
        // We don't do hardlink since we don't check if the underlying FS support it, and all the file type, sorry
        if (hardLink) return false;
        Strings::ReadOnlyUnicodeString source = Strings::convert(getFullPath());
        Strings::ReadOnlyUnicodeString dest = Strings::convert(destination);
        return CreateSymbolicLinkW(source.getData(), dest.getData(), Info(destination).isDir() ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0) != 0;
#elif defined(_POSIX)
        if (hardLink)
            return link(destination, getFullPath()) == 0;
        return symlink(destination, getFullPath()) == 0;
#endif
        return false;
    }

    // Get the metadata information as an opaque buffer.
    String Info::getMetaData() const
    {
#ifdef _WIN32
        WIN32_FILE_ATTRIBUTE_DATA data = {0};
        Strings::ReadOnlyUnicodeString fileName = Strings::convert(getFullPath());
        if (GetFileAttributesExW(fileName.getData(), GetFileExInfoStandard, &data) == FALSE) return "";

        return String::Print("W%llX/%X/%llX/%llX/%llX", ((uint64)data.nFileSizeHigh << 32) | data.nFileSizeLow, data.dwFileAttributes, ((LARGE_INTEGER*)&data.ftCreationTime)->QuadPart, ((LARGE_INTEGER*)&data.ftLastWriteTime)->QuadPart, ((LARGE_INTEGER*)&data.ftLastAccessTime)->QuadPart);
#elif defined(_POSIX)
        struct stat status = {0};
        if (lstat(getFullPath(), &status) != 0) return "";

        if (type != Regular && type != Link && type != Directory && type != Device) return ""; // We don't support metadata saving for FIFO or socket

        // Save symbolic link specially
        if (S_ISLNK(status.st_mode))
        {
            char buffer[1024] = {0}; memset(buffer, 0, ArrSz(buffer));
            ssize_t linkSize = readlink(getFullPath(), buffer, ArrSz(buffer));
            if (linkSize <= 0) return ""; // Can't read this symbolic link, so we can't save it in metadata
            buffer[linkSize] = 0;

            return String::Print("PS%llX/%llX/%X/%llX/%X/%X/%X/%llX/%llX/%llX/%s", (uint64)status.st_dev, (uint64)status.st_ino, status.st_mode, (uint64)status.st_size, status.st_nlink, status.st_uid, status.st_gid, (uint64)status.st_ctime, (uint64)status.st_mtime, (uint64)status.st_atime, buffer);
        }

        // Save device node specially
        if (S_ISCHR(status.st_mode) || S_ISBLK(status.st_mode))
        {
            return String::Print("PT%c%llX/%llX/%X/%llX/%X/%X/%X/%llX/%llX/%llX/%llX", S_ISCHR(status.st_mode) ? 'H' : 'L', (uint64)status.st_dev, (uint64)status.st_ino, status.st_mode, (uint64)status.st_size, status.st_nlink, status.st_uid, status.st_gid, (uint64)status.st_ctime, (uint64)status.st_mtime, (uint64)status.st_atime, (uint64)status.st_rdev);
        }

        // TODO: Check for hardlinks and save them once (not done for now, can be done at a higher level by checking the first 2 part of the tuple)
        return String::Print("P%llX/%llX/%X/%llX/%X/%X/%X/%llX/%llX/%llX", (uint64)status.st_dev, (uint64)status.st_ino, status.st_mode, (uint64)status.st_size, status.st_nlink, status.st_uid, status.st_gid, (uint64)status.st_ctime, (uint64)status.st_mtime, (uint64)status.st_atime);
#else
        // Perform a stat
        return ""; // No metadata or unknown
#endif
    }

    // Get a compressed version of the metadata informations.
    uint32 File::Info::getMetaDataEx(uint8 * buffer, const size_t len) const
    {
#if defined(_POSIX)
        // Compression is posix only
        struct stat status = {0};
        if (lstat(getFullPath(), &status) != 0) return 0;

        if (type != Regular && type != Link && type != Directory && type != Device) return 0;
        if (sizeof(status.st_mode) == 2 && sizeof(status.st_nlink) == 2 && sizeof(status.st_size) == 8)
        {
            size_t cur = 0;
            uint16 metaInf = 0; // This metaInf field is stored to tell how big are the next fields
                                // It's a bit field like this:
                                // [15 14 13 12 11 10 9 8  7 6 5 4 3 2 1 0]
                                // Bit [15] file size is smaller than 2^32 and stored as 32 bits integer
                                // Bit [14] file size is smaller than 2^16 and stored as 16 bits integer
                                // Bit [13] uid is smaller than 2^16 and stored as 16 bits integer (else 32 bits)
                                // Bit [12] gid is smaller than 2^16 and stored as 16 bits integer (else 32 bits)
                                // Bit [11] ctime is the same as mtime, thus skipped (not saved)
                                // Bit [10] atime is later than mtime, and (atime - mtime) is smaller than 2^32 and stored as 32 bits integer
                                // Bit [ 9] atime is later than mtime, and (atime - mtime) is smaller than 2^16 and stored as 16 bits integer
                                // Bit [ 8] nlink is higher than one (in that case, the dev_t and ino_t value are saved after this field)
                                // Bit [ 7] dev_t value is smaller than 2^16 and stored as 16 bits integer (else 32 bits)
                                // Bit [ 6] ino_t value is smaller than 2^32 and stored as 32 bits integer
                                // Bit [ 5] ino_t value is smaller than 2^16 and stored as 16 bits integer
                                // Bit [ 4] for char or block type, rdev_t is smaller than 2^16 and stored as 16 bits integer (else 32 bits)
                                // Bit [3-0] reserved, must be 0
            metaInf |= (1<<15) * (status.st_size < 0x100000000ULL);
            metaInf |= (1<<14) * (status.st_size < 0x10000);
            metaInf |= (1<<13) * (status.st_uid < 0x10000);
            metaInf |= (1<<12) * (status.st_gid < 0x10000);
            metaInf |= (1<<11) * (status.st_ctime == status.st_mtime);
            uint64 timeSinceModif = (uint64)(status.st_atime - status.st_mtime);
            metaInf |= (1<<10) * (timeSinceModif < 0x100000000ULL);
            metaInf |= (1<< 9) * (timeSinceModif < 0x10000);

            metaInf |= (1<< 8) * (status.st_nlink > 1);
            metaInf |= (1<< 7) * (status.st_dev < 0x10000);
            metaInf |= (1<< 6) * (status.st_ino < 0x100000000ULL);
            metaInf |= (1<< 5) * (status.st_ino < 0x10000);
            metaInf |= (1<< 4) * ((S_ISCHR(status.st_mode) || S_ISBLK(status.st_mode)) * status.st_rdev < 0x10000);

            #define Adv(elem)   if (buffer && cur + sizeof(elem) <= len) memcpy(&buffer[cur], &elem, sizeof(elem)); cur += sizeof(elem)
            // Store meta information and mode
            Adv(metaInf);
            Adv(status.st_mode);

            #define AdvCond(Bit, elem, cast)  if ((metaInf & (1<<Bit)) == 0) { cast s = (cast)elem; Adv(s); }

            // Store size with the minimum amount of bytes possible
            AdvCond(15, status.st_size, uint64)
            else AdvCond(14, status.st_size, uint32)
            else { uint16 size = (uint16)status.st_size; Adv(size); }

            // Store uid and gid efficiently
            AdvCond(13, status.st_uid, uint32)
            else { uint16 uid = (uint16)status.st_uid; Adv(uid); }
            AdvCond(12, status.st_gid, uint32)
            else { uint16 gid = (uint16)status.st_gid; Adv(gid); }

            // Store times
            Adv(status.st_mtime);
            AdvCond(11, status.st_ctime, uint64)
            AdvCond(10, timeSinceModif, uint64)
            else AdvCond(9, timeSinceModif, uint32)
            else { uint16 tsm = (uint16)timeSinceModif; Adv(tsm); }

            // Store nlink & dev_t/ino_t
            if ((metaInf & (1<<8)) != 0)
            {
                AdvCond(7, status.st_dev, uint32)
                else { uint16 d = (uint16)status.st_dev; Adv(d); }

                AdvCond(6, status.st_ino, uint64)
                else AdvCond(5, status.st_ino, uint32)
                else { uint16 i = (uint16)status.st_ino; Adv(i); }
                status.st_nlink = 2; // We don't really care about the number of links, it just has to be higher than 1 so we can find such files and figure out the hardlinked items this way
            }
            if (S_ISCHR(status.st_mode) || S_ISBLK(status.st_mode))
            {   // Device type
                AdvCond(4, status.st_rdev, uint32)
                else { uint16 d = (uint16)status.st_rdev; Adv(d); }
            }
            if (S_ISLNK(status.st_mode))
            {
                char _buffer[1024] = {0};
                ssize_t linkSize = readlink(getFullPath(), _buffer, ArrSz(_buffer));
                if (linkSize <= 0) return 0; // Can't read this symbolic link, so we can't save it in metadata
                _buffer[linkSize] = 0;
                if (buffer && cur + linkSize <= len) memcpy(&buffer[cur], _buffer, linkSize);
                cur += linkSize;
            }

            #undef AdvCond
            #undef Adv
            return cur;
        }
#endif
        // Default to non-compressed form
        const String & res = getMetaData();
        if (buffer && len >= res.getLength()) memcpy(buffer, (const char*)res, res.getLength());
        return res.getLength();
    }
    // Expand the compressed metadata buffer received from call to getMetaDataEx.
    String File::Info::expandMetaData(const uint8 * buffer, const size_t len)
    {
#if defined(_POSIX)
        struct stat status = {0};
        if (buffer && sizeof(status.st_mode) == 2 && sizeof(status.st_nlink) == 2 && sizeof(status.st_size) == 8)
        {
            size_t cur = 0;
            uint16 metaInf = 0;
            #define Rd(X) if (cur + sizeof(X) <= len) memcpy(&X, &buffer[cur], sizeof(X)); cur += sizeof(X)
            Rd(metaInf);
            Rd(status.st_mode);

            #define RdCond(Bit, elem, cast)  if ((metaInf & (1<<Bit)) == 0) { cast s = 0; Rd(s); elem = s; }
            #define RdCondE(elem, cast) { cast s = 0; Rd(s); elem = s; }

            // Read size with the minimum amount of bytes possible
            RdCond(15, status.st_size, uint64)
            else RdCond(14, status.st_size, uint32)
            else RdCondE(status.st_size, uint16)

            // Read uid and gid efficiently
            RdCond(13, status.st_uid, uint32)
            else RdCondE(status.st_uid, uint16)
            RdCond(12, status.st_gid, uint32)
            else RdCondE(status.st_gid, uint16)

            // Read times
            Rd(status.st_mtime);
            RdCond(11, status.st_ctime, uint64)
            else status.st_ctime = status.st_mtime;
            int64 timeSinceModif = -1;
            RdCond(10, timeSinceModif, uint64)
            else RdCond(9, timeSinceModif, uint32)
            else RdCondE(timeSinceModif, uint16)
            status.st_atime = (time_t)(timeSinceModif + status.st_mtime);

            // Store nlink & dev_t/ino_t
            if ((metaInf & (1<<8)) != 0)
            {
                RdCond(7, status.st_dev, uint32)
                else RdCondE(status.st_dev, uint16)

                RdCond(6, status.st_ino, uint64)
                else RdCond(5, status.st_ino, uint32)
                else RdCondE(status.st_ino, uint16)
            }
            status.st_nlink = (metaInf & (1<<8)) > 0 ? 2 : 1; // We don't care if it's higher than 2 anyway
            if (S_ISCHR(status.st_mode) || S_ISBLK(status.st_mode))
            {   // Device type
                RdCond(4, status.st_rdev, uint32)
                else RdCondE(status.st_rdev, uint16)

                return String::Print("PT%c%llX/%llX/%X/%llX/%X/%X/%X/%llX/%llX/%llX/%llX", S_ISCHR(status.st_mode) ? 'H' : 'L', (uint64)status.st_dev, (uint64)status.st_ino, status.st_mode, (uint64)status.st_size, status.st_nlink, status.st_uid, status.st_gid, (uint64)status.st_ctime, (uint64)status.st_mtime, (uint64)status.st_atime, (uint64)status.st_rdev);
            }
            if (S_ISLNK(status.st_mode))
            {
                char _buffer[1024] = {0};
                memcpy(_buffer, &buffer[cur], len - cur);
                _buffer[len - cur] = 0;
                return String::Print("PS%llX/%llX/%X/%llX/%X/%X/%X/%llX/%llX/%llX/%s", (uint64)status.st_dev, (uint64)status.st_ino, status.st_mode, (uint64)status.st_size, status.st_nlink, status.st_uid, status.st_gid, (uint64)status.st_ctime, (uint64)status.st_mtime, (uint64)status.st_atime, _buffer);
            }
            // Every other case should get a standard output
            return String::Print("P%llX/%llX/%X/%llX/%X/%X/%X/%llX/%llX/%llX", (uint64)status.st_dev, (uint64)status.st_ino, status.st_mode, (uint64)status.st_size, status.st_nlink, status.st_uid, status.st_gid, (uint64)status.st_ctime, (uint64)status.st_mtime, (uint64)status.st_atime);
 
            #undef RdCondE
            #undef RdCond
            #undef Rd
        }
#endif
        // On any other platform, the extended metadata is still a string
        return String(buffer, len);
    }

    // Set the metadata information from an opaque buffer.
    bool File::Info::setMetaData(String metadata)
    {
#ifdef _WIN32
        // TODO Handle symlinks
        // Check if the given metadata is good
        if (!metadata || metadata[0] != 'W') return false;
        uint64 expectedSize = (uint64)metadata.splitUpTo("/").midString(1, 17).parseInt(16);
        DWORD  attribute = (DWORD)metadata.splitUpTo("/").parseInt(16);
        uint64 creatTime = (uint64)metadata.splitUpTo("/").parseInt(16);
        uint64 writeTime = (uint64)metadata.splitUpTo("/").parseInt(16);
        uint64 accesTime = (uint64)metadata.splitUpTo("/").parseInt(16);

        // Update the time and attribute
        Strings::ReadOnlyUnicodeString fileName = Strings::convert(getFullPath());
        HANDLE hFile = NULL;
        if (restatFile() && doesExist())
        {
            if (size != expectedSize) return false;
            hFile = CreateFileW(fileName.getData(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
        } else
        {
            hFile = CreateFileW(fileName.getData(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
        }

        if (hFile == INVALID_HANDLE_VALUE) return false;
        // Create the file
        FILETIME creat, acces, writ;
        LARGE_INTEGER li;
        li.QuadPart = creatTime; creat.dwLowDateTime = li.LowPart; creat.dwHighDateTime = li.HighPart;
        li.QuadPart = accesTime; acces.dwLowDateTime = li.LowPart; acces.dwHighDateTime = li.HighPart;
        li.QuadPart = writeTime; writ.dwLowDateTime = li.LowPart; writ.dwHighDateTime = li.HighPart;


        SetFileTime(hFile, &creat, &acces, &writ);
        CloseHandle(hFile);
        SetFileAttributesW(fileName.getData(), attribute);
#elif defined(_POSIX)
        if (!metadata || metadata[0] != 'P') return false; // Can't restore non-posix metadata here. Sorry
        // Need to split the metadata array
        bool isSymLink = metadata[1] == 'S';
        bool isDevNode = metadata[1] == 'T';
        // bool isCharDev = metadata[2] == 'H';
        // We don't care about the dev/inode number itself, since we can't restore it.
        // However, we are interested in matching hardlinks so let's extract this.
        metadata.leftTrim("PSTHL");
        String otherData = metadata.fromFirst("/").fromFirst("/");
        String hardlinkHash = metadata.midString(0, metadata.getLength() - otherData.getLength() - 1);
        // For now, we can't restore hardlinks, since we don't know the other part to link with

        // Let's extract everything else
        struct stat status = {0};
        status.st_mode  = (uint32)otherData.splitUpTo("/").parseInt(16);
        status.st_size  = (uint64)otherData.splitUpTo("/").parseInt(16);
        status.st_nlink = (uint32)otherData.splitUpTo("/").parseInt(16);
        status.st_uid   = (uint32)otherData.splitUpTo("/").parseInt(16);
        status.st_gid   = (uint32)otherData.splitUpTo("/").parseInt(16);
        status.st_ctime = (uint64)otherData.splitUpTo("/").parseInt(16);
        status.st_mtime = (uint64)otherData.splitUpTo("/").parseInt(16);
        status.st_atime = (uint64)otherData.splitUpTo("/").parseInt(16);

        restatFile();
        bool isFolder = S_ISDIR(status.st_mode) > 0;
        if (isSymLink)
        {
            // Need to create the symbolic link (if it does not exists yet)
            if (doesExist())
            {
                if (!isLink()) return false; // Don't overwrite an existing file by a symlink
                // Check if it's pointing to the right value
                char buffer[1024] = {0}; memset(buffer, 0, ArrSz(buffer));
                ssize_t linkSize = readlink(getFullPath(), buffer, ArrSz(buffer));
                if (linkSize <= 0) return false; // It's a link, but we can't read it, probably permission error, so let's abort now.

                // Compare the link name with the stored version
                if (otherData != buffer) return false; // Not the same link, don't overwrite it
            } else
            {
                // Need to create one
                if (symlink(otherData, getFullPath()) != 0) return false; // Can't create the symlink, probably permission error
            }
        }
        else if (isDevNode)
        {
            // Need to create the device node (if it does not exists yet)
            status.st_rdev = (uint64)otherData.splitUpTo("/").parseInt(16);
            if (doesExist())
            {
                if (!isDevice()) return false;
                // Check if the existing file is correct already
                struct stat devstatus = {0};
                if (lstat(getFullPath(), &devstatus) != 0) return false;

                if (devstatus.st_rdev != status.st_rdev) return false;
            }
            else
            {
                // Need to create one
                if (mknod(getFullPath(), status.st_mode, status.st_rdev) != 0) return false;
            }
        }
        else if (doesExist())
        {
            if (size != (off_t)status.st_size) return false; // Not the same size, don't modify it
        }
        else if (isFolder)
        {
            // Need to create it
            if (mkdir(getFullPath(), status.st_mode & 0xFFF) != 0) return false;
            // Can't restore the link count, obviously, nor the size.
        }
        else
        {
            // Need to create it
            Platform::FileIndexWrapper fd(creat(getFullPath(), status.st_mode & 0xFFF));
            if (fd == -1) return false; // Error while creating the file
            // Set the size
            if (ftruncate(fd, status.st_size) != 0) return false; // Can't set the file size
        }



        // Set the owner now too
        if (lchown(getFullPath(), status.st_uid, status.st_gid) != 0) return false; // Permission error ?
        // Change the mode too (if required)
        if (isSymLink)
        {
            // If it's a symlink, it's a bit more complex, since the mode need to be restored on the link itself
            // and this operation is not supported on all unices.
            // So try to deal smartly
            // For example, on Linux, all symlink are 0777 anyway, so avoid calling lchmod which is not supported in that platform.
            // On BSD, this should still work as expected
            struct stat currentStat = {0};
            if (lstat(getFullPath(), &currentStat) == 0 && currentStat.st_mode != status.st_mode && lchmod(getFullPath(), status.st_mode) != 0) return false;
        } else if (chmod(getFullPath(), status.st_mode) != 0) return false;
        // Set the time now for this file and we are done
        struct timeval fix[2];
        fix[0].tv_sec = status.st_atime; fix[1].tv_sec = status.st_mtime;
        fix[0].tv_usec = fix[1].tv_usec = 0;

        if (lutimes(getFullPath(), fix) != 0) return false; // Permission error ?
#else
        return false;
#endif
        return true;
    }
    // Analyze the metadata information from an opaque buffer.
    bool File::Info::analyzeMetaData(String metadata)
    {
#ifdef _WIN32
        // TODO Handle symlinks
        // Check if the given metadata is good
        if (!metadata || metadata[0] != 'W') return false;
        size = (uint64)metadata.splitUpTo("/").midString(1, 17).parseInt(16);
        DWORD  attribute = (DWORD)metadata.splitUpTo("/").parseInt(16);
        uint64 creatTime = (uint64)metadata.splitUpTo("/").parseInt(16);
        uint64 writeTime = (uint64)metadata.splitUpTo("/").parseInt(16);
        uint64 accesTime = (uint64)metadata.splitUpTo("/").parseInt(16);



        // Create the file
        FILETIME creat, acces, writ;
        LARGE_INTEGER li;
        li.QuadPart = creatTime; creat.dwLowDateTime = li.LowPart; creat.dwHighDateTime = li.HighPart;
        li.QuadPart = accesTime; acces.dwLowDateTime = li.LowPart; acces.dwHighDateTime = li.HighPart;
        li.QuadPart = writeTime; writ.dwLowDateTime = li.LowPart; writ.dwHighDateTime = li.HighPart;

        creation = Time::convert(creat);
        lastAccess = Time::convert(acces);
        modification = Time::convert(writ);
        type = Info::Regular;
        convertAttributes(attribute, permission, type, owner, group);

#elif defined(_POSIX)
        if (!metadata || metadata[0] != 'P') return false; // Can't restore non-posix metadata here. Sorry

        // Need to split the metadata array
        bool isSymLink = metadata[1] == 'S';
        // bool isDevNode = metadata[1] == 'T';
        // bool isCharDev = metadata[2] == 'H';

        // We don't care about the dev/inode number itself, since we can't restore it.
        // However, we are interested in matching hardlinks so let's extract this.
        metadata.leftTrim("PSTHL");
        String otherData = metadata.fromFirst("/").fromFirst("/");
        String hardlinkHash = metadata.midString(0, metadata.getLength() - otherData.getLength() - 1);
        // For now, we can't restore hardlinks, since we don't know the other part to link with

        // Let's extract everything else
        struct stat status = {0};
        status.st_mode = (uint32)otherData.splitUpTo("/").parseInt(16);
        status.st_size = (uint64)otherData.splitUpTo("/").parseInt(16);
        status.st_nlink = (uint32)otherData.splitUpTo("/").parseInt(16);
        status.st_uid = (uint32)otherData.splitUpTo("/").parseInt(16);
        status.st_gid = (uint32)otherData.splitUpTo("/").parseInt(16);
        status.st_ctime = (uint64)otherData.splitUpTo("/").parseInt(16);
        status.st_mtime = (uint64)otherData.splitUpTo("/").parseInt(16);
        status.st_atime = (uint64)otherData.splitUpTo("/").parseInt(16);

        owner = (uint32)status.st_uid;
        group = (uint32)status.st_gid;
        permission = (uint32)status.st_mode & 0777;
        size = (uint64)status.st_size;
        modification = (double)status.st_mtime;
        creation = (double)status.st_ctime;
        lastAccess = (double)status.st_atime;
        if (S_ISREG(status.st_mode)) type = Regular;
        if (S_ISDIR(status.st_mode)) type = Directory;
        if (S_ISCHR(status.st_mode)) type = Device;
        if (S_ISBLK(status.st_mode)) type = Device;
        if (S_ISFIFO(status.st_mode)) type = FIFO;
        if (S_ISLNK(status.st_mode)) type = Link;
        if (S_ISSOCK(status.st_mode)) type = Socket;

        // Check if the object is a link
        if (isSymLink) type = Link;
#endif
        return true;
    }


    inline String makeLegibleSize(uint64 size)
    {
        const char * suffix[] = { "B", "KB", "MB", "GB", "TB", "PB", "EB" };
        int suffixPos = 0, lastReminder = 0;
        while (size / 1024) { suffixPos++; lastReminder = size % 1024; size /= 1024; }
        return String::Print("%lld.%d%s", size, (lastReminder * 10 / 1024), suffix[suffixPos]);
    }

    inline String getOwnerGroupTxt(uint32 owner, uint32 group)
    {
#ifdef _WIN32
        if (owner == 0 || group == 0)
            return String::Print("System");
#elif defined(_POSIX)
        struct group grp, *gr;
        char grbuf[32768];
        struct passwd pwd, *ppwd;
        char pwbuf[32768];

        String groupTxt = String::Print("%u", group);
        if (getgrgid_r(group, &grp, grbuf, sizeof(grbuf), &gr) == 0)
            groupTxt = grp.gr_name;
        String userTxt = String::Print("%u", owner);
        if (getpwuid_r(owner, &pwd, pwbuf, sizeof(pwbuf), &ppwd) == 0)
            userTxt = pwd.pw_name;
        return userTxt + ":" + groupTxt;
#endif
        return String::Print("%u:%u", owner, group);
    }

    inline String makePerm(uint32 mode)
    {
        static const char *rwx[] = {"---", "--x", "-w-", "-wx", "r--", "r-x", "rw-", "rwx"};
        static char bits[11];

        strcpy(&bits[0], rwx[(mode >> 6) & 7]);
        strcpy(&bits[3], rwx[(mode >> 3) & 7]);
        strcpy(&bits[6], rwx[(mode & 7)]);
        if (mode & Info::OwnerSUID)
            bits[2] = (mode & 0100) ? 's' : 'S';
        if (mode & Info::GroupSUID)
            bits[5] = (mode & 0010) ? 's' : 'l';
        if (mode & Info::StickyBit)
            bits[8] = (mode & 0100) ? 't' : 'T';
        bits[9] = ' ';
        bits[10] = '\0';
        return bits;
    }

    // Print the metadata information to something understandable.
    String File::Info::printMetaData(String metadata)
    {
#ifdef _WIN32
        // TODO Handle symlinks
        // Check if the given metadata is good
        if (!metadata || metadata[0] != 'W') return "<NW>";

        uint64 mdSize = (uint64)metadata.splitUpTo("/").midString(1, 17).parseInt(16);
        DWORD  attribute = (DWORD)metadata.splitUpTo("/").parseInt(16);
        uint64 creatTime = (uint64)metadata.splitUpTo("/").parseInt(16);
        uint64 writeTime = (uint64)metadata.splitUpTo("/").parseInt(16);
        uint64 accesTime = (uint64)metadata.splitUpTo("/").parseInt(16);



        // Create the file
        FILETIME writ;
        LARGE_INTEGER li;
        li.QuadPart = writeTime; writ.dwLowDateTime = li.LowPart; writ.dwHighDateTime = li.HighPart;

        Type typ = Info::Regular;
        uint32 perm = 0, own = 0, grp = 0;
        convertAttributes(attribute, perm, typ, own, grp);

        return (typ == Info::Regular ? "-" : "d") + makePerm(perm) + getOwnerGroupTxt(own, grp).alignedTo(19, -1) + " " + makeLegibleSize(mdSize).alignedTo(7,1) + " " + Time::toLocal(Time::Time(Time::convert(writ))).toDate(true);

#elif defined(_POSIX)
        if (!metadata || metadata[0] != 'P') return "<NP>"; // Can't restore non-posix metadata here. Sorry




        // We don't care about the dev/inode number itself, since we can't restore it.
        // However, we are interested in matching hardlinks so let's extract this.
        metadata.leftTrim("PSTHL");
        String otherData = metadata.fromFirst("/").fromFirst("/");
        String hardlinkHash = metadata.midString(0, metadata.getLength() - otherData.getLength() - 1);
        // For now, we can't restore hardlinks, since we don't know the other part to link with

        // Let's extract everything else
        struct stat status = {0};
        status.st_mode = (uint32)otherData.splitUpTo("/").parseInt(16);
        status.st_size = (uint64)otherData.splitUpTo("/").parseInt(16);
        status.st_nlink = (uint32)otherData.splitUpTo("/").parseInt(16);
        status.st_uid = (uint32)otherData.splitUpTo("/").parseInt(16);
        status.st_gid = (uint32)otherData.splitUpTo("/").parseInt(16);
        status.st_ctime = (uint64)otherData.splitUpTo("/").parseInt(16);
        status.st_mtime = (uint64)otherData.splitUpTo("/").parseInt(16);
        status.st_atime = (uint64)otherData.splitUpTo("/").parseInt(16);


        String typeOfFile = "-";
        switch (status.st_mode & S_IFMT)
        {
        case S_IFBLK:  typeOfFile = "b"; break;
        case S_IFCHR:  typeOfFile = "c"; break;
        case S_IFDIR:  typeOfFile = "d"; break;
        case S_IFIFO:  typeOfFile = "f"; break;
        case S_IFLNK:  typeOfFile = "l"; break;
        case S_IFSOCK: typeOfFile = "s"; break;
        default: break;
        }
        return typeOfFile + makePerm(status.st_mode & 0xFFFF) + getOwnerGroupTxt(status.st_uid, status.st_gid).alignedTo(19, -1) + " " + makeLegibleSize(status.st_size).alignedTo(7, 1) + " " + Time::Time(status.st_mtime, 0).toDate(true);

#endif
        return "<NA>";
    }

    // Check if the given metadata match the current file.
    bool Info::hasSimilarMetadata(String metadata, const Comparand checkMask, const String * override) const
    {
        File::Info useless, tmp;
        if (checkMask == All) return metadata == (override ? *override : getMetaData());

        if (!useless.analyzeMetaData(metadata)) return false;
        if (override)
        {
            if(!tmp.analyzeMetaData(*override)) return false;
        }
        else tmp = *this;
        if (useless.owner != tmp.owner || useless.group != tmp.group || useless.permission != tmp.permission || useless.size != tmp.size || useless.type != tmp.type) return false;

        switch(checkMask)
        {
        case AllButAccessTime:
            if (useless.modification != tmp.modification || useless.creation != tmp.creation) return false;
            return true;
        case AllButTimes:
            return true;
        default:
            return false;
        }
        return false;
    }

    // Get the real path to the file (resolve the links and directory stack stuff)
    String Info::getRealFullPath() const
    {
#ifdef _WIN32
        Strings::ReadOnlyUnicodeString fileName = Strings::convert(getFullPath());
        wchar_t * fullPath = _wfullpath(NULL, fileName.getData(), 0);
        if (fullPath == NULL) return "";

        // Convert to UTF-8 again
        Strings::ReadOnlyUnicodeString unicodePath(fullPath);
        String finalFullPath = Strings::convert(unicodePath);
        free(fullPath);
        return finalFullPath;

#elif defined(_POSIX)
        char * fullPath = realpath(getFullPath(), NULL);
        if (fullPath == NULL) return "";
        String finalFullPath(fullPath);
        free(fullPath);
        return finalFullPath;
#endif
        return path;
    }
    // Get the parent folder
    String Info::getParentFolder() const
    {
        String realParent = Info(path).getRealFullPath();
        if (!realParent) return General::normalizePath(path);
        return realParent;
    }
    // Set the file permission (if applicable).
    bool Info::setPermission(const uint32 permission)
    {
#if defined(_POSIX)
        // Beware that in case of symlink, the final point is changed, not the link itself.
        if (chmod(getFullPath(), permission) != 0) return false;
        this->permission = permission;
        return true;
#endif
        return false; // Not supported. The permission convertion hack is not reversed here as it's not bijective
    }
    // Set the user and group owner.
    bool Info::setOwner(const uint32 userID, const uint32 groupID, const bool followLink)
    {
        if (userID == (uint32)-1 && groupID == (uint32)-1) return false;
#if defined(_POSIX)
        if (followLink)
        {
            if (chown(getFullPath(), userID, groupID) != 0) return false;
        } else if (lchown(getFullPath(), userID, groupID) != 0) return false;
        owner = userID; group = groupID;
        return true;
#endif
        return false;
    }


    // Get a stream from this file.
    BaseStream * Info::getStream(const bool blocking, const bool forceReadOnly, const bool overwrite) const
    {
        if (!checkPermission(Reading))
        {
            if (forceReadOnly) return 0;
            if (blocking) return new Stream(getFullPath(), "wb");
#if WantAsyncFile == 1
            return new AsyncStream(getFullPath(), AsyncStream::ReadWrite);
#else
            return 0;
#endif
        }
        if (blocking) return new Stream(getFullPath(), !forceReadOnly && checkPermission(Writing) ? (overwrite ? "wb" : "r+b") : "rb");
#if WantAsyncFile == 1
        else return new AsyncStream(getFullPath(), !forceReadOnly && checkPermission(Writing) ? AsyncStream::ReadWrite : AsyncStream::Read);
#endif
        return 0;
    }

    Info::Info(const String & fullPath) : size(0), creation(0), modification(0), lastAccess(0), owner(0), group(0), permission(0), type(Regular)
    {
        buildNameAndPath(fullPath);
        restatFile();
    }

    Info::Info(const String & fullPath, const bool expandVar) : size(0), creation(0), modification(0), lastAccess(0), owner(0), group(0), permission(0), type(Regular)
    {
#ifdef _WIN32
        if (expandVar)
        {
            Strings::ReadOnlyUnicodeString fileName = Strings::convert(fullPath);
            wchar_t * dest = 0;
            DWORD size = ExpandEnvironmentStringsW(fileName.getData(), dest, 0);
            if (size)
            {
                dest = new wchar_t[size];
                ExpandEnvironmentStringsW(fileName.getData(), dest, size);
                buildNameAndPath(Strings::convert(Strings::ReadOnlyUnicodeString(dest)));
                delete[] dest;
                restatFile();
                return;
            }
        }
        buildNameAndPath(fullPath);

#elif defined(_POSIX)
        wordexp_t p;
        if (expandVar && wordexp( (const char*)fullPath, &p, 0 ) == 0)
        {
            if (p.we_wordc) buildNameAndPath(Strings::StringArray(p.we_wordv, p.we_wordc).Join(" "));
            else buildNameAndPath(fullPath);
            wordfree(&p);
        }
        else buildNameAndPath(fullPath);

        // for (size_t i=0; i<p.we_wordc;i++ ) printf("%s\n", p.we_wordv[i]);
#endif
        restatFile();
    }


    // Get the next file iteratively
    bool DirectoryIterator::getNextFile(Info & info) const
    {
#ifdef _WIN32
        if (finder == INVALID_HANDLE_VALUE) return false;
        info.name = Strings::convert(Strings::ReadOnlyUnicodeString(data.cFileName));
        if (info.path.getLength() == 0)
            info.path = path.normalizedPath(Platform::Separator, false);
        info.size = ((uint64)data.nFileSizeHigh << 32) | data.nFileSizeLow;
        info.creation = Time::convert(data.ftCreationTime);
        info.lastAccess = Time::convert(data.ftLastAccessTime);
        info.modification = Time::convert(data.ftLastWriteTime);
        info.type = Info::Regular;
        convertAttributes(data.dwFileAttributes, info.permission, info.type, info.owner, info.group);

        if (FindNextFileW(finder, &data) == FALSE)
        {
            FindClose(finder); finder = INVALID_HANDLE_VALUE;
        }
        return true;
#elif defined(_POSIX)
        if (finder == 0)
        {
            finder = opendir(path);
            if (finder == 0) return false;
        }

        // Then read the directory (we don't use readdir_r here because it's unsafe, the dirent's size can't be known beforehand)
        // And readdir is thread safe.
        struct dirent * ent = readdir(finder);
        if (ent == NULL) { closedir(finder); finder = 0; return false; }

        // Ok, need to stat the file name then
        info.name = ent->d_name;
        if (info.path.getLength() == 0)
        {
            if (path.getLength() != 1)
                info.path = path.normalizedPath(Platform::Separator, false);
            else
                info.path = "";
        }
        return info.restatFile();
#endif
    }

#if defined(_POSIX)
    inline Info::Type convertDirType(uint8 dirType)
    {
        switch (dirType)
        {
        case DT_FIFO: return Info::FIFO;
        case DT_BLK:
        case DT_CHR:  return Info::Device;
        case DT_DIR:  return Info::Directory;
        case DT_REG:  return Info::Regular;
        case DT_LNK:  return Info::Link;
        case DT_SOCK: return Info::Socket;
        default:      return (Info::Type)0; // Obviously invalid for unknown type
        }
    }
#endif
    // Get the next file iteratively
    bool DirectoryIterator::getNextFilePath(Info & info) const
    {
#ifdef _WIN32
        if (finder == INVALID_HANDLE_VALUE) return false;
        info.name = Strings::convert(Strings::ReadOnlyUnicodeString(data.cFileName));
        if (info.path.getLength() == 0)
            info.path = path.normalizedPath(Platform::Separator, false);
        info.size = ((uint64)data.nFileSizeHigh << 32) | data.nFileSizeLow;
        info.creation = Time::convert(data.ftCreationTime);
        info.lastAccess = Time::convert(data.ftLastAccessTime);
        info.modification = Time::convert(data.ftLastWriteTime);
        info.type = Info::Regular;
        convertAttributes(data.dwFileAttributes, info.permission, info.type, info.owner, info.group);

        if (FindNextFileW(finder, &data) == FALSE)
        {
            FindClose(finder); finder = INVALID_HANDLE_VALUE;
        }
        return true;
#elif defined(_POSIX)
        if (finder == 0)
        {
            finder = opendir(path);
            if (finder == 0) return false;
        }

        // Then read the directory (see above for why we don't use readdir_r)
        struct dirent * ent = readdir(finder);
        if (ent == NULL) { closedir(finder); finder = 0; return false; }


        // Ok, need to stat the file name then
        info.name = ent->d_name;
        info.path = path.normalizedPath(Platform::Separator, false);;
        info.type = convertDirType(ent->d_type);
        info.size = 0;
        info.modification = 0;
        return true;
#endif
    }

    // Retrieve all the files information at once
    bool DirectoryIterator::getAllFilesAtOnce(InfoArray & array) const
    {
        array.Clear();
        Info info;
        while (getNextFile(info)) array.Append(info);
        return true;
    }
    // Get a minimal file listing from this iterator
    bool DirectoryIterator::getAllFilesAtOnce(NameArray & array, const bool withPath) const
    {
        array.Clear();
#ifdef _WIN32
        WIN32_FIND_DATAW data;
        Strings::ReadOnlyUnicodeString fileName = Strings::convert(path + "*");
        memset(&data, 0, sizeof(data));
        HANDLE hFind = FindFirstFileW(fileName.getData(), &data);

        if (hFind == INVALID_HANDLE_VALUE) return false;
        if (withPath)
            array.Append(path + Strings::convert(Strings::ReadOnlyUnicodeString(data.cFileName)));
        else
            array.Append(Strings::convert(Strings::ReadOnlyUnicodeString(data.cFileName)));


        while (FindNextFileW(hFind, &data) != FALSE)
        {
            if (withPath)
                array.Append(path + Strings::convert(Strings::ReadOnlyUnicodeString(data.cFileName)));
            else
                array.Append(Strings::convert(Strings::ReadOnlyUnicodeString(data.cFileName)));
        }
        FindClose(hFind);
#elif defined(_POSIX)
        struct dirent **namelist;
        int n = scandir(path, &namelist, 0, alphasort);
        if (n < 0) return false;
        if (withPath)
        {
            for (int i = 0; i < n; i++)
            {
                array.Append(path + (const char*)namelist[i]->d_name);
                free(namelist[i]);
            }
        } else
        {
            for (int i = 0; i < n; i++)
            {
                array.Append(namelist[i]->d_name); free(namelist[i]);
            }
        }
        free(namelist);
#endif
        return true;
    }



    DirectoryIterator::DirectoryIterator(const DirectoryIterator & dir) : finder(dir.finder), path(dir.path) { const_cast<DirectoryIterator &>(dir).finder = 0; }
    DirectoryIterator::DirectoryIterator(const String & path) : finder(0), path(path)
    {
#ifdef _WIN32
        Strings::ReadOnlyUnicodeString fileName = Strings::convert(path + "*");
        memset(&data, 0, sizeof(data));
        finder = FindFirstFileW(fileName.getData(), &data);
#endif
    }

    DirectoryIterator::~DirectoryIterator()
    {
#ifdef _WIN32
        if (finder != INVALID_HANDLE_VALUE) FindClose(finder); finder = INVALID_HANDLE_VALUE;
#elif defined(_POSIX)
        if (finder) closedir(finder); finder = 0;
#endif
    }

    // Enumerate directory at the given path
    DirectoryIterator General::listFilesIn(const String & path)
    {
        return DirectoryIterator(normalizePath(path));
    }

    // Normalize a path.
    String General::normalizePath(String pathToNormalize)
    {
        String outputStack;
#if defined(_WIN32) || defined(_POSIX)
#if defined(_WIN32)
        // If there is only forward slash, then treat it as back slash, and restore to forware slash in the end
        bool shouldMirrorPath = pathToNormalize.Find('/') != -1 && pathToNormalize.Find('\\') == -1;
        if (shouldMirrorPath) pathToNormalize = pathToNormalize.replaceAllTokens('/', '\\');
#endif
        while (pathToNormalize)
        {
            // Remove starting "../" segment if the stack is empty
            if (!outputStack && pathToNormalize.midString(0, 3) == ".." PathSeparator) pathToNormalize = pathToNormalize.midString(3, pathToNormalize.getLength());
            // Remove "./" segment
            else if (pathToNormalize.midString(0, 2) == "." PathSeparator) pathToNormalize = pathToNormalize.midString(2, pathToNormalize.getLength());
            // Remove "/./" segment, replaced by "/"
            else if (pathToNormalize.midString(0, 3) == PathSeparator "." PathSeparator) pathToNormalize = PathSeparator + pathToNormalize.midString(3, pathToNormalize.getLength());
            // Replace "/." by "/"
            else if (pathToNormalize == "/.") pathToNormalize = "/";
            // Handle "/.." and "/../" case
            else if (pathToNormalize == PathSeparator ".." || pathToNormalize.midString(0, 4) == PathSeparator ".." PathSeparator)
            {   // Replaced by "/", and pop'd the output stack string
                pathToNormalize = PathSeparator + pathToNormalize.midString(4, pathToNormalize.getLength());
                int lastSegmentPos = outputStack.reverseFind(Platform::Separator, outputStack.getLength());
                if (lastSegmentPos > 0) outputStack = outputStack.midString(0, lastSegmentPos);
                else
                {
                    // If output stack is absolute, we don't kill it
                    #ifdef _WIN32
                    if (outputStack.midString(1, outputStack.getLength()) != ":")
                    #else
                    if (outputStack != PathSeparator)
                    #endif
                        outputStack = "";
                }
            }
            // Handle case where the path is made of only one dot "."
            else if (pathToNormalize == ".") pathToNormalize = "";
            else
            {
                // Is it starting with "/" ?
                int firstSlash = pathToNormalize.Find(Platform::Separator, 0);
                if (firstSlash == 0) firstSlash = pathToNormalize.Find(Platform::Separator, 1);
                // No path separator found, let's output our stack
                if (firstSlash == -1) { outputStack += pathToNormalize; pathToNormalize = ""; }
                else
                {
                    // Add the current segment to the stack
                    outputStack += pathToNormalize.midString(0, firstSlash);
                    pathToNormalize = pathToNormalize.midString(firstSlash, pathToNormalize.getLength());
                }
            }
        }
#endif
#if defined(_WIN32)
        if (shouldMirrorPath) return outputStack.normalizedPath(Platform::Separator).replaceAllTokens('\\', '/');
#endif
        return outputStack.normalizedPath(Platform::Separator);
    }

    // Get the special folder
    String General::getSpecialPath(const SpecialFolder folder)
    {
#ifdef _WIN32
        WCHAR szPath[MAX_PATH];
        switch(folder)
        {
        case Home:
            SHGetFolderPathW(NULL, CSIDL_PERSONAL|CSIDL_FLAG_CREATE, NULL, 0, szPath);
            return Strings::convert(Strings::ReadOnlyUnicodeString(szPath));
        case Root:
            return "C:\\";
        case Programs:
            SHGetFolderPathW(NULL, CSIDL_PROGRAM_FILES, NULL, 0, szPath);
            return Strings::convert(Strings::ReadOnlyUnicodeString(szPath));
        case Temporary:
            GetTempPathW(MAX_PATH, szPath);
            return Strings::convert(Strings::ReadOnlyUnicodeString(szPath));
        case Current:
            GetCurrentDirectoryW(MAX_PATH, szPath);
            return Strings::convert(Strings::ReadOnlyUnicodeString(szPath));
        }
#elif defined(_POSIX)
        switch(folder)
        {
        case Home:
            if (getpwuid(getuid()) != NULL) return getpwuid(getuid())->pw_dir;
            return "~/";
        case Root:
            return "/";
        case Programs:
            return "/usr/";
        case Temporary:
            return "/tmp/";
        case Current:
            {
                static char path[PATH_MAX];
                return getcwd(path, PATH_MAX);
            }
        }
#endif
        return "";
    }


    // Get the total and free space for the given mount point path
    bool General::getDriveUsage(const String & path, uint64 & totalBytes, uint64 & freeBytes)
    {
#ifdef _WIN32
        ULARGE_INTEGER FreeBytesAvailable;
        ULARGE_INTEGER TotalNumberOfBytes;
        ULARGE_INTEGER TotalNumberOfFreeBytes;

        String deducedVolumeName = path;
        if(path.getLength() == 1) deducedVolumeName = deducedVolumeName + ":";

        deducedVolumeName.rightTrim("\\");
        deducedVolumeName += '\\';

        Strings::ReadOnlyUnicodeString volumeNameInUCS2 = Strings::convert(deducedVolumeName);
        if (GetDiskFreeSpaceExW((LPCWSTR)volumeNameInUCS2.getData(), &FreeBytesAvailable, &TotalNumberOfBytes, &TotalNumberOfFreeBytes))
        {
            freeBytes   =   (uint64)FreeBytesAvailable.QuadPart;
            totalBytes  =   (uint64)TotalNumberOfBytes.QuadPart;
            return true;
        }
#elif defined(_POSIX)
        struct statvfs volumeStats;
        if(!statvfs(path, &volumeStats))
        {
            // a block is the allowed littlest size to allocate on the disc
            freeBytes  = (uint64)volumeStats.f_bsize *  (uint64)volumeStats.f_bavail;
            totalBytes = (uint64)volumeStats.f_bsize *  (uint64)volumeStats.f_blocks;
            return true;
        }

#endif
        return false;
    }

#ifdef _WIN32
    // Manage the network link to sort the volumes from mount points
    static inline bool enumerateNetworkResources(Strings::StringArray & result, Strings::StringArray * remoteNames)
    {
        // Awful code to delay load the DLL until required
        HMODULE hMod = LoadLibraryA("Mpr.dll");
        if (hMod == NULL) return false;
        DWORD (APIENTRY *WNetOpenEnumW)(DWORD, DWORD, DWORD, LPNETRESOURCEW, LPHANDLE)  = (DWORD (APIENTRY *)(DWORD, DWORD, DWORD, LPNETRESOURCEW, LPHANDLE))GetProcAddress(hMod, "WNetOpenEnumW");
        if (!WNetOpenEnumW) { FreeLibrary(hMod); return false; }
        DWORD (APIENTRY *WNetEnumResourceW)(HANDLE, LPDWORD, LPVOID, LPDWORD)  = (DWORD (APIENTRY *)(HANDLE, LPDWORD, LPVOID, LPDWORD))GetProcAddress(hMod, "WNetEnumResourceW");
        if (!WNetEnumResourceW) { FreeLibrary(hMod); return false; }
        DWORD (APIENTRY *WNetCloseEnum)(HANDLE)  = (DWORD (APIENTRY *)(HANDLE))GetProcAddress(hMod, "WNetCloseEnum");
        if (!WNetCloseEnum) { FreeLibrary(hMod); return false; }

        HANDLE hEnum = NULL;
        DWORD dwResult = WNetOpenEnumW( RESOURCE_CONNECTED, RESOURCETYPE_ANY, 0, NULL, &hEnum );
        if (dwResult != NO_ERROR) { FreeLibrary(hMod); return false; }

        do
        {
            DWORD cbBuffer = 16384;
            DWORD cEntries = 0xFFFFFFFF;
            LPNETRESOURCEW lpnrDrv = (LPNETRESOURCEW) GlobalAlloc( GPTR, cbBuffer );
            dwResult = WNetEnumResourceW( hEnum, &cEntries, lpnrDrv, &cbBuffer);
            if (dwResult == NO_ERROR)
            {
                for (DWORD i = 0; i < cEntries; i++)
                {
                    if (lpnrDrv[i].lpLocalName != NULL)
                    {
						result.Append(Strings::convert(Strings::ReadOnlyUnicodeString(lpnrDrv[i].lpLocalName)));
                        if (remoteNames) remoteNames->Append(Strings::convert(Strings::ReadOnlyUnicodeString(lpnrDrv[i].lpRemoteName)));
                    }
                }
            }
            GlobalFree( (HGLOBAL) lpnrDrv );
        } while( dwResult != ERROR_NO_MORE_ITEMS );

        bool ret = WNetCloseEnum(hEnum) == NO_ERROR;
        FreeLibrary(hMod);
        return ret;
    }
#endif

    bool General::findMountPoints(Strings::StringArray & paths, Strings::StringArray * remoteNames)
    {
#ifdef _WIN32
        // Disable popup asking to insert a CD or a disk
        UINT previousErrorMode = SetErrorMode(SEM_FAILCRITICALERRORS);
        // If the function succeeds, the return value is a bitmask representing the currently available disk drives. Bit position 0
        // (the least-significant bit) is drive A, bit position 1 is drive B, bit position 2 is drive C, and so on.
        DWORD dwID = GetLogicalDrives();
        for(uint32 i = 0; i < 26; i++)
        {
            // If the drive exist with this letter
            if( ( ( dwID >> i ) &0x00000001 ) == 1 )
            {
                // Construct the name of the drive "ascii code(A) + i"
                wchar_t szDrive[4] = {wchar_t( 65 + i ), L':', L'\\', 0};

                uint32 driveType = GetDriveTypeW(szDrive);
                if(driveType == DRIVE_FIXED)
                {
                    wchar_t szname[MAX_PATH];
                    DWORD lpVolumeSerialNumber;
                    DWORD lpMaximumComponentLength;
                    DWORD lpFileSystemFlags;
                    wchar_t lpFileSystemNameBuffer[MAX_PATH];

                    if(GetVolumeInformationW(szDrive, szname, MAX_PATH, &lpVolumeSerialNumber, &lpMaximumComponentLength, &lpFileSystemFlags, lpFileSystemNameBuffer, MAX_PATH))
                    {
                        paths.Append(Strings::convert(Strings::ReadOnlyUnicodeString(szDrive)));
                        if (remoteNames) remoteNames->Append(Strings::convert(Strings::ReadOnlyUnicodeString(szDrive)));
                    }
                }
            }
        }
        // Restore the current error mode
        SetErrorMode(previousErrorMode);
        // manage the network volumes
        return enumerateNetworkResources(paths, remoteNames);
#elif defined (_LINUX)
        String procs = Info("/proc/mounts").getContent();

        paths.Clear();
        String currentLine = procs.splitUpTo("\n");
        while(currentLine)
        {
            String remote = currentLine.splitFrom(" ");
            String path = currentLine.upToFirst(" ");

            // Check if the access will not be denied
            if (access(path, R_OK | X_OK) == 0)
            {
                if (paths.appendIfNotPresent(path) == (paths.getSize() - 1) && remoteNames)
                    remoteNames->Append(remote);
            }

            currentLine = procs.splitUpTo("\n");
        }
		return true;
#elif defined(_MAC)
        int mountedFS = getfsstat(NULL, 0, 0);
        if (mountedFS <= 0) return false;

        struct statfs * stats = (struct statfs*)malloc(mountedFS * sizeof(struct statfs));

        if (getfsstat(stats, mountedFS * sizeof(struct statfs), 0) <= 0)
        {
            free(stats);
            return false;
        }

        for (int i = 0; i < mountedFS; i++)
        {
            paths.Append(stats[i].f_mntonname);
            if (remoteNames) remoteNames->Append(stats[i].f_mntfromname);
        }
        free(stats);
        return true;
#else
        return false;
#endif
    }

    // Read the stream of the given amount of bytes.
    int Stream::read(char * buffer, int length)
    {
        if (file == NULL) return -1;
        return (int)fread(buffer, 1, length, file);
    }

    // Read a line from the stream.
    int Stream::readLine(char * buffer, int length, const EndOfLine EOL)
    {
        if (file == NULL) return -1;
        int len = (int)fread(buffer, 1, length, file);
        if (len <= 0) return len;

        char * foundPos = 0;
        switch(EOL)
        {
        case Platform::CR:
            foundPos = (char*)memchr(buffer, '\r', len);
            break;
        case Platform::LF:
            foundPos = (char*)memchr(buffer, '\n', len);
            break;
        case Platform::CRLF:
            {
                char * _buffer = buffer;
                int received = len;
                while (1)
                {
                    foundPos = (char*)memchr(_buffer, '\r', received);
                    if (!foundPos) break;
                    if ((int)(foundPos+1 - _buffer) < received && *(foundPos+1) != '\n')
                    {   // Continue searching
                        received -= (int)(foundPos+1 - _buffer);
                        _buffer = foundPos + 1;
                        continue;
                    }
                    break;
                }
            }
            break;
        case Platform::Any:
            foundPos = (char*)memchr(buffer, '\r', len);
            if (!foundPos) foundPos = (char*)memchr(buffer, '\n', len);
            break;
        case Platform::AutoDetect:
            {
                char * endLine = buffer + len;
                for (foundPos = &buffer[0]; foundPos < endLine && *foundPos != '\r' && *foundPos != '\n'; foundPos++)
                    {}
                // Search for '\r\n' pattern
                if (*foundPos == '\r' && foundPos + 1 < endLine && *(foundPos+1) == '\n') foundPos++;
            }
            break;
        }
        if (!foundPos)
        {
            // Rewind and return
            setPosition(getPosition() - len);
            return 0;
        }
        int lineLength = (int)(foundPos - buffer) + (EOL == Platform::CRLF ? 2 : 1);
        setPosition(getPosition() - (len - lineLength));
        buffer[lineLength] = 0;
        return lineLength;
    }

    // Write the given amount of bytes to the stream.
    int Stream::write(const char * buffer, int length)
    {
        if (file == NULL) return -1;
        return (int)fwrite(buffer, 1, length, file);
    }
    // Flush the stream.
    void Stream::flush()
    {
        fflush(file);
    }

    // Get the stream size (if known by advance)
    uint64 Stream::getSize() const
    {
        if (file == NULL) return 0;
        uint64 position = getPosition();
        fseek(file, 0, SEEK_END);
        uint64 size = getPosition();
        const_cast<Stream*>(this)->setPosition(position);
        return size;
    }
    // Get the current pointer position in the stream.
    uint64 Stream::getPosition() const
    {
        if (file == NULL) return 0;
#ifdef _WIN32
        return (uint64)::_ftelli64(file);
#elif defined(_POSIX)
        return (uint64)ftello(file);
#endif
    }
    // Set the current pointer position in the stream
    bool Stream::setPosition(const uint64 offset)
    {
        if (file == NULL) return false;
#ifdef _WIN32
        ::_fseeki64(file, (__int64)offset, SEEK_SET);
#elif defined(_POSIX)
        fseeko(file, (off_t)offset, SEEK_SET);
#endif
        return true;
    }

    // Set the stream size.
    bool Stream::setSize(const uint64 offset)
    {
        if (file == NULL) return false;
#ifdef _WIN32
        if (offset > (uint64)0x7FFFFFFF) return false;
        chsize(_fileno(file), offset);
#elif defined(_POSIX)
        ftruncate(fileno(file), offset);
#endif
        return true;
    }

    // Check if the stream is finished.
    bool Stream::endOfStream() const
    {
        return feof(file) != 0;
    }


    Stream::Stream(const String & fullPath, const String & mode)
        : priv(0), file(NULL)
    {
        if (fullPath.getLength())
        {
#ifdef _WIN32
            Strings::ReadOnlyUnicodeString fileName = Strings::convert(fullPath);
            file = _wfopen(fileName.getData(), Strings::convert(mode).getData());
#elif defined(_POSIX)
            file = fopen(fullPath, mode);
#endif
        }
    }

    Stream::~Stream()
    {
        if (file && fclose(file) < 0) perror("fclose");
        priv = 0;
    }

#if WantAsyncFile == 1
#if defined(_POSIX)
    void aioCompleted( union sigval sigval )
    {
        MonitoringPool::AsyncCompleted * completed = (MonitoringPool::AsyncCompleted *)sigval.sival_ptr;
        if (completed) completed->wasCompleted();
    }
#endif

    // Read the stream of the given amount of bytes.
    int AsyncStream::read(char * buffer, int length)
    {
#ifdef _WIN32
        if (asyncSize && tmpBuffer)
        {
            DWORD dwRead = 0;
            if (GetOverlappedResult(file, &over, &dwRead, FALSE) == TRUE)
            {
                uint64 size = min(asyncSize - readPos, (uint64)length);
                memcpy(buffer, &tmpBuffer[readPos], (size_t)size);
                if (readPos + length >= asyncSize)
                {
                    deleteA0(tmpBuffer);
                    currentPos += asyncSize;
                    asyncSize = 0;
                }
                readPos += size;
                return (int)size;
            } else
            {
                if ((readPos + length) <= (uint64)dwRead)
                {
                    memcpy(buffer, &tmpBuffer[readPos], (size_t)length);
                    readPos += (uint64)length;
                    return length;
                }
                return Asynchronous;
            }
        }

        DWORD dwRead = 0;
        over.Offset = DWORD(currentPos & 0xFFFFFFFF);
        over.OffsetHigh = DWORD(currentPos >> 32);
        deleteA0(tmpBuffer); tmpBuffer = new char[length];
        if (!tmpBuffer) return -1;

        if (ReadFile(file, (LPVOID)tmpBuffer, length, &dwRead, &over))
        {
            currentPos += length;
            memcpy(buffer, tmpBuffer, length);
            deleteA0(tmpBuffer);
            asyncSize = 0;
            return dwRead;
        }
        if (GetLastError() == ERROR_IO_PENDING)
        {
            asyncSize = (uint64)length;
            readPos = 0;
            return Asynchronous;
        }
        return -1;
#elif defined(_POSIX)
        if (file < 0) return -1;
        if (asyncSize && over.aio_buf)
        {
            // If not done already fetch the read size
            if (asyncSize == (size_t)-1)
            {
                int error = aio_error(&over);
                if (error == EINPROGRESS) return Asynchronous; //.aio_ != AIO_ALLDONE)
                if (error == ECANCELED) return 0;
                if (error < 0) return -1;
                asyncSize = aio_return(&over);
            }

            if ((readPos + length) < asyncSize)
            {
                // There is not enough space in given buffer.
                memcpy(buffer, &((char*)over.aio_buf)[readPos], length);
                readPos += (uint64)length;
                currentPos += length;
                return length;
            }
            size_t size = (size_t)(asyncSize - readPos);
            memcpy(buffer, &((char*)over.aio_buf)[readPos], size);
            free((void*)over.aio_buf); over.aio_buf = 0;
            currentPos += size;
            asyncSize = readPos = 0;
            // Reset the event, so we don't wait anymore on any previous attempt
            if (monitored) ((MonitoringPool::AsyncCompleted*)monitored)->completed = false;
            return (int)size;
        } else
        {
            // Ok, need to schedule a asynchronous read operation
            free((void*)over.aio_buf);
            over.aio_buf = malloc(length);
            over.aio_nbytes = length;
            over.aio_offset = currentPos;
            if (monitored)
            {
                over.aio_sigevent.sigev_notify = SIGEV_THREAD;
                over.aio_sigevent.sigev_notify_function = aioCompleted;
                over.aio_sigevent.sigev_notify_attributes = NULL;
                over.aio_sigevent.sigev_value.sival_ptr = monitored;
                // Reset the event, so we don't wait anymore on any previous attempt
                if (monitored) ((MonitoringPool::AsyncCompleted*)monitored)->completed = false;
            } else // Don't let previous call leak
                memset(&over.aio_sigevent, 0, sizeof(over.aio_sigevent));

            ssize_t ret = aio_read(&over);
            if (ret == 0) asyncSize = -1;
            return ret == 0 ? Asynchronous : -1;
        }
#endif
    }

    // Read a line from the stream.
    int AsyncStream::readLine(char * buffer, int length, const EndOfLine EOL)
    {
        // Can't read a line asynchronously
        return -1;
    }

    // Write the given amount of bytes to the stream .
    int AsyncStream::write(const char * buffer, int length)
    {
#ifdef _WIN32
        // Writing is an atomic operation. It's either completed successfully, or not at all.
        if (asyncSize && tmpBuffer)
        {
            DWORD dwRead = 0;
            if (GetOverlappedResult(file, &over, &dwRead, FALSE) == TRUE)
            {
                deleteA0(tmpBuffer);
                currentPos += asyncSize;
                asyncSize = 0;
                return (int)asyncSize;
            }
            return Asynchronous;
        }

        DWORD dwRead = 0;
        over.Offset = DWORD(currentPos & 0xFFFFFFFF);
        over.OffsetHigh = DWORD(currentPos >> 32);
        deleteA0(tmpBuffer); tmpBuffer = new char[length];
        if (!tmpBuffer) return -1;
        memcpy(tmpBuffer, buffer, length);

        if (WriteFile(file, (LPVOID)tmpBuffer, length, &dwRead, &over))
        {
            currentPos += length;
            deleteA0(tmpBuffer);
            asyncSize = 0;
            return dwRead;
        }
        if (GetLastError() == ERROR_IO_PENDING)
        {
            asyncSize = (uint64)length;
            readPos = 0;
            return Asynchronous;
        }
        return -1;
#elif defined(_POSIX)
        if (file < 0) return -1;
        if (asyncSize && over.aio_buf)
        {
            // If not done already fetch the read size
            if (asyncSize == (size_t)-1)
            {
                int error = aio_error(&over);
                if (error == EINPROGRESS) return Asynchronous; //.aio_ != AIO_ALLDONE)
                if (error == ECANCELED) return 0;
                if (error < 0) return -1;
                asyncSize = aio_return(&over);
            }

            int size = (uint32)asyncSize;
            free((void*)over.aio_buf); over.aio_buf = 0;
            currentPos += size;
            asyncSize = readPos = 0;
            // Reset the event, so we don't wait anymore on any previous attempt
            if (monitored) ((MonitoringPool::AsyncCompleted*)monitored)->completed = false;
            return size;
        } else
        {
            // Ok, need to schedule a asynchronous read operation
            free((void*)over.aio_buf);
            over.aio_buf = malloc(length);
            // Ok, copy the asynchronous stuff to the buffer
            if (over.aio_buf == NULL) return -1;
            memcpy((void*)over.aio_buf, buffer, length);

            over.aio_nbytes = length;
            over.aio_offset = currentPos;
            if (monitored)
            {
                over.aio_sigevent.sigev_notify = SIGEV_THREAD;
                over.aio_sigevent.sigev_notify_function = aioCompleted;
                over.aio_sigevent.sigev_notify_attributes = NULL;
                over.aio_sigevent.sigev_value.sival_ptr = monitored;
                // Reset the event, so we don't wait anymore on any previous attempt
                if (monitored) ((MonitoringPool::AsyncCompleted*)monitored)->completed = false;
            } else // Don't let previous call leak
                memset(&over.aio_sigevent, 0, sizeof(over.aio_sigevent));

            ssize_t ret = aio_write(&over);
            if (ret == 0) asyncSize = -1;
            return ret == 0 ? Asynchronous : -1;
        }
#endif
    }

    // Get the stream size (if known by advance)
    uint64 AsyncStream::getSize() const
    {
#ifdef _WIN32
        DWORD lo, hi;
        lo = GetFileSize(file, &hi);
        return ((uint64)(hi)<<32) | lo;
#elif defined(_POSIX)
        if (file < 0) return 0;
        uint64 position = getPosition();
        lseek(file, 0, SEEK_END);
        uint64 size = getPosition();
        const_cast<AsyncStream*>(this)->setPosition(position);
        return size;
#endif
    }
    // Get the current pointer position in the stream.
    uint64 AsyncStream::getPosition() const
    {
#ifdef _WIN32
        return currentPos;
#elif defined(_POSIX)
        if (file < 0) return 0;
        return (uint64)lseek(file, 0, SEEK_CUR);
#endif
    }
    // Set the current pointer position in the stream
    bool AsyncStream::setPosition(const uint64 offset)
    {
#ifdef _WIN32
        // Can't change the position while an operation is pending
        if (asyncSize) return false;
        currentPos = offset;
#elif defined(_POSIX)
        if (file < 0) return false;
        return lseek(file, (off_t)offset, SEEK_SET) != -1;
#endif
        return true;
    }

    // Set the stream size.
    bool AsyncStream::setSize(const uint64 offset)
    {
#ifdef _WIN32
        LONG lo = LONG(offset & 0xFFFFFFFF), hi = LONG(offset >> 32);
        if (SetFilePointer(file, lo, &hi, FILE_BEGIN) == INVALID_SET_FILE_POINTER) return false;
        return SetEndOfFile(file) == TRUE;
#elif defined(_POSIX)
        if (file < 0) return false;
        return ftruncate(file, offset) != -1;
#endif
        return true;
    }

    // Check if the stream is finished.
    bool AsyncStream::endOfStream() const
    {
        return getPosition() == getSize();
    }

    AsyncStream::AsyncStream(const String & fullPath, const OpenMode mode)
        : priv(0), currentPos(0), readPos(0), asyncSize(0)
    {
#ifdef _WIN32
        tmpBuffer = 0;
        if (fullPath.getLength())
        {
            Strings::ReadOnlyUnicodeString fileName = Strings::convert(fullPath);
            memset(&over, 0, sizeof(over));
            over.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
            file = CreateFileW(fileName.getData(), (mode == Read ? GENERIC_READ : (mode == Write ? GENERIC_WRITE : GENERIC_WRITE | GENERIC_READ))
                                , FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL);
        }
#elif defined(_POSIX)
        if (fullPath.getLength())
        {
            if (mode != Read)
            {
                // Posix require a mode here
                mode_t mode = 0666; // Give all permissions, let the user change this afterwards if she needs to.
                file = open(fullPath, O_CREAT | O_NONBLOCK | (mode == Write ? O_WRONLY : O_RDWR), mode);
            }
            else
                file = open(fullPath, O_RDONLY | O_NONBLOCK);

            int fileFlags = fcntl(file, F_GETFL, 0);
            if (fileFlags != -1)
            {
                fcntl(file, F_SETFL, (fileFlags & ~O_NONBLOCK) | (O_NONBLOCK));
            }
            memset(&over, 0, sizeof(over));
            over.aio_fildes = file;
            monitored = 0;
        }
#endif
    }

    AsyncStream::~AsyncStream()
    {
#ifdef _WIN32
        if (file != INVALID_HANDLE_VALUE)
        {
            CancelIo(file);
            CloseHandle(file);
        }
        if (over.hEvent)
            CloseHandle(over.hEvent);
        deleteA0(tmpBuffer);
#elif defined(_POSIX)
        aio_cancel(file, &over);
        if (file >= 0) close(file); file = -1;
#endif
        priv = 0;
    }

    // Check if it's possible to read from this stream
    bool AsyncStream::isReadPossible(const Time::TimeOut & timeout) const
    {
        if (timeout <= 0) return false;
#ifdef _WIN32
        if (asyncSize && tmpBuffer)
        {
            DWORD dwRead = 0;
            if (GetOverlappedResult(file, &over, &dwRead, FALSE) == TRUE)
                return true;
        }
        return false;
#elif defined(_POSIX)
        if (file < 0) return false;
        fd_set set;
        FD_ZERO(&set);
        FD_SET(file, &set);
        struct timeval tv;
        tv.tv_sec = timeout / 1000;
        tv.tv_usec = (timeout % 1000) * 1000;
        // Then select
        int ret = 0;
        while ((ret = ::select(FD_SETSIZE, &set, NULL, NULL, timeout < 0 ? NULL : &tv)) == -1)
        {
            if (errno != EINTR) return false;
            if (timeout.timedOut()) return false;
        }
        timeout.filterError(ret);
        return ret >= 1;
#endif
    }
    // Check if it's possible to write to this stream
    bool AsyncStream::isWritePossible(const Time::TimeOut & timeout) const
    {
        if (timeout <= 0) return false;
#ifdef _WIN32
        if (asyncSize && tmpBuffer)
        {
            DWORD dwRead = 0;
            if (GetOverlappedResult(file, &over, &dwRead, FALSE) == TRUE)
                return true;
        }
        return false;
#elif defined(_POSIX)
        if (file < 0) return false;
        fd_set set;
        FD_ZERO(&set);
        FD_SET(file, &set);
        struct timeval tv;
        tv.tv_sec = timeout / 1000;
        tv.tv_usec = (timeout % 1000) * 1000;
        // Then select
        int ret = 0;
        while ((ret = ::select(FD_SETSIZE, NULL, &set, NULL, timeout < 0 ? NULL : &tv)) == -1)
        {
            if (errno != EINTR) return false;
            if (timeout.timedOut()) return false;
        }
        timeout.filterError(ret);
        return ret >= 1;
#else
        return 0;
#endif
    }

    // Get the internal object
    void * AsyncStream::getInternal() const
    {
#ifdef _WIN32
        return (void*)over.hEvent;
#elif defined(_POSIX)
        return (void*)(int64)file;
#else
        return 0;
#endif
    }


    // Append a socket to this pool
    bool MonitoringPool::appendStream(AsyncStream * _stream)
    {
        AsyncStream * stream = dynamic_cast<AsyncStream*>(_stream);
        if (!stream) return false;
        if (size >= MaxQueueLen) return false;
        AsyncStream ** newPool = (AsyncStream**)realloc(pool, (size+1) * sizeof(*pool));
        if (newPool == NULL) return false;
        pool = newPool;
        pool[size++] = stream;
#ifdef _WIN32
        HANDLE * rSet = (HANDLE*)realloc(readSet, size * sizeof(*readSet));
        if (rSet == NULL) return false;
        readSet = rSet;
        readSet[size - 1] = (HANDLE)stream->getInternal();
        HANDLE * wSet = (HANDLE*)realloc(writeSet, size * sizeof(*writeSet));
        if (wSet == NULL) return false;
        writeSet = wSet;
        writeSet[size - 1] = (HANDLE)stream->getInternal();
        HANDLE * bSet = (HANDLE*)realloc(bothSet, size * sizeof(*bothSet));
        if (bSet == NULL) return false;
        bothSet = bSet;
        bothSet[size - 1] = (HANDLE)stream->getInternal();
#elif defined(_POSIX)
        uint16 * newIndexPool = (uint16*)realloc(indexPool, size * sizeof(*indexPool));
        if (newIndexPool == NULL) return false;
        indexPool = newIndexPool;
        indexPool[size - 1] = 0;
        AsyncCompleted * async = new AsyncCompleted(*this, size-1);
        asyncCB.Append(async);
        stream->monitored = async;
#endif
        return true;
    }
    // Remove a stream from the pool
    bool MonitoringPool::removeStream(AsyncStream * _stream)
    {
        AsyncStream * stream = dynamic_cast<AsyncStream*>(_stream);
        if (!stream) return false;
        // Need to find out the stream in the pool
        for (uint32 i = 0; i < size; i++)
        {
            if (pool[i] == stream)
            {   // Found
                // Swap the value with the last on in the list
                pool[i] = pool[size - 1];
                size--;
                pool[size] = 0;

                // BTW, if we own the stream, we must delete it now
                if (own) delete stream;
#ifdef _WIN32
                readSet[size] = 0;
                writeSet[size] = 0;
                bothSet[size] = 0;
#else
                // Swap the removed index with the last one
                asyncCB.Swap(i, size);
                asyncCB.Remove(size);
                // Fix the index in the swapped value (so all other CB are still at the same place)
                Threading::SharedDataWriter sdw(asyncCB[i].index);
                sdw = i;
#endif
                // We don't realloc smaller anyway
                return true;
            }
        }
        return false;
    }
    // Get the pool size
    uint32 MonitoringPool::getSize() const { return size; }

    // Select the pool for at least an element that is ready
    bool MonitoringPool::select(const bool reading, const bool writing, const Time::TimeOut & timeout) const
    {
        if (timeout <= 0) return false;
#ifdef _WIN32
        waitSet = reading ? (writing ? bothSet : readSet) : (writing ? writeSet : 0);
        triggerCount = WaitForMultipleObjects(size, waitSet, FALSE, timeout);
        timeout.success(); // If we timed out, this will be marked as such too
        if (triggerCount >= WAIT_OBJECT_0 && triggerCount <= (WAIT_OBJECT_0 + size))
            return true;
        return false;
#elif defined(_POSIX)
        // wait for something to do...
        {
            Threading::ScopedLock scope(indexLock);
            // Check if they are ready event in pool already
            if (triggerCount)
            {
                // Seems so, so let's invalidate them only done stream
                for (uint32 i = 0; i < (int)triggerCount; i++)
                {
                    uint32 index = (uint32)indexPool[i];
                    if (((AsyncCompleted*)pool[index]->monitored)->completed == false)
                    {
                        // This stream has been accounted, so let's remove it from the triggered pool
                        indexPool[i] = indexPool[triggerCount - 1];
                        indexPool[triggerCount - 1] = 0;
                        triggerCount--;
                        i--;
                        continue;
                    }
                }
            }
            // Reset the event
            eventReady.Reset();
            // If we have any pending triggered stream, let's say it!
            if (triggerCount) return true;
        }

        // Then wait for the asked amount of time
        bool ret = eventReady.Wait((Threading::TimeOut::Type)(int)timeout);
        timeout.success(); // If we timed out, this will be marked as such too
        return ret;
#endif
    }

    // Check if at least a stream in the pool is ready for reading
    bool MonitoringPool::isReadPossible(const Time::TimeOut & timeout)  const { return select(true, false, timeout); }
    // Check if at least a stream in the pool is ready for writing
    bool MonitoringPool::isWritePossible(const Time::TimeOut & timeout) const { return select(false, true, timeout); }

    // Check which stream was ready in the given pool
    int MonitoringPool::getNextReadyStream(const int index) const
    {
#ifdef _WIN32
        return index == -1 ? (triggerCount - WAIT_OBJECT_0) : -1;
#else
        Threading::ScopedLock scope(indexLock);
        return index + 1 < triggerCount ? index + 1 : -1;
#endif
    }
    // Get the stream at the given position
    AsyncStream * MonitoringPool::operator[] (const int index) { return index >= 0 && index < (int)size ?  pool[index] : 0; }
    // Get the ready stream at the given position
    AsyncStream * MonitoringPool::getReadyAt(const int index)
    {
#ifdef _WIN32
        if (waitSet && (triggerCount - WAIT_OBJECT_0) == (DWORD)index)
        {
            // Need to find out which stream the event refers to
            for (uint32 i = 0; i < size; i++)
            {
                if ((HANDLE)pool[i]->getInternal() == waitSet[index])
                    return pool[i];
            }
        }
        return 0;
#else
        Threading::ScopedLock scope(indexLock);
        return (uint32)index < (uint32)triggerCount ? pool[indexPool[index]] : 0;
#endif
    }

    MonitoringPool::MonitoringPool(const bool own)
#ifdef _WIN32
        : pool(0), size(0), readSet(0), writeSet(0), bothSet(0), triggerCount(0), own(own), waitSet(0)
#else
        : pool(0), size(0), indexPool(0), eventReady(NULL, Threading::Event::ManualReset), triggerCount(0), own(own)
#endif
    { }

    MonitoringPool::~MonitoringPool()
    {
        if (own)
        {
            // Destruct all the owned streams
            for (uint32 i = 0; i < size; i++) { delete pool[i]; pool[i] = 0; }
        }
        size = 0;
        Platform::safeRealloc(pool, 0);
#ifdef _WIN32
        Platform::safeRealloc(readSet, 0);
        Platform::safeRealloc(writeSet, 0);
        Platform::safeRealloc(bothSet, 0);
#elif defined(_POSIX)
        if (!own)
        {
            // Need to cancel all pending streams
            for (uint32 i = 0; i < size; i++)
                aio_cancel(pool[i]->file, &pool[i]->over);
        }
        Platform::safeRealloc(indexPool, 0);
#endif
        triggerCount = 0;
        pool = 0;
    }
#endif

}
