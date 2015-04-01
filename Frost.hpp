// We need the ClassPath's streams
#include "ClassPath/include/Streams/Streams.hpp"
// We need files too
#include "ClassPath/include/File/File.hpp"
// We need ClassPath database too
#include "ClassPath/include/Database/SQLFormat.hpp"
// We need strings
#include "ClassPath/include/Strings/Strings.hpp"
// And hash tables too
#include "ClassPath/include/Hash/HashTable.hpp"
// We need crypto code too for the key stuff
#include "ClassPath/include/Crypto/OpenSSLWrap.hpp"

// We need database model declaration (we are using SQLite here) too
#include "ClassPath/include/Database/Database.hpp"
// We need database querys too
#include "ClassPath/include/Database/Query.hpp"

#define DEFAULT_INDEX	  "__index.db"
#define PROTOCOL_VERSION  "1.0"

// Forward declare some class we'll be using to avoid including a lot of stuff in the header
namespace File { class Chunk; class MultiChunk; }


namespace Frost
{
    /** The kind of memory block we are using */
    typedef Utils::MemoryBlock MemoryBlock;
    /** The kind of String class we are using too */
    typedef Strings::FastString String;

    /** For easier access to complex SQL queries */
    typedef Database::Query::Select Select;
    typedef Database::Query::UnsafeRowIterator RowIterT;
    typedef Database::Query::Delete Delete;
    typedef Database::Query::CreateTempTable CreateTempTable;


    /** This class is used to build sessions keys out of the given user private key.

        The symmetric mode of encryption is used in CTR block mode.
        where the nonce is derived from the SHA256(Multichunk) ^ counter

        The key used for the encryption is derived from the asymmetric encryption
        algorithm.

        It's updated at pseudo-regular interval, and synchronization point are used
        to figure out if the next block is a salt used to update the key or a ciphertext.
        Typically, a key is build like this:
        @verbatim
            // random Salt (256 bits) is generated, || means concatenation.
            key = KDF(Salt || MasterKey)

            cipheredChunk = Salt
            for each encryption block in the multichunk:
               nonce = SHA256(Multichunk) ^ counter
               cipheredChunk = cipheredChunk || AES_CTR_enc(key, Multichunk, nonce)
               // The code below is not implemented yet
               // if length(cipheredData) > securityThreshold:
               //    create new key (see above)
               //    cipheredChunk = cipheredChunk || newSalt
        @endverbatim

        For decrypting, the algorithm run in reverse:
        @verbatim
            Salt = ciphertext[0..256 bits]
            key = KDF(Salt || MasterKey)
            for each encrypted block in the multichunk:
                nonce = SHA256(Multichunk) ^ counter
                clearText = AES_CTR_dec(key, cipherText[0..256], nonce)
        @endverbatim
    */
    class KeyFactory
    {
        // Type definition and enumerations
    public:
        /** The cryptographic primitive we use for asymmetric encrypting */
        typedef Crypto::OSSL_ECIES<NID_secp224k1>   AsymmetricT;
        /** The cryptographic primitive we use for symmetric encrypting */
        typedef Crypto::OSSL_AES                    SymmetricT;
        /** The cryptographic primitive we use for large dataset hashing */
        typedef Crypto::OSSL_SHA256                 BigHashT;

        /** The private key for asymmetric */
        typedef AsymmetricT::PrivateKey             AsymPrivKeyT;
        /** The public key for asymmetric */
        typedef AsymmetricT::PublicKey              AsymPubKeyT;

        /** The Key derivation function to use */
        typedef Hashing::KDF1<256, 256, BigHashT>   KeyDerivFuncT;
        /** The Key derivation function to use for password */
        typedef Hashing::PBKDF1<256, 256, BigHashT> PWKeyDerivFuncT;


        /** The master symmetric key */
        typedef uint8 KeyT[BigHashT::DigestSize];

        // Members
    private:
        /** The master key that's used while the system is running */
        KeyT         masterKey;
        /** The current salt */
        KeyT         salt;

        /** The current counter */
        uint32       counter;
        /** The current opaque nonce */
        KeyT         hashChunkNonce;

        // Interface
    public:
        /** Load the session key out of the given key vault
            @param fileVault       A path to the file vault to open.
            @param cipherMasterKey The ciphered master key that's read from the index database.
            @param password        The password used to protect the private key
            @return empty string on success, or the error message on failure.
                    if the file does not exist,
                    or if the mode does not fit the expected mode (0600)
                    or if the file format does not work */
        String loadPrivateKey(const String & fileVault, const MemoryBlock & cipherMasterKey, const String & password = "", const String & ID = "");
        /** Create file vault if it does not exist, store the new created private key,
            protected by the given password, and generate a master key to be used for
            this session.
            @param cipherMasterKey On output, contains the master key that was encrypted with the
                                   generated private key. This should be saved in the index database.
            @param fileVault       A path to the file vault to create.
            @param password        The password to use to protect the private key
            @return empty string on success, or the error message on failure.
            @return false if the file does already exist
                    (complete directory hierarchy will be created for this file) */
        String createMasterKeyForFileVault(MemoryBlock & cipherMasterKey, const String & fileVault, const String & password = "", const String & ID = "");

        /** Increment the counter and get the current key.
            This must be called before any 'AES_CTR()' call in the algorithm described above */
        void incrementNonce(KeyT & keyOut)
        {
            counter++;
            // Check if all parameters are aligned, and if so, process 4 by 4
            if (((uint64)(&keyOut[0]) & 0x3) == 0)
            {
                for (uint32 i = 0; i < ArrSz(salt); i+= sizeof(counter))
                    *(uint32*)&keyOut[i] = *(uint32*)&hashChunkNonce[i] ^ counter;
                return;
            }
            // Avoid unaligned memory access per loop turn
            uint8 cnt[4] = { (uint8)(counter >> 24), (uint8)((counter >> 16) & 0xFF), (uint8)((counter >> 8) & 0xFF), (uint8)(counter & 0xFF) };
            for (uint32 i = 0; i < ArrSz(salt); i+= sizeof(counter))
            {
                keyOut[i+0] = hashChunkNonce[i+0] ^ cnt[0];
                keyOut[i+1] = hashChunkNonce[i+1] ^ cnt[1];
                keyOut[i+2] = hashChunkNonce[i+2] ^ cnt[2];
                keyOut[i+3] = hashChunkNonce[i+3] ^ cnt[3];
            }
        }
        /** Create a new nonce and reset the counter.
            This creates the 'nonce' in the algorithm described above */
        void createNewNonce(const KeyT & hash) { counter = 0; memcpy(hashChunkNonce, hash, sizeof(hash)); }

        /** Create a new key (and a salt).
            This creates the 'key' in the algorithm described above */
        void createNewKey(KeyT & keyOut)
        {
            Random::fillBlock(salt, ArrSz(salt));
            // Hash the random block to prevent state guessing attacks so no data in output comes from the random output directly.
            BigHashT hash; hash.Start(); hash.Hash(salt, ArrSz(salt)); hash.Finalize(salt);
            deriveNewKey(keyOut);
        }

        /** Get the salt */
        void getCurrentSalt(KeyT & outSalt) const { memcpy(outSalt, salt, ArrSz(salt)); }


        /** Set the current salt (extracted from the ciphertext) */
        void setCurrentSalt(const KeyT & inSalt) { memcpy(salt, inSalt, ArrSz(salt)); }
        /** Derive the key out of the current salt */
        void deriveNewKey(KeyT & keyOut) const
        {
            KeyDerivFuncT kdf;
            kdf.Hash(masterKey, ArrSz(masterKey));
            kdf.finalizeWithExtraInfo(keyOut, salt, (uint32)ArrSz(salt));
        }
    };

    /** Get the key factory singleton */
    KeyFactory & getKeyFactory() { static KeyFactory factory; return factory; }

    /** The progress callback that's called regularly by the backup / restoring process */
    struct ProgressCallback
    {
        enum Action { Backup = 0, Restore, Purge };
        enum FlushMode  { FlushLine = 0, KeepLine, EraseLine };
        String getActionName(const Action action) { const char * actions[] = {"Backup", "Restore", "Purge"}; return actions[action]; }

        /** This method is called while an operation is running.
            The protocol for the sizeDone, totalSize, index and count is as follow:
              1) Each time a new entry is processed, index must be changed (likely increased). The sizeDone value is set to 0, and the totalSize is set to non-zero value.
                 The callback should not validate the line here if it output on the console/database
              2) While the entry is processed, the currentFilename will not change, the index will not change, but the sizeDone and totalSize will likely change.
                 The callback should still not validate the line here
              3) When the entry is done processing, sizeDone and totalSize will be equal.
                 The callback can validate the line here.
              If all sizeDone, totalSize, index, and count are zero, then the line is not validated, action is ignored, only the currentFilename is revelant for output.
            @return false to interrupt the process */
        virtual bool progressed(const Action action, const String & currentFilename, const uint64 sizeDone, const uint64 totalSize, const uint32 index, const uint32 count, const FlushMode mode) = 0;
        /** This method is called when the processing must warn the user.
            @return false to interrupt the process */
        virtual bool warn(const Action action, const String & currentFilename, const String & message, const uint32 sourceLine = 0) { return true; }
        #define WARN_CB(action, file, msg) callback.warn(action, file, msg, __LINE__)
        virtual ~ProgressCallback() {}
    };

    /** The purge strategy.
        @sa purgeBackup */
    enum PurgeStrategy
    {
        Fast            = 1,      //!< The fast strategy for pruning old backup, that's not space efficient.
        FindLostChunk   = Fast,   //!< Find lost chunk and remove them from the database index. If multichunk contains only garbage collected chunks, it's deleted.
        Slow            = 2,      //!< The slow strategy optimize for space, but it's not compute efficient.
        MergeMultiChunk = Slow,   //!< Find lost chunk and remove them from the database index. Recreate complete multichunk out of the remaining one,
                                  //!< downloading them, removing the useless chunk from them, and uploading complete multichunk again.
    };

    /** The overwrite strategy.
        @sa restoreBackup */
    enum OverwritePolicy
    {
        No          = 0, //!< No overwrite, nor deletion allowed
        Yes         = 1, //!< Overwrite and deletion allowed
        Update      = 2, //!< Overwrite allowed if the new item is newer than the one on the filesystem
    };

    /** Some useful methods to convert between internal checksum to hexdecimal */
    namespace Helpers
    {
        /** The allowed compressors */
        enum CompressorToUse
        {
            None = 0,    //!< No compression done to this multichunk
            ZLib = 1,    //!< Using ZLib compression
            BSC  = 2,    //!< Using BSC compression
            // No other compressor supported

            Default = -1,  //!< When not specified, this will select the global 'compressor' value
        };
        /** Convert a small binary blob to a string. */
        String fromBinary(const uint8 * data, const uint32 size, const bool base85 = true);
        /** Convert a string back to the binary blob. */
        bool toBinary(const String & src, uint8 * data, uint32 & size, const bool base85 = true);
        /** Encrypt a given block with AES counter mode.
            @warning Beware that this use the current key factory to figure out the current key and nonce */
        bool AESCounterEncrypt(const KeyFactory::KeyT & nonceRandom, const ::Stream::InputStream & input, ::Stream::OutputStream & output);
        /** Decrypt a given block with AES counter mode.
            @warning Beware that this use the current key factory to figure out the current key and nonce */
        bool AESCounterDecrypt(const KeyFactory::KeyT & nonceRandom, const ::Stream::InputStream & input, ::Stream::OutputStream & output);

        /** Close a currently filled multichunk and save in database and filesystem */
        bool closeMultiChunk(const String & basePath, File::MultiChunk & multiChunk, uint64 multichunkListID, uint64 * totalOutSize, ProgressCallback & callback, uint64 & previousMultichunkID, CompressorToUse actualComp = Default);
        /** Extract a chunk out of a multichunk */
        File::Chunk * extractChunk(String & error, const String & basePath, const String & MultiChunkPath, const uint64 MultiChunkID, const size_t chunkOffset, const String & chunkChecksum, const String & filterMode, File::MultiChunk * cache, ProgressCallback & callback);
        /** Allocate a chunk list ID */
        unsigned int allocateChunkList();
    }

    /** The database model we are following. */
    namespace DatabaseModel
    {
        /** The Index file metadata part */
        struct IndexDescription : public Database::Table<IndexDescription>
        {
            BeginFieldDeclarationDelayEx(IndexDescription)
                DeclareFieldEx(Version, Database::NotNullString, "1.0");
                DeclareField(CipheredMasterKey, String);
                DeclareField(InitialBackupPath, String);
                DeclareField(CurrentRevisionID, unsigned int);
                DeclareField(Description, String);
            EndFieldDeclaration
        };

        /** A chunk declaration.
            We don't use a blob for the checksum, because it's easier to debug with
            a plain old hexadecimal string and the difference is size does not justify the cost */
        struct Chunk : public Database::Table<Chunk>
        {
            BeginFieldDeclarationEx(Chunk)
                DeclareField(ID, Database::LongIndex);
                DeclareFieldWithIndex(Checksum, String, "", true);
                DeclareField(Size, unsigned int);
            EndFieldDeclaration
        };

        /** A logical list of chunks.
            Because chunks will be reused in different files, the files links to this list */
        struct ChunkList : public Database::Table<ChunkList>
        {
            BeginFieldDeclarationDelayEx(ChunkList)
                DeclareField(ID, uint64);
                DeclareFieldWithIndex(ChunkID, uint64, "0", false);
                DeclareField(Offset, uint64);
                DeclareField(Type, int); // This is used to avoid a useless query later on, 0 for file, 1 for multichunk
            EndFieldDeclaration
        };

        /** The multichunk declaration (this is similar to a chunklist,
            but stores the filtering information, and actual location in the remote folder of the data)  */
        struct MultiChunk : public Database::Table<MultiChunk>
        {
            BeginFieldDeclarationEx(MultiChunk)
                DeclareField(ID, Database::Index);
                DeclareFieldWithIndex(ChunkListID, uint64, "0", true);
                DeclareField(FilterListID, unsigned int);
                DeclareField(FilterArgument, String);
                DeclareField(Path, String);
            EndFieldDeclaration
        };

        /** A file entry (this maps files to chunks) - deprecated */
        struct File : public Database::Table<File>
        {
            BeginFieldDeclarationEx(File)
                DeclareField(ID, Database::Index);
                DeclareField(ChunkListID, uint64);
                DeclareField(ParentDirectoryID, unsigned int);
                DeclareField(Metadata, String);
                DeclareFieldEx(Revision, unsigned int, "0");
                DeclareField(Path, Database::NotNullString);
            EndFieldDeclaration
        };

        /** A directory entry - deprecated */
        struct Directory : public Database::Table<Directory>
        {
            BeginFieldDeclarationEx(Directory)
                DeclareField(ID, Database::Index);
                DeclareField(Path, Database::NotNullString);
                DeclareField(ParentDirectoryID, unsigned int);
                DeclareField(Metadata, String);
                DeclareFieldEx(Revision, unsigned int, "0");
            EndFieldDeclaration
        };

        /** A file or directory entry (this maps files to chunks).
            This deprecates the previous Directory & File table that were only growing in size.

            Typically, this tracks both the file type (directory or file) and the state (modified or deleted).

            For example, here's a complete description of successive operations in backups:
            @verbatim
            Initial file tree:
              root
               |--- file1
               |--- file2

            this will lead on first backup these 3 entries:
               - root (type d, rev 1, state m)
               - file1 (type f, rev 1, state m)
               - file2 (type f, rev 1, state m)
            @endverbatim

            Someone adds some files to the root folder:
            @verbatim
            New file tree:
              root
               |--- file1
               |--- file2
               |--- subdir
                      |--- file3

            this will lead on next backup these entries:
               - root (type d, rev 1, state m)
               - file1 (type f, rev 1, state m)
               - file2 (type f, rev 1, state m)

               - root (type d, rev 2, state m)    Due to change in modification time
               - subdir (type d, rev 2, state m)
               - file3 (type f, rev 2, state m)
            @endverbatim

            Then, someone deletes a file:
            @verbatim
            New file tree:
              root
               |--- file1
               |--- subdir
                      |--- file3

            this will lead on next backup these entries:
               - root (type d, rev 1, state m)
               - file1 (type f, rev 1, state m)
               - file2 (type f, rev 1, state m)

               - root (type d, rev 2, state m)
               - subdir (type d, rev 2, state m)
               - file3 (type f, rev 2, state m)

               - root (type d, rev 3, state m)    Due to change in modification time
               - file2 (type f, rev 3, state d)
            @endverbatim

            Then someone deletes a dir:
            @verbatim
            New file tree:
              root
               |--- file1

            this will lead on next backup these entries:
               - root (type d, rev 1, state m)
               - file1 (type f, rev 1, state m)
               - file2 (type f, rev 1, state m)

               - root (type d, rev 2, state m)
               - subdir (type d, rev 2, state m)
               - file3 (type f, rev 2, state m)

               - root (type d, rev 3, state m)
               - file2 (type f, rev 3, state d)

               - root (type d, rev 4, state m)    Due to change in modification time
               - subdir (type d, rev 4, state d)
               - file3 (type f, rev 4, state d)
            @endverbatim

            Then, after some revisions, add another file with the same name as the previous one:
            @verbatim
            New file tree:
              root
               |--- file1
               |--- subdir
                      |--- file3

            this will lead on next backup these entries:
               - root (type d, rev 1, state m)
               - file1 (type f, rev 1, state m)
               - file2 (type f, rev 1, state m)

               - root (type d, rev 2, state m)
               - subdir (type d, rev 2, state m)
               - file3 (type f, rev 2, state m)

               - root (type d, rev 3, state m)
               - file2 (type f, rev 3, state d)

               - root (type d, rev 4, state m)
               - subdir (type d, rev 4, state d)
               - file3 (type f, rev 4, state d)
               [...]
               - root (type d, rev 6, state m)    Due to change in modification time
               - subdir (type d, rev 6, state m)
               - file3 (type f, rev 6, state m)
            @endverbatim
        */
        struct Entry : public Database::Table<Entry>
        {
            BeginFieldDeclarationEx(Entry)
                DeclareField(ID, Database::Index);
                DeclareField(ChunkListID, uint64);
                DeclareField(ParentEntryID, unsigned int);
                DeclareField(Metadata, String);
                DeclareFieldEx(Revision, unsigned int, "0");
                DeclareFieldWithIndex(Path, Database::NotNullString, "", false);
                DeclareFieldEx(Type, unsigned int, "0");   // 0 for File, 1 for Directory
                DeclareFieldEx(State, unsigned int, "0");  // 0 for New/Modified, 1 for Deleted
            EndFieldDeclaration
        };

        /** The revision iteration.
            Each backup increment the revision number. If a file is modified in a revision,
            the previous revision is not deleted (unless pruning is requested).
            If a file is not modified in a revision, its revision number is not modified */
        struct Revision : public Database::Table<Revision>
        {
            BeginFieldDeclarationEx(Revision)
                DeclareField(ID, Database::Index);
                DeclareField(TimeSinceEpoch, uint64);
                DeclareField(RevisionTime, String);
                DeclareFieldEx(FileCount, unsigned int, "0");
                DeclareFieldEx(DirCount, unsigned int, "0");
                DeclareFieldEx(InitialSize, uint64, "0");
                DeclareFieldEx(BackupSize, uint64, "0");
            EndFieldDeclaration
        };

        /** Declare the database format we are using */
        struct FrostDB : public Database::Base<FrostDB>
        {
            BeginTableDeclaration(FrostDB)
                DeclareTable(IndexDescription)
                DeclareTable(Revision)
                DeclareTable(Entry)
                DeclareTable(MultiChunk)
                DeclareTable(ChunkList)
                DeclareTable(Chunk)
            EndTableDeclarationRegister(FrostDB)
//                DeclareTable(Directory)
//                DeclareTable(File)
        };

        /** The database complete URL to use */
        extern String databaseURL;

        BeginDatabaseConnection
            DeclareDatabaseWithComplexDBNameDynURL(FrostDB, "FrostDB", databaseURL, DEFAULT_INDEX)
        EndDatabaseConnection
    }

    // The backup functions
    /** Backup the given folder.
        @param folderToBackup   This the root of the folder to backup. All files will be saved in the backup relative to this root folder
        @param backupTo         The folder to store the multichunk into.
        @param revisionID       The current backup revision identifier
        @param callback         The progress callback that's called at regular interval
        @param strategy         The backing up strategy ('Slow' means reopening last multichunk to append to it thus creating less files in backup folder, 'Fast' is default)
        @return A string describing the error, or an empty string on success */
    String backupFolder(const String & folderToBackup, const String & backupTo, const unsigned int revisionID, ProgressCallback & callback, const PurgeStrategy strategy = Fast);
    /** List available backups.
        @param folderToBackup   This the root of the folder to backup. All files will be saved in the backup relative to this root folder
        @param backupTo         The folder to store the multichunk into.
        @param revisionID       The current backup revision identifier
        @param withList         If true, the file list is display for this revision
        @return The number of revisions listed */
    unsigned int listBackups(const ::Time::Time & startTime = ::Time::Epoch, const ::Time::Time & endTime = ::Time::MaxTime, const bool withList = false);
    /** Purge a backup from the given folder to space some space.
        @param chunkFolder      This the root of the folder where the chunk were saved
        @param strategy         The strategy to follow while globbering space to purge
        @param upToRevision     The last revision to clean up (inclusive)
        @return A string describing the error, or an empty string on success */
    String purgeBackup(const String & chunkFolder, ProgressCallback & callback, const PurgeStrategy strategy = Fast, const unsigned int upToRevision = 0);
    /* Restore a backup to the given folder.
        @param folderToRestore  This the root of the folder to restore the files into. All files will be restored from their relative position in the backup to this root folder
        @param restoreFrom      The folder to load the multichunk from.
        @param revisionID       The current backup revision identifier
        @param callback         The progress callback that's called at regular interval
        @return A string describing the error, or an empty string on success */
    String restoreBackup(const String & folderToRestore, const String & restoreFrom, const unsigned int revisionID, ProgressCallback & callback, const size_t maxCacheSize = 64*1024*1024);
}

