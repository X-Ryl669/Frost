// We need classical includes
#include <stdio.h>

// We need our declaration
#include "Frost.hpp"
// We need scoped pointer too
#include "ClassPath/include/Utils/ScopePtr.hpp"
// We need hex dump output
#include "ClassPath/include/Utils/Dump.hpp"
// We need encoding too
#include "ClassPath/include/Encoding/Encode.hpp"
// We need database constraint too
#include "ClassPath/include/Database/Constraints.hpp"
// We need the folder scanner too
#include "ClassPath/include/File/ScanFolder.hpp"
// We need the chunker too
#include "ClassPath/include/File/TTTDChunker.hpp"
// We need compression too
#include "ClassPath/include/Streams/CompressStream.hpp"
#include "ClassPath/include/Compress/BSCLib.hpp"


// The global option map
Strings::StringMap optionsMap;
// The warning log that's displayed on output
Strings::StringArray warningLog;
// Error code that's returned to bail out of int functions
const int BailOut = 26748;


namespace Frost
{

    bool dumpState = false, wasBackingUp = false, backupWorked = false;
    unsigned int previousRevID = 0;
    
    void debugMem(const uint8 * buffer, const uint32 len, const String & title = "")
    {
        if (!dumpState) return;
        String out;
        Utils::hexDump(out, buffer, len, 16, true, false);
        fprintf(stdout, "%s%s\n", (const char*)title, (const char*)out);
    }
    
    // This will be used later on when i18n'ing the software
#ifdef __GNUC__
    __attribute__((format_arg(1)))
#endif
    const char * __trans__(const char * format)
    {   // Monothreaded conversion here, it's a shame
        static String translated;
        translated = format;
        return (const char*)translated;
    }
    String TRANS(const String & value) { return __trans__(value); }
    
    
    String KeyFactory::loadPrivateKey(const String & fileVault, const MemoryBlock & cipherMasterKey, const String & password, const String & ID)
    {
        File::Info vault(fileVault, true);
        if (!vault.doesExist()) return TRANS("Key vault file does not exist");
        
        // Check the permission for the file
#if defined(_POSIX)
        if (vault.getPermission() != 0600) return TRANS("Key vault file permissions are bad, expecting 0600");
#endif
        Strings::FastString keyVaultContent = vault.getContent();
        if (!keyVaultContent) return TRANS("Unable to read the key vault file");
        
        // Parse the file to find out the ID in the list
        String keySizeAndID = keyVaultContent.splitUpTo("\n");
        String encKey = keyVaultContent.splitUpTo("\n");
        String keyID = keySizeAndID.fromFirst(" ");
        while (keyID != ID)
        {
             keySizeAndID = keyVaultContent.splitUpTo("\n");
             encKey = keyVaultContent.splitUpTo("\n");
             keyID = keySizeAndID.fromFirst(" ");
        }
        if (keyID != ID) return TRANS("Could not find a key with the specified ID: ") + ID;
        
        
        debugMem(cipherMasterKey.getConstBuffer(), cipherMasterKey.getSize(), "Ciphered master key");
        debugMem(keyVaultContent, keyVaultContent.getLength(), "Base85 content");
        
        
        // We need to load the ciphered private key out of the fileVault for our ID
        int encryptedKeySize = keySizeAndID;
        Utils::ScopePtr<MemoryBlock> cipherKey(MemoryBlock::fromBase85(encKey, encKey.getLength()));
        if (!cipherKey) return TRANS("Bad format for the key vault");
        debugMem(cipherKey->getConstBuffer(), cipherKey->getSize(), "Encrypted content key");
        
        // Then try to decode it with the given password
        // We need to derive the low-entropy password to build a Hash out of it, and use that to decrypt the private key
        // we have generated earlier.
        PWKeyDerivFuncT hash;
        KeyT pwKey;
        // Cat the password multiple time until it fit the required input size
        MemoryBlock inputPW(BigHashT::DigestSize);
        inputPW.stripTo(0);
        while (inputPW.getSize() < BigHashT::DigestSize)
            inputPW.Append(password, password.getLength() + 1); // Add 0 to differentiate "a" from "aa" or "aaa" etc...
        hash.Hash(inputPW.getConstBuffer(), inputPW.getSize());
        hash.Finalize(pwKey);
        debugMem(pwKey, ArrSz(pwKey), "Password key");
      
        
        // Then create the block to decrypt
        SymmetricT sym;
        sym.setKey(pwKey, (SymmetricT::BlockSize)ArrSz(pwKey), 0, (SymmetricT::BlockSize)ArrSz(pwKey));

        MemoryBlock decKey( (uint32)((encryptedKeySize + (ArrSz(pwKey) - 1) ) / ArrSz(pwKey)) * ArrSz(pwKey) );
        MemoryBlock clearKey(decKey.getSize());
        sym.Decrypt(cipherKey->getConstBuffer(), clearKey.getBuffer(), cipherKey->getSize()); // ECB mode used for a single block anyway
        debugMem(clearKey.getConstBuffer(), clearKey.getSize(), "Encryption key");
        
        
        // And finally decode the cipherMasterKey to the master key.
        AsymmetricT::PrivateKey key;
        if (!key.Import(clearKey.getConstBuffer(), encryptedKeySize, 0)) return TRANS("Bad key from the key vault"); // Bad key

        AsymmetricT asym;
        if (!asym.Decrypt(cipherMasterKey.getConstBuffer(), cipherMasterKey.getSize(), masterKey, ArrSz(masterKey), key))
            return TRANS("Can't decrypt the master key with the given key vault. Did you try with the wrong remote ?");
        debugMem(masterKey, ArrSz(masterKey), "Master key");
        
        
        return "";
    }
    
    String KeyFactory::createMasterKeyForFileVault(MemoryBlock & cipherMasterKey, const String & fileVault, const String & password, const String & ID)
    {
        // Check for file existence first.
        File::Info vault(fileVault, true);
        if (vault.doesExist())
        {
            Strings::FastString keyVaultContent = vault.getContent();
            if (!keyVaultContent) return TRANS("Unable to read the existing key vault file");
            
            // Parse the file to find out the ID in the list
            int count = 1;
            String keySizeAndID = keyVaultContent.splitUpTo("\n");
            String encKey = keyVaultContent.splitUpTo("\n");
            String keyID = keySizeAndID.fromFirst(" ");
            while (keyID != ID)
            {
                 keySizeAndID = keyVaultContent.splitUpTo("\n");
                 encKey = keyVaultContent.splitUpTo("\n");
                 keyID = keySizeAndID.fromFirst(" ");
                 count++;
            }
            if (keyID == ID) return TRANS("This ID already exists in the key vault: ") + fileVault + String("[")+ count + String("] => ") + ID;
        }
        File::Info parentFolder(vault.getParentFolder());
        if (parentFolder.doesExist() && !parentFolder.isDir()) return TRANS("The parent folder for the key vault file exists but it's not a directory: ") + fileVault;
        
        // Generate a lot of random data, and that'll become the master key.
        {
            uint8 randomData[2 * BigHashT::DigestSize];
            Random::fillBlock(randomData, ArrSz(randomData), true);
        
            // Create the master key
            BigHashT hash;
            hash.Start(); hash.Hash(randomData, ArrSz(randomData)); hash.Finalize(masterKey);
            
            debugMem(masterKey, ArrSz(masterKey), "Master key");
        }
        
        // Then, we need to generate a Asymmetric key pair, and export the key pair
        AsymmetricT asym;
        AsymmetricT::PrivateKey key;
        if (!asym.Generate(key)) return TRANS("Failed to generate a private key");
        
        // Export the key
        MemoryBlock exportedKey(key.getRequiredArraySize());
        if (!key.Export(exportedKey.getBuffer(), exportedKey.getSize())) return TRANS("Failed to export the private key");
        debugMem(exportedKey.getConstBuffer(), exportedKey.getSize(), "EC_IES Private key");
        
        // Encrypt the master key now
        if (!cipherMasterKey.ensureSize(asym.getCiphertextLength(ArrSz(masterKey)), true)) return TRANS("Failed to allocate memory for the ciphered master key");
        if (!asym.Encrypt(masterKey, ArrSz(masterKey), cipherMasterKey.getBuffer(), cipherMasterKey.getSize())) return TRANS("Failed to encrypt the master key");
        debugMem(cipherMasterKey.getConstBuffer(), cipherMasterKey.getSize(), "Ciphered master key");
        
        
        // We need to derive the low-entropy password to build a Hash out of it, and use that to encrypt the private key
        // we have generated earlier.
        PWKeyDerivFuncT hash;
        KeyT pwKey;
        // Cat the password multiple time until it fit the required input size
        MemoryBlock inputPW(BigHashT::DigestSize);
        inputPW.stripTo(0);
        while (inputPW.getSize() < BigHashT::DigestSize)
            inputPW.Append(password, password.getLength() + 1); // Add 0 to differentiate "a" from "aa" or "aaa" etc...
        hash.Hash(inputPW.getConstBuffer(), inputPW.getSize());
        hash.Finalize(pwKey);
        debugMem(pwKey, ArrSz(pwKey), "Password key");
        
        
        // Then create the block to encrypt
        MemoryBlock encKey( (uint32)((exportedKey.getSize() + (ArrSz(pwKey) - 1) ) / ArrSz(pwKey)) * ArrSz(pwKey) );
        MemoryBlock cipherKey(encKey.getSize());
        // Fill the key
        memcpy(encKey.getBuffer(), exportedKey.getConstBuffer(), exportedKey.getSize());
        // Finish by some random data (we don't care about it, since we'll drop it finally)
        Random::fillBlock(encKey.getBuffer() + exportedKey.getSize(), encKey.getSize() - exportedKey.getSize());
        debugMem(encKey.getConstBuffer(), encKey.getSize(), "Encryption key");
        
        
        SymmetricT sym;
        sym.setKey(pwKey, (SymmetricT::BlockSize)ArrSz(pwKey), 0, (SymmetricT::BlockSize)ArrSz(pwKey));
        sym.Encrypt(encKey.getConstBuffer(), cipherKey.getBuffer(), encKey.getSize()); // ECB mode used for a single block anyway
        debugMem(cipherKey.getConstBuffer(), cipherKey.getSize(), "Encrypted content key");
        
        // And finally create the output key vault with this
        if (!parentFolder.doesExist() && !parentFolder.makeDir(true)) return TRANS("Can't create the parent folder for the key vault file");
        
        Utils::ScopePtr<MemoryBlock> base85Encoded(cipherKey.toBase85());
        debugMem(base85Encoded->getConstBuffer(), base85Encoded->getSize(), "Base85 Encrypted content key");
        
        if (!vault.setContent(String::Print("%d %s\n%s\n", (int)exportedKey.getSize(), (const char*)ID, (const char*)String(base85Encoded->getConstBuffer(), base85Encoded->getSize())), true)) return TRANS("Can't set the key vault file content");
        if (!vault.setPermission(0600)) return TRANS("Can't set the key vault file permission to 0600");
        return "";
    }
    
    namespace DatabaseModel { String databaseURL = ""; }
    namespace Helpers
    {
        enum CompressorToUse
        {
            ZLib = 0,
            BSC  = 1,
            // No other compressor supported
        } compressor;
        
        // Base 85 encoding
        String fromBinary(const uint8 * data, const uint32 size, const bool base)
        {
            String ret;
            uint32 outSize = base ? (size * 5 + 3) / 4 : size * 2;
            if (base)
            {
                if (!Encoding::encodeBase85(data, size, (uint8*)ret.Alloc(outSize), outSize)) return "";
            } else if (!Encoding::encodeBase16(data, size, (uint8*)ret.Alloc(outSize), outSize)) return "";
            
            ret.releaseLock((int)outSize);
            return ret;
        }
        // Base 85 decoding
        bool toBinary(const String & src, uint8 * data, uint32 & size, const bool base)
        {
            return base ? Encoding::decodeBase85((const unsigned char*)src, src.getLength(), data, size)
                        : Encoding::decodeBase16((const unsigned char*)src, src.getLength(), data, size);
        }
        
        // Encrypt a block in AES counter mode
        bool AESCounterEncrypt(const KeyFactory::KeyT & nonceRandom, const ::Stream::InputStream & input, ::Stream::OutputStream & output)
        {
            KeyFactory::KeyT nonce = {0}, key = {0}, salt = {0}, plainText = {0}, cipherText = {0};
            getKeyFactory().createNewKey(key);
            getKeyFactory().getCurrentSalt(salt);
            
            // Write the salt to the output stream
            if (!output.write(salt)) return false;
            
            getKeyFactory().createNewNonce(nonceRandom);
            Crypto::OSSL_AES cipher;
            cipher.setKey(key, (Crypto::BaseSymCrypt::BlockSize)ArrSz(key), 0, (Crypto::BaseSymCrypt::BlockSize)ArrSz(key));
            
            for (uint64 i = 0; i < input.fullSize(); i += ArrSz(nonce))
            {
                // Increment the nonce including the counter
                getKeyFactory().incrementNonce(nonce);
                // Read the data
                uint64 inputSize = input.read(plainText, (uint64)ArrSz(plainText));
                if (inputSize == (uint64)-1) return false;
                // And encrypt the data
                if (!Crypto::CTR_BlockProcess(cipher, nonce, salt)) return false;
                
                // Encrypt the data
                Crypto::Xor(cipherText, plainText, salt, (size_t)inputSize);
                
                if (output.write(cipherText, inputSize) != inputSize) return false;
            }
            return true;
        }
        // Decrypt a given block with AES counter mode.
        bool AESCounterDecrypt(const KeyFactory::KeyT & nonceRandom, const ::Stream::InputStream & input, ::Stream::OutputStream & output)
        {
            // Get the salt from the system
            KeyFactory::KeyT nonce = {0}, key = {0}, salt = {0}, plainText = {0}, cipherText = {0};
            
            if (!input.read(salt)) return false;
            getKeyFactory().setCurrentSalt(salt);
            getKeyFactory().deriveNewKey(key);
            
            getKeyFactory().createNewNonce(nonceRandom);
            Crypto::OSSL_AES cipher;
            cipher.setKey(key, (Crypto::BaseSymCrypt::BlockSize)ArrSz(key), 0, (Crypto::BaseSymCrypt::BlockSize)ArrSz(key));
            memset(key, 0, ArrSz(key));
            
            for (uint64 i = ArrSz(salt); i < input.fullSize(); i += ArrSz(nonce))
            {
                // Increment the nonce including the counter
                getKeyFactory().incrementNonce(nonce);
                // Read the data
                uint64 inputSize = input.read(cipherText, (uint64)ArrSz(cipherText));
                if (inputSize == (uint64)-1) return false;
                // And encrypt the data
                if (!Crypto::CTR_BlockProcess(cipher, nonce, salt)) return false;
                
                // Decrypt the data
                Crypto::Xor(plainText, cipherText, salt, (size_t)inputSize);
                
                if (output.write(plainText, inputSize) != inputSize) return false;
            }
            
            return true;
        }
        
        bool closeMultiChunk(const String & backupTo, File::MultiChunk & multiChunk, uint64 multiChunkID, uint64 * totalOutSize)
        {
            // We need this for the nonce
            KeyFactory::KeyT chunkHash;
            multiChunk.getChecksum(chunkHash);
            
            const String & multiChunkHash = Helpers::fromBinary(chunkHash, ArrSz(chunkHash), false);
            // Then filter the multichunk, compress it and encrypt it
            ::Stream::OutputMemStream compressedStream;
            
            switch (compressor)
            {
            case ZLib:
                {   // Compress the data
                    Compression::ZLib * zlib = new Compression::ZLib;
                    zlib->setCompressionFactor(1.0f);
                    // It owns the pointer
                    ::Stream::CompressOutputStream compressor(compressedStream, zlib);
                    if (!multiChunk.writeHeaderTo(compressor)) return false;
                    if (!multiChunk.writeDataTo(compressor)) return false;
                    break;
                }
            case BSC:
                {   // Compress the data
                    ::Stream::CompressOutputStream compressor(compressedStream, new Compression::BSCLib);
                    if (!multiChunk.writeHeaderTo(compressor)) return false;
                    if (!multiChunk.writeDataTo(compressor)) return false;
                    break;
                }
            default:
                return false;
            }
            
            {   // Encode the data now
                ::Stream::MemoryBlockStream compressedData(compressedStream.getBuffer(), compressedStream.fullSize());
                if (totalOutSize) *totalOutSize += compressedStream.fullSize();
                ::Stream::OutputFileStream chunkFile(backupTo + multiChunkHash + ".#");
                if (!Helpers::AESCounterEncrypt(chunkHash, compressedData, chunkFile))
                    return false;
            }
            
            const char * compressorName[] = { "zLib", "BSC" };
            // Create the chunk in the database now
            DatabaseModel::MultiChunk dbMChunk;
            dbMChunk.ChunkListID = multiChunkID;
            dbMChunk.FilterListID = 3;
            dbMChunk.FilterArgument = String::Print("%d:%s:AES_CTR", File::MultiChunk::MaximumSize, compressorName[compressor]);
            dbMChunk.Path = multiChunkHash + ".#";
            dbMChunk.ID = Database::Index::WantNewIndex;
            
            multiChunk.Reset();
            return true;
        }      
        
        File::Chunk * extractChunk(String & error, const String & basePath, const String & MultiChunkPath, const uint64 MultiChunkID, const size_t chunkOffset, const String & chunkChecksum, const String & filterMode, File::MultiChunk * cache)
        {
            error = "";
            File::MultiChunk localMultichunk, & mchunk = cache ? *cache : localMultichunk;
            
            if (mchunk.getOpaque() != MultiChunkID)
            {
                 // Decode this multichunk to find out the chunk to store in file
                ::Stream::InputFileStream chunkFile(basePath + MultiChunkPath);
                ::Stream::OutputMemStream compressedData;
                
                KeyFactory::KeyT chunkHash;
                uint32 chunkHashSize = (uint32)ArrSz(chunkHash);
                if (!Helpers::toBinary(MultiChunkPath.upToLast("."), chunkHash, chunkHashSize, false)
                    || chunkHashSize != (uint32)ArrSz(chunkHash))
                {
                    error = TRANS("Error while decoding the hash of the multichunk: ") + MultiChunkPath;
                    return 0;
                }
                
                // Decrypt it now
                if (filterMode.fromLast(":") == "AES_CTR")
                {   // We only support counter mode for now
                    if (!Helpers::AESCounterDecrypt(chunkHash, chunkFile, compressedData))
                    {
                        error = TRANS("Can not decode the multichunk: ") + MultiChunkPath;
                        return 0;
                    }
                }
                
                // Decompress it
                String compUsed = filterMode.fromTo(":", ":");
                if (compUsed == "zLib")
                {   // And zLib 
                    ::Stream::MemoryBlockStream compressedStream(compressedData.getBuffer(), compressedData.fullSize());
                    {
                        Compression::ZLib * zlib = new Compression::ZLib;
                        zlib->setCompressionFactor(1.0f);
                        ::Stream::DecompressInputStream decompressor(compressedStream, zlib);
                        if (!mchunk.loadHeaderFrom(decompressor))
                        {
                            error = TRANS("Can not decompress header from multichunk: ") + MultiChunkPath;
                            return 0;
                        }
                        if (!mchunk.loadDataFrom(decompressor))
                        {
                            error = TRANS("Can not decompress data from multichunk: ") + MultiChunkPath;
                            return 0;
                        }
                    }
                } else if (compUsed == "BSC")
                {   // And BSCLib 
                    ::Stream::MemoryBlockStream compressedStream(compressedData.getBuffer(), compressedData.fullSize());
                    {
                        ::Stream::DecompressInputStream decompressor(compressedStream, new Compression::BSCLib);
                        if (!mchunk.loadHeaderFrom(decompressor))
                        {
                            error = TRANS("Can not decompress header from multichunk: ") + MultiChunkPath;
                            return 0;
                        }
                        if (!mchunk.loadDataFrom(decompressor))
                        {
                            error = TRANS("Can not decompress data from multichunk: ") + MultiChunkPath;
                            return 0;
                        }
                    }
                }

                // Assert the decompressed data is still correct
                KeyFactory::KeyT chunkTest;
                mchunk.getChecksum(chunkTest);
                if (memcmp(chunkTest, chunkHash, ArrSz(chunkHash)))
                {
                    error = TRANS("Corruption detected in multichunk: ") + MultiChunkPath;
                    return 0;
                }
                
                // It worked, so let's save it to avoid reloading it
                mchunk.setOpaque(MultiChunkID);
            }
            
            // Ok, extract the chunk
            uint8 chunkCS[Hashing::SHA1::DigestSize];
            uint32 chunkCSSize = (uint32)ArrSz(chunkCS);
            if (!Helpers::toBinary(chunkChecksum, chunkCS, chunkCSSize) || chunkCSSize != (uint32)ArrSz(chunkCS))
            {
                error = TRANS("Bad checksum for chunk with checksum: ") + chunkChecksum;
                return 0;
            }
        
            File::Chunk * chunk = mchunk.findChunk(chunkCS, (size_t)chunkOffset);
            return chunk;        
        }
        
        unsigned int allocateChunkList()
        {
            BuildPool(DatabaseModel::ChunkList, chunkListPool, ID, _C::Max());
            return chunkListPool.count ? chunkListPool[0].ID + 1 : 1;
        }
      
        
    }
    // Initialize the database connection, and bootstrap it if required.
    String initializeDatabase(const String & backupPath, unsigned int & revisionID, MemoryBlock & cipheredMasterKey)
    {
        if (!Database::SQLFormat::initialize(DEFAULT_INDEX, DatabaseModel::databaseURL, "", "", 0))
            return TRANS("Can't initialize the database with the given parameters.");
        
        String currentTime = Time::LocalTime::Now().toDate();
        
        // Check if the database exits and open it in that case.
        if (!Database::SQLFormat::checkDatabaseExists(0))
        {
            // Need to create it
            if (!Database::SQLFormat::createModelsForAllConnections())
                return TRANS("Failed to create the tables in the database from the given model");
                
            DatabaseModel::IndexDescription index;
            index.Version = PROTOCOL_VERSION;
            index.InitialBackupPath = backupPath;
            if (cipheredMasterKey.getSize())
            {
                Utils::ScopePtr<MemoryBlock> base85Key = cipheredMasterKey.toBase85();
                if (!base85Key) return TRANS("Failed to convert the ciphered master key to base85");
                index.CipheredMasterKey = String(base85Key->getConstBuffer(), base85Key->getSize());
            }
            index.Description = "Backup of " + backupPath + " started on " + currentTime + " finished on";
            index.synchronize("Version");
            previousRevID = 0;
        }
        
        
        // Create a new revision now if provided a path to backup
        if (backupPath)
        {
            DatabaseModel::Revision rev;
            rev.RevisionTime = currentTime;
            rev.TimeSinceEpoch = (uint64)time(NULL);
            rev.ID = Database::Index::WantNewIndex;
            revisionID = rev.ID;
            
            wasBackingUp = true;
        }

        BuildPool(DatabaseModel::IndexDescription, pool, Version, _C::Equal(PROTOCOL_VERSION));
        if (pool.count)
        {
            // Get the current revision identifier
            previousRevID = pool[0].CurrentRevisionID;
            
            const String & masterKey = pool[0].CipheredMasterKey;
            if (!cipheredMasterKey.rebuildFromBase85(masterKey, masterKey.getLength()))
                return TRANS("Invalid ciphered master key in the database. The database is likely corrupted.");
            if (backupPath)
            {
                pool[0].CurrentRevisionID = revisionID;
                pool[0].synchronize("Version");
            } else revisionID = previousRevID;
        }
        return "";
    }
    
    // Finalize the database, updating the database description when done. 
    void finalizeDatabase()
    {
        // Check if we were backing up
        if (wasBackingUp)
        {
            BuildPool(DatabaseModel::IndexDescription, pool, Version, _C::Equal(PROTOCOL_VERSION));
            if (pool.count)
            {
                // Check if a backup operation actually happened
                if (backupWorked)
                {
                    // Update the description
                    pool[0].Description = ((String)pool[0].Description).upToFirst("finished on") + "finished on " + Time::LocalTime::Now().toDate();
                    pool[0].synchronize("Version");
                }
                else
                {
                    // We need to rollback to a previous revision
                    BuildConstraint(DatabaseModel::Revision, prevRevisions, ID, _C::Max());
                    BuildConstraint(DatabaseModel::Revision, notNull, InitialSize, _C::NotEqual(0));
            
                    Database::Pool<DatabaseModel::Revision> revPool = Database::find(notNull.And(prevRevisions));
                    if (revPool.count)
                        pool[0].CurrentRevisionID = revPool[0].ID;
                    else pool[0].CurrentRevisionID = 0;
                    pool[0].Description = "Reverted to last known good revision on " + Time::LocalTime::Now().toDate();
                    pool[0].synchronize("Version");

                    // Then delete all bad revisions
                    BuildConstraint(DatabaseModel::Revision, nullRev, InitialSize, _C::IsNull());
                    Database::deleteInDB(nullRev);
                }
            }
        }
        Database::SQLFormat::finalize((uint32)-1);
    }
    


    /** Create a list of files in a directory based on the Entry's in the database.
        @param dirPath      The directory path to look for
        @param entryList    On output, contains a sorted list of entries' ID in the specified directory
        @param revID        The maximum revision ID to include
        @return The highest directory entry ID if found, or 0 on error */
    unsigned int createActualEntryListInDir(const String & dirPath, Strings::StringArray & entryList, const unsigned int revID)
    {
        entryList.Clear();
        
        // Find the directory with the given path
        // We want to stop searching as soon as a directory has its State "deleted"
        // SELECT FROM Entry WHERE Path = path AND Revision <= revID AND Revision > (SELECT Revision FROM Entry WHERE Path = path AND State = deleted AND Revision <= revID AND Type == 1 ORDER BY Revision DESC LIMIT 1)    
        RowIterT deletedDir = ((((Select("Revision").From("Entry").Where("Path") == dirPath).And("State") == 1).And("Revision") <= revID).And("Type") == 1).OrderBy("Revision", false).Limit(1);
        unsigned int lowerRev = 0;
        if (deletedDir) lowerRev = (unsigned int)deletedDir["Revision"];
        
        const Select dirEntry = ((((Select("*").From("Entry").Where("Path") == dirPath).And("Revision") <= revID).And("Type") == 1).And("Revision") > lowerRev).OrderBy("Revision", false);
        Database::Pool<DatabaseModel::Entry> dirEntries = dirEntry;
        if (!dirEntries.count) return 0; // No directory found lower than the given revision
        
        // The dirEntries pool contains a chain of directory entries.
        
        // We will then select any entries that are refering to any ID in the dirEntries pool
        Database::Pool<DatabaseModel::Entry> entries = (Select("*").From("Entry").Where("ParentEntryID").In(dirEntry.Refine("ID")).And("Revision") <= revID).OrderBy("Path", true, "Revision", false);
        // Then for each entry, let's build the final entry list
        String lastPath = "*"; // Impossible path
        for (unsigned int i = 0; i < entries.count; i++)
        {
            if (entries[i].Path != lastPath)
            {
                // New file, let's figure out if it needs to be included or not in the list
                if (entries[i].State != 1)
                    entryList.Append(String::Print("%u", (unsigned int)entries[i].ID));
                    
                lastPath = entries[i].Path;
            }
        }
        
        // Now we have the update list of entries in the directory
        return dirEntries[0].ID;
    }

    /** This is used as a wrapper over a DatabaseModel::Entry to avoid doing a lot of queries on the database */
    class FileMDEntry
    {
        /** The file entry ID in database */
        unsigned int ID;
        /** The file metadata */
        String metadata;
        
        // Interface
    public:
        /** Convert to unsigned int for the item ID */
        operator unsigned int() const { return ID; }
        /** Get the metadata */
        const String & getMetaData() const { return metadata; }
    
        FileMDEntry(const unsigned int ID, const String & md) : ID(ID), metadata(md) {}
    };
    
    typedef Container::HashTable<FileMDEntry, String, Container::HashKey<String> > PathIDMapT;
    /** Create a list of files in a directory based on the Entry's in the database.
        @param dirPath  The directory path to look for
        @param fileList On output, contains a sorted list of files and directories path in the specified directory
        @param revID    The maximum revision ID to include
        @return The directory ID if found, or 0 on error */
    unsigned int createFileListInDir(const String & dirPath, PathIDMapT & fileList, const unsigned int revID)
    {
        fileList.clearTable();
        Strings::StringArray entries;
        
        unsigned int dirID = createActualEntryListInDir(dirPath, entries, revID);
        if (dirID == 0) return 0;
        
        // Then find all files and directory and sort them
        
        // Find the directory with the given path
        RowIterT fileEntries = Select("Path", "ID", "Metadata").From("Entry").Where("ID").In(entries).OrderBy("Path", true);
        if (!fileEntries) return 0; // No directory found lower than the given revision
        
        while (fileEntries)
        {
            fileList.storeValue(fileEntries["Path"], new FileMDEntry(fileEntries["ID"], fileEntries["Metadata"]), true);
            ++fileEntries;
        }
        return dirID;
    }
    
    /** Create the list of files and directories based on the Entry's in the database. 
        @param fileList On output, contains a sorted list of files and directories path in the specified directory
        @param revID    The maximum revision ID to include
        @return true if the revision is valid */
    bool createFileListInRev(PathIDMapT & fileList, const unsigned int revID)
    {
        fileList.clearTable();
        // We need to find out all the valid directories in the given revision.
        Database::Pool<DatabaseModel::Entry> directories = ((Select("*").From("Entry").Where("Type") == 1).And("Revision") <= revID).OrderBy("Path", true, "Revision", false);
        String lastPath = "*";
        unsigned int step = 1;
        for (unsigned int i = 0; i < directories.count; i += step)
        {
            // For each directory alive, we have to find out the min and max revision
            step = 1;
            if (directories[i].Path != lastPath)
            {
                lastPath = directories[i].Path;
                // We don't care about deleted directories
                if (directories[i].State == 1) continue;
                // New dir that's alive, let's save it
                fileList.storeValue(directories[i].Path, new FileMDEntry(directories[i].ID, directories[i].Metadata), true);
                
                // Then find out all the files that refers to this directory
                Strings::StringArray dirID = String::Print("%u", (unsigned int)directories[i].ID);
                while (i + step < directories.count && directories[i + step].Path == lastPath)
                {
                    if (directories[i + step].State == 1) break;
                    
                    // Save this revision ID in the list of ID to include when searching for files
                    dirID.Append(String::Print("%u", (unsigned int)directories[i + step].ID));
                    step++;
                }
                
                // Then find any file that has a parent directory ID in the list above
                Database::Pool<DatabaseModel::Entry> files = (((Select("*").Max("Revision", "MaxRev").From("Entry").Where("Type") == 0).And("Revision") <= revID).And("ParentEntryID").In(dirID).And("State") == 0).GroupBy("Path");
                for (unsigned int j = 0; j < files.count; j++)
                    fileList.storeValue(files[j].Path, new FileMDEntry(files[j].ID, files[j].Metadata), true);
            }
        }
        return true;
    }
    
    // Very basic algorithm to make a size readable by a human easily
    String makeLegibleSize(uint64 size)
    {
        const char * suffix[] = { "B", "KB", "MB", "GB", "TB", "PB", "EB" };
        int suffixPos = 0, lastReminder = 0;
        while (size / 1024) { suffixPos++; lastReminder = size % 1024; size /= 1024; }
        return String::Print("%lld.%d%s", size, (lastReminder * 10 / 1024), suffix[suffixPos]);
    }
    String makeLegibleTime(uint64 ms)
    {
        const char * suffix[] = { "ms", "sec", "min", "hour", "day" };
        const int base[] = { 1000, 60, 60, 24, 1<<30};
        int suffixPos = 0, lastReminder = 0;
        while (suffixPos < 4 && ms / base[suffixPos]) { lastReminder = ms % base[suffixPos]; ms /= base[suffixPos]; suffixPos++; }
        return String::Print("%lld.%d%s", ms, suffixPos ? (lastReminder * 10) / base[suffixPos - 1] : 0, suffix[suffixPos]);
    }
        
    
    struct ConsoleProgressCallback : public ProgressCallback
    {
        int lastProgress;
        uint32 lastIndex;
        uint32 lastCount;
        uint64 lastSize;
        uint32 lastTime;
        int lastSpeed; 
        
        virtual bool progressed(const Action action, const String & currentFilename, const uint64 sizeDone, const uint64 totalSize, const uint32 index, const uint32 count)
        {
            if (lastIndex != index || lastCount != count)
            {
                lastProgress = 0;
                lastSize = 0;
                lastIndex = index;
                lastCount = count;
             }
            
            // Special signal to display the output non-flushed
            if (sizeDone == 0)
            {
                fprintf(stdout, "\r%s: %s [%u/%u]                                     ", (const char*)TRANS(getActionName(action)), (const char*)currentFilename, index, count);
                if (!totalSize) fprintf(stdout, "\n");
                else fflush(stdout);
                return true;
            }
            
            uint32 currentTime = Time::getTimeWithBase(1000);
            int progress = totalSize ? (int)((sizeDone * 100) / totalSize) : 100;
            if (progress != lastProgress)
            {
                if (progress != 100)
                {
                    int duration = currentTime - lastTime;
                    int speed = duration ? (sizeDone - lastSize) * 1000 / duration : 0;
                    // Window average to smooth the reported speed
                    const int windowSize = 128;
                    lastSpeed = (lastSpeed * (windowSize - 1)) / windowSize + (speed - lastSpeed) / windowSize;
                    int remaining = lastSpeed ? (totalSize - sizeDone) * 1000 / lastSpeed : 0;
                    
                    fprintf(stdout, "\r%s: %s %2d%%:%s/s (rem: %s) [%u/%u]            ", (const char*)TRANS(getActionName(action)), (const char*)currentFilename, progress, (const char*)makeLegibleSize(lastSpeed), (const char*)makeLegibleTime(remaining), index, count);
                    fflush(stdout);
                }
                else
                    fprintf(stdout, "\r%s: %s [%u/%u]                                     \n", (const char*)TRANS(getActionName(action)), (const char*)currentFilename, index, count);
                lastProgress = progress;
            }
            lastSize = sizeDone;
            lastTime = currentTime;
            return true;
        }
        
        virtual bool warn(const Action action, const String & currentFilename, const String & message)
        {
            warningLog.Append(currentFilename + ": " + message);
            fprintf(stderr, TRANS("\nWARNING %s: %s\n"), (const char*)currentFilename, (const char*)message);
            return true;
        }
        
        ConsoleProgressCallback() : lastProgress(0), lastIndex(0), lastCount(0), lastSize(0), lastTime(0), lastSpeed(0) {}
    };
    
    /** A filter class that's accepting all files */
    struct AllFiles : public File::Scanner::FileFilter
    {
        mutable uint32 count;
        ProgressCallback & callback;
        virtual bool matchFile(const String & fileName) const
        {
            count++;
            if ((count % 100) == 0)
                callback.progressed(ProgressCallback::Backup, TRANS("...scanning... ") + fileName, 0, 1, 0, count);
            return true;
        }
        AllFiles(ProgressCallback & callback) : count(0), callback(callback) {}
    };
    /** The file filter that's accepting all files and backuping them */
    struct BackupFile : public File::Scanner::EventIterator::FileFoundCB
    {
        ProgressCallback & callback;
        const String & backupTo;
        const String folderToBackup;
        const unsigned int revID;
        uint32 seen;
        uint32 total;
        
        uint32 fileCount;
        uint32 dirCount;
        uint64 totalInSize;
        uint64 totalOutSize;
        
        File::TTTDChunker chunker;
        File::MultiChunk multiChunk;
        uint64           multiChunkListID;
        
        PathIDMapT           prevFilesInDir;
        String               prevParentFolder;
                
        // Check if a file has content to save
        bool hasContent(File::Info & info)
        {
            return info.isFile() && !info.isDir() && !info.isLink();
        }
        
        unsigned int findParentDirectoryID(const String & strippedFilePath)
        {
            String parentPath = File::General::normalizePath(strippedFilePath + "/../").normalizedPath(Platform::Separator, false);
            
            Database::Pool<DatabaseModel::Entry> pool = (Select("ID", "State").From("Entry").Where("Path") == parentPath).OrderBy("Revision", false).Limit(1);
            if (pool.count)
            {
                // This should not happen, since we traverse the file hierarchy directories first and a
                // new directory should already be inserted in the DB
                Assert(pool[0].State == 0);
                return pool[0].ID;
            }
            return 0;
        }
        
        String checkMostRecentEntryMetadata(const String & strippedFilePath)
        {
            RowIterT entry = (Select("State", "Metadata").From("Entry").Where("Path") == strippedFilePath).OrderBy("Revision", false).Limit(1);
            if (entry && entry["State"] == "0")
                return entry["Metadata"];
            return "";
        }
                

        
        void deleteRemainingEntry(unsigned int ID)
        {
            DatabaseModel::Entry entry;
            entry.ID = ID; // Fetch the previous entry
            if (entry.Type == 1)
            {
                // If it's a directory, we need to remove all the sub files too
                // First, we need to find out all the revisions where this directory entry exist
                RowIterT lastDeleteRev = (((Select("Revision").From("Entry").Where("Path") == entry.Path).And("State") == 1).And("Type") == 1).OrderBy("Revision", false).Limit(1);
                Select dirValidRevs = ((Select("ID").From("Entry").Where("Path") == entry.Path).And("Type") == 1).And("Revision") > lastDeleteRev["Revision"];
                
                // Then select all the entries that refer to this directory
                RowIterT subEntries = ((Select("ID", "Path").From("Entry").Where("ParentEntryID").In(dirValidRevs).And("Revision") > lastDeleteRev["Revision"]).And("State") == 0).OrderBy("Path", true, "Revision", false);
                String lastPath = "*";
                while (subEntries)
                {   // We are getting the list of entries refering to the deleted directory, ordered by path, and then revision (max first).
                    // If an entry was modified in a previous backup, it will appear twice with the same path, so the first revision in the result set is the highest one.
                    // We want the highest revision in order to get the most up-to-date metadata
                    String path = subEntries["Path"];
                    if (path != lastPath)
                    {
                        deleteRemainingEntry((unsigned int)subEntries["ID"]);
                        lastPath = path;
                    }
                    ++subEntries;
                }
            }
            entry.ID = Database::Index::WantNewIndex; // Clone this entry
            entry.Revision = revID;
            entry.State = 1; // And say it's deleted
            // We are done
        }
        
        virtual bool fileFound(File::Info & info, const String & strippedFilePath)
        {
            // Compute stats first
            uint32 entriesCount = info.getEntriesCount();
            if (info.isDir()) total += entriesCount;
            seen++;
            
            // Ok, backup this file, if required (we lie about the size here)
            if (!callback.progressed(ProgressCallback::Backup, TRANS("Analysing: ") + info.name, 0, 1, seen, total))
                return false;
                
            // Check if the parent folder was changed
            String parentFolder = info.getParentFolder();
            if (parentFolder != prevParentFolder)
            {
                // It is, so let's find out the deleted items (and mark them as removed in the database)
                PathIDMapT::IterT iter = prevFilesInDir.getFirstIterator();
                while (iter.isValid())
                {
                    deleteRemainingEntry(*(*iter));
                    ++iter;
                }
                
                // Then we need to compute the last database index for this folder
                String relativeParentPath = File::General::normalizePath(strippedFilePath + "/../").normalizedPath(Platform::Separator, false);
                createFileListInDir(relativeParentPath, prevFilesInDir, revID);
                // We don't care about the result here, as if the directory does not exists (yet), it'll be created below
                
                prevParentFolder = parentFolder;
            }

            // We will extract the metadata out of this file first
            String metadata = info.getMetaData();
            
            prevFilesInDir.removeValue(strippedFilePath);
            
            
            // Now handle special file directly
            if (info.isLink())
            {
                // Check if the link point to outside the backup folder (in that case warn the user that it'll not be backuped)
                String backupFullPath = File::Info(folderToBackup).getRealFullPath();
                String currentFullPath = info.getRealFullPath();
                if (currentFullPath.midString(0, backupFullPath.getLength()) != backupFullPath)
                {
                    if (!callback.warn(ProgressCallback::Backup, info.name, TRANS("Symbolic link points outside of the backup folder, the content will not be saved, only the link")))
                        return false;
                }
            }
            
            if (strippedFilePath == PathSeparator && findParentDirectoryID(strippedFilePath + "a") == 0)
            {
                // Create the root folder if it does not exists yet
                DatabaseModel::Entry file;
                file.ChunkListID = 0;
                file.ParentEntryID = 0;
                file.Metadata = metadata;
                file.Path = strippedFilePath;
                file.Revision = revID;
                file.Type = 1;
                file.State = 0;
                file.ID = Database::Index::WantNewIndex;
                dirCount++;
                return callback.progressed(ProgressCallback::Backup, info.name, 0, 0, seen, total);                
            }
            
            // We'll need the parent directory ID to link with
            unsigned int parentDirID = findParentDirectoryID(strippedFilePath);
            if (!parentDirID)
                return !callback.warn(ProgressCallback::Backup, info.name, TRANS("The parent directory does not exists in the database")) && false;

            // Check if the entry already exists in the database
            String dbMeta = checkMostRecentEntryMetadata(strippedFilePath);
            
            if (!dbMeta || !info.hasSimilarMetadata(dbMeta, File::Info::AllButAccessTime, &metadata))
            {
                if (info.isLink() || info.isDevice() || info.isDir())
                {
                    DatabaseModel::Entry file;
                    file.ChunkListID = 0;
                    file.ParentEntryID = parentDirID;
                    file.Metadata = metadata;
                    file.Path = strippedFilePath;
                    file.Revision = revID;
                    file.Type = info.isDir() ? 1 : 0;
                    file.State = 0;
                    file.ID = Database::Index::WantNewIndex;
                    if (info.isDir()) dirCount++;
                    else fileCount++;
                } else if (info.isFile())
                {
                    // We need to chunk it
                    // It does not exist, or is modified, so create a new entity in the database
                    Database::Transaction transaction;
                    // We need to chunk it
                    File::Chunk temporaryChunk;
                    ::Stream::InputFileStream stream(info.getFullPath());
                    
                    // Create a new entity in the DB for this chunk list
                    DatabaseModel::ChunkList chunkList;
                    chunkList.Type = 0;
                    bool hasData = false;
                    
                    // And for the multichunk list
                    DatabaseModel::ChunkList multiChunkList;
                    
                    // Build the list of chunk ID for storage in the DB
                    uint64 streamOffset = stream.currentPosition();
                    uint64 fullSize = stream.fullSize();
                    totalInSize += fullSize;
                    while (chunker.createChunk(stream, temporaryChunk))
                    {
                        if (!callback.progressed(ProgressCallback::Backup, info.name, streamOffset, fullSize, seen, total))
                            return false;

                        // Ok, got a chunk, let's first figure out if we need to store it in the database
                        const String & chunkChecksum = Helpers::fromBinary(temporaryChunk.checksum, ArrSz(temporaryChunk.checksum));
                        BuildPool(DatabaseModel::Chunk, chunkPool, Checksum, _C::Equal(chunkChecksum));
                        if (chunkPool.count)
                        {
                            // Seems like the chunk already exist on the system, so there's no point in backing it up again.
                            chunkList.ChunkID = chunkPool[0].ID;
                            chunkList.Offset = streamOffset;
                            if (!hasData)
                            {
                                chunkList.ID = Helpers::allocateChunkList();
                                hasData = true;
                            }
                            chunkList.synchronize(); // This insert the object in the database
                        }
                        else
                        {
                            // The chunk does not exist, so let's append to the current multichunk, and create an entry for it
                            if (!multiChunk.canFit(temporaryChunk.size))
                            {
                                // Close this multichunk, and apply filters
                                if (!Helpers::closeMultiChunk(backupTo, multiChunk, multiChunkListID, &totalOutSize))
                                    return false;
                                // Allocate a new chunk list
                                multiChunkListID = 0;
                            }
                            // Append to the current multichunk
                            size_t offsetInMC = multiChunk.getSize();
                            uint8 * chunkBuffer = multiChunk.getNextChunkData(temporaryChunk.size, temporaryChunk.checksum);
                            if (!chunkBuffer)
                                return false;
                            memcpy(chunkBuffer, temporaryChunk.data, temporaryChunk.size);
                            
                            // Then add to the chunk list
                            DatabaseModel::Chunk chunk;
                            chunk.Checksum = chunkChecksum;
                            chunk.Size = temporaryChunk.size;
                            chunk.ID = Database::LongIndex::WantNewIndex;
                            
                            chunkList.ChunkID = chunk.ID;
                            chunkList.Offset = streamOffset;
                            if (!hasData)
                            {
                                chunkList.ID = Helpers::allocateChunkList();
                                hasData = true;
                            }
                            chunkList.synchronize(); // Insert in the database
                            Assert(streamOffset + temporaryChunk.size == stream.currentPosition());
                            streamOffset = stream.currentPosition();
                            
                            // And remember in which multichunk it is in too
                            multiChunkList.Type = 1;
                            multiChunkList.ChunkID = chunk.ID;
                            multiChunkList.Offset = (uint64)offsetInMC;
                            if (!multiChunkListID)
                                multiChunkListID = Helpers::allocateChunkList();
                            multiChunkList.ID = multiChunkListID;
                                
                            multiChunkList.synchronize();
                        }
                    }
                    
                    
                    // Ok, done with synchronization, insert in DB
                    if (hasData)
                        chunkList.synchronize();
                    
                    DatabaseModel::Entry file;
                    file.ChunkListID = hasData ? chunkList.ID : 0;
                    file.ParentEntryID = parentDirID;
                    file.Metadata = metadata;
                    file.Path = strippedFilePath;
                    file.Revision = revID;
                    file.Type = 0;
                    file.State = 0;
                    file.ID = Database::Index::WantNewIndex;
                    
                    transaction.shouldCommit();
                    fileCount++;
                }
                else
                {
                    if (!callback.warn(ProgressCallback::Backup, info.name, TRANS("Non regular type (fifo, pipe or socket) are not backed up.")))
                        return false;
                }
            }
            return callback.progressed(ProgressCallback::Backup, info.name, 0, 0, seen, total);
        }
        
        /** Finish the current multichunk, as it's the end of the backup process */
        bool finishMultiChunk()
        {
            // Check if we started a multichunk (need to close it in that case)
            if (multiChunk.getSize())
            {
                Assert(multiChunkListID);
                if (!Helpers::closeMultiChunk(backupTo, multiChunk, multiChunkListID, &totalOutSize))
                    return false;
            }
            // Save the statistics
            DatabaseModel::Revision rev;
            rev.FileCount = fileCount;
            rev.DirCount = dirCount;
            rev.InitialSize = totalInSize;
            rev.BackupSize = totalOutSize;
            rev.ID = revID;
            
            // If we found something to analyze, then the backup worked
            if (totalInSize) Frost::backupWorked = true;
            
            return true;
        }
        
        BackupFile(ProgressCallback & callback, const String & backupTo, const unsigned int revID, const String & rootFolder)
            : callback(callback), backupTo(backupTo),
              folderToBackup(rootFolder.normalizedPath(Platform::Separator, true)), revID(revID), seen(0), total(1), 
              fileCount(0), dirCount(0), totalInSize(0), totalOutSize(0),
              multiChunkListID(0), prevParentFolder("*")
        {}
    };
    
    
    class RestoreFile
    {
        ProgressCallback & callback;
        const String & folderTrimmed;
        const String backupFolder;
        
        OverwritePolicy overwritePolicy;
    
    public:
        
        /** Restore a single file from the database
            @return 0 on success, -1 on error, 1 on warning */
        int restoreFile(const DatabaseModel::Entry & file, String & errorMessage, const uint32 current, const uint32 total)
        {
    #define WarnAndReturn(Msg) callback.warn(ProgressCallback::Restore, file.Path, TRANS( Msg )) ? 1 : -1
    #define ERR(Msg) { errorMessage = Msg; return -1; }
            // We need to figure out if the file is a regular file, we do so by checking its metadata field
            File::Info outFile(folderTrimmed + file.Path);
            if (!outFile.analyzeMetaData(file.Metadata))
            {
                errorMessage = TRANS("Bad metadata found in database");
                return WarnAndReturn("Bad metadata for this file, it's ignored for restoring");
            }

            if (!callback.progressed(ProgressCallback::Restore, folderTrimmed + file.Path, 0, outFile.size, current, total))
                ERR(TRANS("Interrupted in output"));
             
             
            // Check if the file was deleted in the backup
            if (file.State == 1)
            {
                if (!outFile.doesExist()) return 0; // Ignore deleted file that does not exists on the system.
                
                // We need to delete the file from the system, depending on the overwrite policy
                if (overwritePolicy == No)
                    return WarnAndReturn("This file already exists and is deleted in the backup, and no overwrite specified");
                if (overwritePolicy == Update && outFile.modification < File::Info(outFile.getFullPath()).modification)
                    return WarnAndReturn("This file already exists in the restoring folder and is newer than the backup which is deleted");
                    
                if (!File::Info(outFile.getFullPath()).remove())
                    ERR(TRANS("Can not remove file on the system: ") + file.Path);
                    
                return 0;
            }
            // Check if the file already exists
            if (outFile.doesExist())
            {
                if (file.Metadata != File::Info(outFile.getFullPath()).getMetaData())
                {
                    switch(overwritePolicy)
                    {
                    case No: return WarnAndReturn("This file already exists and is different in the restoring folder, and no overwrite specified");
                    case Update:
                        if (outFile.modification < File::Info(outFile.getFullPath()).modification)
                            return WarnAndReturn("This file already exists in the restoring folder and is newer than the backup");
                        break;
                    case Yes:
                    default:
                        break;
                    }
                }
            }

            // Ok, now check if it's a regular file that need its content to be restored
            if (outFile.isFile())
            {
                // Seem so, let's restore it now.
                
                // The initial approach that used to work was doing multiple request per chunk and as such, was very slow.
                // The new approach is using the filtering capabilities of the SQL engine to perform a single request with all informations included.
                // Basically, we'll be joining the ChunkList table for the File's chunk and the MultiChunk's chunk, and joining the Multichunk table too to get the
                // MultiChunk path to read from. This will also simplify caching the multichunk in a later stage
                // The request will look like this:
                // SELECT a.ID AS ID, a.ChunkID AS ChunkID, a.Offset AS MultichunkOffset, b.Offset AS FileOffset, c.ID AS MultichunkID, c.FilterListID AS FilterListID, c.FilterArgument AS FilterArgument, c.Path AS Path FROM ChunkList a
                // INNER JOIN ChunkList b ON a.ChunkID = b.ChunkID
                // INNER JOIN MultiChunk c ON a.ID = c.ChunkListID
                // WHERE b.ID = file.ChunkListID AND a.Type = 1
                RowIterT iter = (((((Select().Alias("a.ID", "ID").Alias("a.ChunkID", "ChunkID").Alias("a.Offset", "MCOffset")
                                             .Alias("b.Offset", "FileOffset")
                                             .Alias("c.ID", "MCID").Alias("c.FilterListID", "FilterListID").Alias("c.FilterArgument", "FilterArgument").Alias("c.Path", "MCPath")
                                             .Alias("d.Checksum", "Checksum")
                                             .From("ChunkList a")
                                             .InnerJoin("ChunkList b").On("a.ChunkID") == _U("b.ChunkID"))
                                             .InnerJoin("MultiChunk c").On("a.ID") == _U("c.ChunkListID"))
                                             .InnerJoin("Chunk d").On("a.ChunkID") == _U("d.ID"))
                                             .Where("b.ID") == file.ChunkListID).And("a.Type") == 1).OrderBy("FileOffset", true);
                
                
                ::Stream::OutputFileStream stream(outFile.getFullPath());
                
                // @todo: Provide some better caching of decompressed data
                File::MultiChunk mchunk;
                
                while (iter)
                {
                    // Check for the filter ID
                    if (iter["FilterListID"] != "3")
                    {
                        errorMessage = TRANS("Unknown filter ID");
                        return WarnAndReturn("Unknown filter ID");
                    }
                    
                    // Extract a chunk out of this multichunk
                    File::Chunk * chunk = Helpers::extractChunk(errorMessage, backupFolder, iter["MCPath"], (uint64)(int64)iter["MCID"], (size_t)(int64)iter["MCOffset"], iter["Checksum"], iter["FilterArgument"], &mchunk);
                    if (!chunk || errorMessage) return -1;
                    
                    // Ok, if we got a chunk, let's save it
                    if (!chunk)
                        ERR(TRANS("Missing chunk for this file: ") + (String)iter["ChunkID"]);
                    if (stream.write(chunk->data, chunk->size) != (uint64)chunk->size)
                        ERR(TRANS("Can't write the file (disk full ?)"));
                        
                    if (!callback.progressed(ProgressCallback::Restore, folderTrimmed + file.Path, stream.currentPosition(), outFile.size, current, total))
                        ERR(TRANS("Interrupted in output"));
                    
                    // Select next row
                    ++iter;
                }
            }
            
            // Then restore the metadata here
            if (!outFile.setMetaData(file.Metadata))
            {
                errorMessage = TRANS("Failed to restore metadata");
                return WarnAndReturn("Failed to restore the file's metadata");
            }
    #undef WarnAndReturn
    #undef ERR
            return 0;
        }
        
        RestoreFile(ProgressCallback & callback, const String & folderTrimmed, const String & backupFolder, OverwritePolicy policy)
            : callback(callback), folderTrimmed(folderTrimmed), backupFolder(backupFolder.normalizedPath(Platform::Separator, true)),
              overwritePolicy(policy)
        {}
    };
    
    // Backup the given folder 
    String backupFolder(const String & folderToBackup, const String & backupTo, const unsigned int revisionID, ProgressCallback & callback)
    {
        // The complete logic is here
        
        // Step one, we'll make a stack of data to find out what to backup
        if (!callback.progressed(ProgressCallback::Backup, TRANS("...scanning..."), 0, 1, 0, 1))
            return TRANS("Error with output");
        File::FileItemArray items; File::Scanner::FileFilters filters;
        BackupFile processor(callback, backupTo, revisionID, folderToBackup);
        // Initiate the pump
        File::Info rootFolder(folderToBackup, true);
        processor.fileFound(rootFolder, PathSeparator);
        File::Scanner::EventIterator iterator(true, processor);
        
        if (File::Scanner::scanFolderGeneric(folderToBackup, ".", items, iterator, false))
            return TRANS("Can't scan the backup folder");
        
        if (!processor.finishMultiChunk())
            return TRANS("Can't close the last multichunk");
        
        return "";
    }
    

    
    // List available backups
    struct CompareStringPath
    {
        static inline int compareData(const String & _first, const String & _second)
        {
            String first = _first.fromFirst("Z /"); String second = _second.fromFirst("Z /");
            if (!first || !second) { first = _first; second = _second; } // No metadata ?
            int ret = memcmp(first.getData(), second.getData(), min(first.getLength(), second.getLength()));
            if (ret) return ret;
            return first.getLength() < second.getLength() ? -1 : 1;
        }
    };
    unsigned int listBackups(const ::Time::Time & startTime, const ::Time::Time & endTime, const bool withList)
    {
        BuildPool(DatabaseModel::Revision, pool, TimeSinceEpoch, _C::Between((uint64)startTime.asNative(), (uint64)endTime.asNative()));
        if (!pool.count)
            fprintf(stdout, "%s", (const char*)TRANS("No revision found\n"));
        else
        {
            for (unsigned int i = 0; i < pool.count; i++)
            {
                if ((uint64)pool[i].InitialSize)
                    fprintf(stdout, (const char*)TRANS("Revision %d happened on %s, linked %d files and %d directories, cumulative size %s (backup is %s, saved %d%%)\n"), (int)pool[i].ID, (const char*)(String)pool[i].RevisionTime,
                                             (uint32)pool[i].FileCount, (uint32)pool[i].DirCount, (const char*)makeLegibleSize(pool[i].InitialSize),
                                             (const char*)makeLegibleSize(pool[i].BackupSize), 100 - (100 * (uint64)pool[i].BackupSize) / (uint64)pool[i].InitialSize);
                else
                    fprintf(stdout, (const char*)TRANS("Revision %d happened on %s, linked %d files and %d directories, cumulative size %s (backup is %s, saved 100%%)\n"), (int)pool[i].ID, (const char*)(String)pool[i].RevisionTime,
                                             (uint32)pool[i].FileCount, (uint32)pool[i].DirCount, (const char*)makeLegibleSize(pool[i].InitialSize),
                                             (const char*)makeLegibleSize(pool[i].BackupSize));
                                             
                // Then make a list of files for this revision
                PathIDMapT fileList;
                if (withList && createFileListInRev(fileList, pool[i].ID))
                {
                    Strings::StringArray filePaths;
                    // Prepare the file list in the format we like
                    PathIDMapT::IterT iter = fileList.getFirstIterator();
                    while (iter.isValid())
                    {
                        String md = (*iter)->getMetaData();
                        String metaData = File::Info::printMetaData(md);
                        if (metaData)
                        {
                            filePaths.Append(String::Print(TRANS("%s %s [rev%u:id%u]"), (const char*)metaData, (const char*)*(iter.getKey()), (unsigned int)pool[i].ID, (unsigned int)*(*iter)));
                        }
                        else
                            filePaths.Append(String::Print(TRANS("%s [rev%u:id%u]"), (const char*)*(iter.getKey()), (unsigned int)pool[i].ID, (unsigned int)*(*iter)));
                        ++iter;
                    }
                    // Sort the file list

                    CompareStringPath cs;
                    Container::Algorithms<Strings::StringArray>::sortContainer(filePaths, cs);
                    // Show it
                    for (size_t j = 0; j < filePaths.getSize(); j++)
                        fprintf(stdout, "\t%s\n", (const char*)filePaths[j]);
                }
            }
        }
        return (unsigned int)pool.count;
    }
    
    // Purge old backups
    String purgeBackup(const String & chunkFolder, ProgressCallback & callback, const PurgeStrategy strategy, const unsigned int upToRevision)
    {
        // First, we need to figure out the chunks that are in the given revision but in no other revision (orphans)
        if (!callback.progressed(ProgressCallback::Purge, TRANS("...scanning..."), 0, 1, 0, 1))
            return TRANS("Error with output");
            
            
        // This block is there to show that a transaction is around all these operations
        {
            // On any error, the operations below will be aborted
            Database::Transaction transaction;
            
            // First find orphan chunks (chunk that refers to file not anymore in the wanted revision range)
            
            // We are building two sets
            // Let's take the following example database as a base of thought
            /*
               Rev 1:
                 Modified /a
                 Modified /b
                 Modified /c
                 Modified /d
                
               Rev 2:
                 Modified /a
                 Deleted  /b
                 (no change) /c
                 (no change) /d
                 
                 
               Rev 3:
                 Deleted  /a
                 Modified /b (new file, but same name)
                 (no change) /c
                 (no change) /d
              
               Rev 4:
                 Deleted  /c
                 (no change) /d
                 Modified /e
            */
            // 1) The deleted set:
            // For each file in the database (distinct by path), select the highest revision that's above the given revision range where the file was deleted.
            // In the increasing revision order, as soon as a file is not deleted anymore, we stop the selection.
            // In the previous example, if the given revision limit is 2, the deleted set should contain:
            //  /a[1] /a[2] /a[3] /b[1] /b[2]
            // This is because at revision 3, the only remaining files are (new) /b and /c /d
            // Please notice that even if the given revision limit is 2, we have to remove /a from revision 3, but not /c from revision 4 as it would break
            // rev 3 integrity
            const Select deletedSet = Select().Distinct("ID").From(
                                    (((((Select().Alias("a.ID", "ID").From("Entry a").InnerJoin("Entry b").On("a.Path") == _U("b.Path")).And("b.Revision") < _U("a.Revision"))
                                               .Where("a.Revision") <= (upToRevision+1)).And("a.State") == 1).And("a.Type") == 0)
                                                             .UnionAll(
                                    (((((Select().Alias("b.ID", "ID").From("Entry a").InnerJoin("Entry b").On("a.Path") == _U("b.Path")).And("b.Revision") < _U("a.Revision"))
                                               .Where("a.Revision") <= (upToRevision+1)).And("a.State") == 1).And("a.Type") == 0)));
            
            // Remember this to avoid having such a huge query called multiple time
            CreateTempTable deletedEntryTable = CreateTempTable("DeletedSet", true).As(Select("*").From("Entry").Where("ID").In(deletedSet));

            
            // 2) The remaining set:
            // For each file in the database (distinct by path), select the lowest revision that's above a revision were the file was deleted.
            // In the previous example, if the given revision limit is 2, the remaining set should contain:
            //  /c[1] /b[3] /d[1] /e[4]
            // Please notice that even if the given revision limit is 2, we have to keep /c and /d from revision 1
            const Select remainingSet = Select("ChunkListID").From("Entry").Where("ID").NotIn(deletedSet).And("State") == 0;
    /*
                
            
            // File to purge are those below the given revision (whatever state), plus those who are deleted in any
            // later revision (but not modified since the given revision)
            // This is very tricky to select, so we split the selection in small part (easier to understand)
            // SELECT * FROM Entry WHERE (   ID IN (SELECT a.ID FROM Entry a INNER JOIN Entry b ON a.Path = b.Path AND a.Type = 0 AND a.State = 1 WHERE b.State != a.State AND b.Revision < a.Revision AND a.Revision > 1)
            //                           OR ID IN(SELECT a.ID FROM Entry a INNER JOIN Entry b ON a.Path = b.Path AND a.Type = 0 AND a.State = 1 WHERE b.State != a.State AND b.Revision > a.Revision AND a.Revision > 1))
            //                        AND ID NOT IN(SELECT a.ID FROM Entry a INNER JOIN Entry b ON a.Path = b.Path AND a.Type = 0 AND a.State = 1 WHERE b.State != a.State AND b.Revision < a.Revision AND b.Revision > 1);
            const Select allDelFiles = (((((Select("a.ID").From("Entry a").InnerJoin("Entry b").On("a.Path") == _U("b.Path")).And("a.Type") == 0).And("a.State") == 1)
                                         .Where("b.State") != _U("a.State")).And("b.Revision") < _U("a.Revision")).And("a.Revision") > upToRevision; // If there is a deleted file, it'll be listed here
            const Select allNewFiles = (((((Select("a.ID").From("Entry a").InnerJoin("Entry b").On("a.Path") == _U("b.Path")).And("a.Type") == 0).And("a.State") == 1)
                                         .Where("b.State") != _U("a.State")).And("b.Revision") > _U("a.Revision")).And("a.Revision") > upToRevision; // This will list only deleted file where the same file is recreated in a later revision
            const Select notThoseFiles = (((((Select("a.ID").From("Entry a").InnerJoin("Entry b").On("a.Path") == _U("b.Path")).And("a.Type") == 0).And("a.State") == 1)
                                         .Where("b.State") != _U("a.State")).And("b.Revision") < _U("a.Revision")).And("b.Revision") > upToRevision; // This will list new file after the expected revision that were deleted in a later revision
                                        
            const Select deletedFiles = Select("ChunkListID").From("Entry").Where("(ID").In(allDelFiles).Or("ID").In(allNewFiles).eP()
                                             .And("ID").NotIn(notThoseFiles);
                                             
            // We are doing complex stuff here, so we are using the low level database selection code here
            const Select purgeFiles = (Select("ChunkListID").From("Entry").Where("Revision") <= upToRevision).And("Type") == 0;
            // We are only interested in the chunk, so this selection is enough
            const Select keepFiles = ((Select("ChunkListID").From("Entry").Where("Revision") > upToRevision).And("Type") == 0).And("State") == 0;

            const Select purgeChunkList = Select("ChunkID").From("ChunkList").Where("ID").In(purgeFiles).Or("ID").In(deletedFiles);
            const Select keepChunkList = Select("ChunkID").From("ChunkList").Where("ID").In(keepFiles);
            const Select purgeChunks = Select("ID").From("Chunk").Where("ID").In(purgeChunkList).And("ID").NotIn(keepChunkList);
      */
            const Select purgeChunkList = Select("ChunkID").From("ChunkList").Where("ID").In(Select("ChunkListID").From("Entry").Where("ID").In(deletedSet));
            const Select keepChunkList = Select("ChunkID").From("ChunkList").Where("ID").In(remainingSet);
            const Select purgeChunks = Select("ID").From("Chunk").Where("ID").In(purgeChunkList).And("ID").NotIn(keepChunkList);
            
            // Count the likely orphans
            int likelyOrphansChunks = purgeChunks.getCount();
            if (!likelyOrphansChunks)
                return TRANS("No orphan chunks to purge");
            int allChunks = Select("*").From("Chunk").getCount();
            if (!callback.progressed(ProgressCallback::Purge, TRANS("... found likely orphans chunks ..."), 0, 0, likelyOrphansChunks, allChunks))
                return TRANS("Error with output");        
            
            // Find the multichunks that only have those chunks
            // Multichunks either use
            // 1) Only these chunks
            // 2) None of these chunks
            // 3) At least one of these chunks, but not only these
            // So, if we select all Multichunks that are using those chunks, we'll get case 1 and 3 above
            // If we select all Multichunks that are not using those chunks, we'll get case 2 and 3 above
            // Finally, to get only case 1, we have to interset both request above (request 1 and not request 2)
            const Select usingOrphans = (Select().Distinct("ID").From("ChunkList").Where("ChunkID").In(purgeChunks).And("Type") == 1).And("ID").IsNotNull();  // Case 3 and 1
            const Select notUsingOrphans = (Select().Distinct("ID").From("ChunkList").Where("ChunkID").NotIn(purgeChunks).And("Type") == 1).And("ID").IsNotNull(); // Case 2 and 3

            const Select orphansMC = Select("*").From("MultiChunk").Where("ChunkListID").In(usingOrphans).And("(ChunkListID").NotIn(notUsingOrphans).Or(notUsingOrphans).IsNull().eP();
            // Remember them, because once we'll have deleted them, we will not be able to find them anymore
            CreateTempTable orphansMCTable = CreateTempTable("OrphansMultiChunk", true).As(orphansMC);
            
            Database::Pool<DatabaseModel::MultiChunk> orphanMultichunks = orphansMC;
            
            if (!callback.progressed(ProgressCallback::Purge, TRANS("... found orphans multichunks ..."), 0, 0, 0, orphanMultichunks.count))
                return TRANS("Error with output");
                

            // Then purge the orphans multichunks
            String chunkRoot = File::Info(chunkFolder.normalizedPath(Platform::Separator, true), true)
                                       .getFullPath().normalizedPath(Platform::Separator, true);   // Expand environnement variables
            uint64 purgedSize = 0;
            for (unsigned int i = 0; i < orphanMultichunks.count; i++)
            {
                if (!callback.progressed(ProgressCallback::Purge, orphanMultichunks[i].Path, 0, 0, i, orphanMultichunks.count))
                    return TRANS("Error with output");
                File::Info multichunk(chunkRoot + orphanMultichunks[i].Path);
                purgedSize += multichunk.size;
                if (!multichunk.remove())
                {
                    if (!callback.warn(ProgressCallback::Purge, orphanMultichunks[i].Path, TRANS("Can not remove this multichunk")))
                        return TRANS("Can not remove a multichunk");
                }
            }
           
            // Then delete the orphans chunks from database
            
            // Show some log
            const Select reallyOrphans = Select("ID").From("Chunk").Where("ID").In(Select("ChunkID").From("ChunkList").Where("ID").In(orphansMC.Refine("ChunkListID")));
            int reallyOrphansCount = reallyOrphans.getCount();
            if (!callback.progressed(ProgressCallback::Purge, TRANS("... deleting really orphans chunks ..."), 0, 0, reallyOrphansCount, allChunks))
                return TRANS("Error with output");
            
            // Then delete them from the database
            reallyOrphans.Delete();
            
            // Find out the orphan directories too
            // These are the directories that are referred by the orphans files & directories, and only those
            const Select orphanDirs = (Select("*").From("Entry").Where("Type") == 1)
                                           .And("ID").In((Select("ParentEntryID").From("Entry").Where("Revision") <= (upToRevision+1)).And("State") == 1)
                                           .And("ID").NotIn((Select("ParentEntryID").From("Entry").Where("Revision") > upToRevision).And("State") == 0);
            
            orphanDirs.Delete();
            // Delete the multichunks from the database
            Delete().From("MultiChunk").Where("ID").In(Select("ID").From("OrphansMultiChunk")).execute();
            // Delete the useless chunk list now
            Delete().From("ChunkList").Where("ID").In(Select("ChunkListID").From("OrphansMultiChunk"))
                                          .Or("ID").In(Select("ChunkListID").From("DeletedSet")).execute();
                                          
            // Then removes the files themselves
            Delete().From("Entry").Where("ID").In(Select("ID").From("DeletedSet")).execute();
            
            // Now enter the slow mode if requested
            if (strategy == Slow)
            {
                // On any error, we'll commit what we did before
                transaction.shouldCommit(true);
                
                // The idea here is to find out all orphans chunks, and from there, find out in which multichunk they still appear.
                // Then, for each multichunk with orphans chunks, we'll extract the good chunks and create another multichunk made of those.
                // The ending multichunks will be replaced in the DB.
                
                // 1) Find orphans chunks
                const Select orphanChunks = Select("ID").From("Chunk").Where("ID").NotIn(
                                                   Select("ChunkID").From("ChunkList").Where("ID").In(
                                                      Select("ChunkListID").From("Entry").Where("Type") == 0));
                
                int finalOrphanChunks = orphanChunks.getCount();
                if (!finalOrphanChunks)
                    return TRANS("No more orphan chunks to purge");
                if (!callback.progressed(ProgressCallback::Purge, TRANS("... found remaining orphans chunks ..."), 0, 0, finalOrphanChunks, allChunks))
                    return TRANS("Error with output");
                    
                // Figure out who's linking them
                const Select multiChunkWithOrphans = (Select("*").From("ChunkList").Where("ChunkID").In(orphanChunks).And("Type") == 1).OrderBy("ID", true);
                
                // Then for each different multichunk that has orphans.
                // In order to be interruptible, we'll sort them based on the number of orphans chunk, so we'll rewrite the chunks with
                // the highest number of orphans chunks first, as the gain will be higher
                Database::Pool<DatabaseModel::ChunkList> chunkListWithOrphans = multiChunkWithOrphans;
                typedef Tree::AVL::Tree<unsigned int, float> MultiChunkTreeT;
                MultiChunkTreeT amountRatio;
                unsigned int previousChunkListID = 0; unsigned int tmpCount = 0;
                for (unsigned int i = 0; i < chunkListWithOrphans.count; i++)
                {
                    tmpCount ++;
                    if (previousChunkListID != chunkListWithOrphans[i].ID)
                    {
                        if (!previousChunkListID)
                        {
                            previousChunkListID = chunkListWithOrphans[i].ID;
                            continue;
                        }
                        int chunksInMultiChunk = (Select("*").From("ChunkList").Where("ID") == previousChunkListID).getCount();
                        amountRatio.insertObject(previousChunkListID, 1.0f - (float)(tmpCount - 1) / (float)chunksInMultiChunk); // Make sure the higher is the lower

                        previousChunkListID = chunkListWithOrphans[i].ID;
                        tmpCount = 1;
                    }
                }
                if (previousChunkListID)
                {
                    int chunksInMultiChunk = (Select("*").From("ChunkList").Where("ID") == previousChunkListID).getCount();
                    amountRatio.insertObject(previousChunkListID, 1.0f - (float)tmpCount / (float)chunksInMultiChunk); // Make sure the higher is the lower
                }
                
                // Then from the lowest one, let's start making new multichunk
                MultiChunkTreeT::SortedIterT iter = amountRatio.getFirstSortedIterator();
                uint64 consumedOutSize = 0;
                File::MultiChunk cache, newOne;
                unsigned int newChunkListID = 0;
                int cleanedCount = 0;
                while (iter.isValid())
                {
                    if (!callback.progressed(ProgressCallback::Purge, TRANS("Processing multichunk"), 0, 0, cleanedCount+1, amountRatio.getSize()))
                        return TRANS("Error with output");
               
                    // Create a multichunk for it
                    DatabaseModel::MultiChunk mChunk;
                    if (!mChunk.ChunkListID.find((*iter)))
                        return TRANS("Can not find a multichunk for the specified ChunkList ID") + (*iter);
                            
                    DatabaseModel::ChunkList newChunkList;
                    // First we extract all valid chunks out of the multichunk
                    Database::Pool<DatabaseModel::ChunkList> multichunk = Select("*").From("ChunkList").Where("ChunkID").NotIn(orphanChunks).And("ID") == (*iter);
                    
                    String error;
                    for (unsigned int i = 0; i < multichunk.count; i++)
                    {
                        // Then we need to read the complete multichunk to get each chunk that's alive
                        // Find the chunk checksum to ensure about integrity
                        DatabaseModel::Chunk currentChunk;
                        currentChunk.ID = multichunk[i].ChunkID;
                        
                        // Read the chunk out of the multichunk
                        File::Chunk * localChunk = Helpers::extractChunk(error, chunkRoot, mChunk.Path, (uint64)(int64)mChunk.ID, (size_t)(uint64)multichunk[i].Offset, currentChunk.Checksum, mChunk.FilterArgument, &cache);
                        if (error || !localChunk)
                            return error;
                        
                        // Then we need to store it in a new multichunk
                        if (!newOne.canFit(localChunk->size))
                        {
                            // Close this multichunk
                            Helpers::closeMultiChunk(chunkRoot, newOne, newChunkListID, &consumedOutSize);
                            newChunkListID = 0;
                        }
                        // Append to current multichunk
                        size_t offsetInMC = newOne.getSize();
                        uint8 * chunkBuffer = newOne.getNextChunkData(localChunk->size, localChunk->checksum);
                        if (!chunkBuffer)
                            return TRANS("Can not allocate memory for storing the chunk: ") + (uint64)currentChunk.ID;
                        memcpy(chunkBuffer, localChunk->data, localChunk->size);
                        
                        newChunkList.ChunkID = currentChunk.ID;
                        newChunkList.Offset = (uint64)offsetInMC;
                        newChunkList.Type = 1;
                        if (!newChunkListID)
                            newChunkListID = Helpers::allocateChunkList();
                        newChunkList.ID = newChunkListID;
                        newChunkList.synchronize();
                    }
                    
                    // Finally delete the useless multichunk anymore
                    if (!callback.progressed(ProgressCallback::Purge, mChunk.Path, 0, 0, cleanedCount, amountRatio.getSize()))
                        return TRANS("Error with output");
                    File::Info multichunkFile(chunkRoot + mChunk.Path);
                    purgedSize += multichunkFile.size;
                    if (!multichunkFile.remove())
                    {
                        if (!callback.warn(ProgressCallback::Purge, mChunk.Path, TRANS("Can not remove this multichunk")))
                            return TRANS("Can not remove a multichunk");
                    }
                    // Remove the now useless multichunk out of the database
                    (Delete().From("MultiChunk").Where("ID") == mChunk.ID).execute();
                    // Remove the now useless chunklist ID out of the database too
                    (Delete().From("ChunkList").Where("ID") == (*iter)).execute();
                    
                    // Ok, get to the next multichunk to clean
                    ++iter;
                    ++cleanedCount;
                }
                
                // Check if we need to close the current multichunk
                if (newOne.getSize())
                {
                    Assert(newChunkListID);
                    if (!Helpers::closeMultiChunk(chunkRoot, newOne, newChunkListID, &consumedOutSize))
                        return TRANS("Can not close and save the last multichunk, data is now lost");
                }
                
                // Don't account for the new output in the purged size
                purgedSize -= consumedOutSize;
            }
            
            if (!callback.progressed(ProgressCallback::Purge, TRANS("... purge finished and saved ..."), 0, 0, purgedSize, purgedSize))
                return TRANS("Error with output");

            transaction.shouldCommit();
        }
        // Vacuum the database to get back the last few bytes
        Database::SQLFormat::optimizeTables(0);        
        return "";
    }
    
    // Restore a backup to the given folder
    String restoreBackup(const String & folderToRestore, const String & restoreFrom, const unsigned int revisionID, ProgressCallback & callback)
    {
        // The complete logic is here
        if (!callback.progressed(ProgressCallback::Restore, TRANS("...analysing backup..."), 0, 1, 0, 1))
            return TRANS("Error in output");
            
        OverwritePolicy overwritePolicy = No;
        
        String * overwrite = optionsMap["overwrite"];
        if (overwrite && *overwrite == "yes") overwritePolicy = Yes;
        if (overwrite && *overwrite == "update") overwritePolicy = Update;
        
  
        // We start by creating the directories
        String folderTrimmed = File::Info(folderToRestore.normalizedPath(Platform::Separator, true), true) // Expand env variable
                                   .getFullPath().normalizedPath(Platform::Separator, false);
        
        // We have to count the directories and file to restore, in order to display meaningful statistics
        PathIDMapT fileList;
        if (!createFileListInRev(fileList, revisionID))
            return TRANS("Can not get any file or directory from this revision");
            
          
        // Need to find out all the directories required for this revision, in order to restore them
        Database::Pool<DatabaseModel::Entry> dirPool = ((Select("*").From("Entry").Where("Revision") <= revisionID).And("Type") == 1).OrderBy("Path", true, "Revision", false);
        
        uint32 total = (uint32)fileList.getSize(), current = 0;
        String lastPath = "*";
        RestoreFile restore(callback, folderTrimmed, restoreFrom, overwritePolicy);
        unsigned int skip = 1;
        for (unsigned int i = 0; i < dirPool.count; i+= skip)
        {
            // Reinitialize the skip counter (used to avoid checking numerous times the same value)
            skip = 1;
            // Result is ordered by path (ascending), then revision (descending)
            if (dirPool[i].Path == lastPath) continue;
            
            lastPath = dirPool[i].Path;
            File::Info dir(folderTrimmed + lastPath);

            // Show the list of directories to restore
            if (!callback.progressed(ProgressCallback::Restore, folderTrimmed + lastPath, 0, 1, ++current, total))
                return TRANS("Interrupted in output");

            // Is the directory deleted in the database and not in the system ?
            if (dirPool[i].State == 1)
            {
                if (dir.doesExist())
                {
                    if (!dir.isDir())
                        return TRANS("This file is a directory in the backup, but an actual file on the system: ") + lastPath;
                        
                    String metadata = dirPool[i].Metadata;
                    if (overwritePolicy == No)
                        continue; // If the last revision deleted the directory, let's skip it
                    else if (overwritePolicy == Update)
                    {
                        File::Info outDir;
                        outDir.analyzeMetaData(metadata);
                    
                        if (outDir.modification <= File::Info(lastPath).modification)
                            continue;
                    }
                    // Removing the directory as specified in the policy
                    if (!File::Info(lastPath).remove())
                        return TRANS("Can not remove this directory on the system: ") + lastPath;
                }
                // Ok, applied the change, let's continue
                continue;
            }
            
                
            if (!dir.makeDir())
                return TRANS("Failed to create directory: ") + dir.getFullPath();
            
            if (!callback.progressed(ProgressCallback::Restore, folderTrimmed + lastPath, 0, 0, current, total))
                return TRANS("Interrupted in output");
                
            // Find all the files that are linked to this directory
            // Check the lower limit in the directory revision ID (if any)
            unsigned int lowerRevID = 0;
            while (skip + i < dirPool.count && dirPool[skip + i].Path == lastPath)
            {
                if (dirPool[skip + i].State == 1)
                {
                    lowerRevID = dirPool[skip + i].ID;
                    break;
                }
                skip++;
            }
            
            // Select all the directory that are still valid for this revision
            Select dirPossibility = ((Select("ID").From("Entry").Where("Revision") <= revisionID).And("Path") == lastPath).And("Revision") > lowerRevID;
            Database::Pool<DatabaseModel::Entry> filePool = ((Select("*").From("Entry").Where("Revision") <= revisionID).And("Type") == 0).And("ParentEntryID").In(dirPossibility).OrderBy("Path", true, "Revision", false);
            String lastFilePath = "*";
            for (unsigned int j = 0; j < filePool.count; j++)
            {
                String errorMessage;
                // We only care about the last revision for files and subdir
                if (filePool[j].Path != lastFilePath)
                {
                    current++;
                    if (restore.restoreFile(filePool[j], errorMessage, current, total) < 0)
                        return errorMessage;
                    lastFilePath = filePool[j].Path;
                }
            }
        }
        
        // Ok done restoring
        return "";
    }
}



#define DEFAULT_KEYVAULT  "~/.frost/keys"
using Frost::TRANS;

int showHelpMessage(const Frost::String & error = "")
{
    if (error) fprintf(stderr, "error: %s\n\n", (const char*)TRANS(error));
    
    printf("Frost (C) Copyright 2013 - Cyril RUSSO (This software is BSD licensed) \n");
    printf(TRANS("Frost is a tool used to efficiently backup and restore files to/from a remote\n"
           "place with no control other the remote server software.\n"
           "No warranty of any kind is provided for the use of this software.\n"
           "Current version: %d. \n\n"
           "Usage:\n"
           "  Actions:\n"
           "\t--restore dir [rev]\tRestore the revision (default: last) to the given directory (either backup or restore mode is supported)\n"
           "\t--backup dir\t\tBackup the given directory (either backup or restore mode is supported)\n"
           "\t--purge [rev]\t\tPurge the given remote backup directory up to the given revision number (use --list to find out)\n"
           "\t--list [range]\t\tList the current backup in the specified index (required) and time range in UTC (in the form 'YYYYMMDDHHmmSS YYYYMMDDHHmmSS')\n"
           "\t--filelist [range]\tList the current backup in the specified index (required) and time range in UTC, including the file list in this revision\n"
           "\t--test [name]\t\tRun the test with the given name -developer only- use -v for more verbose mode, 'help' to get a list of available tests\n"
           "\t--help [security]\tGet help on the security features and advices of Frost\n"
           "  Required parameters for backup, purge and restore:\n"
           "\t--remote url\t\tThe URL (can be a directory) to save/restore backup to/from\n"
           "\t--index path\t\tThe path to the index file that's used to store the backup's specific data. " DEFAULT_INDEX " is appended to this path, it defaults to remote_url\n"
           "\t--keyvault file\t\tPath to a file containing the private key used to decrypt/encrypt the backup data. Default to '" DEFAULT_KEYVAULT "'. If the key does not exist, it'll be created\n"
           "\t--keyid id\t\tThe key identifier if storing multiple keys in the key vault.\n"
           "  Optional parameters for backup and restore:\n"
           "\t--cache [sizeMB]\tThe cache size holding the decoded multichunks in MB (default is 64) - restore only\n"
           "\t--overwrite [policy]\tThe policy for overwriting/deleting files on the restore folder if they exists (either 'yes', 'no', 'update')\n"
           "\t--multichunk [sizeKB]\tWhile backing up, files are cut in variable sized chunk, and these chunks are concat in multichunk files (with size in KB) saved on the target (default is 250)\n"
           "\t                     \tIf you have a large amount of data to backup, a bigger number will create less files in the backup directory, the downside being that purging will take more time\n"
           "\t                     \tIf you backup often, and purge at regular interval, the default should allow fast restoring and purging\n"
           "\t--compression [bsc]\tYou can change the compression library to use (default is zlib). Using 'bsc' is faster than LZMA and gives better compression ratio.\n"
           "\t                     \tHowever, 'bsc' also changes the multichunk size to 10240.\n"
           "\t--strategy [mode]    \tThe purging strategy, 'fast' for removing lost chunk from database, but does not reassemble multichunks\n"
           "\t                     \t'slow' for rebuilding multichunks after fast pruning. This will save the maximum backup amount, at the price of much longer processing\n"
           ),
#include "build/build-number.txt"
    );
    return EXIT_SUCCESS;
}

int showSecurityMessage()
{
    printf("Frost (C) Copyright 2013 - Cyril RUSSO All right reserved \n");
    printf(TRANS("Frost is a tool used to efficiently backup and restore files to/from a remote\n"
           "place with no control other the remote server software.\n"
           "Current version: %d. \n\n"
           "Security advices and features:\n"
           "  Algorithm description:\n"
           "\tBy default, Frost is using AES256 symmetric encryption algorithm in counter mode.\n"
           "\tFrost splits each file in chunks of data, then concatenate each chunks in multichunk.\n"
           "\tWhen a multichunk is full, it's likely compressed, then encrypted with AES256_CTR\n"
           "\tThe key used for this encryption is derived from a master key (never saved) and a random\n"
           "\tvalue (called a salt) that's saved in the encrypted stream.\n"
           "\tThe master key is created on the first backup randomly, and protected by a password you\n"
           "\tmust supply for each operation. The (encrypted) master key is then saved in the keyvault file.\n\n"
           "  Security consideration:\n"
           "\tBy itself the keyvault file does not allow to decrypt a encrypted backup set. However, it's\n"
           "\tvulnerable to brute force attack on the password used to decrypt it.\n"
           "\tAs such, unless you trust the storage location for your backup, you should not save the keyvault\n"
           "\twith the backup storage location.\n"
           "\tConcerning the index file, it contains the link to all file name/path, size and metadata (like\n"
           "\towner, modification time...) in clear. It does not contains anything about your files content, but\n"
           "\tdepending on your paranoia, you might also want to avoid storing it along the backup data.\n\n"
           "  Performance consideration:\n"
           "\tFrost does not provide any facility to access a remote URL by itself (yet), but on numerous POSIX\n"
           "\tsystem, a userspace file-system facility (like FUSE) allows to access remote site directly via the\n"
           "\tfilesystem layer.\n"
           "\tIn that case, access to this remote mount point might prove slow. To optimize access and backup speed\n"
           "\tyou should keep the index database locally (either by transfering it before and after the process)\n"
           "\tThe keyvault is never modified by Frost after first backup, so you might as well leave it on a server\n"
           "\tor locally depending on your security concerns.\n"
           ),
#include "build/build-number.txt"
    );
    return EXIT_SUCCESS;
}

int checkTests(Strings::StringArray & options)
{
    // Check for test mode
    size_t optionPos = 0;
    Logger::ConsoleSink debugSink(~0);
    
    Frost::dumpState = options.indexOf("-v") != options.getSize();
    if (Frost::dumpState) Logger::setDefaultSink(&debugSink);
    
    if ((optionPos = options.indexOf("--test")) != options.getSize())
    {
        Strings::FastString testName = "key";
        if (optionPos + 1 != options.getSize()) testName = options[optionPos+1].Trimmed();
        
        // Run tests now
        if (testName == "help")
        {
            printf("Frost (C) Copyright 2013 - Cyril RUSSO All right reserved \n");
            printf(TRANS("Current version: %d. \n\nTest mode help:\n"
                   "\tkey\t\tTest cryptographic system, by creating a new vault, and master key, and reading it back\n"
                   "\tdb\t\tTest database code, by creating a default database, filling it and reading it back\n"
                   "\troundtrip\tTest a complete roundtrip backup and restore, of fake created file, with specific attributes\n"
                   "\tpurge\t\tTest an update to a previous roundtrip test, and purging the initial revision\n"),
#include "build/build-number.txt"
                   );
            return EXIT_SUCCESS;
        }
        else if (testName == "key")
        {
#define ERR(Msg, ...) { fprintf(stderr, ::Frost::__trans__(Msg), ##__VA_ARGS__); Database::SQLFormat::finalize((uint32)-1); return -1; }
            // We will create a file vault, and an fake index file, and try to load it again and compare
            File::Info("./testVault").remove();
            Frost::MemoryBlock cipheredMasterKey;
            Frost::String result = Frost::getKeyFactory().createMasterKeyForFileVault(cipheredMasterKey, "./testVault", "password");
            if (result) ERR("Creating the master key failed: %s\n", (const char*)result);
            
            // Try to read it back
            result = Frost::getKeyFactory().loadPrivateKey("./testVault", cipheredMasterKey, "password");
            if (result) ERR("Reading back the master key failed: %s\n", (const char*)result);

            fprintf(stderr, "Success\n");
            return EXIT_SUCCESS;
            
        }
        else if (testName == "db")
        {
            // Create a fake database URL
            Frost::DatabaseModel::databaseURL = "./";
            // Wipe out the database
            File::Info(Frost::DatabaseModel::databaseURL + DEFAULT_INDEX).remove();
            // And recreate it
            unsigned int revisionID = 0; Frost::MemoryBlock cipheredMasterKey;
            Frost::String result = Frost::initializeDatabase("test/", revisionID, cipheredMasterKey);
            if (result) ERR("Creating the database failed: %s\n", (const char*)result);

            if (!revisionID || cipheredMasterKey.getSize())
                ERR("Incoherent database bootstrapping: %d with initial key size %d\n", revisionID, cipheredMasterKey.getSize());

            // Ok, finalize the database
            Frost::finalizeDatabase();
            fprintf(stderr, "Success\n");
            return EXIT_SUCCESS;
        }
        else if (testName == "roundtrip")
        {
            // First create some specific files to save
            File::Info("./test/").remove();
            File::Info("./testBackup/").remove();
            File::Info("./testRestore/").remove();
            if (!File::Info("./testBackup/").makeDir()) ERR("Failed creating the backup folder ./testBackup/\n");
            if (!File::Info("./testRestore/").makeDir()) ERR("Failed creating the restoring folder ./testRestore/\n");
            if (!File::Info("./test/").makeDir()) ERR("Failed creating the test folder ./test/\n");
            
            {
                // Ok, then fill some content in some files
                if (!File::Info("./test/basicFile.txt").setContent("This is a very basic file content"))
                    ERR("Can't create basic file in the test directory");
                if (!File::Info("./ex/Hurt.txt").copyTo("./test/smallFile.txt"))
                    ERR("Can't copy lyric file in the test directory");
                if (!File::Info("./ex/RomeoAndJulietS2.txt").copyTo("./test/"))
                    ERR("Can't copy scene 2 file in the test directory");
                if (!File::Info("./ex/RomeoAndJulietS3.txt").copyTo("./test/"))
                    ERR("Can't copy scene 3 file in the test directory");
                if (!File::Info("./ex/TheMerchantOfVeniceA3S1.txt").copyTo("./test/"))
                    ERR("Can't copy scene 1 file in the test directory");

                
                File::Info filePerms("./test/fileWithPerms.txt");
                if (!filePerms.setContent("This is a file with some permissions"))
                    ERR("Can't create basic file with permissions in the test directory");
                if (!filePerms.setPermission(0700))
                    ERR("Can't set the file permissions for the test vectors");
                
                if (!File::Info("./test/symLink.txt").createAsLinkTo("basicFile.txt"))
                    ERR("Can't create a symbolic link to the basic file");
                if (!File::Info("./test/subDir").makeDir())
                    ERR("Can't create a subdirectory");
                if (!File::Info("./test/subDir/hardLink.txt").createAsLinkTo("./test/fileWithPerms.txt", true))
                    ERR("Can't create a hard link to the permission file");
                    
                // Test a big file (32MB) with some redundancy to check for deduplication
                Stream::OutputFileStream stream("./test/bigFile.bin");
                Frost::MemoryBlock bigFile;
                for (int i = 0; i < 16 * 1024; i++)
                {
                    uint8 randomData[1024];
                    Random::fillBlock(randomData, ArrSz(randomData), i==0);
                    bigFile.Append(randomData, ArrSz(randomData));
                }
                bigFile.Append(bigFile.getConstBuffer() + 3, bigFile.getSize() - 3);
                    
                if (stream.write(bigFile.getConstBuffer(), bigFile.getSize()) != (uint64)bigFile.getSize())
                    ERR("Can't fill the big file");
            }
            
            // Then, let backup this!
            Frost::ConsoleProgressCallback console;
            // Create a new key
            File::Info("./testBackup/keyVault").remove();
            Frost::MemoryBlock cipheredMasterKey;
            Frost::String result = Frost::getKeyFactory().createMasterKeyForFileVault(cipheredMasterKey, "./testBackup/keyVault", "password");
            if (result) ERR("Creating the master key failed: %s\n", (const char*)result);
            
            // Create a database too
            Frost::DatabaseModel::databaseURL = "./testBackup/";
            // Wipe out the database
            File::Info(Frost::DatabaseModel::databaseURL + DEFAULT_INDEX).remove();
            // And recreate it
            unsigned int revisionID = 0;
            result = Frost::initializeDatabase("test/", revisionID, cipheredMasterKey);
            if (result) ERR("Creating the database failed: %s\n", (const char*)result);

            // Then backup the folder
            result = Frost::backupFolder("test/", "./testBackup/", revisionID, console);
            if (result) ERR("Can't backup the test folder: %s\n", (const char*)result);
            
            if (Frost::listBackups() != 1) ERR("Can't list the created backup\n");
            
            // Finalize the database and clean up all our stuff before restoring
            if (!cipheredMasterKey.Extract(0, cipheredMasterKey.getSize()))
                ERR("Can't reset the ciphered master key\n");
            
            /////////////////////////////// Restoring here /////////////////////////////////
            // Ok, let's re-open again all of stuff
            revisionID = 0;
            result = Frost::initializeDatabase("", revisionID, cipheredMasterKey);
            if (result) ERR("Can't re-open the database: %s\n", (const char*)result);
            if (!cipheredMasterKey.getSize()) ERR("Bad readback of the ciphered master key\n");
            
            result = Frost::getKeyFactory().loadPrivateKey("./testBackup/keyVault", cipheredMasterKey, "password");
            if (result) ERR("Reading back the master key failed: %s\n", (const char*)result);
            
            // Restore the backup for the given revision
            result = Frost::restoreBackup("./testRestore/", "./testBackup/", revisionID, console);
            if (result) ERR("Can't restore the backup: %s\n", (const char*)result);
            
            /////////////////////////////// Testing here ////////////////////////////////////
            // Compare the files
            system("diff -ur test testRestore > diffOutput.txt 2>&1");
            Frost::String output = File::Info("diffOutput.txt").getContent();
            if (output.getLength())
                ERR("Comparing failed: %s\n", (const char*)output);
            
            // Finalize the database
            Frost::finalizeDatabase();
            fprintf(stderr, "Success\n");
            return EXIT_SUCCESS;
            
        } else if (testName == "purge")
        {
            // Delete a big file to figure out what happens to the final directory
            File::Info("./test/bigFile.bin").remove();
            // Then backup, and purge the remaning
            unsigned int revisionID = 0;
            Frost::ConsoleProgressCallback console;
            // Create a new key
            Frost::MemoryBlock cipheredMasterKey;
            Frost::String result;
            
            // Check a database too
            Frost::DatabaseModel::databaseURL = "./testBackup/";
            
            result = Frost::initializeDatabase("test/", revisionID, cipheredMasterKey);
            if (result) ERR("Creating the database failed: %s\n", (const char*)result);

            result = Frost::getKeyFactory().loadPrivateKey("./testBackup/keyVault", cipheredMasterKey, "password");
            if (result) ERR("Reading back the master key failed: %s\n", (const char*)result);

            // Then backup the folder
            result = Frost::backupFolder("test/", "./testBackup/", revisionID, console);
            if (result) ERR("Can't backup the test folder: %s\n", (const char*)result);
            
            if (Frost::listBackups() != 2) ERR("This test needs to be run after a roundtrip test\n");
            
            // Then purge the database from the last revision
            result = Frost::purgeBackup("./testBackup/", console, Frost::Slow, 1);
            if (result) ERR("Can't purge the last backup: %s\n", (const char*)result);

            Frost::finalizeDatabase();
            fprintf(stderr, "Success\n");
            return EXIT_SUCCESS;
#undef ERR
        }
        else
        {
            showHelpMessage();
            return -1;
        }
    }
    return BailOut;
}

::Time::Time parseTime(const Strings::FastString & time)
{
    int year  = time.midString(0, 4);
    int month = time.midString(4, 2);
    int day   = time.midString(6, 2);
    int hour  = time.midString(8, 2);
    int min   = time.midString(10, 2);
    int sec   = time.midString(12, 2);
    
    return ::Time::Time(year ? year - 1900 : 0, month ? month - 1 : 0, day, hour, min, sec);
}

bool getOptionParameters(const Strings::StringArray & options, const Strings::FastString & option, Strings::StringArray & params)
{
    params.Clear();
    size_t optionPos = options.indexOf("--" + option);
    if (optionPos != options.getSize())
    {
        // Find the next option in the argument list
        size_t nextArg = options.lookUp("--", optionPos + 1);
        params = options.Extract(optionPos + 1, nextArg);
        return true;
    }
    return false;
}


int checkOption(Strings::StringArray & options, const Strings::FastString & option, bool numeric = false)
{
    Strings::StringArray param;
    if (getOptionParameters(options, option, param))
    {
        if (param.getSize() != 1) return showHelpMessage("Invalid number of argument");
        Strings::FastString * optionValue = new Strings::FastString(param[0].Trimmed());
        if (numeric && optionValue->invFindAnyChar("0123456789") != -1)
        {
            delete optionValue;
            return showHelpMessage(TRANS("Expecting numerical value for: ") + option);
        }
        optionsMap.storeValue(option, optionValue, true);
        return 1;
    }
    return -1;
}

int handleAction(Strings::StringArray & options, const Strings::FastString & action)
{
    Strings::StringArray params;
    if (!getOptionParameters(options, action, params)) return BailOut;
    
    if (!optionsMap["index"]) return showHelpMessage("Bad argument for " + action + ", index path missing");
    
    

    // Common database initialization code
    Frost::DatabaseModel::databaseURL = optionsMap["index"]->normalizedPath(Platform::Separator, true);
    if (!File::Info(Frost::DatabaseModel::databaseURL, true).doesExist()) return showHelpMessage("Bad argument for " + action + ", index path does not exists");

    Frost::MemoryBlock              cipheredMasterKey;
    unsigned int                    revisionID = 0;
    Frost::ConsoleProgressCallback  console;
#define ERR(Msg, ...) { fprintf(stderr, ::Frost::__trans__(Msg), ##__VA_ARGS__); ::Frost::finalizeDatabase(); return -1; }
    
    if (action == "list" || action == "filelist")
    {
        ::Time::Time startTime = ::Time::Epoch, endTime = ::Time::MaxTime;
 
        if (params.getSize() >= 2)
        {   // Both start and stop specified
            if (params[0].invFindAnyChar("0123456789") != -1) return showHelpMessage("Bad argument for start list time range");
            if (params[1].invFindAnyChar("0123456789") != -1) return showHelpMessage("Bad argument for end list time range");
            startTime = parseTime(params[0]);
            endTime = parseTime(params[1]);
        } else if (params.getSize() == 1)
        {   // Only end time specified
            if (params[0].invFindAnyChar("0123456789") != -1) return showHelpMessage("Bad argument for end list time range");
            endTime = parseTime(params[0]);
        }
        
        Frost::String result = Frost::initializeDatabase("", revisionID, cipheredMasterKey);
        if (result)
        {
            fprintf(stderr, "%s", (const char*)TRANS("Can't read or initialize the database:" + Frost::DatabaseModel::databaseURL + "/" DEFAULT_INDEX));
            fprintf(stderr, "%s", (const char*)result);
            return EXIT_FAILURE;
        }
        
        
        Frost::listBackups(startTime, endTime, action == "filelist");
        Frost::finalizeDatabase();
        return EXIT_SUCCESS;
    }
    
    // From now, all other actions require a password
    if (!optionsMap["remote"]) return showHelpMessage("Bad argument for " + action + ", remote missing (that's where the backup is saved)");
 
    Frost::String pass, result,
                  remote = optionsMap["remote"]->normalizedPath(Platform::Separator, true),
                  keyID = optionsMap["keyid"] ? *optionsMap["keyid"] : "";
    
    {
        // Ask for password
        char password[256];
        size_t passLen = ArrSz(password);
        if (!Platform::queryHiddenInput("Password:", password, passLen))
            ERR("Can't query a password, do you have a terminal or console running ?");
        
        pass = password;
        memset(password, 0, passLen);
    }
    
    
    if (action == "purge")
    {
        // Purge the directory now for useless chunks
        if (!optionsMap["remote"]) return showHelpMessage("Bad argument for purge, remote missing (that's where the backup is saved)");
        
        result = Frost::initializeDatabase("", revisionID, cipheredMasterKey);
        if (result) ERR("Can't re-open the database: %s\n", (const char*)result);
        if (!cipheredMasterKey.getSize()) ERR("Bad readback of the ciphered master key\n");
        
        result = Frost::getKeyFactory().loadPrivateKey(*optionsMap["keyvault"], cipheredMasterKey, pass, keyID);
        pass = ""; // Password is not required anymore, let's wipe it
        if (result) ERR("Reading back the master key failed (bad password ?): %s\n", (const char*)result);
        
        // Purge the backup up to the given revision
        if (params[0] && (int)params[0]) revisionID = params[0];
        else ERR("No revision ID given. I won't purge the complete backup set implicitely, purge aborted\n");
        
        Frost::PurgeStrategy strategy = optionsMap["strategy"] ? (*optionsMap["strategy"] == "slow" ? Frost::Slow : Frost::Fast) : Frost::Fast;
        result = Frost::purgeBackup(*optionsMap["remote"], console, strategy, revisionID);
        if (result) ERR("Can't purge the backup: %s\n", (const char*)result);

        Frost::finalizeDatabase();
        if (warningLog.getSize()) fputs((const char*)(warningLog.Join("\n")+"\n"), stderr);
 
        return EXIT_SUCCESS;
    }
    if (action == "backup")
    {
        // Ok, start the system now
        Frost::String backup = params[0].normalizedPath(Platform::Separator, true);
        if (!File::Info(backup, true).doesExist() || !File::Info(backup, true).isDir())
            return showHelpMessage("Bad argument for backup, the --backup parameter is not a folder");
            
        // Check if it's the first time the backup is made
        if (!Database::SQLFormat::initialize(DEFAULT_INDEX, Frost::DatabaseModel::databaseURL, "", "", 0))
            ERR("Can't initialize the database with the given parameters.");
        if (!Database::SQLFormat::checkDatabaseExists(0))
        {
            // Need to create it now
            result = Frost::getKeyFactory().createMasterKeyForFileVault(cipheredMasterKey, *optionsMap["keyvault"], pass, keyID);
            if (result) ERR("Creating the master key failed: %s\n", (const char*)result);
            result = Frost::initializeDatabase(backup, revisionID, cipheredMasterKey);
        } else
        {
            if (!File::Info(*optionsMap["keyvault"], true).doesExist())
                ERR("The database exists, but the keyvault does not. Either delete the database, either set the path to the keyvault\n");
                
            result = Frost::initializeDatabase(backup, revisionID, cipheredMasterKey);
            if (!result)
            {   // It exists already, so load the private key
                result = Frost::getKeyFactory().loadPrivateKey(*optionsMap["keyvault"], cipheredMasterKey, pass, keyID);
                if (result) ERR("Reading back the master key failed (bad password ?): %s\n", (const char*)result);
            }
        }

        pass = ""; // Password is not required anymore, let's wipe it
        if (result) ERR("Can't read or initialize the database: %s\n%s", (const char*)(Frost::DatabaseModel::databaseURL + "/" DEFAULT_INDEX), (const char*) result);
        
        // Then backup the folder
        result = Frost::backupFolder(backup, remote, revisionID, console);
        if (result) ERR("Can't backup the test folder: %s\n", (const char*)result);

        // Display some statistics
        Frost::DatabaseModel::Revision rev;
        rev.ID = revisionID;
        console.progressed(Frost::ProgressCallback::Backup, Frost::String::Print(Frost::__trans__("Finished: %s, (source size: %lld, backup size: %lld, %u files, %u directories)"),
                                            (const char*)backup, (uint64)rev.InitialSize, (uint64)rev.BackupSize, (uint32)rev.FileCount, (uint32)rev.DirCount), 1, 1, (uint32)rev.FileCount, (uint32)rev.FileCount);
        
        // Need to be called anyway
        Frost::finalizeDatabase();
        if (warningLog.getSize()) fputs((const char*)(warningLog.Join("\n")+"\n"), stderr);
        return EXIT_SUCCESS;
    }
    if (action == "restore")
    {
        result = Frost::initializeDatabase("", revisionID, cipheredMasterKey);
        if (result) ERR("Can't re-open the database: %s\n", (const char*)result);
        if (!cipheredMasterKey.getSize()) ERR("Bad readback of the ciphered master key\n");
        
        result = Frost::getKeyFactory().loadPrivateKey(*optionsMap["keyvault"], cipheredMasterKey, pass, keyID);
        pass = ""; // Password is not required anymore, let's wipe it
        if (result) ERR("Reading back the master key failed (bad password ?): %s\n", (const char*)result);
        
        // Restore the backup for the given revision
        if (params[1] && (int)params[1]) revisionID = params[1];
        result = Frost::restoreBackup(params[0], remote, revisionID, console);
        if (result) ERR("Can't restore the backup: %s\n", (const char*)result);

        // Need to be called anyway
        Frost::finalizeDatabase();
        if (warningLog.getSize()) fputs((const char*)(warningLog.Join("\n")+"\n"), stderr);
        return EXIT_SUCCESS;
    }
#undef ERR
    return BailOut;
}

namespace Database { String constructFilePath(String fullPath, const String & URL); }
struct ExitErrorCallback : public Database::DatabaseConnection::ClassErrorCallback
{
    virtual void databaseErrorCallback(Database::DatabaseConnection * connection, const uint32 index, const ErrorType & error, const Database::String & message)
    {
        const char * errorType[] = { "UNK", "RQT", "CON" };
        Logger::log(Logger::Error | Logger::Database, "DB ERROR(%d,%s): %s", index, errorType[(int)error], (const char*)message);
        Logger::log(Logger::Error | Logger::Database, "DB ERROR : Database path used: %s", (const char*)Database::constructFilePath(DEFAULT_INDEX, Frost::DatabaseModel::databaseURL));
        
        // Now exit the software
        Database::SQLFormat::finalize((uint32)-1);
        exit(EXIT_FAILURE);
    }
} exitErrorCallback;


int main(int argc, char ** argv)
{
    // Install callbacks for errors and logging
    Database::SQLFormat::setErrorCallback(exitErrorCallback);

    // Build the options array
    Strings::StringArray options(argv, (size_t)argc);
    if (options.getSize() < 2)
        return showHelpMessage();

    Frost::Helpers::compressor = Frost::Helpers::ZLib;
    
    // This also works for tests, so test it before entering any tests
    if (checkOption(options, "compression") == EXIT_SUCCESS) return EXIT_SUCCESS;
    // Check for bsc selection
    if (optionsMap["compression"] && *optionsMap["compression"] == "bsc")
    {   // Remember the compressor selected
        Frost::Helpers::compressor = Frost::Helpers::BSC;
        File::MultiChunk::setMaximumSize(10240 * 1024);
        optionsMap.storeValue("multichunk", new Strings::FastString("10240"), true);
    }
    
    // Test mode first
    int tested = checkTests(options);
    if (tested != BailOut) return tested == EXIT_SUCCESS ? EXIT_SUCCESS : EXIT_FAILURE;
    
    Strings::StringArray params;
    if (getOptionParameters(options, "help", params))
    {
        if (params.getSize() && params[0] == "security") return showSecurityMessage();
        return showHelpMessage();
    }
    
    // Optional first
    if (checkOption(options, "cache", true) == EXIT_SUCCESS) return EXIT_SUCCESS;
    if (checkOption(options, "overwrite") == EXIT_SUCCESS) return EXIT_SUCCESS;
    if (checkOption(options, "strategy") == EXIT_SUCCESS) return EXIT_SUCCESS;
    if (checkOption(options, "keyid") == EXIT_SUCCESS) return EXIT_SUCCESS;


    if (checkOption(options, "multichunk", true) == EXIT_SUCCESS) return EXIT_SUCCESS;
    
    if (optionsMap["multichunk"])
        File::MultiChunk::setMaximumSize(((uint32)*optionsMap["multichunk"]) * 1024);
    
    if (optionsMap["overwrite"]
        && *optionsMap["overwrite"] != "yes"
        && *optionsMap["overwrite"] != "no"
        && *optionsMap["overwrite"] != "update")
        return showHelpMessage("Bad argument for overwrite");
         
    if (optionsMap["strategy"]
        && *optionsMap["strategy"] != "slow"
        && *optionsMap["strategy"] != "fast")
        return showHelpMessage("Bad argument for strategy");

    int remoteOpt = checkOption(options, "remote");
    if (remoteOpt == EXIT_SUCCESS) return EXIT_SUCCESS;
    if (remoteOpt == 1) // Found a remote, set the default index position
        optionsMap.storeValue("index", new Strings::FastString(*optionsMap["remote"]));

    if (checkOption(options, "index") == EXIT_SUCCESS) return EXIT_SUCCESS;
    
    optionsMap.storeValue("keyvault", new Strings::FastString(DEFAULT_KEYVAULT));
    if (checkOption(options, "keyvault") == EXIT_SUCCESS) return EXIT_SUCCESS;



    // Test for actions now
    int ret = 0;
    if ((ret = handleAction(options, "list")) != BailOut) return ret;
    if ((ret = handleAction(options, "filelist")) != BailOut) return ret;
    if ((ret = handleAction(options, "purge")) != BailOut) return ret;
    if ((ret = handleAction(options, "backup")) != BailOut) return ret;
    if ((ret = handleAction(options, "restore")) != BailOut) return ret;
    
    return showHelpMessage("Either backup, purge or restore mode required");
}
