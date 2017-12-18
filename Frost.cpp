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
// We need the folder scanner too
#include "ClassPath/include/File/ScanFolder.hpp"
// We need the chunker too
#include "ClassPath/include/File/TTTDChunker.hpp"
// We need compression too
#include "ClassPath/include/Streams/CompressStream.hpp"
#include "ClassPath/include/Compress/BSCLib.hpp"
// We need loggers too
#include "ClassPath/include/Logger/Logger.hpp"
// We need StringMap too
#include "ClassPath/include/Hash/StringMap.hpp"

// The global option map
Strings::StringMap optionsMap;
// The warning log that's displayed on output
Strings::StringArray warningLog;
// Error code that's returned to bail out of int functions
const int BailOut = 26748;


namespace Frost
{

    bool wasBackingUp = false, backupWorked = false, dumpTimeRequired = false, exitRequired = false;
    bool safeIndex = false;
    int dumpLevel = 0; // This is the verbosity level
    unsigned int previousRevID = 0;

#ifdef _POSIX
    void asyncProcess(int signal)
    {
        static const char stopping[] = "\n|  Stopping, please wait...  |\n";
        switch (signal)
        {
        case SIGUSR2: dumpTimeRequired = true; return;
        case SIGINT: exitRequired = true; write(2, stopping, sizeof(stopping)); fsync(2); return;
        default: return;
        }
    }
#endif

    void debugMem(const uint8 * buffer, const uint32 len, const String & title = "")
    {
        if (dumpLevel < 2) return;
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

    void derivePassword(KeyFactory::KeyT & pwKey, const String & password)
    {
        // We need to derive the low-entropy password to build a Hash out of it, and use that to decrypt the private key
        // we have generated earlier.
        KeyFactory::PWKeyDerivFuncT hash;
        // Cat the password multiple time until it fit the required input size
        MemoryBlock inputPW(KeyFactory::BigHashT::DigestSize);
        inputPW.stripTo(0);
        while (inputPW.getSize() < KeyFactory::BigHashT::DigestSize)
            inputPW.Append(password, password.getLength() + 1); // Add 0 to differentiate "a" from "aa" or "aaa" etc...
        hash.Hash(inputPW.getConstBuffer(), inputPW.getSize());
        hash.Finalize(pwKey);
    }

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
        KeyT derivedPassword;
        derivePassword(derivedPassword, password);
        debugMem(derivedPassword, ArrSz(derivedPassword), "Password key");


        // Then create the block to decrypt
        SymmetricT sym;
        sym.setKey(derivedPassword, (SymmetricT::BlockSize)ArrSz(derivedPassword), 0, (SymmetricT::BlockSize)ArrSz(derivedPassword));

        MemoryBlock decKey( (uint32)((encryptedKeySize + (ArrSz(derivedPassword) - 1) ) / ArrSz(derivedPassword)) * ArrSz(derivedPassword) );
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

        // Derive the password key
        KeyT pwKey;
        derivePassword(pwKey, password);
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

        if (!vault.setContent(String::Print("%d %s\n%s\n", (int)exportedKey.getSize(), (const char*)ID, (const char*)String(base85Encoded->getConstBuffer(), base85Encoded->getSize())), File::Info::Append)) return TRANS("Can't set the key vault file content");
        if (!vault.setPermission(0600)) return TRANS("Can't set the key vault file permission to 0600");
        return "";
    }

    namespace DatabaseModel { String databaseURL = ""; }
  #define MapAs(X, Y, Offset) (X*)(Y + Offset)
    namespace FileFormat
    {
        struct _CondScopeProfiler
        {
            uint32 startTime;
            const char * name;
            _CondScopeProfiler(const char * name, const bool force = false) : startTime(dumpTimeRequired || force ?  Time::getTimeWithBase(65536) : 0), name(name) { }
            ~_CondScopeProfiler() { if (dumpTimeRequired) { uint32 duration = Time::getTimeWithBase(65536) - startTime; fprintf(stderr, "Time: %s => %.3fs\n", name, duration / 65536.0); } }
        };
        #define CondScopeProfiler ::Frost::FileFormat::_CondScopeProfiler profile (__FUNCTION__)

        template <int instanceID>
        struct _AccScopeProfiler
        {
            uint32 & getAccTime() { static uint32 accTime; return accTime; }
            uint32 & getCount() { static uint32 count; return count; }
            void flush() { fprintf(stderr, "Time: %s => avg %.3fs over %u runs (total: %.3fs)\n", name, getAccTime() / (getCount() * 65536.0), getCount(), getAccTime() / 65536.0); getCount() = getAccTime() =  0; }
            uint32 startTime;
            const char * name;
            _AccScopeProfiler(const char * name) : startTime(dumpTimeRequired ?  Time::getTimeWithBase(65536) : 0), name(name) { }
            ~_AccScopeProfiler() { if (dumpTimeRequired && startTime) { uint32 duration = Time::getTimeWithBase(65536) - startTime; getAccTime() += duration; getCount()++; } }
        };
        #define AccScopeProfiler(X) ::Frost::FileFormat::_AccScopeProfiler<X> profile (__FUNCTION__)


        // Start a new revision for this backup file
        bool IndexFile::startNewRevision(const unsigned rev)
        {
            const uint32 revision = rev ? rev : catalog->revision + 1;
            if (readOnly) return false;
            fileTree.revision = revision;
            metadata.Reset();
            if (!rev) metadata.Append(String::Print(TRANS("Revision %u created on %s"), revision, (const char*)Time::LocalTime::Now().toDate()));
            return true;
        }

        // Resize the chunk index map
        bool IndexFile::resizeChunkIndexMap()
        {
            if (!chunkIndices) return true;
            return chunkIndices->resize(); //@todo use a generator to save memory while growing the table
        }



        // Append a chunk to this index file
        bool IndexFile::appendChunk(Chunk & chunk, const uint32 forceUID)
        {
            if (readOnly) return false;
            if (!forceUID) chunk.UID = maxChunkID++ + 1;
            AccScopeProfiler(1);
            // local.chunks.insertSorted(chunk);
            // consolidated.chunks.insertSorted(chunk);
            uint32 chunkIndex = (uint32)consolidated.chunks.getSize();
            consolidated.chunks.Append(chunk);  // Should be O(1)
            return chunkIndices->storeValue(chunk.checksum, chunkIndex);  // This too
        }

        // Find a chunk based on its checksum
        uint32 IndexFile::findChunk(Chunk & chunk) const
        {
            AccScopeProfiler(2);
            uint32 * pos = chunkIndices->getValue(chunk.checksum);
            if (!pos) return -1;
            return consolidated.chunks[*pos].UID;
//            return consolidated.findChunk(chunk);
        }

        // Append a multichunk to this file
        bool IndexFile::appendMultichunk(Multichunk * mchunk, ChunkList * list)
        {
            if (readOnly || !mchunk || !list) return false;
            // Make sure it does not exists already
            mchunk->listID = maxChunkListID + 1;
            list->UID = maxChunkListID + 1;
            if (multichunks.storeValue(mchunk->UID, mchunk) && chunkList.storeValue(list->UID, list))
            {
                maxChunkListID++;
                return true;
            }
            return false;
        }

        bool IndexFile::appendFileItem(FileTree::Item * item, ChunkList * list)
        {
            if (readOnly || !item || !list) return false;
            // Make sure it does not exists already
            list->UID = maxChunkListID + 1;
            item->fixed->chunkListID = list->UID;
            CondScopeProfiler;
            fileTree.items.Append(item);
            if (chunkList.storeValue(list->UID, list))
            {
                maxChunkListID++;
                return true;
            }
            return false;
        }

        /** Dump the current information for all items in this index */
        String IndexFile::dumpIndex(uint32 rev) const
        {
            rev = rev == 0 ? getCurrentRevision() : rev;
            String ret = String::Print(TRANS("Revision: %u\n=>Header object\n"), rev);
            ret += header->dump();
            ret += TRANS("\n=> Catalog object\n");
            // Start with the catalog
            const Catalog * cat = getCatalogForRevision(rev);
            if (!cat) return ret + TRANS("Catalog not found, stopping\n");
            ret += cat->dump();
            // Now deal with metadata
            ret += TRANS("\n=> Metadata\n");
            MetaData met;
            if (cat->optionMetadata.fileOffset() && Load(met, cat->optionMetadata))
                ret += met.dump();
            // Then filter arguments
            ret += TRANS("\n=> Filter arguments\n");
            FilterArguments fa;
            if (cat->optionFilterArg.fileOffset() && Load(fa, cat->optionFilterArg))
                ret += fa.dump();

            // Then with the fileTree
            ret += TRANS("\n=> File tree\n");
            FileTree ft(rev, true);
            if (!Load(ft, cat->fileTree)) return ret += TRANS("File tree not found, stopping\n");
            ret += ft.dump();

            // Then the chunk lists
            ret += TRANS("\n=> Chunk lists\n");
            ChunkList cl;
            Offset chunkListOffset = cat->chunkLists;
            ret += String::Print(" ChunkList count: %u\n", cat->chunkListsCount);
            for (uint32 i = 0; i < cat->chunkListsCount; i++)
            {
                if (Load(cl, chunkListOffset)) ret += cl.dump();
                chunkListOffset.fileOffset(chunkListOffset.fileOffset() + cl.getSize());
            }

            // Read all previous multichunks now
            ret += TRANS("\n=> Multichunks\n");
            const Multichunk * mc;
            Offset mcOffset = cat->multichunks;
            ret += String::Print(" Multichunks count: %u\n", cat->multichunksCount);
            for (uint32 i = 0; i < cat->multichunksCount; i++)
            {
                if (Map(mc, mcOffset)) ret += mc->dump();
                mcOffset.fileOffset(mcOffset.fileOffset() + mc->getSize());
            }

            // Then do the multchunks array
            ret += TRANS("\n=> Chunks\n");
            Chunks chunks;
            if (LoadRO(chunks, cat->chunks)) ret += chunks.dump();
            return ret;
        }

        template <typename T>
        static uint32 getListSize(T & list)
        {
            uint32 ret = 0;
            typename T::IterT iter = list.getFirstIterator();
            while (iter.isValid())
            {
                ret += (*iter)->getSize();
                ++iter;
            }
            return ret;
        }

        // In a signal handler, we are not supposed to allocate, or call any system function, so this must be called in the processing loop.
        String IndexFile::dumpMemStat() const
        {
            uint64 total = 0, current = 0;
            String ret = String::Print("Header size: %llu bytes\n", (current = header->getSize())); total += current;
            ret += String::Print("Catalog size: %llu bytes\n", (current = catalog->getSize())); total += current;
            ret += String::Print("Consolidated chunks size: %llu bytes\n", (current = consolidated.getSize())); total += current;
            ret += String::Print("Chunks index table size: %llu bytes\n", (current = chunkIndices->getMemUsage())); total += current;
            ret += String::Print("Readonly chunks list size: %llu bytes\n", (current = getListSize(chunkListRO))); total += current;
            ret += String::Print("Chunks list size: %llu bytes\n", (current = getListSize(chunkList))); total += current;
            ret += String::Print("Multichunks size: %llu bytes\n", (current = getListSize(multichunks))); total += current;
            ret += String::Print("Readonly multichunks size: %llu bytes\n", (current = getListSize(multichunksRO))); total += current;
            ret += String::Print("FilterArg size: %llu bytes\n", (current = arguments.getSize())); total += current;
            ret += String::Print("Metadata size: %llu bytes\n", (current = metadata.getSize())); total += current;
            ret += String::Print("FileTree size: %llu bytes\n", (current = fileTree.getSize())); total += current;
            ret += String::Print("Readonly fileTree size: %llu bytes\n", (current = fileTreeRO.getSize())); total += current;
            ret += String::Print("Total size: %llu bytes\n", total);
            return ret;
        }


        // Create a new file from scratch.
        String IndexFile::createNew(const String & filePath, const Utils::MemoryBlock & cipheredMasterKey, const String & backupPath)
        {
            File::Info info(filePath, true);
            if (info.doesExist()) return TRANS("File already exists: ") + filePath;
            if (cipheredMasterKey.getSize() != MainHeader::CipheredMasterKeySize) return TRANS("Invalid ciphered master key format");
            file = new Stream::MemoryMappedFileStream(info.getFullPath(), true);
            if (!file) return TRANS("Out of memory");
            // Compute the size required for the metadata and filter arguments and header
            metadata.info.Clear();
            metadata.Append(backupPath);
            metadata.Append(TRANS("Initial backup started on ") + Time::LocalTime::Now().toDate());

            uint64 size = MainHeader::getSize();

            if (!file->map(0, size)) return TRANS("Could not allocate file space for creation. Is disk full?");
            // Ok, create the buffers now for this file
            uint8 * filePtr = file->getBuffer();
            if (!filePtr) return TRANS("Failed to get a pointer on the mapped area");

            header = *MapAs(MainHeader, filePtr, 0);
            new(header) MainHeader(); // Force construction of the object
            catalog = new Catalog(0); // This is required for previous linking
            memcpy(header->cipheredMasterKey, cipheredMasterKey.getConstBuffer(), ArrSz(header->cipheredMasterKey));

            // Ok, header is written, let's unmap the area
            readOnly = false;
            maxChunkID = 0; maxChunkListID = 0; maxMultichunkID = 0; prevRevisionMaxChunkID = 0;
            fileTree.revision = 1;
            chunkIndices = new ChunkIndexMap(65535, &consolidated.chunks);
            return "";
        }
        // Load a file from the given storage
        String IndexFile::readFile(const String & filePath, const bool readWrite)
        {
            File::Info info(filePath, true);
            if (!info.doesExist()) return TRANS("File does not exists: ") + filePath;
            file = new Stream::MemoryMappedFileStream(info.getFullPath(), readWrite);
            if (!file) return TRANS("Out of memory");
            // Check if we can map the complete file (right now, it's much easier this way)
            if (!file->map()) return TRANS("Could not open the given file (permission error ?): ") + filePath;
            readOnly = !readWrite;

            // Ok, create the buffers now for this file
            uint8 * filePtr = file->getBuffer();
            if (!filePtr) return TRANS("Failed to get a pointer on the mapped area");

            header = *MapAs(MainHeader, filePtr, 0);
            if (!header->isCorrect(file->fullSize())) return TRANS("Given index format not correct");
            uint64 catalogOffset = header->catalogOffset.fileOffset();
            if (!catalogOffset) catalogOffset = file->fullSize() - Catalog::getSize();
            // Get the offset to the catalog for reading
            catalog = *MapAs(Catalog, filePtr, catalogOffset);

            if (!catalog->isCorrect(file->fullSize(), catalogOffset))
                return TRANS("Catalog in file is corrupted.");


            // Now we have a catalog, let's extract all the data we need
            // Fuse the chunks
            maxChunkID = 0;
            consolidated.Clear();
            chunkIndices = 0;
            maxChunkListID = 0;
            multichunksRO.clearTable();
            multichunks.clearTable();
            maxMultichunkID = 0;
            arguments.arguments.Clear();
            metadata.info.Clear();

            Catalog * c = catalog;
            while (c)
            {
                if (dumpLevel > 1) c->dump();

                Chunks chunk(c->revision);
                if (!chunk.loadReadOnly(filePtr + c->chunks.fileOffset(), file->fullSize() - c->chunks.fileOffset())) return String::Print(TRANS("Could not read the chunks for revision %d"), c->revision);
                if (chunk.revision != c->revision) return String::Print(TRANS("Unexpected chunks revision %u for catalog revision %u"), chunk.revision, c->revision);

                // Insert all chunks in the consolidated array (this can take some time)
                for (size_t i = 0; i < chunk.chunks.getSize(); i++)
                {
                    if (chunk.chunks[i].UID > maxChunkID) maxChunkID = chunk.chunks[i].UID;
//                    if (readWrite) consolidated.chunks.insertSorted(chunk.chunks[i]);
//                    else
                        consolidated.chunks.Append(chunk.chunks[i]); // Not sorted, we'll sort them later on
                }

                // Read all chunk lists now
                uint64 chunkListOffset = c->chunkLists.fileOffset();
                for (uint32 i = 0; i < c->chunkListsCount; i++)
                {
                    ChunkList * cl = new ChunkList();
                    if (!cl) return TRANS("Out of memory");
                    if (!cl->load(filePtr + chunkListOffset, file->fullSize() - chunkListOffset)) return TRANS("Could not load chunk list");

                    if (!chunkListRO.storeValue(cl->UID, cl)) return String::Print(TRANS("Chunk list with UID %u already exist"), cl->UID);
                    if (cl->UID > maxChunkListID) maxChunkListID = cl->UID;
                    chunkListOffset += cl->getSize();
                }

                // Read all previous multichunks now
                uint64 multichunkOffset = c->multichunks.fileOffset();
                for (uint32 i = 0; i < c->multichunksCount; i++)
                {
                    Multichunk * mc = MapAs(Multichunk, filePtr, multichunkOffset);
                    if (!mc->isCorrect(file->fullSize(), file->fullSize() - multichunkOffset)) return String::Print(TRANS("Invalid %u-th multichunk in revision %u"), i, c->revision);
                    if (mc->UID > maxMultichunkID) maxMultichunkID = mc->UID;
                    multichunksRO.storeValue(mc->UID, mc);

                    multichunkOffset += mc->getSize();
                }

                // Read filter arguments
                if (!arguments.arguments.getSize() && c->optionFilterArg.fileOffset())
                {
                    if (!arguments.load(filePtr + c->optionFilterArg.fileOffset(), file->fullSize() - c->optionFilterArg.fileOffset()))
                        return String::Print(TRANS("Could not read the filters' argument for revision %u"), c->revision);
                    if (!arguments.isCorrect(file->fullSize(), c->optionFilterArg.fileOffset()))
                        return String::Print(TRANS("Bad filters' arguments for revision %u"), c->revision);
                }

                // Read metadata
                if (!metadata.info.getSize() && c->optionMetadata.fileOffset())
                {
                    if (!metadata.load(filePtr + c->optionMetadata.fileOffset(), file->fullSize() - c->optionMetadata.fileOffset()))
                        return String::Print(TRANS("Could not read the metadata for revision %u"), c->revision);
                    if (!metadata.isCorrect(file->fullSize(), c->optionMetadata.fileOffset()))
                        return String::Print(TRANS("Bad metadata for revision %u"), c->revision);
                }

                c = c->previous.fileOffset() ? MapAs(Catalog, filePtr, c->previous.fileOffset()) : 0;
            }

            // Read the last filetree (that's the only required for now)
            fileTree.Clear();
            fileTreeRO.Clear();
            if (!fileTreeRO.load(filePtr + catalog->fileTree.fileOffset(), file->fullSize() - catalog->fileTree.fileOffset()))
                return String::Print(TRANS("Could not load the file tree for revision %u"), catalog->revision);

            ChunkUIDSorter sorter;
            if (!readWrite) Container::Algorithms<Container::PlainOldData<Chunk>::Array>::sortContainer(consolidated.chunks, sorter); // This is only using UID to sort
//            else            Container::Algorithms<Container::PlainOldData<Chunk>::Array>::sortContainer(consolidated.chunks, consolidated.chunks[0]); // This is using size and checksum to sort
            else
            {
                chunkIndices = new ChunkIndexMap(consolidated.chunks.getSize() * 2, &consolidated.chunks);
                // Insert all chunks index in the map
                for (size_t i = 0; i < consolidated.chunks.getSize(); i++)
                {
                    if (!chunkIndices->storeValue(consolidated.chunks.getElementAtUncheckedPosition(i).checksum, i))
                        return String::Print(TRANS("Could not insert the chunk at pos %u with UID: %u"), (uint32)i, consolidated.chunks.getElementAtUncheckedPosition(i).UID);
                }
                prevRevisionMaxChunkID = maxMultichunkID;
            }
            // Ok, done loading this file
            return "";
        }
        const Chunk * IndexFile::findChunk(const uint32 uid) const
        {
            size_t pos = 0;
            CondScopeProfiler;
            Chunk item(uid);
            if (readOnly)
            {   // The consolidated array is sorted by UID, so we can do a O(log N) search here
                ChunkUIDSorter sorter;
                pos = Container::Algorithms<Container::PlainOldData<Chunk>::Array>::searchContainer(consolidated.chunks, sorter, item);
                if (pos == consolidated.chunks.getSize() || consolidated.chunks.getElementAtPosition(pos).UID != uid) return 0;
            }
            else
            {
                // This is going to be very slow O(N)
                pos = consolidated.chunks.indexOf(item);
                if (pos == consolidated.chunks.getSize()) return 0;
            }
            return &consolidated.chunks.getElementAtPosition(pos);
        }

        // Close the file (and make sure mapping is actually correct)
        String IndexFile::close()
        {
            if (!file || readOnly || (fileTree.items.getSize() == 0 && !metadata.modified))
            {
                file = 0; catalog = 0; header = 0; chunkIndices = 0;
                fileTree.Clear(); fileTreeRO.Clear();
                metadata.Reset(); arguments.Reset();
                consolidated.Clear();       prevRevisionMaxChunkID = 0; maxChunkID = 0;
                chunkListRO.clearTable();   chunkList.clearTable();     maxChunkListID = 0;
                multichunks.clearTable();   multichunksRO.clearTable(); maxMultichunkID = 0;
                return ""; // Nothing to do or no modifications done
            }

            Chunks local(0);
            // O(n) operation here for a huge list, but hopefully, this will be in cache most of the time
            // This will also consume all new chunks memory additionally
            for (size_t i = 0; i < consolidated.chunks.getSize(); i++)
            {
                if (consolidated.chunks.getElementAtUncheckedPosition(i).multichunkID > prevRevisionMaxChunkID)
                    local.chunks.Append(consolidated.chunks.getElementAtUncheckedPosition(i));
            }

            // Get a coarse approximation of the required size for the file expansion required
            uint64 requiredAdditionalSize = fileTree.getSize() + (arguments.modified ? arguments.getSize() : 0) + (metadata.modified ? metadata.getSize() : 0) + multichunks.getSize() * Multichunk::getSize()
                                            + local.getSize() + Catalog::getSize();
            // We need to iterate the chunklists to know their size
            ChunkLists::IterT cl = chunkList.getFirstIterator();
            while (cl.isValid()) { requiredAdditionalSize += (*cl)->getSize(); ++cl; }

            uint64 initialSize = file->fullSize();
            uint64 initialCatalog = header->catalogOffset.fileOffset();
            if (!initialCatalog && initialSize > header->getSize()) initialCatalog = initialSize - Catalog::getSize();

            // Make sure we can allocate such size now on file
            Offset prevOptMetadata = catalog->optionMetadata, prevFilterArg = catalog->optionFilterArg;
            if (!file->map(0, file->fullSize() + requiredAdditionalSize))
                return String::Print(TRANS("Cannot allocate %llu more bytes for the index file, is disk full?"), requiredAdditionalSize);
            uint8 * filePtr = file->getBuffer();
            // Starting from this point, the previous mapping are no more valid, so we can't refer to them
            //===========================================================================================================

            uint32 prevRev = initialCatalog ? (*MapAs(Catalog, filePtr, initialCatalog)).revision : 0;

            // Ok, start by writing all the new informations
            Catalog cat(prevRev + 1);
            uint64 wo = initialSize;
            cat.chunks.fileOffset(wo);
            // Write the new chunk array
            local.revision = prevRev + 1; // Fix the revision
            local.write(filePtr + wo); wo += local.getSize();
            // Write the chunk list
            cat.chunkLists.fileOffset(wo);
            cat.chunkListsCount = chunkList.getSize();
            {
                ChunkLists::IterT iter = chunkList.getFirstIterator();
                while (iter.isValid())
                {
                    (*iter)->write(filePtr + wo); wo += (*iter)->getSize();
                    ++iter;
                }
            }
            // Write the multichunk list
            cat.multichunks.fileOffset(wo);
            cat.multichunksCount = multichunks.getSize();
            {
                Multichunks::IterT iter = multichunks.getFirstIterator();
                while (iter.isValid())
                {
                    (*iter)->write(filePtr + wo); wo += (*iter)->getSize();
                    ++iter;
                }
            }
            // We need to write the file tree too
            cat.fileTree.fileOffset(wo);
            fileTree.write(filePtr + wo); wo += fileTree.getSize();

            // Check if we need to write the arguments
            if (arguments.modified)
            {
                cat.optionFilterArg.fileOffset(wo);
                arguments.write(filePtr + wo); wo += arguments.getSize();
            } else cat.optionFilterArg = prevFilterArg;
            // Check if we need to write the metadata
            if (metadata.modified)
            {
                cat.optionMetadata.fileOffset(wo);
                metadata.write(filePtr + wo); wo += metadata.getSize();
            } else cat.optionMetadata = prevOptMetadata;

            cat.previous.fileOffset(initialCatalog);
            // Now we can write the catalog
            if (wo + cat.getSize() != file->fullSize()) return TRANS("Invalid file size computation");
            cat.write(filePtr + wo);
            file->unmap(true);
            file = 0;
            return "";
        }

        // Get the file base name for this multichunk
        String Multichunk::getFileName() const
        {
            String ret; uint32 outSize = (uint32)(ArrSz(checksum) * 2);
            if (!Encoding::encodeBase16(checksum, ArrSz(checksum), (uint8*)ret.Alloc(ArrSz(checksum)*2), outSize)) return "";
            ret.releaseLock((int)outSize);
            ret += ".#";
            return ret;
        }

        Utils::OwnPtr<FileTree> IndexFile::getFileTree(const uint32 revision)
        {
            // Check easy first, with no destruction
            if (!revision || !file) return 0;
            if (!readOnly && revision == fileTree.revision) return fileTree;
            if (revision == fileTreeRO.revision) return fileTreeRO;
            if (revision > fileTree.revision && revision > fileTreeRO.revision) return 0;

            // Ok, need to extract the other revisions
            uint8 * filePtr = file->getBuffer();
            Catalog * c = catalog;
            while (c)
            {
                if (c->revision == revision)
                {
                    Utils::OwnPtr<FileTree> ft(new FileTree(revision));
                    if (!ft->load(filePtr + c->fileTree.fileOffset(), file->fullSize() - c->fileTree.fileOffset())) return 0;
                    return ft;
                }
                c = MapAs(Catalog, filePtr, c->previous.fileOffset());
            }
            return 0;
        }

        MetaData IndexFile::getFirstMetaData()
        {
            const Catalog * c = catalog;
            // Find the first catalog
            while (c && c->previous.fileOffset()) Map(c, c->previous);
            // Then extract the metadata from it
            MetaData ret;

            // All error would lead to returning the metadata object anyway
            if (c->optionMetadata.fileOffset())
                Load(ret, c->optionMetadata.fileOffset());
            return ret;
        }
    }
  #undef MapAs
    namespace Helpers
    {
        CompressorToUse compressor;

        // The entropy threshold
        double entropyThreshold = 1.0;

        // Excluded file list if found
        String excludedFilePath;
        // Included file list if found
        String includedFilePath;

        // The index file we are using
        FileFormat::IndexFile indexFile;



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
        // Encrypt or decrypt using AES counter mode.
        bool AESCounterProcess(const KeyFactory::KeyT & key, const KeyFactory::KeyT & nonceRandom, const ::Stream::InputStream & input, ::Stream::OutputStream & output, ProgressCallback & callback, uint8 * inputHash, uint8 * outputHash)
        {
            Crypto::OSSL_AES cipher;
            cipher.setKey(key, (Crypto::BaseSymCrypt::BlockSize)ArrSz(key), 0, (Crypto::BaseSymCrypt::BlockSize)ArrSz(key));
            
            KeyFactory::KeyT nonce = {}, inputData = {}, tmp = {}, outputData = {};
            memcpy(&nonce[0], &nonceRandom[0], 8);
    
            Crypto::OSSL_SHA256 hash;
            hash.Start();

            callback.progressed(inputHash ? ProgressCallback::Backup : ProgressCallback::Restore, "Processing: " DEFAULT_INDEX, 0, input.fullSize(), 1, 1, ProgressCallback::KeepLine);
            uint64 counter = 0;
            for (uint64 i = 0; i < input.fullSize(); i += ArrSz(nonce))
            {
                // Increment the nonce including the counter
                counter++;
                memcpy(&nonce[8], &counter, sizeof(counter));
                // Read the data
                uint64 inputSize = input.read(inputData, (uint64)ArrSz(inputData));
                if (inputSize == (uint64)-1)
                    return callback.warn(inputHash ? ProgressCallback::Backup : ProgressCallback::Restore, DEFAULT_INDEX, "Could not read from file") && false;

                callback.progressed(inputHash ? ProgressCallback::Backup : ProgressCallback::Restore, "Processing: " DEFAULT_INDEX, i, input.fullSize(), 1, 1, ProgressCallback::KeepLine);

                // Hash the data
                if (inputHash) hash.Hash(inputData, inputSize);
                
                // And encrypt the data (yes, even for decrypting)
                if (!Crypto::CTR_BlockProcess(cipher, nonce, tmp))
                    return callback.warn(inputHash ? ProgressCallback::Backup : ProgressCallback::Restore, DEFAULT_INDEX, "Could not encrypt or decrypt data") && false;
                // Encrypt or decrypt the data
                Crypto::Xor(outputData, inputData, tmp, (size_t)inputSize);
                // Hash the output data if requested
                if (outputHash) hash.Hash(outputData, inputSize);

                if (output.write(outputData, inputSize) != inputSize)
                    return callback.warn(inputHash ? ProgressCallback::Backup : ProgressCallback::Restore, DEFAULT_INDEX, "Could not write to file") && false;
            }
            // Then store the hash back at the right position
            if (inputHash) hash.Finalize(inputHash);
            if (outputHash) hash.Finalize(outputHash);
            callback.progressed(inputHash ? ProgressCallback::Backup : ProgressCallback::Restore, "Processing: " DEFAULT_INDEX, input.fullSize(), input.fullSize(), 1, 1, ProgressCallback::FlushLine);
            
            // Ok, done
            return true;
        }
        
        // Ensure the index file is available or recreate if not
        String ensureValidIndexFile(const String & encryptedIndexPath, const String & localIndexPath, const KeyFactory::KeyT & key, ProgressCallback & callback, const bool forceDecryption)
        {
            File::Info encFile(encryptedIndexPath, true);
            File::Info decFile(localIndexPath, true);
            if (!encFile.doesExist() && forceDecryption) return TRANS("Encrypted file does not exist :") + encryptedIndexPath;
            if (!encFile.doesExist())
            {
                if (!decFile.doesExist()) return TRANS("Both encrypted and cached index file are missing");
                return ""; // Nothing we can do here
            }
            // Ok, now we need to decrypt the file or use the existing cached file
            if (!forceDecryption)
            {
                if (decFile.modification == encFile.modification && encFile.size == (decFile.size + sizeof(FileFormat::CipheredIndexHeader)))
                    return ""; // Use the existing cached file (don't try to decrypt first)
            }
            // Ok, here, we must decrypt the index file so let's process!
            callback.progressed(ProgressCallback::Restore, "Decrypting: " DEFAULT_INDEX, 0, encFile.size, 1, 1, ProgressCallback::KeepLine);
            Stream::InputFileStream input(encFile.getFullPath());
            Stream::OutputFileStream output(decFile.getFullPath());
            
            FileFormat::CipheredIndexHeader indexHeader;
            if (input.read(&indexHeader, sizeof(indexHeader)) != sizeof(indexHeader)) return TRANS("Could not read header in encrypted file: ") + encryptedIndexPath;
            if (!indexHeader.isValid()) return TRANS("Invalid header from encrypted index");
            
            // Extract the nonce from it now
            KeyFactory::KeyT nonce = {}, hash = {};
            memcpy(nonce, indexHeader.nonce, ArrSz(indexHeader.nonce));
            // Then decrypt the file
            if (!AESCounterProcess(key, nonce, input, output, callback, 0, hash)) return TRANS("Error while decrypting the index file: ") + encryptedIndexPath;
            // Assert it's valid
            if (memcmp(hash, indexHeader.hash, sizeof(hash))) return TRANS("The hash of the decrypted index does not match the input file.");
            // Ok, done
            return "";
        }

        static String getFilterArgument(CompressorToUse actualComp)
        {
            if (actualComp == Default) actualComp = compressor;
            const char * compressorName[] = { "none", "zLib", "BSC" };
            return String::Print("%d:%s:AES_CTR", File::MultiChunk::MaximumSize, compressorName[actualComp]);
        }

        static uint16 getFilterArgumentIndex(CompressorToUse actualComp, FileFormat::IndexFile * idxFile = 0)
        {
            const String & filterArg = getFilterArgument(actualComp);
            FileFormat::IndexFile & idx = idxFile ? *idxFile : indexFile;
            uint16 index = idx.getFilterArguments().getArgumentIndex(filterArg);
            if (index == idx.getFilterArguments().arguments.getSize())
                return idx.getFilterArguments().appendArgument(filterArg);
            return index;
        }
        typedef Utils::ScopePtr<FileFormat::ChunkList> & ChunkListT;

        bool closeMultiChunkBin(String & chunkPath, File::MultiChunk & multiChunk, uint64 * totalOutSize, ProgressCallback & callback, CompressorToUse actualComp, KeyFactory::KeyT & chunkHash)
        {
            bool worthTelling = multiChunk.getSize() > 2*1024*1024;
            if (worthTelling && !callback.progressed(ProgressCallback::Backup, TRANS("Closing multichunk"), 0, 0, 0, 0, ProgressCallback::KeepLine))
                return false;
            // We need this for the nonce
            multiChunk.getChecksum(chunkHash);

            const String & multiChunkHash = Helpers::fromBinary(chunkHash, ArrSz(chunkHash), false);
            // Then filter the multichunk, compress it and encrypt it
            ::Stream::OutputMemStream compressedStream;
            if (worthTelling && !callback.progressed(ProgressCallback::Backup, TRANS("Compressing multichunk"), 0, 0, 0, 0, ProgressCallback::KeepLine))
                return false;

            if (actualComp == Default) actualComp = compressor;
            switch (actualComp)
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
            case None:
                {   // Avoid compressing the data
                    if (!multiChunk.writeHeaderTo(compressedStream)) return false;
                    if (!multiChunk.writeDataTo(compressedStream)) return false;
                    break;
                }
            default:
                return false;
            }

            {   // Encode the data now
                if (worthTelling && !callback.progressed(ProgressCallback::Backup, TRANS("Encrypting multichunk"), 0, 0, 0, 0, ProgressCallback::KeepLine))
                    return false;

                ::Stream::MemoryBlockStream compressedData(compressedStream.getBuffer(), compressedStream.fullSize());
                if (totalOutSize) *totalOutSize += compressedStream.fullSize();
                chunkPath += multiChunkHash + ".#";
                ::Stream::OutputFileStream chunkFile(chunkPath);
                if (!Helpers::AESCounterEncrypt(chunkHash, compressedData, chunkFile))
                    return false;
            }

            if (worthTelling && !callback.progressed(ProgressCallback::Backup, TRANS("Multichunk closed"), 0, 0, 0, 0, ProgressCallback::KeepLine))
                return false;
            return true;
        }

        bool closeMultiChunk(const String & backupTo, File::MultiChunk & multiChunk, ChunkListT multiChunkID, uint64 * totalOutSize, ProgressCallback & callback, uint64 & previousMultiChunkID, uint64 & currentMultiChunkID, CompressorToUse actualComp)
        {
            KeyFactory::KeyT chunkHash;
            String backPath = backupTo;
            if (!closeMultiChunkBin(backPath, multiChunk, totalOutSize, callback, actualComp, chunkHash)) return false;

            if (previousMultiChunkID)
            {
                // Might need to update the previous multichunk ID
                FileFormat::Multichunk * mc = indexFile.getMultichunk((uint16)previousMultiChunkID);
                if (mc->listID == multiChunkID->UID)
                {   // Same multichunk, so let's modify it (remove the previous file)
                    File::Info(backupTo + mc->getFileName()).remove();
                    // Update it (this will modify the file multichunk position)
                    mc->filterArgIndex = getFilterArgumentIndex(actualComp);
                    memcpy(mc->checksum, chunkHash, ArrSz(chunkHash));
                    previousMultiChunkID = 0;
                    multiChunk.Reset();
                    return true;
                }
            }
            FileFormat::Multichunk * mc = new FileFormat::Multichunk(currentMultiChunkID);
            mc->filterArgIndex = getFilterArgumentIndex(actualComp);
            memcpy(mc->checksum, chunkHash, ArrSz(chunkHash));
            indexFile.appendMultichunk(mc, multiChunkID.Forget());

            multiChunk.Reset();
            currentMultiChunkID = 0; // On next usage, will allocate a new one
            return true;
        }

        struct ChunkCache
        {
            Utils::ScopePtr<File::MultiChunk> chunk;
            time_t                            lastAccessTime;
            ChunkCache(File::MultiChunk * chunk) : chunk(chunk), lastAccessTime(time(NULL)) {}
        };
        struct MultiChunkCache
        {
            typedef Container::HashTable<ChunkCache, uint64> MultiChunkHash;
            MultiChunkHash       hash;
            const size_t         maxCacheSize;
            size_t               totalCacheSize;

            File::MultiChunk * getChunk(const uint64 id) { ChunkCache * cache = hash.getValue(id); if (cache) { cache->lastAccessTime = time(NULL); return cache->chunk; } return 0; }
            bool storeChunk(File::MultiChunk * chunk, const uint64 id)
            {
                if (!chunk) return false;
                // Check the cumulative size
                if (totalCacheSize + chunk->getSize() > maxCacheSize)
                {
                    // Need to prune the oldest multichunk from the cache
                    MultiChunkHash::IterT iter = hash.getFirstIterator();
                    time_t oldest = time(NULL); uint64 oldestHash = 0; size_t oldSize = 0;
                    while(iter.isValid())
                    {
                        if ((*iter)->lastAccessTime < oldest) { oldest = (*iter)->lastAccessTime; oldestHash = *iter.getKey(); oldSize = (*iter)->chunk->getSize(); }
                        ++iter;
                    }

                    totalCacheSize -= oldSize;
                    hash.removeValue(oldestHash);
                }
                totalCacheSize += chunk->getSize();
                return hash.storeValue(id, new ChunkCache(chunk));
            }

            MultiChunkCache(const size_t maxCacheSize) : maxCacheSize(maxCacheSize), totalCacheSize(0) {}
        };

        String readMultichunk(const String & fullMultiChunkPath, const String & filterMode, File::MultiChunk & mchunk, ProgressCallback & callback)
        {
            // Decode this multichunk to find out the chunk to store in file
            ::Stream::InputFileStream chunkFile(fullMultiChunkPath);
            bool worthTelling = chunkFile.fullSize() > (uint64)2*1024*1024;

            ::Stream::OutputMemStream compressedData;

            KeyFactory::KeyT chunkHash;
            uint32 chunkHashSize = (uint32)ArrSz(chunkHash);
            if (worthTelling && !callback.progressed(ProgressCallback::Restore, TRANS("Checking multichunk integrity"), 0, 0, 0, 0, ProgressCallback::KeepLine))
                return "Interrupted";

            if (!Helpers::toBinary(fullMultiChunkPath.fromLast("/").upToLast("."), chunkHash, chunkHashSize, false)
                || chunkHashSize != (uint32)ArrSz(chunkHash))
                return TRANS("Error while decoding the hash of the multichunk: ") + fullMultiChunkPath;

            // Decrypt it now
            if (worthTelling && !callback.progressed(ProgressCallback::Restore, TRANS("Decrypting multichunk"), 0, 0, 0, 0, ProgressCallback::KeepLine))
                return "";
            // We only support counter mode for now
            if (filterMode.fromLast(":") == "AES_CTR" && !Helpers::AESCounterDecrypt(chunkHash, chunkFile, compressedData))
                return TRANS("Can not decode the multichunk: ") + fullMultiChunkPath;

            // Decompress it
            if (worthTelling && !callback.progressed(ProgressCallback::Restore, TRANS("Decompressing multichunk"), 0, 0, 0, 0, ProgressCallback::KeepLine))
                return "";

            // Ensure multichunk size is large enough
            size_t multiChunkSize = (size_t)filterMode.upToFirst(":").parseInt(10);
            if (multiChunkSize > File::MultiChunk::MaximumSize) File::MultiChunk::setMaximumSize(multiChunkSize);

            // Then decompress
            String compUsed = filterMode.fromTo(":", ":");
            if (compUsed == "zLib")
            {   // And zLib
                ::Stream::MemoryBlockStream compressedStream(compressedData.getBuffer(), compressedData.fullSize());
                {
                    Compression::ZLib * zlib = new Compression::ZLib;
                    zlib->setCompressionFactor(1.0f);
                    ::Stream::DecompressInputStream decompressor(compressedStream, zlib);
                    if (!mchunk.loadHeaderFrom(decompressor))
                        return TRANS("Can not decompress header from multichunk: ") + fullMultiChunkPath;
                    if (!mchunk.loadDataFrom(decompressor))
                        return TRANS("Can not decompress data from multichunk: ") + fullMultiChunkPath;
                }
            } else if (compUsed == "BSC")
            {   // And BSCLib
                ::Stream::MemoryBlockStream compressedStream(compressedData.getBuffer(), compressedData.fullSize());
                {
                    ::Stream::DecompressInputStream decompressor(compressedStream, new Compression::BSCLib);
                    if (!mchunk.loadHeaderFrom(decompressor))
                        return TRANS("Can not decompress header from multichunk: ") + fullMultiChunkPath;

                    if (!mchunk.loadDataFrom(decompressor))
                        return TRANS("Can not decompress data from multichunk: ") + fullMultiChunkPath;
                }
            } else if (compUsed == "none")
            {   // And no compression
                ::Stream::MemoryBlockStream compressedStream(compressedData.getBuffer(), compressedData.fullSize());
                if (!mchunk.loadHeaderFrom(compressedStream))
                    return TRANS("Can not read header from multichunk: ") + fullMultiChunkPath;

                if (!mchunk.loadDataFrom(compressedStream))
                    return TRANS("Can not read data from multichunk: ") + fullMultiChunkPath;
            } else return TRANS("Compressor not supported: ") + compUsed;

            // Assert the decompressed data is still correct
            KeyFactory::KeyT chunkTest;
            if (worthTelling && !callback.progressed(ProgressCallback::Restore, TRANS("Checking data integrity"), 0, 0, 0, 0, ProgressCallback::KeepLine))
                return "";
            mchunk.getChecksum(chunkTest);

            if (memcmp(chunkTest, chunkHash, ArrSz(chunkHash)))
                return TRANS("Corruption detected in multichunk: ") + fullMultiChunkPath;

            // Ok, done
            return "";
        }

        File::Chunk * extractChunkBin(String & error, const String & basePath, const String & MultiChunkPath, const uint64 MultiChunkID, const size_t chunkOffset, const uint8 * chunkCS, const String & filterMode, MultiChunkCache & cache, ProgressCallback & callback)
        {
            File::MultiChunk * cached = cache.getChunk(MultiChunkID);

            if (!cached)
            {
                cached = new File::MultiChunk;
                if (!cached)
                {
                    error = TRANS("Not enough memory to allocate multichunk: ") + MultiChunkPath;
                    return 0;
                }
                File::MultiChunk & mchunk = *cached;

                error = readMultichunk(basePath + MultiChunkPath, filterMode, mchunk, callback);
                if (error) return 0;

                // It worked, so let's save it to avoid reloading it
                if (!cache.storeChunk(cached, MultiChunkID))
                {
                    error = TRANS("Can not store multichunk in cache: ") + MultiChunkPath;
                    return 0;
                }
            }

            // Ok, extract the chunk
            File::Chunk * chunk = cached->findChunk(chunkCS, (size_t)chunkOffset);
            return chunk;
        }


        File::Chunk * extractChunk(String & error, const String & basePath, const String & MultiChunkPath, const uint64 MultiChunkID, const size_t chunkOffset, const String & chunkChecksum, const String & filterMode, MultiChunkCache & cache, ProgressCallback & callback)
        {
            error = "";
            // Ok, extract the chunk
            uint8 chunkCS[Hashing::SHA1::DigestSize];
            uint32 chunkCSSize = (uint32)ArrSz(chunkCS);
            if (!Helpers::toBinary(chunkChecksum, chunkCS, chunkCSSize) || chunkCSSize != (uint32)ArrSz(chunkCS))
            {
                error = TRANS("Bad checksum for chunk with checksum: ") + chunkChecksum;
                return 0;
            }
            return extractChunkBin(error, basePath, MultiChunkPath, MultiChunkID, chunkOffset, chunkCS, filterMode, cache, callback);
        }

        uint32 allocateChunkList()
        {
            return indexFile.allocateChunkListID();
        }
    }
    // Initialize the database connection, and bootstrap it if required.
    String initializeDatabase(const String & backupPath, unsigned int & revisionID, MemoryBlock & cipheredMasterKey)
    {
        // Check if we are opening or creating an index file now
        const String & indexPath = DatabaseModel::databaseURL + DEFAULT_INDEX;
        if (!File::Info(indexPath).doesExist())
        {
            revisionID = 1;
            return Helpers::indexFile.createNew(indexPath, cipheredMasterKey, backupPath);
        }
        // File exists, let's create a new revision if required
        const String & ret = Helpers::indexFile.readFile(indexPath, backupPath);
        if (ret) return ret;
        cipheredMasterKey = Helpers::indexFile.getCipheredMasterKey().getMovable();
        if (backupPath && !Helpers::indexFile.startNewRevision())
            return TRANS("Could not start a new revision in index file.");
        revisionID = Helpers::indexFile.getCurrentRevision();
        return "";
    }

    // Finalize the database, updating the database description when done.
    void finalizeDatabase()
    {
        if (wasBackingUp)
        {
            if (backupWorked)
                Helpers::indexFile.getMetaData().info[Helpers::indexFile.getMetaData().info.getSize() - 1] += TRANS(" finished on ") + Time::LocalTime::Now().toDate();
            else
                Helpers::indexFile.getMetaData().info[Helpers::indexFile.getMetaData().info.getSize() - 1]  = TRANS("Reverted to last known good revision on ") + Time::LocalTime::Now().toDate();
        }
        const String & ret = Helpers::indexFile.close();
        if (ret) fputs(ret + "\n", stderr); // Don't silent the error if any
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
    /** The cache of files for a revision */
    typedef Container::HashTable<FileMDEntry, String, Container::HashKey<String> > PathIDMapT;

    /** The index array */
    typedef Container::PlainOldData<uint32>::Array IndexArray;
    /** Collect the list of files in a directory based on the Entry's in the database.
        @param dirPath      The directory path to look for
        @param entryList    On output, contains a sorted list of index of files in the File Tree
        @param fileTree     The file under consideration
        @return The index in the file tree for the directory + 1, or 0 on error */
    uint32 createActualEntryListInDir(const String & dirPath, IndexArray & entryList, Utils::OwnPtr<FileFormat::FileTree> & fileTree)
    {
        CondScopeProfiler;
        entryList.Clear();

        uint32 parentIndex = fileTree->findItem(dirPath);
        if (parentIndex == fileTree->notFound()) return 0;

        // Then iterate all children with this parent
        for (size_t i = 0; i < fileTree->items.getSize(); i++)
        {
            if (fileTree->items[i].fixed && fileTree->items[i].fixed->parentID == (parentIndex+1))
                entryList.Append(i);
        }
        return parentIndex+1;
    }

    /** Create a list of files in a directory based on the Entry's in the database.
        @param dirPath  The directory path to look for
        @param fileList On output, contains a sorted list of files and directories path in the specified directory
        @param revID    The maximum revision ID to include
        @return The directory ID if found, or 0 on error */
    bool createFileListInDir(const String & dirPath, PathIDMapT & fileList, const unsigned int revID)
    {
        CondScopeProfiler;
        fileList.clearTable();

        Utils::OwnPtr<FileFormat::FileTree> fileTree = Helpers::indexFile.getFileTree(revID);
        if (!fileTree) return false;

        IndexArray entries;

        uint32 dirID = createActualEntryListInDir(dirPath, entries, fileTree);
        if (dirID == 0 || entries.getSize() == 0) return false;

        // Then find all files and directory and sort them
        for (size_t i = 0; i < entries.getSize(); i++)
            fileList.storeValue(fileTree->getItemFullPath(entries[i]), new FileMDEntry(entries[i], fileTree->items[entries[i]].getMetaData()), true);
        return true;
    }

    /** Create a list of files in a directory based on the Entry's in the database.
        @param dirPath  The directory path to look for
        @param fileList On output, contains a sorted list of files and directories path in the specified directory
        @param revID    The maximum revision ID to include
        @return The directory ID if found, or 0 on error */
    bool createFileListInDir(const String & dirPath, PathIDMapT & fileList, Utils::OwnPtr<FileFormat::FileTree> & fileTree)
    {
        CondScopeProfiler;
        fileList.clearTable();
        if (!fileTree) return true;

        IndexArray entries;

        uint32 dirID = createActualEntryListInDir(dirPath, entries, fileTree);
        if (dirID == 0 || entries.getSize() == 0) return false;

        // Then find all files and directory and sort them
        for (size_t i = 0; i < entries.getSize(); i++)
            fileList.storeValue(fileTree->getItemFullPath(entries[i]), new FileMDEntry(entries[i], fileTree->items[entries[i]].getMetaData()), true);
        return true;
    }

    /** Create the list of files and directories based on the Entry's in the database.
        @param fileList On output, contains a sorted list of files and directories path in the specified directory
        @param revID    The maximum revision ID to include
        @return true if the revision is valid */
    bool createFileListInRev(PathIDMapT & fileList, const unsigned int revID)
    {
        CondScopeProfiler;
        fileList.clearTable();

        // First find the parent index for the given path
        Utils::OwnPtr<FileFormat::FileTree> fileTree = Helpers::indexFile.getFileTree(revID);
        if (!fileTree) return false;

        for (uint32 i = 0; i < fileTree->items.getSize(); i++)
            fileList.storeValue(fileTree->getItemFullPath(i), new FileMDEntry(i, fileTree->items[i].getMetaData()), true);

        return true;
    }
    /** Create a list of directories based on the Entry's in the database.
        @param dirList  On output, will be filled with the directories to create for restoring
        @param revID    The maximum revision ID to include
        @return true if the revision is valid and output was filled */
    bool createDirListInRev(Strings::StringArray & dirList, const unsigned int revID)
    {
        CondScopeProfiler;
        dirList.Clear();
        // First find the parent index for the given path
        Utils::OwnPtr<FileFormat::FileTree> fileTree = Helpers::indexFile.getFileTree(revID);
        if (!fileTree) return false;

        for (uint32 i = 0; i < fileTree->items.getSize(); i++)
        {
            FileFormat::FileTree::Item * item = fileTree->getItem(i);
            File::Info a;
            if (a.analyzeMetaData(item->getMetaData()) && a.isDir())
                dirList.Append(fileTree->getItemFullPath(i));
        }

        // Then sort the directory array
        Strings::CompareString cmp;
        Container::Algorithms<Strings::StringArray>::sortContainer(dirList, cmp);
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
        int     lastProgress;
        uint32  lastIndex;
        uint32  lastCount;
        uint64  lastSize;
        uint32  lastTime;
        int     lastSpeed;
        FILE *  out;
        int     lastLineSize;

        // Helpers
    private:
        bool flushLine(int lineSize, bool flush)
        {
            // Clear the previous line's characters with spaces if required
            if (lastLineSize > lineSize) fprintf(out, "%*c", lastLineSize - lineSize, ' ');
            if (flush) fprintf(out, "\n");
            else       fflush(out);
            lastLineSize = lineSize;
            return true;
        }

    public:
        virtual bool progressed(const Action action, const String & currentFilename, const uint64 sizeDone, const uint64 totalSize, const uint32 index, const uint32 count, const FlushMode mode)
        {
            int lineSize = lastLineSize;
            if (mode == EraseLine) { fprintf(out, "\r"); return flushLine(lineSize, false); }
            if (mode == KeepLine || mode == FlushLine) fprintf(out, "\r");
            // Special signal to display the current filename only
            if (!sizeDone && !totalSize && !index && !count)
            {
                lineSize = fprintf(out, "%s", (const char*)currentFilename);
                return flushLine(lineSize, mode == FlushLine);
            }
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
                lineSize = fprintf(out, "%s: %s [%u/%u]", (const char*)TRANS(getActionName(action)), (const char*)currentFilename, index, count);
                //if (!totalSize) fprintf(stdout, "\n");
                return flushLine(lineSize, mode == FlushLine);
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
                    const int windowSize = 32;
                    lastSpeed = (lastSpeed * (windowSize - 1)) / windowSize + (speed - lastSpeed) / windowSize;
                    int remaining = lastSpeed ? (totalSize - sizeDone) * 1000 / lastSpeed : 0;

                    lineSize = fprintf(out, "%s: %s %2d%%:%s/s (rem: %s) [%u/%u]", (const char*)TRANS(getActionName(action)), (const char*)currentFilename, progress, (const char*)makeLegibleSize(lastSpeed), (const char*)makeLegibleTime(remaining), index, count);
                }
                else
                    lineSize = fprintf(out, "%s: %s [%u/%u]", (const char*)TRANS(getActionName(action)), (const char*)currentFilename, index, count);
                lastProgress = progress;
            }
            lastSize = sizeDone;
            lastTime = currentTime;
            return flushLine(lineSize, mode == FlushLine);
        }

        virtual bool warn(const Action action, const String & currentFilename, const String & message, const uint32 sourceLine = 0)
        {
            warningLog.Append(String::Print("%s(%d): %s", (const char*)currentFilename, sourceLine, (const char*)message));
            fprintf(stderr, TRANS("\nWARNING %s(%d): %s\n"), (const char*)currentFilename, sourceLine, (const char*)message);
            return true;
        }

        ConsoleProgressCallback(const bool standardOutput = true) : lastProgress(0), lastIndex(0), lastCount(0), lastSize(0), lastTime(0), lastSpeed(0), out(standardOutput ? stdout : stderr), lastLineSize(0) {}
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
                callback.progressed(ProgressCallback::Backup, TRANS("...scanning... ") + fileName, 0, 1, 0, count, ProgressCallback::KeepLine);
            return true;
        }
        AllFiles(ProgressCallback & callback) : count(0), callback(callback) {}
    };

    /** Match the excluded files */
    class MatchExcludedFiles
    {
        struct MatchAFile { virtual bool isExcluded(const String & relPath) const { return false; } virtual String getRule() const { return ""; } virtual ~MatchAFile() {} };
        struct MatchSimpleRule : public MatchAFile { String rule; String getRule() const { return rule; } bool isExcluded(const String & relPath) const { return relPath.Find(rule) != -1; } MatchSimpleRule(const String & rule) : rule(rule) {} };
        struct MatchRegEx : public MatchAFile { String regEx; bool inv; mutable void * capts; int capCount; String getRule() const { return "Regexp: " + regEx; } bool isExcluded(const String & relPath) const { int capCount = 0; bool a = relPath.regExFit(regEx, true, &capts, &capCount); return inv ? !a:a; }
                                                MatchRegEx(const String & regEx, const bool inv = false) : regEx(regEx), capts(0), capCount(0), inv(inv) {} ~MatchRegEx() { free(capts); capCount = 0; } };

        typedef Container::NotConstructible<MatchAFile>::IndexList MatchArray;
        MatchArray excMatches, incMatches;

        void buildMatchList(const String & filePath, MatchArray & matches)
        {
            // Get a list of rules in this file
            Strings::StringArray rules(File::Info(filePath, true).getContent());
            for (size_t i = 0; i < rules.getSize(); i++)
            {
                const String & rule = rules.getElementAtUncheckedPosition(i);
                if (!rule.Trimmed()) continue; // Empty lines are ignored

                if (rule.midString(0, 2) == "r/")
                    matches.Append(new MatchRegEx(rule.midString(2, rule.getLength())));
                else if (rule.midString(0, 2) == "R/")
                    matches.Append(new MatchRegEx(rule.midString(2, rule.getLength()), true));
                else matches.Append(new MatchSimpleRule(rule));
            }
        }
    public:
        /** Get the rules used (this is used when verbose) */
        String getRules() const
        {
            String ret = "Excluded:\n";
            for (size_t i = 0; i < excMatches.getSize(); i++)
                ret += excMatches[i].getRule() + "\n";
            ret += "Included after exclusion:\n";
            for (size_t i = 0; i < incMatches.getSize(); i++)
                ret += incMatches[i].getRule() + "\n";
            return ret;
        }
        /** Complete isExcluded function that applies the complete logic of exclusion and then inclusion */
        bool isExcluded(const String & relPath, bool * isExcluded = 0) const
        {
            CondScopeProfiler;
            for (size_t i = 0; i < excMatches.getSize(); i++)
                if (excMatches.getElementAtUncheckedPosition(i)->isExcluded(relPath))
                {
                    if (isExcluded) *isExcluded = true;
                    for (size_t j = 0; j < incMatches.getSize(); j++)
                        if (incMatches.getElementAtUncheckedPosition(j)->isExcluded(relPath))
                            return false;
                    return true;
                }
            return false;
        }

        MatchExcludedFiles()
        {
            if (!Helpers::excludedFilePath) return;
            buildMatchList(Helpers::excludedFilePath, excMatches);
            if (Helpers::includedFilePath) buildMatchList(Helpers::includedFilePath, incMatches);
        }
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
        File::MultiChunk  compMultiChunk, encMultiChunk;
        uint64            compMultiChunkListID, encMultiChunkListID;
        uint64            compPreviousMCID, encPreviousMCID;
        uint64            compMCID, encMCID;

        String               prevParentFolder;
        MatchExcludedFiles   excludes;

        PathIDMapT           prevFilesInDir;
        uint32               prevParentID;
        Utils::OwnPtr<FileFormat::FileTree> fileTree, prevFileTree;
        Utils::MemoryBlock   metadataTmp;
        Utils::ScopePtr<FileFormat::Multichunk> compMultichunk, encMultichunk;
        Utils::ScopePtr<FileFormat::ChunkList> compMultichunkList, encMultichunkList;
        bool                 worthSaving;

        // Check if a file has content to save
        bool hasContent(File::Info & info)
        {
            return info.isFile() && !info.isDir() && !info.isLink();
        }

        // Returns true if the file is different (else fills the previous chunklist ID if applicable)
        bool checkDifferentFile(File::Info & info, const String & strippedFilePath, const String & metadata, uint32 & prevChunkListID)
        {
            CondScopeProfiler;
            if (!prevFileTree) return true;
            uint32 prevItemID = prevFileTree->findItem(strippedFilePath);
            if (prevItemID == prevFileTree->notFound()) return true;

            if (info.hasSimilarMetadata(prevFileTree->getItem(prevItemID)->getMetaData(), File::Info::AllButAccessTime, &metadata))
            {
                prevChunkListID = prevFileTree->getItem(prevItemID)->getChunkListID();
                return false;
            }
            return true;
        }

        virtual bool fileFound(File::Info & info, const String & strippedFilePath)
        {
            if (Frost::exitRequired) return false; // Premature stopping

            CondScopeProfiler;
            if (!fileTree) return WARN_CB(ProgressCallback::Backup, info.name, TRANS("Invalid File Tree found. Are you trying to backup using a bad revision ID ?"));
            // Compute stats first
            uint32 entriesCount = info.getEntriesCount();
            if (info.isDir()) total += entriesCount;
            seen++;

            // Ok, backup this file, if required (we lie about the size here)
            if (!callback.progressed(ProgressCallback::Backup, TRANS("Analysing: ") + info.name, 0, 1, seen, total, ProgressCallback::KeepLine))
                return false;
            bool isExcludedInitially = false;
            if (excludes.isExcluded(strippedFilePath, &isExcludedInitially))
            {   // This file is excluded
                if (!callback.progressed(ProgressCallback::Backup, TRANS("Excluded: ") + info.name, 0, 0, seen, total, ProgressCallback::FlushLine))
                    return false;
                return true;
            } else if (isExcludedInitially)
            {   // This file was excluded, but then later on was reincluded
                if (!callback.progressed(ProgressCallback::Backup, TRANS("Excluded at first then reincluded: ") + info.name, 0, 1, seen, total, dumpLevel ?  ProgressCallback::FlushLine : ProgressCallback::KeepLine))
                    return false;
            }

            // We will extract the metadata out of this file first
            uint32 size = info.getMetaDataEx(metadataTmp.getBuffer(), metadataTmp.getSize());
            if (size != metadataTmp.getSize())
            {
                bool needExtract = size > metadataTmp.getSize();
                if (!metadataTmp.ensureSize(size, true))
                    return WARN_CB(ProgressCallback::Backup, info.name, TRANS("Could not allocate buffer for metadata"));
                if (needExtract) info.getMetaDataEx(metadataTmp.getBuffer(), metadataTmp.getSize());
            }
            String metadata = info.expandMetaData(metadataTmp.getConstBuffer(), metadataTmp.getSize());
            if (dumpLevel > 1)
            {
                String metadataCheck = info.getMetaData();
                if (metadataCheck.fromFirst("/").fromFirst("/") != metadata.fromFirst("/").fromFirst("/"))
                {
                    // They should match perfectly, recompute them to debug them
                    info.getMetaDataEx(metadataTmp.getBuffer(), metadataTmp.getSize());
                }
                fprintf(stdout, "Mismatch in metadata %s vs %s\n", (const char*)metadata, (const char*)metadataCheck);
            }

            // Now handle special file directly
            if (info.isLink())
            {
                // Check if the link point to outside the backup folder (in that case warn the user that it'll not be backuped)
                String backupFullPath = File::Info(folderToBackup).getRealFullPath();
                String currentFullPath = info.getRealFullPath();
                if (currentFullPath.midString(0, backupFullPath.getLength()) != backupFullPath)
                {
                    if (!WARN_CB(ProgressCallback::Backup, info.name, TRANS("Symbolic link points outside of the backup folder, the content will not be saved, only the link")))
                        return false;
                }
            }

            if (strippedFilePath == PathSeparator && fileTree->findItem(strippedFilePath) == fileTree->notFound())
            {
                // Create the root folder if it does not exists yet
                fileTree->appendItem(&FileFormat::FileTree::Item::createNew(false).setMetaData(metadataTmp.getConstBuffer(), (uint16)metadataTmp.getSize()).setChunkListID(0).setParentID(0));
                dirCount++;
                return callback.progressed(ProgressCallback::Backup, info.name, 0, 0, seen, total, ProgressCallback::KeepLine);
            }
            String parentFolder = info.getParentFolder();
            if (parentFolder != prevParentFolder)
            {
                uint32 parentID = fileTree->findItem(strippedFilePath.upToLast("/"));
                if (parentID == fileTree->notFound())
                {
                    WARN_CB(ProgressCallback::Backup, info.name, TRANS("File found in subdir before dir was seen: ") + strippedFilePath);
                    return false;
                }

                // Check if we have remaining entries in the file list (in that case, this means they were deleted)
                if (prevFilesInDir.getSize()) worthSaving = true;

                prevParentID = parentID;
                prevParentFolder = parentFolder;

                String relativeParentPath = File::General::normalizePath(strippedFilePath + "/../").normalizedPath(Platform::Separator, false);
                createFileListInDir(relativeParentPath, prevFilesInDir, prevFileTree);
            }

            // Remove this file from the the previous file list because we've seen it now
            prevFilesInDir.removeValue(strippedFilePath);

            // We'll need the parent directory ID to link with

            // Check if the entry already exists in the database
            uint32 prevChunkListID = 0;
            if (!checkDifferentFile(info, strippedFilePath, metadata, prevChunkListID))
            {
                FileFormat::_CondScopeProfiler profile("SameFile");
                // The file already exists in the previous file tree, so we'll skip chunking and all other process, just copy the relevant informations
                fileTree->appendItem(&FileFormat::FileTree::Item::createNew(false).setMetaData(metadataTmp.getConstBuffer(), (uint16)metadataTmp.getSize())
                                                                                    .setBaseName(info.name)
                                                                                    .setChunkListID(prevChunkListID)
                                                                                    .setParentID(prevParentID+1));
            }
            else
            {
                worthSaving = true;
                if (info.isLink() || info.isDevice() || info.isDir())
                {
                    FileFormat::_CondScopeProfiler profile("LinkDevOrDir");
                    fileTree->appendItem(&FileFormat::FileTree::Item::createNew(false).setMetaData(metadataTmp.getConstBuffer(), (uint16)metadataTmp.getSize())
                                                                                        .setBaseName(info.name).setChunkListID(0).setParentID(prevParentID+1));
                } else if (info.isFile())
                {
                    FileFormat::_CondScopeProfiler profile("FileSave", true);
                    // We need to chunk it
                    File::Chunk temporaryChunk;
                    ::Stream::InputFileStream stream(info.getFullPath());

                    Utils::ScopePtr<FileFormat::FileTree::Item> item(&FileFormat::FileTree::Item::createNew(false));
                    item->setMetaData(metadataTmp.getConstBuffer(), (uint16)metadataTmp.getSize()).setBaseName(info.name).setParentID(prevParentID+1);
                    bool hasData = false;
                    Utils::ScopePtr<FileFormat::ChunkList> fileList(new FileFormat::ChunkList);

                    // Build the list of chunk ID for storage in the DB
                    uint64 streamOffset = stream.currentPosition();
                    uint64 fullSize = stream.fullSize();
                    totalInSize += fullSize;
                    bool chunkCreation = true;
                    while (true)
                    {
                        {   // We want to profile the time it takes to create chunks
                            AccScopeProfiler(3);
                            if (!chunker.createChunk(stream, temporaryChunk)) break;
                        }
                        if (!callback.progressed(ProgressCallback::Backup, info.name, streamOffset, fullSize, seen, total, ProgressCallback::KeepLine))
                            return false;

                        FileFormat::Chunk tmpChunk(temporaryChunk.checksum, temporaryChunk.size);
                        // Ok, got a chunk, let's first figure out if we need to store it in the database
                        uint32 chunkID = Helpers::indexFile.findChunk(tmpChunk);
                        if (chunkID == (uint32)-1)
                        {
                            // The chunk does not exist, so let's append to the current multichunk, and create an entry for it

                            // We need to figure out where this chunk should go
                            double entropy = 0;
                            if (Helpers::entropyThreshold < 1.0)
                            {   // We want to profile the time it takes to compute entropy (if it's worth it)
                                AccScopeProfiler(4);
                                entropy = compMultiChunk.getChunkEntropy(&temporaryChunk);
                            }
                            File::MultiChunk & multiChunk = entropy <= Helpers::entropyThreshold ? compMultiChunk : encMultiChunk;
                            FileFormat::Multichunk * mc = entropy <= Helpers::entropyThreshold ? compMultichunk : encMultichunk;
                            Helpers::ChunkListT & mcl = entropy <= Helpers::entropyThreshold ? compMultichunkList : encMultichunkList;
                            uint64 & previousMCID = entropy <= Helpers::entropyThreshold ? compPreviousMCID : encPreviousMCID;
                            uint64 & currentMCID = entropy <= Helpers::entropyThreshold ? compMCID : encMCID;

                            if (!multiChunk.canFit(temporaryChunk.size))
                            {
                                // Close this multichunk, and apply filters
                                if (!Helpers::closeMultiChunk(backupTo, multiChunk, mcl, &totalOutSize, callback, previousMCID, currentMCID, entropy <= Helpers::entropyThreshold ? Helpers::Default : Helpers::None))
                                    return false;
                            }
                            
                            // If no ID set for the multichunk, set one now
                            if (!currentMCID)
                                currentMCID = Helpers::indexFile.allocateMultichunkID();
                            
                            // Make sure we have a chunk list to store too
                            if (!mcl) mcl = new FileFormat::ChunkList(0, true);

                            // Append to the current multichunk
                            size_t offsetInMC = multiChunk.getSize();
                            uint8 * chunkBuffer = multiChunk.getNextChunkData(temporaryChunk.size, temporaryChunk.checksum);
                            if (!chunkBuffer) return false;

                            memcpy(chunkBuffer, temporaryChunk.data, temporaryChunk.size);

                            // Then add to the chunk list for multichunk
                            chunkID = Helpers::indexFile.allocateChunkID();
                            mcl->appendChunk(chunkID, offsetInMC);
                            // And remember in which multichunk it is in too
                            tmpChunk.multichunkID = currentMCID; // This is safe because it returns the next multichunk's ID until it's closed & saved
                            if (Helpers::indexFile.shouldResizeChunkIndexMap())
                            {
                                if (!callback.progressed(ProgressCallback::Backup, TRANS("Resizing the chunk index table (too small)"), 0, 0, 0, 0, ProgressCallback::KeepLine))
                                    return false;
                                if (!Helpers::indexFile.resizeChunkIndexMap())
                                {
                                    WARN_CB(ProgressCallback::Backup, info.name, TRANS("Error while resizing the chunk index hash table while processing: ") + strippedFilePath);
                                    return false;
                                }
                            }
                            Helpers::indexFile.appendChunk(tmpChunk);

                            Assert(streamOffset + temporaryChunk.size == stream.currentPosition());
                            hasData = true;
                        }
                        fileList->appendChunk(chunkID);
                        streamOffset = stream.currentPosition();
                    }


                    // Ok, done with synchronization, insert in index
                    Helpers::indexFile.appendFileItem(item.Forget(), fileList.Forget());
                    fileCount++;
                }
                else
                {
                    if (!WARN_CB(ProgressCallback::Backup, info.name, TRANS("Non regular type (fifo, pipe or socket) are not backed up.")))
                        return false;
                }
            }
            if (dumpTimeRequired)
            {
                fprintf(stderr, "\n%s", (const char*)Helpers::indexFile.dumpMemStat());
                ::Frost::FileFormat::_AccScopeProfiler<3> createChunk("createChunk"); createChunk.flush();
                ::Frost::FileFormat::_AccScopeProfiler<4> entropy("computeEntropy"); entropy.flush();
                ::Frost::FileFormat::_AccScopeProfiler<2> findChunk("IndexFile::findChunk"); findChunk.flush();
                ::Frost::FileFormat::_AccScopeProfiler<1> appendChunk("IndexFile::appendChunk"); appendChunk.flush();
                dumpTimeRequired = false;
                fprintf(stderr, "\n");
            }

            return callback.progressed(ProgressCallback::Backup, info.name, 0, 0, seen, total, ProgressCallback::FlushLine);
        }

        /** Accessible wrapper from outside to finish the multichunks */
        bool finishMultiChunks()
        {
            if (!finishMultiChunk(compMultiChunk, compMultichunkList, compPreviousMCID, compMCID, Helpers::Default)) return false;
            if (!finishMultiChunk(encMultiChunk, encMultichunkList, encPreviousMCID, encMCID, Helpers::None)) return false;

            // Do we have any deleted file ?
            if (prevFilesInDir.getSize()) worthSaving = true;

            // If we found something to analyze, then the backup worked
            if (totalInSize)
            {
                Frost::backupWorked = true;
                Helpers::indexFile.getMetaData().Append(String::Print("FileCount: %u", fileCount));
                Helpers::indexFile.getMetaData().Append(String::Print("DirCount: %u", dirCount));
                Helpers::indexFile.getMetaData().Append(String::Print("InitialSize: %lld", totalInSize));
                Helpers::indexFile.getMetaData().Append(String::Print("BackupSize: %lld", totalOutSize));
                // If we were appending to a multichunk, remove the previous multichunk
                //!< @todo

                // Then close the index too
                String error = Helpers::indexFile.close();
                if (error)
                {
                    WARN_CB(ProgressCallback::Backup, TRANS("Error"), error);
                    return false;
                }
            }
            // If there was no noticeable changes, don't record this backup
            if (!worthSaving) Helpers::indexFile.backupWasEmpty();
            return callback.progressed(ProgressCallback::Backup, TRANS("Done"), 0, 0, 0, 0, ProgressCallback::FlushLine);
        }



        /** Finish the current multichunk, as it's the end of the backup process */
        bool finishMultiChunk(File::MultiChunk & multiChunk, Helpers::ChunkListT & multiChunkList, uint64 & previousMCID, uint64 & currentMCID, const Helpers::CompressorToUse comp)
        {
            // Check if we started a multichunk (need to close it in that case)
            if (multiChunk.getSize())
            {
                Assert(multiChunkList);
                if (!Helpers::closeMultiChunk(backupTo, multiChunk, multiChunkList, &totalOutSize, callback, previousMCID, currentMCID, comp))
                    return false;
            }
            return true;
        }

        BackupFile(ProgressCallback & callback, const String & backupTo, const unsigned int revID, const String & rootFolder, PurgeStrategy strategy)
            : callback(callback), backupTo(backupTo),
              folderToBackup(rootFolder.normalizedPath(Platform::Separator, true)), revID(revID), seen(0), total(1),
              fileCount(0), dirCount(0), totalInSize(0), totalOutSize(0),
              compMultiChunkListID(0), encMultiChunkListID(0), compPreviousMCID(0), encPreviousMCID(0), compMCID(0), encMCID(0), prevParentFolder("*")
              , prevParentID(0), fileTree(Helpers::indexFile.getFileTree(revID)), prevFileTree(Helpers::indexFile.getFileTree(revID - 1)), worthSaving(false)
        {
            /* TODO
            if (strategy == Slow)
            {
                // Need to reopen last multichunks if it makes any sense
                reopenMultichunk(Helpers::Default, compMultiChunk, compMultiChunkListID, compPreviousMCID);
                reopenMultichunk(Helpers::None, encMultiChunk, encMultiChunkListID, encPreviousMCID);
            }
            */
        }
    };


    class RestoreFile
    {
        ProgressCallback & callback;
        const String & folderTrimmed;
        const String backupFolder;

        OverwritePolicy overwritePolicy;
        Helpers::MultiChunkCache cache;

        Utils::OwnPtr<FileFormat::FileTree> tree;


    public:
        /** Helper method that's extracting a file to the given stream */
        int restoreSingleFile(Stream::OutputStream & stream, String & errorMessage, uint64 chunkListID, const String & filePath, const uint64 fileSize, const uint32 current = 0, const uint32 total = 1)
            {
    #define ERR(Msg) { errorMessage = Msg; return -1; }
            FileFormat::ChunkList * chunkList = Helpers::indexFile.getChunkList((uint32)chunkListID);
            if (!chunkList)
            {
                errorMessage = TRANS("Invalid chunklist for file: ") + filePath;
                return 1;
            }
            const FileFormat::Chunks & chunks = Helpers::indexFile.getTotalChunks();
            for (size_t i = 0; i < chunkList->chunksID.getSize(); i++)
            {
                const uint32 chunkID = chunkList->chunksID.getElementAtUncheckedPosition(i);
                // Then search for this chunk in the consolidated chunks array
                const FileFormat::Chunk * chunkIndex = Helpers::indexFile.findChunk(chunkID);
                if (!chunkIndex)
                    ERR(TRANS("While processing this file, it's missing chunk index: ") + chunkID);

                // Find the multichunk who's storing this chunk
                const FileFormat::Multichunk * mchunk = Helpers::indexFile.getMultichunk(chunkIndex->multichunkID);
                if (!mchunk)
                    ERR(TRANS("Missing multichunk index for this file: ") + chunkIndex->multichunkID);

                // We need the offset in the multichunk's chunklist so it's faster to restore
                FileFormat::ChunkList * mcChunkList = Helpers::indexFile.getChunkList(mchunk->listID);
                size_t chunkOffset = (size_t)-1;
                if (mcChunkList) chunkOffset = mcChunkList->getChunkOffset(chunkID);

                errorMessage = "";
                File::Chunk * chunk = Helpers::extractChunkBin(errorMessage, backupFolder, mchunk->getFileName(), mchunk->UID, chunkOffset, chunkIndex->checksum, Helpers::indexFile.getFilterArguments().getArgument(mchunk->filterArgIndex), cache, callback);
                if (!chunk || errorMessage) return -1;

                // Ok, if we got a chunk, let's save it
                if (!chunk)
                    ERR(TRANS("Missing chunk for this file: ") + chunkID);
                if (stream.write(chunk->data, chunk->size) != (uint64)chunk->size)
                    ERR(TRANS("Can't write the file (disk full ?)"));

                if (!callback.progressed(ProgressCallback::Restore, folderTrimmed + filePath, stream.currentPosition(), fileSize, current, total, stream.currentPosition() != fileSize ? ProgressCallback::KeepLine : ProgressCallback::FlushLine))
                    ERR(TRANS("Interrupted in output"));
            }
            return 0; // Done
        }


        /** File removed, let's apply the same on the file system */
        int removeFile(const String & filePath, String & errorMessage, const uint32 current, const uint32 total)
        {
        #define WarnAndReturn(Msg) WARN_CB(ProgressCallback::Restore, filePath, TRANS( Msg )) ? 1 : -1
            File::Info outFile(folderTrimmed + filePath);
            if (!outFile.doesExist()) return 0; // Ignore deleted file that does not exists on the system.

            // We need to delete the file from the system, depending on the overwrite policy
            if (overwritePolicy == No)
                return WarnAndReturn("This file already exists and is deleted in the backup, and no overwrite specified");
            if (overwritePolicy == Update && outFile.modification < File::Info(outFile.getFullPath()).modification)
                return WarnAndReturn("This file already exists in the restoring folder and is newer than the backup which is deleted");

            if (!File::Info(outFile.getFullPath()).remove())
                ERR(TRANS("Can not remove file on the system: ") + filePath);
            return 0;
        }

        /** Restore a single file from the database
            @return 0 on success, -1 on error, 1 on warning */
        int restoreFile(const uint32 fileIndex, String & errorMessage, const uint32 current, const uint32 total)
        {
            const String & filePath = tree->getItemFullPath(fileIndex);
            const FileFormat::FileTree::Item * item = tree->getItem(fileIndex);

            // We need to figure out if the file is a regular file, we do so by checking its metadata field
            File::Info outFile(folderTrimmed + filePath);
            if (!outFile.analyzeMetaData(item->getMetaData()))
            {
                errorMessage = TRANS("Bad metadata found in database");
                return WarnAndReturn("Bad metadata for this file, it's ignored for restoring");
            }

            if (!callback.progressed(ProgressCallback::Restore, folderTrimmed + filePath, 0, outFile.size, current, total, ProgressCallback::KeepLine))
                ERR(TRANS("Interrupted in output"));


            // Check if the file already exists
            if (outFile.doesExist())
            {
                if (item->getMetaData() != File::Info(outFile.getFullPath()).getMetaData())
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
                ::Stream::OutputFileStream stream(outFile.getFullPath());
                int ret = restoreSingleFile(stream, errorMessage, item->getChunkListID(), filePath, outFile.size, current, total);
                if (ret == 1) return WarnAndReturn(errorMessage);
                if (ret < 0) return ret;
            }
            else
            {
                if (!callback.progressed(ProgressCallback::Restore, outFile.getFullPath(), 0, 0, current, total, ProgressCallback::FlushLine))
                    ERR(TRANS("Interrupted in output"));
            }

            // Then restore the metadata here
            if (!outFile.setMetaData(item->getMetaData()))
            {
                errorMessage = TRANS("Failed to restore metadata");
                return WarnAndReturn("Failed to restore the file's metadata");
            }
        #undef WarnAndReturn
        #undef ERR
            return 0;
        }


        RestoreFile(ProgressCallback & callback, const String & folderTrimmed, const String & backupFolder, OverwritePolicy policy, const size_t maxCacheSize, const uint32 revisionID)
            : callback(callback), folderTrimmed(folderTrimmed), backupFolder(backupFolder.normalizedPath(Platform::Separator, true)),
              overwritePolicy(policy), cache(maxCacheSize)
            , tree(Helpers::indexFile.getFileTree(revisionID))
        {}
    };

    // Backup the given folder
    String backupFolder(const String & folderToBackup, const String & backupTo, const unsigned int revisionID, ProgressCallback & callback, const PurgeStrategy strategy)
    {
        // The complete logic is here

        File::FileItemArray items; File::Scanner::FileFilters filters;
        BackupFile processor(callback, backupTo, revisionID, folderToBackup, strategy);
        if (dumpLevel)
            callback.progressed(ProgressCallback::Backup, TRANS("Exclusion and inclusion rules\n=============================\n") + processor.excludes.getRules(), 0, 0, 0, 0, ProgressCallback::FlushLine);
        // Initiate the pump
        // Step one, we'll make a stack of data to find out what to backup
        if (!callback.progressed(ProgressCallback::Backup, TRANS("...scanning..."), 0, 1, 0, 1, ProgressCallback::KeepLine))
            return TRANS("Error with output");
        File::Info rootFolder(folderToBackup, true);
        processor.fileFound(rootFolder, PathSeparator);
        File::Scanner::EventIterator iterator(true, processor);

        if (File::Scanner::scanFolderGeneric(folderToBackup, ".", items, iterator, false) && !exitRequired)
            return TRANS("Can't scan the backup folder");

        if (!processor.finishMultiChunks())
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
        const FileFormat::Catalog * catalog = Helpers::indexFile.getCatalog();
        uint32 count = 0;
        FileFormat::MetaData md;
        while (catalog)
        {
            if (catalog->time >= startTime.Second() && catalog->time <= endTime.Second())
            {
                if (!Helpers::indexFile.Load(md, catalog->optionMetadata))
                    fprintf(stdout, (const char*)TRANS("Revision %d happened on %s\n"), (int)catalog->revision, (const char*)::Time::Time(catalog->time).toDate());
                else
                {
                    const String & initialSize = md.findKey("InitialSize").fromFirst(": ");
                    if (initialSize)
                    {
                        const int64 is = initialSize, bs = md.findKey("BackupSize").fromFirst(": ");
                        // Extract the total backup size from the metadata
                        fprintf(stdout, (const char*)TRANS("Revision %d happened on %s, linked %u files and %u directories, cumulative size %s (backup is %s, saved %d%%)\n"), (int)catalog->revision, (const char*)::Time::Time(catalog->time).toDate(),
                                                     (uint32)md.findKey("FileCount").fromFirst(": "), (uint32)md.findKey("DirCount").fromFirst(": "), (const char*)makeLegibleSize(is),
                                                     (const char*)makeLegibleSize(bs), 100 - (100 * (uint64)bs) / (uint64)is);
                    }
                    else
                        // Extract the total backup size from the metadata
                        fprintf(stdout, (const char*)TRANS("Revision %d happened on %s, linked %u files and %u directories, cumulative size %s (backup is %s, saved 100%%)\n"), (int)catalog->revision, (const char*)::Time::Time(catalog->time).toDate(),
                                                     (uint32)md.findKey("FileCount").fromFirst(": "), (uint32)md.findKey("DirCount").fromFirst(": "), (const char*)makeLegibleSize((int64)initialSize),
                                                     (const char*)makeLegibleSize((int64)md.findKey("BackupSize").fromFirst(": ")));

                }
                // Then make a list of files for this revision
                PathIDMapT fileList;
                if (withList && createFileListInRev(fileList, catalog->revision))
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
                            filePaths.Append(String::Print(TRANS("%s %s [rev%u:id%u]"), (const char*)metaData, (const char*)*(iter.getKey()), (unsigned int)catalog->revision, (unsigned int)*(*iter)));
                        }
                        else
                            filePaths.Append(String::Print(TRANS("%s [rev%u:id%u]"), (const char*)*(iter.getKey()), (unsigned int)catalog->revision, (unsigned int)*(*iter)));
                        ++iter;
                    }
                    // Sort the file list
                    CompareStringPath cs;
                    Container::Algorithms<Strings::StringArray>::sortContainer(filePaths, cs);
                    // Show it
                    for (size_t j = 0; j < filePaths.getSize(); j++)
                        fprintf(stdout, "\t%s\n", (const char*)filePaths[j]);
                }
                count++;
            }
            if (!catalog->previous.fileOffset() || !Helpers::indexFile.Map(catalog, catalog->previous)) break;
        }
        if (!count)
            fprintf(stdout, "%s", (const char*)TRANS("No revision found\n"));
        return count;
    }

    // Purge old backups
    String purgeBackup(const String & chunkFolder, ProgressCallback & callback, const PurgeStrategy strategy, const unsigned int upToRevision)
    {
        // First, we need to figure out the chunks that are in the given revision but in no other revision (orphans)
        if (!callback.progressed(ProgressCallback::Purge, TRANS("...scanning..."), 0, 1, 0, 1, ProgressCallback::KeepLine))
            return TRANS("Error with output");

        /* The basic algorithm here is to build 3 arrays of chunks successively
           The first chunk array (All) contains the current status of the index file (that is including all revisions' chunks)
           The second chunk array (B) is build by concatenating the chunk in revisions up to given one
           The third chunk array (C) is build by concatenating the chunk in revision starting from the given one + 1 to the last version.
           Finally a last chunk array is build so that it contains chunks in (B) that are not in (C)
           While doing so, all multichunks infering this list is remembered.

           For each remembered multichunk, we assert a "remove" value, that is equal to the number of chunks to remove in this
           multichunk divided by the number of chunks in the multichunk.
           If this ratio is 1.0 then we can remove the multichunk.
           Else, we simple sort the list of multichunks by this ratio.

           The multichunk with the biggest ratio will be repacked first until all multichunk are respecting the given strategy threshold.

           Finally, a new index file is rewritten with the remaining stuff from the initial file. */
        typedef Container::PlainOldData<uint32>::Array UIDArray;
        typedef Container::PlainOldData<uint16>::Array MCUIDArray;
        UIDArray chunksInPrev, chunksInNext;
        UIDArray chunkListsToRemove;
        unsigned int rev = 1;
        while (rev <= upToRevision)
        {
            Utils::OwnPtr<FileFormat::FileTree> ft(Helpers::indexFile.getFileTree(rev));
            if (!ft) { ++rev; continue; } // The first revisions might be missing already in the archive already

            for (uint32 file = 0; file < ft->notFound(); file++) // Not found is also the size of the items list
            {
                uint32 chunkListID = ft->getItem(file)->getChunkListID();
                chunkListsToRemove.Append(chunkListID);
                // Then query this list of chunks for all chunks ID to account for
                FileFormat::ChunkList * cl = Helpers::indexFile.getChunkList(chunkListID);

                if (cl)
                {
                    for (size_t i = 0; i < cl->chunksID.getSize(); i++)
                        chunksInPrev.Append(cl->chunksID[i]);
                }
            }
            ++rev;
        }
        if (!chunksInPrev.getSize())  // No chunks found ? We're done
            return "";

        while (rev <= Helpers::indexFile.getCurrentRevision())
        {
            Utils::OwnPtr<FileFormat::FileTree> ft(Helpers::indexFile.getFileTree(rev));
            if (!ft)
                return TRANS("Could not find the given revision: ") + rev;

            for (uint32 file = 0; file < ft->notFound(); file++) // Not found is also the size of the items list
            {
                uint32 chunkListID = ft->getItem(file)->getChunkListID();
                // Then query this list of chunks for all chunks ID to account for
                FileFormat::ChunkList * cl = Helpers::indexFile.getChunkList(chunkListID);

                if (cl)
                {
                    for (size_t i = 0; i < cl->chunksID.getSize(); i++)
                        chunksInNext.Append(cl->chunksID[i]);
                }
            }
            ++rev;
        }


        if (!callback.progressed(ProgressCallback::Purge, TRANS("...building list of chunks to remove..."), 0, 1, 0, 1, ProgressCallback::KeepLine))
            return TRANS("Error with output");

        // Ok, now the compute the remaining chunk array (the chunks that need removing)
        struct CompareUint32
        {
            static inline int compareData(const uint32 a, const uint32 b) { return a < b ? -1 : a==b ? 0 : 1; }
        } comp;
        Container::Algorithms<UIDArray>::sortContainer(chunksInPrev, comp);
        Container::Algorithms<UIDArray>::sortContainer(chunksInNext, comp);

        UIDArray removeChunks, keepChunks;
        MCUIDArray multichunkToRework;

        const uint32 allChunks = (uint32)Helpers::indexFile.getTotalChunks().chunks.getSize();
        for (size_t i = 0; i < chunksInPrev.getSize(); i++)
        {
            const uint32 chunkUID = chunksInPrev.getElementAtUncheckedPosition(i);
            if (chunksInNext.indexOfSorted(chunkUID) == chunksInNext.getSize())
            { // This chunk was not found in the later revision, and can be removed
                const FileFormat::Chunk * chunk = Helpers::indexFile.findChunk(chunkUID);
                if (!chunk)
                    return TRANS("Unexpected: Chunk not found with UID ") + chunkUID;
                if (removeChunks.indexOfSorted(chunkUID) == removeChunks.getSize())
                {
                    removeChunks.insertSorted(chunkUID);
                    multichunkToRework.appendIfNotPresent(chunk->multichunkID);
                }
            } else if (keepChunks.indexOfSorted(chunkUID) == keepChunks.getSize())
            {   // Remember the chunks we must keep
                keepChunks.insertSorted(chunkUID);
            }
        }

        // Ok, great, now we have the list of chunks to remove, let's find out the multichunks where to remove them
        if (!callback.progressed(ProgressCallback::Purge, TRANS("... found orphans chunks ..."), 0, 0, (uint32)removeChunks.getSize(), allChunks, ProgressCallback::FlushLine))
            return TRANS("Error with output");

        // Then analyze and sort all multichunks for finding out which one to rework first
        struct MCSortRank
        {
            float rank; // The higher to 1.0, the more important it is to remove
            uint16 id; // When the former are the same, sort on the lowest ID first
            bool operator == (const MCSortRank & k) const { return memcmp(this, &k, sizeof(*this)) == 0; }
            bool operator <= (const MCSortRank & k) const { return (rank < k.rank) || (rank == k.rank && id <= k.id); }

            MCSortRank(const float rank = 0, const uint16 id = 0) : rank(rank), id(id) {}
        };
        typedef Container::PlainOldData<MCSortRank>::Array MultichunkUsageT;
        MultichunkUsageT multichunksSorter;
        for (size_t i = 0; i < multichunkToRework.getSize(); i++)
        {
            FileFormat::Multichunk * mc = Helpers::indexFile.getMultichunk(multichunkToRework[i]);
            if (!mc) return TRANS("Unexpected: Multichunk not found with UID ") + (uint32)multichunkToRework[i];

            FileFormat::ChunkList * cl = Helpers::indexFile.getChunkList(mc->listID);
            uint32 removedChunksCount = 0;
            for (size_t c = 0; c < cl->chunksID.getSize(); c++)
            {
                if (removeChunks.indexOfSorted(cl->chunksID[c]) != removeChunks.getSize()) removedChunksCount++;
            }

            multichunksSorter.insertSorted(MCSortRank((float)removedChunksCount / cl->chunksID.getSize(), multichunkToRework[i]));
        }

        if (!callback.progressed(ProgressCallback::Purge, TRANS("... found affected multichunks ..."), 0, 0, (uint32)multichunksSorter.getSize(), Helpers::indexFile.getMultichunkCount(), ProgressCallback::FlushLine))
            return TRANS("Error with output");

        // From now on, we'll start to build a new IndexFile starting from the next revision after purging.
        FileFormat::IndexFile newIndex;
        String initialBackupPath = Helpers::indexFile.getFirstMetaData().getBackupPath();
        String tempIndexPath = chunkFolder+"/__purgeIndex.frost";
        String error = newIndex.createNew(tempIndexPath, Helpers::indexFile.getCipheredMasterKey(), initialBackupPath);
        if (error) return error;

        // We need to pre-copy all the filter arguments from the current index to the new index since they might get modified while purging
        newIndex.getFilterArguments().arguments = Helpers::indexFile.getFilterArguments().arguments;
        newIndex.getFilterArguments().modified = true;


        // Ok, now we have a sorted list of multichunks (from 0 (no chunks to remove) to 1 (all chunks to remove))
        // Let's act accordingly now to remove or rework the given multichunks.
        // We want to be atomic here, so we'll not remove any multichunk yet until we are sure the new (purged) index file is good
        // So we store the list of multichunks to remove (they'll be removed if no error happened before leaving this method)

        // We must have a kind of "previousMultichunkID => newMultichunkID" map because after merging they'll have a new UID
        // Hopefully, the new ID is always equal to a previous one, and there should be less (or exactly as many) multichunks after purging.
        // So, we simply use an index for the ID in the multichunkToRework array.
        MCUIDArray multichunksToRemove;
        // 2 multichunks need to be stored in the cache
        Helpers::MultiChunkCache cache(64*1024*1024);

        // The multichunks we are working with
        Utils::ScopePtr<FileFormat::Multichunk>  compMultichunk(new FileFormat::Multichunk), encMultichunk(new FileFormat::Multichunk);
        Utils::ScopePtr<FileFormat::ChunkList>  compMultichunkList(new FileFormat::ChunkList(0, true)), encMultichunkList(new FileFormat::ChunkList(0, true));
        File::MultiChunk compMC, encMC;
        FileFormat::ChunkLists &  newChunkList = *newIndex.getChunkLists();
        FileFormat::Multichunks & newMultichunks = *newIndex.getMultichunks();

        // RAII for cleaning any multichunk we have created upon failing purging
        struct CleanMultichunksOnExit
        {
            Strings::StringArray    createdMultichunks;
            void success() { createdMultichunks.Clear(); }
            void appendMC(const String & path) { createdMultichunks.Append(path); }
            ~CleanMultichunksOnExit() { for (size_t i = 0; i < createdMultichunks.getSize(); i++) File::Info(createdMultichunks[i], true).remove(); }
        } mcGuard;

        float purgeThreshold = (int)strategy / 100.0f;
        for (size_t i = multichunksSorter.getSize(); i; --i)
        {
            MCSortRank & rank = multichunksSorter.getElementAtUncheckedPosition(i-1);
            // Special case: all chunks must be removed
            if (rank.rank == 1.0f)
            {
                multichunksToRemove.Append(rank.id);
                multichunksSorter.Remove(i-1);
                i = multichunksSorter.getSize();
                continue;
            }

            // If we've reached the expected strategy goal, we stop the (hard) work.
            if (rank.rank <= purgeThreshold) break;
            // Ok, we need to rework a multichunk here
            // We have opened 2 multichunks (one for compression + encryption, one for the encryption only) already
            // We'll be selecting the multichunk to store into depending on the filtering used for the current multichunk.
            FileFormat::Multichunk * currentMC = Helpers::indexFile.getMultichunk(rank.id);
            if (!currentMC) return TRANS("Error: Could not find multichunk with ID: ") + rank.id;
            String filterMode = Helpers::indexFile.getFilterArgumentForMultichunk(rank.id);

            bool shouldCompress = filterMode.fromTo(":", ":") != "none";
            Utils::ScopePtr<FileFormat::Multichunk> & outMC = !shouldCompress ? encMultichunk : compMultichunk;
            Utils::ScopePtr<FileFormat::ChunkList> &  outCL = !shouldCompress ? encMultichunkList : compMultichunkList;
            File::MultiChunk &      destMC = !shouldCompress ? encMC : compMC;

            // If UID not assigned yet, let's assign it now, we are reusing an existing multichunk
            if (outCL->UID == 0) { outCL->UID = currentMC->listID; outMC->UID = currentMC->UID; outMC->listID = outCL->UID; }

            // Then we'll open the chunklist refered by this multichunk and for each chunk in the list assert if it's used
            // or not. If it's used, we'll extract the chunk and add to the current multichunk.
            FileFormat::ChunkList * cl = Helpers::indexFile.getChunkList(currentMC->listID);
            if (!cl)    return TRANS("Errror: Could not find the list of chunks with ID: ") + currentMC->listID;

            for (size_t c = 0; c < cl->chunksID.getSize(); c++)
            {
                const uint32 chunkID = cl->chunksID.getElementAtUncheckedPosition(c);
                if (removeChunks.indexOfSorted(chunkID) == removeChunks.getSize())
                {
                    // We should keep this chunk
                    const FileFormat::Chunk * chunk = Helpers::indexFile.findChunk(chunkID);
                    if (!chunk) return TRANS("Error: Could not find the chunk with ID: ") + chunkID;

                    // Copy data to the new multichunk
                    //=================================================
                    File::Chunk * chunkData = Helpers::extractChunkBin(error, chunkFolder, currentMC->getFileName(), rank.id, cl->offsets.getElementAtUncheckedPosition(c), chunk->checksum, filterMode, cache, callback);
                    if (!chunkData) return TRANS("Error: Could not extract chunk data for ID: ") + chunkID;

                    // If the current multichunk is full, we have to close it, compress it and encrypt it, and open a new one.
                    if (!destMC.canFit(chunkData->size))
                    {
                        // Close this multichunk, and apply filters
                        KeyFactory::KeyT chunkHash;
                        String chunkFile = chunkFolder;
                        if (!Helpers::closeMultiChunkBin(chunkFile, destMC, 0, callback, shouldCompress ? Helpers::Default : Helpers::None, chunkHash))
                            return TRANS("Error: Closing multichunk failed");

                        mcGuard.appendMC(chunkFile);
                        outMC->filterArgIndex = Helpers::getFilterArgumentIndex(shouldCompress ? Helpers::Default : Helpers::None, &newIndex);
                        memcpy(outMC->checksum, chunkHash, ArrSz(chunkHash));

                        uint16 mcID = outMC->UID;
                        if (mcID == currentMC->UID)
                        {
                            // This should never happen, since we are removing chunks, we should be able to fit at least the same number of chunks in a multichunk
                            return TRANS("Error: We should be able to reassign ID for multichunks");
                        }
                        newChunkList.storeValue(outMC->listID, outCL.Forget());
                        newMultichunks.storeValue(mcID, outMC.Forget());
                        // Reset it now for next chunk
                        outCL = new FileFormat::ChunkList(0, true);
                        outMC = new FileFormat::Multichunk;
                        destMC.Reset();
                        // Need to make sure the multichunk ID for this chunk is different from the one we started with
                        outCL->UID = currentMC->listID; outMC->UID = currentMC->UID; outMC->listID = outCL->UID;
                    }
                    size_t offsetInMC = destMC.getSize();
                    uint8 * chunkBuffer = destMC.getNextChunkData(chunkData->size, chunkData->checksum);
                    if (!chunkBuffer) return TRANS("Error: Could not get a free buffer to store the chunk with ID: ") + chunkID;

                    memcpy(chunkBuffer, chunkData->data, chunkData->size);
                    //================ Done ===========================

                    // Then deal with meta information here
                    // We should also store the chunkID for all written chunks in order to map them to new multichunkID to a list.
                    // To avoid consuming useless memory, we mutate the chunk's MultichunkID directly in the current index file's consolidated array to the new value.
                    outCL->appendChunk(chunkID, offsetInMC);
                    // Trick here is correct, because we know the chunk is in memory and not on the file
                    const_cast<FileFormat::Chunk*>(chunk)->multichunkID = outMC->UID;
                }
            }


            // This is finally repeated until all multichunks are processed (only in case of slow purging strategy or level below threshold)
            if (!callback.progressed(ProgressCallback::Purge, String::Print(TRANS("Processed multichunk %s with ratio %g"), (const char*)currentMC->getFileName(), rank.rank), 0, 0, multichunksSorter.getSize() - i, multichunksSorter.getSize(), ProgressCallback::KeepLine))
                return TRANS("Interrupted in output");

            // Mark this multichunk so it's removed
            multichunksToRemove.Append(rank.id);
        }

        if (!callback.progressed(ProgressCallback::Purge, TRANS("Done processed multichunks...                                                  "), 0, 0, multichunksSorter.getSize(), multichunksSorter.getSize(), ProgressCallback::KeepLine))
            return TRANS("Interrupted in output");

        // Finally, close all remaining multichunks
        KeyFactory::KeyT chunkHash;
        if (encMC.getSize())
        {
            String chunkFile = chunkFolder;
            if (!Helpers::closeMultiChunkBin(chunkFile, encMC, 0, callback, Helpers::None, chunkHash))
                return TRANS("Error: Closing multichunk failed");

            mcGuard.appendMC(chunkFile);
            encMultichunk->filterArgIndex = getFilterArgumentIndex(Helpers::None, &newIndex);
            memcpy(encMultichunk->checksum, chunkHash, ArrSz(chunkHash));

            newChunkList.storeValue(encMultichunk->listID, encMultichunkList.Forget());
            uint16 encID = encMultichunk->UID;
            newMultichunks.storeValue(encID, encMultichunk.Forget());
        }
        if (compMC.getSize())
        {
            String chunkFile = chunkFolder;
            if (!Helpers::closeMultiChunkBin(chunkFile, compMC, 0, callback, Helpers::Default, chunkHash))
                return TRANS("Error: Closing multichunk failed");

            mcGuard.appendMC(chunkFile);
            compMultichunk->filterArgIndex = getFilterArgumentIndex(Helpers::Default, &newIndex);
            memcpy(compMultichunk->checksum, chunkHash, ArrSz(chunkHash));

            newChunkList.storeValue(compMultichunk->listID, compMultichunkList.Forget());
            uint16 compID = compMultichunk->UID;
            newMultichunks.storeValue(compID, compMultichunk.Forget());
        }

        // In the new index file, the first revision will be 1 (and not revisionID + 1)
        // So, first add the chunks for the next revisions. Because we want to be smart, we'll only pack a single chunk array in the file for the first
        // revision, it'll then be a mix of the chunks used in the next revisions plus the actual revisionID+1's chunks
        for (size_t i = 0; i < keepChunks.getSize(); i++)
        {
            const uint32 chunkUID = keepChunks.getElementAtUncheckedPosition(i);
            const FileFormat::Chunk * chunk = Helpers::indexFile.findChunk(chunkUID);
            newIndex.appendChunk(const_cast<FileFormat::Chunk &>(*chunk), chunkUID); // We force the UID as we don't want to mutate all chunklists later on
        }
        // Ok, let's append all remaining chunk for this revision
        uint32 maxRev = Helpers::indexFile.getCurrentRevision() - upToRevision;
        for (uint32 rev = upToRevision+1; rev <= Helpers::indexFile.getCurrentRevision(); rev++)
        {
            FileFormat::Chunks chunks;
            const FileFormat::Catalog * catalog = Helpers::indexFile.getCatalogForRevision(rev);
            if (!catalog)
                return TRANS("Error while fetching catalog for revision: ") + rev;
            if (!Helpers::indexFile.LoadRO(chunks, catalog->chunks))
                return TRANS("Error while fetching chunks for revision: ") + rev;
            // Chunks to save for this revision
            for (size_t c = 0; c < chunks.chunks.getSize(); c++)
                newIndex.appendChunk(const_cast<FileFormat::Chunk &>(chunks.chunks[c]), chunks.chunks[c].UID);

            // We need to copy the ChunkLists, Multichunks, Metadata, FileTree, FilterArgument
            // So copy the chunklists for this revision too
            //    First: Read all chunk lists now
            FileFormat::Offset clOff = catalog->chunkLists;
            for (uint32 i = 0; i < catalog->chunkListsCount; i++)
            {
                Utils::ScopePtr<FileFormat::ChunkList> cl = new FileFormat::ChunkList();
                if (!cl) return TRANS("Out of memory for chunklist");
                if (!Helpers::indexFile.Load(*cl, clOff)) return TRANS("Error: Could not load chunk list");

                // Then save them in the new file
                if (!newIndex.getChunkLists()->storeValue(cl->UID, cl)) return TRANS("Error: Could not store the chunk list in new list");
                clOff.fileOffset(clOff.fileOffset() + cl->getSize());
                cl.Forget();
            }

            // Need to load all the chunk list from the file tree too
            FileFormat::FileTree ft(rev, true);
            if (!Helpers::indexFile.Load(ft, catalog->fileTree)) return TRANS("Error: Could not load the file tree for revision: ") + rev;
            for (size_t i = 0; i < ft.items.getSize(); i++)
            {
                uint32 clID = ft.items[i].getChunkListID();
                if (!clID) continue;
                FileFormat::ChunkList * cl = Helpers::indexFile.getChunkList(clID);
                if (!cl) return TRANS("Error: Could not find the chunk list for file: ") + ft.items[i].getBaseName();

                // Then save them in the new file
                if (!newIndex.getChunkLists()->storeValue(cl->UID, new FileFormat::ChunkList(*cl))) return TRANS("Error: Could not store the chunk list in new list");
                clOff.fileOffset(clOff.fileOffset() + cl->getSize());
            }

            // Then read all previous multichunks now
            FileFormat::Offset mcOff = catalog->multichunks;
            for (uint32 i = 0; i < catalog->multichunksCount; i++)
            {
                Utils::ScopePtr<FileFormat::Multichunk> mc = new FileFormat::Multichunk();
                if (!mc) return TRANS("Out of memory for multichunk");
                if (!Helpers::indexFile.Load(*mc, mcOff)) return TRANS("Error: Could not load multichunk");

                // Then save them in the new file
                if (!newIndex.getMultichunks()->storeValue(mc->UID, mc)) return TRANS("Error: Could not store the multichunk in new table");
                mcOff.fileOffset(mcOff.fileOffset() + mc->getSize());
                mc.Forget();
            }

            // Next do the metadata
            if (catalog->optionMetadata.fileOffset())
            {
                if (!Helpers::indexFile.LoadRO(newIndex.getMetaData(), catalog->optionMetadata)) TRANS("Error: Could not load metadata for revision: ") + rev;
                newIndex.getMetaData().modified = true;
            }
            // Filter arguments should already be copied to the last index value
            
            // Finally save the file tree
            Utils::OwnPtr<FileFormat::FileTree> newFT = newIndex.getFileTree(rev - upToRevision);
            if (!Helpers::indexFile.Load(*newFT, catalog->fileTree)) TRANS("Error: Could not load the file tree for revision: ") + rev;
            // Fix the file tree revision too
            newFT->revision = rev - upToRevision; // We are shifting the revision number here, so we must account for it

            if (!callback.progressed(ProgressCallback::Purge, TRANS("... done saving of revision ..."), 0, 0, (uint32)rev - upToRevision, maxRev, ProgressCallback::FlushLine))
                return TRANS("Error with output");

            // Ok, let's save this revision back to the nex index file
            error = newIndex.close();
            if (error) return error;
            // Reopen for a new revision
            error = newIndex.readFile(tempIndexPath, true);
            if (error) return error;
            // Start a new revision in this file
            if (!newIndex.startNewRevision(rev - upToRevision + 1)) return TRANS("Could not start new revision :") + (rev - upToRevision + 1);
        }

        newIndex.backupWasEmpty();
        error = newIndex.close();
        if (error) return error;

        // Great, it should be finished by now, let's remove the initial index file, and useless multichunks
        if (dumpLevel < 2)
        {
            for (size_t i = 0; i < multichunksToRemove.getSize(); i++)
            {
                uint16 mcID = multichunksToRemove[i];
                FileFormat::Multichunk * mc = Helpers::indexFile.getMultichunk(mcID);
                if (mc) File::Info(chunkFolder + mc->getFileName(), true).remove();
            }
            Helpers::indexFile.close();
            File::Info(tempIndexPath, true).moveTo(chunkFolder + DEFAULT_INDEX);
        }

        if (!callback.progressed(ProgressCallback::Purge, TRANS("... purge finished and saved ..."), 0, 0, maxRev, maxRev, ProgressCallback::FlushLine))
            return TRANS("Error with output");

        mcGuard.success();
        return "";
    }



    // Restore a backup to the given folder
    String restoreBackup(const String & folderToRestore, const String & restoreFrom, const unsigned int revisionID, ProgressCallback & callback, const size_t maxCacheSize)
    {
        // The complete logic is here
        if (!callback.progressed(ProgressCallback::Restore, TRANS("...analysing backup..."), 0, 1, 0, 1, ProgressCallback::KeepLine))
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

        uint32 total = (uint32)fileList.getSize(), current = 0;
        String lastPath = "*";
        RestoreFile restore(callback, folderTrimmed, restoreFrom, overwritePolicy, maxCacheSize, revisionID);

        // Compute a list of files in the restoring folder, as we might need to remove them later on
        if (!callback.progressed(ProgressCallback::Restore, TRANS("...analysing restore folder..."), 0, 1, 0, 1, ProgressCallback::KeepLine))
            return TRANS("Error in output");

        // Need to create all required directories first
        Strings::StringArray dirs;
        if (!createDirListInRev(dirs, revisionID))
            return TRANS("Can not get the directory list from this revision");

        String errorMessage;

        for (size_t i = 0; i < dirs.getSize(); i++)
        {
            const String & dir = dirs.getElementAtUncheckedPosition(i);
            FileMDEntry * entry = fileList.getValue(dir);
            if (!entry)
                return TRANS("Inconsistency in the file list for restoring the directory: ")+dir;
            if (restore.restoreFile(*(entry), errorMessage, current, total) < 0)
                return errorMessage;
            ++current;
        }

        File::FileItemArray files;
        File::Scanner::FileFilters filters;
        File::Scanner::scanFolderFilename(folderTrimmed, "/", files, filters, true); // If it fails, we don't care


        PathIDMapT::IterT iter = fileList.getFirstIterator();
        while (iter.isValid())
        {
            lastPath = *iter.getKey();
            File::Info dir(folderTrimmed + lastPath);

            // Remove the file from the current file list if found (this is O(N) search here, could be faster if sorting the array)
            for (size_t i = 0; i < files.getSize(); i++)
                if (files[i].name == lastPath)
                {
                    files.Remove(i); break;
                }

            if (dir.isDir()) { ++iter; continue; }

            // Show the list of directories to restore
            if (!callback.progressed(ProgressCallback::Restore, folderTrimmed + lastPath, 0, 1, current, total, ProgressCallback::KeepLine))
                return TRANS("Interrupted in output");

            if (restore.restoreFile(*(*iter), errorMessage, current, total) < 0)
                return errorMessage;

            ++current;
            ++iter;
        };

        // All files that remains in the file array should be deleted now as they are not in backup
        for (size_t i = 0; i < files.getSize(); i++)
        {
            lastPath = folderTrimmed + files[i].name;
            if (restore.removeFile(lastPath, errorMessage, current, total) < 0)
                return errorMessage;
        }

        // Ok done restoring
        return "";
    }
    // Restore a backed up file
    String restoreSingleFile(const String & fileToRestore, const String & restoreFrom, const unsigned int revisionID, ProgressCallback & callback, const size_t maxCacheSize)
    {
        // The complete logic is here
        if (!callback.progressed(ProgressCallback::Restore, TRANS("...analysing backup..."), 0, 1, 0, 1, ProgressCallback::KeepLine))
            return TRANS("Error in output");

        // We have to count the directories and file to restore, in order to display meaningful statistics
        PathIDMapT fileList;
        if (!createFileListInRev(fileList, revisionID))
            return TRANS("Can not get any file or directory from this revision");

        FileMDEntry * entry = fileList.getValue(fileToRestore);
        if (!entry) return TRANS("File path not found to restore (use --filelist to get a list of available files)");
        // Check if it's a regular file (we can only extract regular files with this command)
        File::Info entryMD;
        entryMD.analyzeMetaData(entry->getMetaData());
        if (!entryMD.isFile()) return TRANS("This file path does not refer to a file. Only files could be extracted this way");

        String baseFolder = "";
        RestoreFile restore(callback, baseFolder, restoreFrom, No, maxCacheSize, revisionID);
        String errorMsg;
        FileFormat::FileTree::Item * item = Helpers::indexFile.getFileTree(revisionID)->getItem((unsigned int)*entry); // Call uint operator
        int ret = restore.restoreSingleFile(Stream::StdOutStream::getInstance(), errorMsg, item->getChunkListID(), fileToRestore, entryMD.size);
        if (ret < 0) return errorMsg;

        // Ok done restoring
        return "";
    }


}



#define DEFAULT_KEYVAULT  "~/.frost/keys"
using Frost::TRANS;

int showHelpMessage(const Frost::String & error = "")
{
    if (error) fprintf(stderr, "error: %s\n\n", (const char*)TRANS(error));

    printf("Frost (C) Copyright 2017 - Cyril RUSSO (This software is BSD licensed) \n");
    printf(TRANS("Frost is a tool used to efficiently backup and restore files to/from a remote\n"
           "place with no control other the remote server software.\n"
           "No warranty of any kind is provided for the use of this software.\n"
           "Current version: 3 build %d. \n\n"
           "Backup set from Frost version 3 use a different index file format and are not compatible with version 1's SQLite based files.\n"
           "In version 3, deduplication improved but also broke previous duplication detection from version 2.\n"
           "If you need to upgrade to version 3, run a new backup of your directory, you'll loose history for your backup set using previous versions.\n\n"
           "Usage:\n"
           "  Actions:\n"
           "\t--restore dir [rev]\tRestore the revision (default: last) to the given directory (either backup or restore mode is supported)\n"
           "\t--backup dir\t\tBackup the given directory (either backup or restore mode is supported)\n"
           "\t--purge [rev]\t\tPurge the given remote backup directory up to the given revision number (use --list to find out)\n"
           "\t--list [range]\t\tList the current backup in the specified index (required) and time range in UTC (in the form 'YYYYMMDDHHmmSS YYYYMMDDHHmmSS')\n"
           "\t--filelist [range]\tList the current backup in the specified index (required) and time range in UTC, including the file list in this revision\n"
           "\t--cat path [rev]\tLocate the file for the given path and optional revision number (remote is required), extract it to the standard output\n"
           "\t--test [name]\t\tRun the test with the given name -developer only- use -v for more verbose mode, 'help' to get a list of available tests\n"
           "\t--password pw\t\tSet the password so it's not queried on the terminal. Avoid this if launched from prompt as it'll end in your bash's history\n"
           "\t--dump\t\t\tDump the object content for the specified index (required). This is a kind of index file check done manually ;-)\n"
           "\t--decryptindex\t\tDecrypt the specified ciphered index (required). This is required if your local copy is missing\n"
           "\t--help [security]\tGet help on the security features and advices of Frost\n"
           "  Required parameters for backup, purge and restore:\n"
           "\t--remote url\t\tThe URL (can be a directory) to save/restore backup to/from\n"
           "\t--index path\t\tThe path to the index file that's used to store the backup's specific data. " DEFAULT_INDEX " is appended to this path, it defaults to remote_url\n"
           "\t--keyvault file\t\tPath to a file containing the private key used to decrypt/encrypt the backup data. Default to '" DEFAULT_KEYVAULT "'. If the key does not exist, it'll be created\n"
           "\t--keyid id\t\tThe key identifier if storing multiple keys in the key vault.\n"
           "  Optional parameters for backup and restore:\n"
           "\t--safeindex\t\tEnable ciphering the index file and store it in the remote folder too (backup only), --index is required for the clear index file path\n"
           "\t--verbose\t\tEnable verbosity (use -vv for VERY verbose mode)\n"
           "\t--cache [size]\t\tThe cache size (possible suffix: K,M,G) holding the decoded multichunks (default is 64M) - restore only\n"
           "\t--overwrite [policy]\tThe policy for overwriting/deleting files on the restore folder if they exists (either 'yes', 'no', 'update')\n"
           "\t--multichunk [size]\tWhile backing up, files are cut in variable sized chunk, and these chunks are concat in multichunk files saved on the target (default is 250K, possible suffix: K,M,G)\n"
           "\t                     \tIf you have a large amount of data to backup, a bigger number will create less files in the backup directory, the downside being that purging will take more time\n"
           "\t                     \tIf you backup often, and purge at regular interval, the default should allow fast restoring and purging\n"
           "\t--compression [bsc]\tYou can change the compression library to use (default is zlib). Using 'bsc' is faster than LZMA and gives better compression ratio.\n"
           "\t                     \tHowever, 'bsc' also changes the multichunk size to 25MB.\n"
           "\t--strategy [mode]    \tThe purging strategy. By default 'fast', when purging from previous revision, a new index file is created that's built from the cleaned index.\n"
           "\t                     \tHowever, multichunks are not rebuild to remove lost chunks. When using 'slow', multichunks are rebuilt too to remove lost chunks. This incurs reading\n"
           "\t                     \tand writing many multichunk (which might not be desirable if storage is remote). If you enter a value x between 0 (slow) and 100 (fast), the multichunk will be\n"
           "\t                     \tscanned and only get processed/cleaned if the number of chunks to remove is higher than x%% from the number of chunks in the multichunks.\n"
           "\t--exclude list.exc \tYou can specify a file containing the exclusion list for backup. This file is read line-by-line (one rule per line)\n"
           "\t                     \tIf a line starts by 'r/' the exclusion rule is considered as a regular expression otherwise the rule is matched if the analyzed file path contains the rule.\n"
           "\t                     \tThis also means that if you need to exclude a file whose name starts by 'r/', you need to write 'r/r/'.\n"
           "\t                     \tEven if the regular expression returns a partial match, the file is excluded, so you need to be very strict on the rules declaration.\n"
           "\t                     \tIf the rule starts by 'R/' then it's matched inverted (that is, the file is excluded if it does NOT fit the rule).\n"
           "\t                     \tTo get more details about the regular expression engine, run --help regex\n"
           "\t--include list.inc \tYou can specify a file containing the inclusion list for backup (only if an exclude list is used). This file is read line-by-line (one rule per line)\n"
           "\t                     \tIf a line starts by 'r/' the inclusion rule is considered as a regular expression otherwise the rule is matched if the analyzed file path contains the rule.\n"
           "\t                     \tEven if the regular expression returns a partial match, the file is included, so you need to be very strict on the rules declaration.\n"
           "\t                     \tIf the rule starts by 'R/' then it's matched inverted (that is, the file is included if it does NOT fit the rule).\n"
           "\t                     \tInclusion list happens after exclusion (that is, inclusion is only tested to re-include files that would have been excluded without it)\n"
           "\t                     \tFor example, if you need to exclude the complete 'subDir' folder, except 'subDir/important', the exclude list should contain 'subDir/' and the include list\n"
           "\t                     \tshould contain 'subDir/important'. The final '/' is important in the exclude list else 'subDir' folder will not be saved yet it's required for the included file.\n"
           "\t--entropy threshold\tBy default, multichunks are compressed before encryption. This behavior might be undesirable for hard to compress data (like mp3/jpg/mp4/etc),\n"
           "\t                     \tbecause compression will take time for nothing and will not save any more space. Frost can detect such case by computing entropy for the multichunk and only\n"
           "\t                     \tcompress it when its entropy is below the given threshold (default is 1.0 meaning everything will be below this threshold hence will get compressed)\n"
           "\t                     \tIf you don't know what threshold to set for your data, you can use '--test entropy' with your data set, Frost will print the current entropy value for the test\n"

           ),
#include "build/build-number.txt"
    );
    return EXIT_SUCCESS;
}

int showSecurityMessage()
{
    printf("Frost (C) Copyright 2014 - Cyril RUSSO (This software is BSD licensed) \n");
    printf(TRANS("Frost is a tool used to efficiently backup and restore files to/from a remote\n"
           "place with no control other the remote server software.\n"
           "No warranty of any kind is provided for the use of this software.\n"
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
           "\tyou should keep the index file locally (either by transfering it before and after the process)\n"
           "\tThe keyvault is never modified by Frost after first backup, so you might as well leave it on a server\n"
           "\tor locally depending on your security concerns.\n"
           "  Space usage/Speed tradeoff configuration:\n"
           "\tWhile using version 1, we have spotted decrease in performance while the backup set is becoming large\n"
           "\tAfter profiling, the bottleneck happened in the database code where simple access to indexed data was\n"
           "\textremely slow (up to few seconds). Starting with version 2, the new index file format is being used,\n"
           "\tand this has some impact on your Frost settings:\n"
           "\t1- New index is much smaller. We expect to fit the index file in memory for usual backup set size\n"
           "\t2- Access algorithm are all made O(log N) when O(1) is not possible.\n"
           "\t3- File format is made as less as mutable as possible. While backing up, no change is made past the\n"
           "\t   file header. File is recreated on purging, and no modification is done on restoring.\n"
           "\t4- File format is made to be memory mapped as much as possible. This means that accesses will be fast\n"
           "\t   and the operating system will be able to swap the non-used part if memory is lacking\n"
           "\t5- Index file are not endianness neutral. You can not save a backup on little endian system (amd64,\n"
           "\t   x86, ARM) and restore on a big endian system. However, the storage format used is type-size clear\n"
           "\t   so a backup on a 32 bits system will be restoreable/continueable on a 64 bits system and viceversa.\n\n"
           "\tSize limits have been selected to have as less impact as possible, yet, you must be aware of them:\n"
           "\t1- Index file format is limited to 16GB (typically a 100k files / 250GB dataset uses 500MB)\n"
           "\t   However, on a 32-bits machine, index file format will be limited to the maximum memory\n"
           "\t   map size (likely 2GB to 3.5GB on linux).\n"
           "\t2- There is only 65536 possible multichunks. When starting the backup set, you can specify the size\n"
           "\t   of the multichunk. By default, multichunks are 25MB in size (you can change this with the\n"
           "\t   --multichunk option), so the maximum backup set can be 65536*25MB = 1.64TB at worse\n"
           "\t   If your backup set is made of very big files, you should increase the multichunks' size\n"
           "\t   If your backup set is made of many small files, you should split your backup set in many backups\n"
           "\n\tStarting from version 3, the chunker algorithm was changed for a more efficient version, but this\n"
           "\tbreaks the previous detected chunk and thus, backing up could no detect chunk from previous version\n"
           "\tas duplicate. It's safer to rebackup everything so a version change in the file format was triggered.\n"
           "\tAlso, index file can be automatically ciphered (using --safeindex option) and then stored in the\n"
           "\thostile remote folder. This is safer but takes more time per backup to cipher the index file.\n"
           
           ),
#include "build/build-number.txt"
    );
    return EXIT_SUCCESS;
}

int showRegExMessage()
{
    printf("Frost (C) Copyright 2014 - Cyril RUSSO (This software is BSD licensed) \n");
    printf(TRANS("Frost is a tool used to efficiently backup and restore files to/from a remote\n"
           "place with no control other the remote server software.\n"
           "No warranty of any kind is provided for the use of this software.\n"
           "Current version: %d. \n\n"
           "Supported Regular Expression pattern for exclusion file:\n"
           "\t.\t\tMatch any character\n"
"\t^\t\tMatch beginning of a buffer\n"
"\t$\t\tMatch end of a buffer\n"
"\t()\t\tGrouping and substring capturing -useless, no backward search-\n"
"\t[...]\t\tMatch any character from set\n"
"\t[^...]\t\tMatch any character but ones from set\n"
"\t\\s\t\tMatch whitespace\n"
"\t\\S\t\tMatch non-whitespace\n"
"\t\\d\t\tMatch decimal digit\n"
"\t\\r\t\tMatch carriage return\n"
"\t\\n\t\tMatch newline\n"
"\t+\t\tMatch one or more times (greedy)\n"
"\t+?\t\tMatch one or more times (non-greedy)\n"
"\t*\t\tMatch zero or more times (greedy)\n"
"\t*?\t\tMatch zero or more times (non-greedy)\n"
"\t?\t\tMatch zero or once\n"
"\t\\xDD\t\tMatch byte with hex value 0xDD\n"
"\t\\meta\t\tMatch one of the meta character: ^$().[*+\\?\n"),
#include "build/build-number.txt"
    );
    return EXIT_SUCCESS;
}

int checkTests(Strings::StringArray & options)
{
    // Check for test mode
    size_t optionPos = 0;

    if ((optionPos = options.indexOf("--test")) != options.getSize())
    {
        Strings::FastString testName = "key";
        Strings::FastString arg = "";
        if (optionPos + 1 != options.getSize()) testName = options[optionPos+1].Trimmed();
        if (optionPos + 2 != options.getSize()) arg = options[optionPos+2].Trimmed();

        // Run tests now
        if (testName == "help")
        {
            printf("Frost (C) Copyright 2014 - Cyril RUSSO All right reserved \n");
            printf(TRANS("Current version: %d. \n\nTest mode help:\n"
                   "\tkey\t\tTest cryptographic system, by creating a new vault, and master key, and reading it back\n"
                   "\troundtrip\tTest a complete roundtrip backup and restore, of fake created file, with specific attributes\n"
                   "\tpurge\t\tTest an update to a previous roundtrip test, and purging the initial revision\n"
                   "\tfs\t\tTest some simple filesystem operations (independant from any other tests)\n"
                   "\tcomp\t\tTest compression and decompression engine for pseudo random input (independant from any other tests) (use compf if it fails, to reproduce same condition)\n"
                   "\tentropy file\tCompute the entropy for the given file and display it (reported chunk entropy is only data based, multichunk entropy includes chunk headers)\n"),
#include "build/build-number.txt"
                   );
            return EXIT_SUCCESS;
        }
        else if (testName == "key")
        {
#define ERR(Msg, ...) { fprintf(stderr, ::Frost::__trans__(Msg), ##__VA_ARGS__); return -1; }
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
                    ERR("Can't create basic file in the test directory\n");
                if (!File::Info("./ex/Hurt.txt").copyTo("./test/smallFile.txt"))
                    ERR("Can't copy lyric file from ex/Hurt.txt in the test directory\n");
                if (!File::Info("./ex/RomeoAndJulietS2.txt").copyTo("./test/"))
                    ERR("Can't copy scene 2 file in the test directory\n");
                if (!File::Info("./ex/RomeoAndJulietS3.txt").copyTo("./test/"))
                    ERR("Can't copy scene 3 file in the test directory\n");
                if (!File::Info("./ex/TheMerchantOfVeniceA3S1.txt").copyTo("./test/"))
                    ERR("Can't copy scene 1 file in the test directory\n");


                File::Info filePerms("./test/fileWithPerms.txt");
                if (!filePerms.setContent("This is a file with some permissions"))
                    ERR("Can't create basic file with permissions in the test directory\n");
                if (!filePerms.setPermission(0700))
                    ERR("Can't set the file permissions for the test vectors\n");

                if (!File::Info("./test/symLink.txt").createAsLinkTo("basicFile.txt"))
                    ERR("Can't create a symbolic link to the basic file\n");
                if (!File::Info("./test/subDir").makeDir())
                    ERR("Can't create a subdirectory\n");
                if (!File::Info("./test/subDir/hardLink.txt").createAsLinkTo("./test/fileWithPerms.txt", true))
                    ERR("Can't create a hard link to the permission file\n");

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
                    ERR("Can't fill the big file\n");
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
            if (arg == "bsc")
            {
                Frost::Helpers::compressor = Frost::Helpers::BSC;
                File::MultiChunk::setMaximumSize(25*1024*1024);
            }
            if (arg == "big")
            {
                File::MultiChunk::setMaximumSize(25*1024*1024);
            }
            result = Frost::backupFolder("test/", "./testBackup/", revisionID, console);
            if (result) ERR("Can't backup the test folder: %s\n", (const char*)result);

            result = Frost::initializeDatabase("", revisionID, cipheredMasterKey);
            if (result) ERR("Can't open the index file: %s\n", (const char*)result);
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
        }
        else if (testName == "purge")
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
            if (arg == "bsc")
            {
                Frost::Helpers::compressor = Frost::Helpers::BSC;
                File::MultiChunk::setMaximumSize(25*1024*1024);
            } else if (arg == "big")
            {
                File::MultiChunk::setMaximumSize(25*1024*1024);
            }
            result = Frost::backupFolder("test/", "./testBackup/", revisionID, console, arg == "bsc" ? Frost::Slow : Frost::Fast);
            if (result) ERR("Can't backup the test folder: %s\n", (const char*)result);

            Frost::finalizeDatabase();
            result = Frost::initializeDatabase("", revisionID, cipheredMasterKey);
            if (Frost::listBackups() != 2) ERR("This test needs to be run after a roundtrip test\n");

            // Then purge the database from the last revision
            Frost::finalizeDatabase();
            result = Frost::initializeDatabase("", revisionID, cipheredMasterKey);
            result = Frost::purgeBackup("./testBackup/", console, Frost::Slow, 1);
            if (result) ERR("Can't purge the last backup: %s\n", (const char*)result);

            Frost::finalizeDatabase();
            fprintf(stderr, "Success\n");
            return EXIT_SUCCESS;
        }
        else if (testName == "fs")
        {
            File::Info("./test/").remove();
            File::Info("./testBackup/").remove();
            File::Info("./testRestore/").remove();
            if (!File::Info("./testBackup/").makeDir()) ERR("Failed creating the backup folder ./testBackup/\n");
            if (!File::Info("./testRestore/").makeDir()) ERR("Failed creating the restoring folder ./testRestore/\n");
            if (!File::Info("./test/").makeDir()) ERR("Failed creating the test folder ./test/\n");

            {
                // Ok, then fill some content in some files
                if (!File::Info("./test/basicFile.txt").setContent("This is a very basic file content"))
                    ERR("Can't create basic file in the test directory\n");
                if (!File::Info("./ex/Hurt.txt").copyTo("./test/smallFile.txt"))
                    ERR("Can't copy lyric file from ex/Hurt.txt in the test directory\n");
                if (!File::Info("./ex/RomeoAndJulietS2.txt").copyTo("./test/"))
                    ERR("Can't copy scene 2 file in the test directory\n");
                // Change the user permission for this file
                if (!File::Info("./test/basicFile.txt").setPermission(0600))
                    ERR("Can't set the permission for the basic file\n");
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

            Frost::finalizeDatabase();
            result = Frost::initializeDatabase("", revisionID, cipheredMasterKey);
            if (result) ERR("Can't open the index file: %s\n", (const char*)result);
            if (Frost::listBackups() != 1) ERR("Can't list the created backup\n");

            // Reported issue #3
            // Delete a file and backup again
            File::Info("./test/smallFile.txt").remove();

            Frost::finalizeDatabase();
            result = Frost::initializeDatabase("test/", revisionID, cipheredMasterKey);
            if (result) ERR("Creating the database failed: %s\n", (const char*)result);
            result = Frost::backupFolder("test/", "./testBackup/", revisionID, console);
            if (result) ERR("Can't backup the test folder: %s\n", (const char*)result);

            Frost::finalizeDatabase();
            result = Frost::initializeDatabase("", revisionID, cipheredMasterKey);
            if (result) ERR("Can't open the index file: %s\n", (const char*)result);
            if (Frost::listBackups() != 2) ERR("Can't list the created backup\n");

            // Add another file and backup
            if (!File::Info("./ex/RomeoAndJulietS3.txt").copyTo("./test/"))
                ERR("Can't copy scene 3 file in the test directory\n");

            Frost::finalizeDatabase();
            result = Frost::initializeDatabase("test/", revisionID, cipheredMasterKey);
            if (result) ERR("Creating the database failed: %s\n", (const char*)result);
            result = Frost::backupFolder("test/", "./testBackup/", revisionID, console);
            if (result) ERR("Can't backup the test folder: %s\n", (const char*)result);

            Frost::finalizeDatabase();
            result = Frost::initializeDatabase("", revisionID, cipheredMasterKey);
            if (result) ERR("Can't open the index file: %s\n", (const char*)result);
            if (Frost::listBackups() != 3) ERR("Can't list the created backup\n");

            // Finalize the database and clean up all our stuff before restoring
            if (!cipheredMasterKey.Extract(0, cipheredMasterKey.getSize()))
                ERR("Can't reset the ciphered master key\n");

            /////////////////////////////// Restoring here /////////////////////////////////
            // Ok, let's re-open again all of stuff
            File::Info("./test/RomeoAndJulietS3.txt").remove();
            revisionID = 0;
            result = Frost::initializeDatabase("", revisionID, cipheredMasterKey);
            if (result) ERR("Can't re-open the database: %s\n", (const char*)result);
            if (!cipheredMasterKey.getSize()) ERR("Bad readback of the ciphered master key\n");

            result = Frost::getKeyFactory().loadPrivateKey("./testBackup/keyVault", cipheredMasterKey, "password");
            if (result) ERR("Reading back the master key failed: %s\n", (const char*)result);

            // Restore the backup for the given revision
            result = Frost::restoreBackup("./testRestore/", "./testBackup/", 2, console);
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
        }
        else if (testName == "comp")
        {
            // Test compression engines on random and pseudo random data
            // Generate a seed for making sure the tests are reproducable
            uint32 seed[4];
            if (!arg)
            {
                Random::fillBlock((uint8*)seed, sizeof(seed), true);
                fprintf(stderr, "Seed used: %08X%08X%08X%08X\n", seed[0], seed[1], seed[2], seed[3]);
            }
            else
            {
                // Parse the seed argument
                if (sscanf((const char*)arg, "%08X%08X%08X%08X", &seed[0], &seed[1], &seed[2], &seed[3]) != 4)
                {
                    fprintf(stderr, "Can not parse the seed format\n");
                    return -1;
                }
            }
            Random::getDefaultGenerator().Init((uint8*)seed, sizeof(seed));
            while (true)
            {
                // Generate a bloc of random memory, filling only one byte out of two
                Utils::MemoryBlock mem(64*1024*1024);
                Random::fillBlock(mem.getBuffer(), 16*1024*1024);
                // Then mix up some byte to make compression works
                uint8 * buf = mem.getBuffer();
                for (int i = 0; i < 16*1024*1024; i+= 2)
                {
                    buf[i + 16*1024*1024] = buf[i+1];
                    buf[i + 1] = buf[i];
                }
                // Not so simple, let's make it harder for compression
                for (int i = 0; i < 32*1024*1024 - 4; i+= 5)
                {
                    buf[i + 2] = (buf[i] - buf[i+1]) > 10 ? buf[i]+2 : buf[i+1];
                    buf[i + 4] = (buf[i+3] + buf[i+2] + buf[i+1] + buf[i]) / 3;
                    buf[i + 32*1024*1024] = buf[i+2];
                    buf[i + 32*1024*1024 + 3] = buf[i];
                    buf[i + 32*1024*1024 + 4] = buf[i+1];
                }

                // Ok, let's spread a lot of common word in the buffer to compress too
                for (int i = 0; i < 100000; i++)
                {
                    memcpy(buf+Random::numberBetween(0, 63*1024*1024), "igloo ", 6);
                    memcpy(buf+Random::numberBetween(0, 63*1024*1024), " house ", 7);
                    memcpy(buf+Random::numberBetween(0, 63*1024*1024), "modern fixture", 14);
                    memcpy(buf+Random::numberBetween(0, 63*1024*1024), "WTF", 4);
                }

                // Buffer ready, let's save it to a file for later inspection
                fprintf(stderr, "Buffer ready for compression\n");
                ::Stream::MemoryBlockStream srcData(buf, mem.getSize());
                {
                    ::Stream::OutputFileStream ofs("origin.raw");
                    if (!::Stream::copyStream(srcData, ofs))
                        ERR("Can not save to origin.raw\n");
                    srcData.setPosition(0);
                }
                fprintf(stderr, "Buffer saved to origin.raw\n");
                ::Stream::OutputMemStream compressedStream;
                // Compress the data
                {
                    ::Stream::CompressOutputStream compressor(compressedStream, new Compression::BSCLib);
                    if (!::Stream::copyStream(srcData, compressor))
                        ERR("Compressing failed\n");
                }
                fprintf(stderr, "Buffer compressed\n");

                // Copy the compressed output to a file too
                {
                    ::Stream::OutputFileStream ofs("comp.bsc");
                    ::Stream::copyStream(::Stream::MemoryBlockStream(compressedStream.getBuffer(), compressedStream.fullSize()), ofs);
                }
                fprintf(stderr, "Compressed buffer saved to comp.bsc\n");

                // Uncompress the data now
                ::Stream::OutputMemStream decompressedStream;
                ::Stream::MemoryBlockStream compressedInStream(compressedStream.getBuffer(), compressedStream.fullSize());
                {
                     ::Stream::DecompressInputStream decompressor(compressedInStream, new Compression::BSCLib);
                     if (!::Stream::copyStream(decompressor, decompressedStream))
                         ERR("Can not decompressed the compressed data\n");
                }
                fprintf(stderr, "Compressed buffer decompressed\n");
                // Save the decompressed file for later inspection
                {
                    ::Stream::OutputFileStream ofs("decomp.raw");
                    ::Stream::copyStream(::Stream::MemoryBlockStream(decompressedStream.getBuffer(), decompressedStream.fullSize()), ofs);
                }
                fprintf(stderr, "Decompressed buffer saved to decomp.raw\n");

                // Compare the stream now, byte by byte
                const uint8 * bufDec = decompressedStream.getBuffer();
                if (decompressedStream.fullSize() != mem.getSize())
                    ERR("Mismatch in data round file size (got %lld, expected %lld)\n", decompressedStream.fullSize(), (uint64)mem.getSize());
                for (uint32 i = 0; i < mem.getSize(); i++)
                {
                    if (buf[i] != bufDec[i])
                       ERR("Error at position %u (got %02X expected %02X)\n", i, bufDec[i], buf[i]);
                }
                fprintf(stderr, "Success\n");
                return EXIT_SUCCESS;
            }
        }
        else if (testName == "compf")
        {
            ::Stream::InputFileStream srcData("origin.raw");

            ::Stream::OutputMemStream compressedStream;
            // Compress the data
            {
                ::Stream::CompressOutputStream compressor(compressedStream, new Compression::BSCLib);
                if (!::Stream::copyStream(srcData, compressor))
                    ERR("Compressing failed\n");
            }
            fprintf(stderr, "Buffer compressed\n");

            // Copy the compressed output to a file too
            {
                ::Stream::OutputFileStream ofs("comp.bsc");
                ::Stream::copyStream(::Stream::MemoryBlockStream(compressedStream.getBuffer(), compressedStream.fullSize()), ofs);
            }
            fprintf(stderr, "Compressed buffer saved to comp.bsc\n");

            // Uncompress the data now
            ::Stream::OutputMemStream decompressedStream;
            ::Stream::MemoryBlockStream compressedInStream(compressedStream.getBuffer(), compressedStream.fullSize());
            {
                ::Stream::DecompressInputStream decompressor(compressedInStream, new Compression::BSCLib);
                if (!::Stream::copyStream(decompressor, decompressedStream))
                    ERR("Can not decompressed the compressed data\n");
            }
            fprintf(stderr, "Compressed buffer decompressed\n");
            // Save the decompressed file for later inspection
            {
                ::Stream::OutputFileStream ofs("decomp.raw");
                ::Stream::copyStream(::Stream::MemoryBlockStream(decompressedStream.getBuffer(), decompressedStream.fullSize()), ofs);
            }
            fprintf(stderr, "Decompressed buffer saved to decomp.raw\n");
            fprintf(stderr, "Success\n");
            return EXIT_SUCCESS;
        }
        else if (testName == "entropy" && arg)
        {
            File::Info file(arg, true);
            if (!file.doesExist())
                ERR("File not found");

            File::TTTDChunker chunker;
            File::MultiChunk  multiChunk;
            // Open the file, split in chunks and compute entropy for each chunk (and also maintain average & deviation values)
            File::Chunk temporaryChunk;
            ::Stream::InputFileStream stream(file.getFullPath());

            // Build the list of chunk ID for computing entropy
            uint64 streamOffset = stream.currentPosition();
            uint64 fullSize = stream.fullSize();
            uint32 multichunkCount = 0, chunkCount = 0, chunkTotalCount = 0;
            double chunkMaxEntropy = 0, chunkMinEntropy = 1.0, chunkAvg = 0;
            double chunkTotalMaxEntropy = 0, chunkTotalMinEntropy = 1.0, chunkTotalAvg = 0;
            double mchunkMaxEntropy = 0, mchunkMinEntropy = 1.0, mchunkAvg = 0;

            while (chunker.createChunk(stream, temporaryChunk))
            {
                // Ok, got a chunk, let's compute entropy for this chunk

                // The chunk does not exist, so let's append to the current multichunk, and create an entry for it
                if (!multiChunk.canFit(temporaryChunk.size))
                {
                    double multichunkEntropy = multiChunk.getEntropy();
                    fprintf(stderr, "Multichunk %d (file pos: %lld) of size %d has computed entropy of %g\n", multichunkCount, streamOffset, (int)multiChunk.getSize(), multichunkEntropy);
                    multichunkCount++;
                    fprintf(stderr, "Chunks statistics: (min %g / avg %g / max %g)\n", chunkMinEntropy, chunkAvg / (double)chunkCount, chunkMaxEntropy);

                    mchunkAvg += multichunkEntropy;
                    if (mchunkMaxEntropy < multichunkEntropy) mchunkMaxEntropy = multichunkEntropy;
                    if (mchunkMinEntropy > multichunkEntropy) mchunkMinEntropy = multichunkEntropy;

                    chunkCount = 0; chunkMinEntropy = 1.0; chunkMaxEntropy = chunkAvg = 0;
                    multiChunk.Reset();
                }
                // Append to the current multichunk
                uint8 * chunkBuffer = multiChunk.getNextChunkData(temporaryChunk.size, temporaryChunk.checksum);
                if (!chunkBuffer)
                    ERR("Unexpected behaviour for multichunk data extraction\n");

                memcpy(chunkBuffer, temporaryChunk.data, temporaryChunk.size);
                double chunkEntropy = multiChunk.getChunkEntropy(&temporaryChunk);
                fprintf(stdout, "Chunk %d (file pos: %lld) of size %d has computed entropy of %g\n", chunkCount, streamOffset, temporaryChunk.size, chunkEntropy);
                chunkCount++; chunkTotalCount++;
                chunkAvg += chunkEntropy; chunkTotalAvg += chunkEntropy;
                if (chunkMaxEntropy < chunkEntropy) chunkMaxEntropy = chunkEntropy;
                if (chunkTotalMaxEntropy < chunkEntropy) chunkTotalMaxEntropy = chunkEntropy;

                if (chunkMinEntropy > chunkEntropy) chunkMinEntropy = chunkEntropy;
                if (chunkTotalMinEntropy > chunkEntropy) chunkTotalMinEntropy = chunkEntropy;

                Assert(streamOffset + temporaryChunk.size == stream.currentPosition());
                streamOffset += temporaryChunk.size;
            }
            double multichunkEntropy = multiChunk.getEntropy();
            fprintf(stderr, "Multichunk %d (file pos: %lld) of size %d has computed entropy of %g\n", multichunkCount, streamOffset, (int)multiChunk.getSize(), multichunkEntropy);
            multichunkCount++;
            fprintf(stderr, "Chunks statistics: (min %g / avg %g / max %g)\n", chunkMinEntropy, chunkAvg / (double)chunkCount, chunkMaxEntropy);
            // Compute final statistics
            fprintf(stderr, "Multichunks statistics: (min %g / avg %g / max %g)\n", mchunkMinEntropy, mchunkAvg / (double)multichunkCount, mchunkMaxEntropy);
            fprintf(stderr, ">>> Global chunks statistics: (min %g / avg %g / max %g) -- This should be used to set entropy threshold\n", chunkTotalMinEntropy, chunkTotalAvg / (double)chunkTotalCount, chunkTotalMaxEntropy);
            fprintf(stderr, "Success\n");
            return EXIT_SUCCESS;
        }
#undef ERR
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
        if (numeric && optionValue->invFindAnyChar("0123456789KMG") != -1)
        {
            delete optionValue;
            return showHelpMessage(TRANS("Expecting numerical value (accepted also K, M or G suffix) for: ") + option);
        }
        optionsMap.storeValue(option, optionValue, true);
        return 1;
    }
    return -1;
}

int64 parseNumericSuffixed(const Strings::FastString & option)
{
    int64 parsed = option.parseInt(10);
    uint8 suffix = option.midString(-1, 1)[0];
    if (suffix == 'K') parsed *= 1024;
    if (suffix == 'M') parsed *= 1024 * 1024;
    if (suffix == 'G') parsed *= 1024 * 1024 * 1024;
    return parsed;
}

int handleAction(Strings::StringArray & options, const Strings::FastString & action)
{
    Strings::StringArray params;
    if (!getOptionParameters(options, action, params)) return BailOut;

    if (Frost::safeIndex && !optionsMap["index"]) return showHelpMessage("If using --safeindex you need to specify the clear index path with --index");
    if (!optionsMap["index"]) return showHelpMessage("Bad argument for " + action + ", index path missing");



    // Common database initialization code
    Frost::DatabaseModel::databaseURL = optionsMap["index"]->normalizedPath(Platform::Separator, true);
    if (!File::Info(Frost::DatabaseModel::databaseURL, true).doesExist())
    {
        if (action == "backup" && optionsMap["remote"] && *optionsMap["remote"] == *optionsMap["index"])
        {   // If the remote folder does not exists, let's create it
            File::Info remoteFolder(optionsMap["remote"]->normalizedPath(Platform::Separator, true));
            if (!remoteFolder.doesExist()) remoteFolder.makeDir();
            
        }   // Else, it's an error
        else return showHelpMessage("Bad argument for " + action + ", index path does not exists: " + Frost::DatabaseModel::databaseURL);
    }
    
    Frost::MemoryBlock              cipheredMasterKey;
    unsigned int                    revisionID = 0;
    Frost::ConsoleProgressCallback  console(action != "cat");
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
            fprintf(stderr, "%s", (const char*)TRANS("Can't read or initialize the index (do you need to decrypt it ?):" + Frost::DatabaseModel::databaseURL + "/" DEFAULT_INDEX));
            fprintf(stderr, "%s", (const char*)result);
            return EXIT_FAILURE;
        }


        Frost::listBackups(startTime, endTime, action == "filelist");
        Frost::finalizeDatabase();
        return EXIT_SUCCESS;
    }
    if (action == "dump")
    {
        Frost::String result = Frost::initializeDatabase("", revisionID, cipheredMasterKey);
        if (result) ERR("Can't re-open the index (do you need to decrypt it ?): %s\n", (const char*)result);
        if (!cipheredMasterKey.getSize()) ERR("Bad readback of the ciphered master key\n");
        for (uint32 i = 1; i <= revisionID; i++)
        {
            fprintf(stderr, "%s", (const char*)Frost::Helpers::indexFile.dumpIndex(i));
        }
        Frost::finalizeDatabase();
        return EXIT_SUCCESS;
    }


    // From now, all other actions require a password
    if (!optionsMap["remote"]) return showHelpMessage("Bad argument for " + action + ", remote missing (that's where the backup is saved)");

    Frost::String pass, result,
                  remote = optionsMap["remote"]->normalizedPath(Platform::Separator, true),
                  keyID = optionsMap["keyid"] ? *optionsMap["keyid"] : "";

    if (!optionsMap["password"])
    {
        // Ask for password
        char password[256];
        size_t passLen = ArrSz(password);
        if (!Platform::queryHiddenInput("Password:", password, passLen))
            ERR("Can't query a password, do you have a terminal or console running ?");

        pass = password;
        memset(password, 0, passLen);
    } else { pass = *optionsMap["password"]; optionsMap.removeValue("password"); }

    if (action == "decryptindex")
    {
        Frost::KeyFactory::KeyT key;
        Frost::derivePassword(key, pass);
        pass = ""; // Password is not required anymore, let's wipe it

        // Then decrypt the given index file
        result = Frost::Helpers::ensureValidIndexFile( remote + DEFAULT_INDEX ".aes", Frost::DatabaseModel::databaseURL + DEFAULT_INDEX, key, console, true);
        if (result) ERR("Decrypting the index file failed (bad password ?): %s\n", (const char*)result);
        
        return EXIT_SUCCESS;
    }


    if (action == "purge")
    {
        // Purge the directory now for useless chunks
        if (!optionsMap["remote"]) return showHelpMessage("Bad argument for purge, remote missing (that's where the backup is saved)");

        Frost::KeyFactory::KeyT key;
        Frost::derivePassword(key, pass);
        result = Frost::Helpers::ensureValidIndexFile( remote + DEFAULT_INDEX ".aes", Frost::DatabaseModel::databaseURL + DEFAULT_INDEX, key, console, false);
        if (result) ERR("Decrypting the index file failed (bad password ?): %s\n", (const char*)result);
        memset(key, 0, ArrSz(key));

        result = Frost::initializeDatabase("", revisionID, cipheredMasterKey);
        if (result) ERR("Can't re-open the database: %s\n", (const char*)result);
        if (!cipheredMasterKey.getSize()) ERR("Bad readback of the ciphered master key\n");

        result = Frost::getKeyFactory().loadPrivateKey(*optionsMap["keyvault"], cipheredMasterKey, pass, keyID);
        pass = ""; // Password is not required anymore, let's wipe it
        if (result) ERR("Reading back the master key failed (bad password ?): %s\n", (const char*)result);

        // Purge the backup up to the given revision
        if (params[0] && (int)params[0]) revisionID = params[0];
        else ERR("No revision ID given. I won't purge the complete backup set implicitely, purge aborted\n");

        Frost::String strategyTxt = optionsMap["strategy"] ? *optionsMap["strategy"] : "100";
        if (strategyTxt == "slow") strategyTxt = "0";

        Frost::PurgeStrategy strategy = (Frost::PurgeStrategy)(int)strategyTxt; // Convert to a threshold value here
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
        File::Info backupPath(backup, true);
        if (!backupPath.doesExist() || !backupPath.isDir())
            return showHelpMessage("Bad argument for backup, the --backup parameter is not a folder");

        // Prefix search for the index path in the backup path (this is an error)
        if (File::Info(Frost::DatabaseModel::databaseURL, true).getRealFullPath().midString(0, backupPath.getRealFullPath().getLength()) == backupPath.getRealFullPath())
            return showHelpMessage("The (output) index path can not be found in the given backup path");
        
        Frost::String indexFile = File::Info(Frost::DatabaseModel::databaseURL).isDir() ? Frost::DatabaseModel::databaseURL + DEFAULT_INDEX : Frost::DatabaseModel::databaseURL;

        Frost::KeyFactory::KeyT key;
        Frost::derivePassword(key, pass);
        result = Frost::Helpers::ensureValidIndexFile( remote + DEFAULT_INDEX ".aes", indexFile, key, console, false);
        if (result && File::Info(remote + DEFAULT_INDEX ".aes").doesExist()) ERR("Decrypting the index file failed (bad password ?): %s\n", (const char*)result);
        if (!Frost::safeIndex) memset(key, 0, ArrSz(key));

        {
            if (!File::Info(indexFile).doesExist())
            {
                if (File::Info(*optionsMap["keyvault"], true).doesExist()) ERR("Index file not found while keyvault exist already. Remove the keyvault, or fix the index file path");

                result = Frost::getKeyFactory().createMasterKeyForFileVault(cipheredMasterKey, *optionsMap["keyvault"], pass, keyID);
                if (result) ERR("Creating the master key failed: %s\n", (const char*)result);
            }

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
        Frost::PurgeStrategy strategy = optionsMap["strategy"] ? (*optionsMap["strategy"] == "slow" ? Frost::Slow : Frost::Fast) : Frost::Fast; // Strategy set to 0 should reopen the previous backup set
        result = Frost::backupFolder(backup, remote, revisionID, console, strategy);
        if (result) ERR("Can't backup the test folder: %s\n", (const char*)result);

        // Display some statistics
        console.progressed(Frost::ProgressCallback::Backup, "", 0, 0, 0, 0, Frost::ProgressCallback::FlushLine);
        console.progressed(Frost::ProgressCallback::Backup, Frost::String::Print(Frost::__trans__("Finished: %s, (source size: %lld, backup size: %lld, %u files, %u directories)"),
                                            (const char*)backup, (int64)Frost::Helpers::indexFile.getMetaData().findKey("InitialSize").fromFirst(": "), (int64)Frost::Helpers::indexFile.getMetaData().findKey("BackupSize").fromFirst(": "), (uint32)Frost::Helpers::indexFile.getMetaData().findKey("FileCount").fromFirst(": "), (uint32)Frost::Helpers::indexFile.getMetaData().findKey("DirCount").fromFirst(": ")), 1, 1, (uint32)Frost::Helpers::indexFile.getMetaData().findKey("FileCount").fromFirst(": "), (uint32)Frost::Helpers::indexFile.getMetaData().findKey("FileCount").fromFirst(": "),
                                            Frost::ProgressCallback::FlushLine);

        // Need to be called anyway
        Frost::finalizeDatabase();
        // Check if we need to encrypt the index file
        if (Frost::safeIndex)
        {
        
            Stream::OutputFileStream output(remote + DEFAULT_INDEX ".aes");
            Stream::InputFileStream input(indexFile);
            
            Frost::FileFormat::CipheredIndexHeader indexHeader;
            Random::fillBlock(indexHeader.nonce, ArrSz(indexHeader.nonce), true);

            if (output.write(&indexHeader, sizeof(indexHeader)) != sizeof(indexHeader)) ERR("Could not write the output header to: %s\n", (const char*)(remote + DEFAULT_INDEX ".aes"));
            
            Frost::KeyFactory::KeyT nonce, hash;
            memcpy(nonce, indexHeader.nonce, ArrSz(indexHeader.nonce));
            
            if (!Frost::Helpers::AESCounterProcess(key, nonce, input, output, console, indexHeader.hash, 0)) ERR("Error while encrypting the index\n");
            if (!output.setPosition(0)) ERR("Could not seek back to the start of the ciphered index file\n");

            // Update the header now we know about the hash of the input file
            if (output.write(&indexHeader, sizeof(indexHeader)) != sizeof(indexHeader)) ERR("Could not write the output header to: %s\n", (const char*)(remote + DEFAULT_INDEX ".aes"));
        }
        
        if (Frost::safeIndex)
            // Need to set the ciphered file's modification time now it's done writing and closed
            File::Info(remote + DEFAULT_INDEX ".aes", true).setModifiedTime(File::Info(indexFile, true).modification);
        
        if (warningLog.getSize()) { fputs("\nReceived warnings:\n", stderr); fputs((const char*)(warningLog.Join("\n")+"\n"), stderr); }
        return EXIT_SUCCESS;
    }
    if (action == "restore")
    {
        Frost::KeyFactory::KeyT key;
        Frost::derivePassword(key, pass);
        result = Frost::Helpers::ensureValidIndexFile( remote + DEFAULT_INDEX ".aes", Frost::DatabaseModel::databaseURL + DEFAULT_INDEX, key, console, false);
        if (result) ERR("Decrypting the index file failed (bad password ?): %s\n", (const char*)result);
        memset(key, 0, ArrSz(key));
    
        result = Frost::initializeDatabase("", revisionID, cipheredMasterKey);
        if (result) ERR("Can't re-open the database: %s\n", (const char*)result);
        if (!cipheredMasterKey.getSize()) ERR("Bad readback of the ciphered master key\n");

        result = Frost::getKeyFactory().loadPrivateKey(*optionsMap["keyvault"], cipheredMasterKey, pass, keyID);
        pass = ""; // Password is not required anymore, let's wipe it
        if (result) ERR("Reading back the master key failed (bad password ?): %s\n", (const char*)result);

        // Restore the backup for the given revision
        if (params[1] && (int)params[1]) revisionID = params[1];
        result = Frost::restoreBackup(params[0], remote, revisionID, console, (size_t)parseNumericSuffixed(*optionsMap["cache"]));
        if (result) ERR("Can't restore the backup: %s\n", (const char*)result);

        // Need to be called anyway
        Frost::finalizeDatabase();
        if (warningLog.getSize()) { fputs("\nReceived warnings:\n", stderr); fputs((const char*)(warningLog.Join("\n")+"\n"), stderr); }
        return EXIT_SUCCESS;
    }
    if (action == "cat")
    {
        Frost::KeyFactory::KeyT key;
        Frost::derivePassword(key, pass);
        result = Frost::Helpers::ensureValidIndexFile( remote + DEFAULT_INDEX ".aes", Frost::DatabaseModel::databaseURL + DEFAULT_INDEX, key, console, false);
        if (result) ERR("Decrypting the index file failed (bad password ?): %s\n", (const char*)result);
        memset(key, 0, ArrSz(key));

        result = Frost::initializeDatabase("", revisionID, cipheredMasterKey);
        if (result) ERR("Can't re-open the database: %s\n", (const char*)result);
        if (!cipheredMasterKey.getSize()) ERR("Bad readback of the ciphered master key\n");

        result = Frost::getKeyFactory().loadPrivateKey(*optionsMap["keyvault"], cipheredMasterKey, pass, keyID);
        pass = ""; // Password is not required anymore, let's wipe it
        if (result) ERR("Reading back the master key failed (bad password ?): %s\n", (const char*)result);
        // Restore the backup for the given revision
        if (params[1] && (int)params[1]) revisionID = params[1];
        result = Frost::restoreSingleFile(params[0], remote, revisionID, console, (size_t)parseNumericSuffixed(*optionsMap["cache"]));
        if (result) ERR("Can't restore the file: %s\n", (const char*)result);

        // Need to be called anyway
        Frost::finalizeDatabase();
        if (warningLog.getSize()) fputs((const char*)(warningLog.Join("\n")+"\n"), stderr);
        return EXIT_SUCCESS;
    }
#undef ERR
    return BailOut;
}

#if WithFUSE == 1

#define FUSE_USE_VERSION 26
#include <fuse.h>
#include <fuse_opt.h>

struct FrostFSOptions
{
    char *  remote;
    char *  index;
    char *  keyVault;
    char *  password;
    int     showVersion;
    int     showHelp;
    int     showDebug;
    FrostFSOptions() : remote(0), index(0), keyVault(0), password(0), showVersion(0), showHelp(0), showDebug(0) {}
};


static struct fuse_opt frost_opts[] =
{
    {"--remote=%s",     offsetof(struct FrostFSOptions, remote), 0},
    {"--index=%s",      offsetof(struct FrostFSOptions, index), 0},
    {"--keyvault=%s",   offsetof(struct FrostFSOptions, keyVault), 0},
    {"--password=%s",   offsetof(struct FrostFSOptions, password), 0},
    {"--verbose",       offsetof(struct FrostFSOptions, showDebug), 1},
    {"-h",              offsetof(struct FrostFSOptions, showHelp), 1},
    {"-V",              offsetof(struct FrostFSOptions, showVersion),1},
    {"--version",       offsetof(struct FrostFSOptions, showVersion),1},
    FUSE_OPT_END
};

struct NullProgressCallback : public Frost::ProgressCallback
{
    typedef Frost::String String;

    virtual bool progressed(const Action action, const String & currentFilename, const uint64 sizeDone, const uint64 totalSize, const uint32 index, const uint32 count, const FlushMode mode)
    {
        return true;
    }

    virtual bool warn(const Action action, const String & currentFilename, const String & message, const uint32 sourceLine = 0)
    {
        return true;
    }
};


struct FrostFSOps
{
    typedef Frost::String String;
    typedef Frost::FileFormat::FileTree   FileTree;
    typedef Container::HashTable<FileTree, uint32> FileTreeMap;
    /* We intend to have as many TLSIndex instances as there are threads.
       So we use TLS to store the instances.
       There is only a single IndexFile, but we cache the file tree based on revision in the TLS.
       So we have a map of FileTree based on the revision. */



    // Static Members
public:
    // Access to the members below requires locking
//    static Threading::Lock                          lock;
    static Frost::String                            indexFilePath; // Complete file path
    static Frost::String                            remoteFolder; // Complete remote folder
    static uint32                                   maxMultichunkSize;
    static uint32                                   maxRevisionID;
    static Threading::Thread::LocalVariable::Key    tls;
    static FileTreeMap                              fileTrees;
    static NullProgressCallback                     nullCB;

    // Static interface (used by all threads)
public:
    struct TLSIndex
    {
        Frost::Helpers::MultiChunkCache cache;

        static TLSIndex * constructTLSIndex(Threading::Thread::LocalVariable::Key, TLSIndex *) { return new TLSIndex; }
        static void destructTLSIndex(TLSIndex * obj) { delete obj; }

    private:
        TLSIndex() : cache(maxMultichunkSize * 2)
        {
//            Threading::ScopedLock scope(FrostFSOps::lock);
//            Frost::String error = indexFile.readFile(FrostFSOps::indexFilePath);
//            if (error) fprintf(stderr, "Could not read the index file %s\n", (const char*)FrostFSOps::indexFilePath);

        }

    };
    friend struct TLSIndex;

    struct ReadCache
    {
        TLSIndex  *     index;
        uint32          chunkListID;
        ReadCache(TLSIndex * index, uint32 id) : index(index), chunkListID(id) {}
    };



    static TLSIndex * getTLS()
    {
        Threading::Thread::LocalVariable * var = Threading::Thread::getLocalVariable(tls);
        if (!var)
        {   // No locking here since this should be done on the main thread
            TLSIndex * index = TLSIndex::constructTLSIndex(0, 0);
            tls = Threading::Thread::appendLocalVariable(index, TLSIndex::constructTLSIndex, TLSIndex::destructTLSIndex);
            var = Threading::Thread::getLocalVariable(tls);
        }
        GetLocalVariable(TLSIndex, index, tls);
        return index;
    }
    static void cleanTLS()
    {
        Threading::Thread::removeLocalVariable(tls);
    }

    static int checkPath(const char * pathStr, String & filePath, FileTree *& ft, TLSIndex *& index)
    {
        index = getTLS();
        if (!index) return -EIO;
        if (pathStr[0] != '/') return -ENOENT;

        String path(pathStr+1);
        uint32 rev = path;
        if (!rev) return -ENOENT;
        filePath = "/" + path.fromFirst("/");
        ft = FrostFSOps::fileTrees.getValue(rev);
        if (!ft) return -ENOENT;
        if (Frost::dumpLevel) fprintf(stdout, "checkPath: rev%u, path: %s\n", rev, (const char*)filePath);
        return 0;
    }

    // Getattr is the only function that's serialized by FUSE.
    static int getattr(const char* pathStr, struct stat* info)
    {
        bool isRoot = strcmp(pathStr, "/") == 0;
        if (isRoot || String(pathStr).regExFit("^/[0-9]+$"))
        {
            info->st_mode = 040555; // We don't support anything else than read-only directory
            info->st_size = 4096;
            info->st_nlink = 3;
            info->st_uid = fuse_get_context()->uid;
            info->st_gid = fuse_get_context()->gid;
            // Root folder ?
            if (isRoot) return 0; // Could not fill any more stuff in the structure

            // Root directory, let's create fake entries
            uint32 rev = String(pathStr).fromFirst("/");

            const Frost::FileFormat::Catalog * c = Frost::Helpers::indexFile.getCatalogForRevision(rev);
            if (!c) return -ENOENT;

            info->st_ctime = c->time;
            info->st_mtime = c->time;
            info->st_atime = c->time;
            return 0;
        }

        TLSIndex * index; String path; FileTree * ft = 0;
        int ret = checkPath(pathStr, path, ft, index);
        if (ret) return ret;


        uint32 itemID = ft->findItem(path);
        if (itemID == ft->notFound()) return -ENOENT;
        // Then get the associated item
        Frost::FileFormat::FileTree::Item * item = ft->getItem(itemID);
        // Read the stat structure
        if (!item->fixed || !item->metaData) return -EIO;
        if (Frost::dumpLevel) fprintf(stdout, "getattr path: %s [%s]\n", (const char*)path, (const char*)item->getMetaData());
        return File::Info::expandMetaDataNative(item->metaData, item->fixed->metadataSize, info) ? 0 : -EIO;
    }

    static int readdir(const char* pathStr, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
    {
        if (strcmp(pathStr, "/") == 0)
        {
            // Need to save the revisions here
            for (uint32 i = 1; i <= maxRevisionID; i++)
            {
                if (filler(buf, (const char*)String::Print("%u", i), 0, 0))
                    return 0;
            }
            return 0;
        }

        TLSIndex * index; String path; FileTree * ft = 0;
        int ret = checkPath(pathStr, path, ft, index);
        if (ret) return ret;

        uint32 itemID = ft->findItem(path);
        if (itemID == ft->notFound()) return -ENOENT;

        // Need to list all files for this folder
        int count = 0;
        for (size_t i = 0; i < ft->items.getSize(); i++)
        {
            if (ft->items[i].fixed && ft->items[i].fixed->parentID == (itemID+1))
            {
                if (filler(buf, (const char*)ft->items[i].getBaseName(), 0, 0))
                    return 0;
                count++;
            }
        }
        if (Frost::dumpLevel) fprintf(stdout, "readdir path: %s [%d]\n", (const char*)path, count);
        return 0;
    }

    static int open(const char *pathStr, struct fuse_file_info * fi)
    {
        TLSIndex * index; String path; FileTree * ft = 0;
        int ret = checkPath(pathStr, path, ft, index);
        if (ret) return ret;

        uint32 itemID = ft->findItem(path);
        if (itemID == ft->notFound()) return -ENOENT;

        // Need to figure out the type of the file pointed by
        FileTree::Item * item = ft->getItem(itemID);

        int maxLinkIter = 0;
        while (maxLinkIter < 30)
        {
            String metadata = item->getMetaData(), symlink;
            File::Info info("dumb");
            if (!info.analyzeMetaData(metadata, &symlink)) return -EIO;

            if (info.isLink())
            {
                // Need to read the link
                String linkedItem = File::General::normalizePath(ft->getItemFullPath(itemID) + symlink);
                uint32 linkID = ft->findItem(linkedItem);
                if (linkID == ft->notFound()) return -ENOENT;
                item = ft->getItem(linkID);
                maxLinkIter++;
                continue;
            }
            if (info.isDir()) return -EISDIR;
            if (!info.isFile()) return -EACCES;


            fi->fh = (uint64)new ReadCache(index, item->getChunkListID());
            if (Frost::dumpLevel) fprintf(stdout, "open path: %s [%u]\n", (const char*)path, item->getChunkListID());
            return 0;
        }
        return -ELOOP;
    }

    static int release(const char *pathStr, struct fuse_file_info * fi)
    {
        // We actually don't care about the file to close, since we don't open it anyway.
        if (!fi || !fi->fh) return 0;
        ReadCache * rc = (ReadCache*)fi->fh;
        if (Frost::dumpLevel) fprintf(stdout, "close path: %s [%u]\n", pathStr, rc->chunkListID);
        delete rc;
        return 0;
    }



    static int read(const char *pathStr, char *buf, size_t  size, off_t offset, struct fuse_file_info * fi)
    {
        if (!fi || !fi->fh) return -EIO;

        ReadCache * rc = (ReadCache*)fi->fh;
        if (Frost::dumpLevel) fprintf(stdout, "read path: %s [%lld to %lld]\n", pathStr, offset, offset + size);

        // Now we have the chunk list, let's find the chunks required for this operation
        const Frost::FileFormat::ChunkList * cl = Frost::Helpers::indexFile.getChunkList(rc->chunkListID);
        // Skip the offset to find out which chunk to read from
        size_t startIndex = 0;
        for(; startIndex < cl->chunksID.getSize(); startIndex++)
        {
            const Frost::FileFormat::Chunk * chunk = Frost::Helpers::indexFile.findChunk(cl->chunksID[startIndex]);
            if (!chunk) return -EIO;
            if (offset < chunk->size) break;
            offset -= chunk->size;
        }

        // Ok, now we have the first chunk to read, let's read it
        int ret = 0;
        String errorMessage;
        while (size && startIndex < cl->chunksID.getSize())
        {
            uint32 chunkID = cl->chunksID[startIndex];
            const Frost::FileFormat::Chunk * chunk = Frost::Helpers::indexFile.findChunk(chunkID);
            if (!chunk) return -EIO;

            // Get the multichunk where this chunk is stored
            const Frost::FileFormat::Multichunk * mchunk = Frost::Helpers::indexFile.getMultichunk(chunk->multichunkID);
            if (!mchunk) return -EIO;

            // We need the offset in the multichunk's chunklist so it's faster to restore
            Frost::FileFormat::ChunkList * mcChunkList = Frost::Helpers::indexFile.getChunkList(mchunk->listID);
            size_t chunkOffset = (size_t)-1;
            if (mcChunkList) chunkOffset = mcChunkList->getChunkOffset(chunkID);

            errorMessage = "";
            File::Chunk * chunkF = Frost::Helpers::extractChunkBin(errorMessage, FrostFSOps::remoteFolder, mchunk->getFileName(), mchunk->UID, chunkOffset, chunk->checksum, Frost::Helpers::indexFile.getFilterArguments().getArgument(mchunk->filterArgIndex), rc->index->cache, FrostFSOps::nullCB);
            if (!chunkF || errorMessage) return -EIO;

            // Ok, if we got a chunk, let's save it
            int minSize = min((int)(chunkF->size - offset), (int)size);
            memcpy(&buf[ret], &chunkF->data[offset], minSize);

            offset = 0;
            size -= minSize;
            ret += minSize;
            startIndex++;
        }

        // Need to find the chunklist for the given file
        return ret;
    }

    static int readlink(const char *pathStr, char* buf, size_t size)
    {
        TLSIndex * index; String path; FileTree * ft = 0;
        int ret = checkPath(pathStr, path, ft, index);
        if (ret) return ret;

        uint32 itemID = ft->findItem(path);
        if (itemID == ft->notFound()) return -ENOENT;
        if (!buf || !size) return -EFAULT;

        // Need to figure out the type of the file pointed by
        FileTree::Item * item = ft->getItem(itemID);
        String metadata = item->getMetaData(), symlink;
        File::Info info("dumb");
        if (!info.analyzeMetaData(metadata, &symlink)) return -EIO;

        if (!info.isLink()) return -EINVAL;
        // Buffer is not zero terminated
        int nameSize = min((int)size-1, (int)symlink.getLength());
        memcpy(buf, (const char*)symlink, nameSize);
        buf[nameSize] = 0;
        if (Frost::dumpLevel) fprintf(stdout, "readlink path: %s [%s]\n", (const char*)path, (const char*)symlink);
        return 0;
    }

    static int statfs(const char *pathStr, struct statvfs *stat)
    {
        // We are only interested in the last revision for the FS size
        const Frost::FileFormat::Catalog * c = Frost::Helpers::indexFile.getCatalogForRevision(FrostFSOps::maxRevisionID);
        if (!c) return -ENOENT;

        stat->f_namemax = 1024; // We have to give something here, and we don't need to find the maximum file name here.
        stat->f_fsid = 0;       // We don't know
        stat->f_frsize = 512;   // We have to give something here too
        stat->f_bsize = 512;    // We have to give something here too
        stat->f_flag = ST_RDONLY; // True, we don't support write here
        stat->f_bavail = stat->f_bfree = 0;

        Frost::FileFormat::MetaData md;
        if (c->optionMetadata.fileOffset() && !Frost::Helpers::indexFile.LoadRO(md, c->optionMetadata)) return -EIO;
        const String & initialSize = md.findKey("InitialSize").fromFirst(": ");
        if (initialSize) stat->f_blocks = (int64)initialSize / stat->f_bsize;
        else stat->f_blocks = 0;
        if (Frost::dumpLevel) fprintf(stdout, "statvfs path: %s [%u blocks]\n", (const char*)pathStr, (uint32)stat->f_blocks);
        return 0;
    }

    static int chmod(const char* pathStr, mode_t mode)
    {
        // a noop since mtp doesn't support permissions. But we need to pretend
        // to do it to make things like "cp -r" and the mac finder happy.
        return 0;
    }


};

Frost::String                            FrostFSOps::indexFilePath; // Complete file path
Frost::String                            FrostFSOps::remoteFolder; // Complete remote folder
uint32                                   FrostFSOps::maxMultichunkSize = 0;
uint32                                   FrostFSOps::maxRevisionID = 0;
Threading::Thread::LocalVariable::Key    FrostFSOps::tls;
FrostFSOps::FileTreeMap                  FrostFSOps::fileTrees;
NullProgressCallback                     FrostFSOps::nullCB;


static struct fuse_operations frost_oper = { 0, };


// FUSE system is implemented here
int main(int argc, char ** argv)
{
    // Fill the operations we support
    frost_oper.getattr  = FrostFSOps::getattr;
    frost_oper.readdir  = FrostFSOps::readdir;
    frost_oper.open     = FrostFSOps::open;
    frost_oper.release  = FrostFSOps::release;
    frost_oper.read     = FrostFSOps::read;
    frost_oper.statfs   = FrostFSOps::statfs;
    frost_oper.chmod    = FrostFSOps::chmod;
    frost_oper.readlink = FrostFSOps::readlink;

    FrostFSOptions options;

    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    if (fuse_opt_parse(&args, &options, frost_opts, 0)==-1)
    {
        fprintf(stderr, "Error parsing arguments\n");
        return -1;
    }

    if (options.showHelp) fuse_opt_add_arg(&args, "-h");
    if (options.showVersion) fuse_opt_add_arg(&args, "-V");
    if (options.showDebug) { Frost::dumpLevel = 1; fuse_opt_add_arg(&args, "-f"); }


    if (options.showVersion)
        printf("Frost Fuse version: 2 (build number %d)\n",
#include "build/build-number.txt"
        );

    if (options.showHelp)
    {
        fuse_main(argc, argv, &frost_oper, 0);

        printf("\nFrost Fuse specific options:\n"
               "\t--password=<password>             The password to use to decypher the master key [BEWARE OF YOUR BASH HISTORY], this is optional\n"
               "\t--remote=/path/to/remote          The path where the remote is stored\n"
               "\t--index=/path/to/index            The path where the index file is stored (if empty, using remote path)\n"
               "\t--keyvault=/path/to/keyvaultFile  The path where to the key vault file (if empty, using " DEFAULT_KEYVAULT ")\n");
    }

    if (!options.remote)
    {
        fprintf(stderr, "Remote is required, use -h to get help\n");
        return 0;
    }

    FrostFSOps::remoteFolder = Frost::String(options.remote).normalizedPath();
    FrostFSOps::indexFilePath = options.index ? Frost::String(options.index) : Frost::String::Print("%s/" DEFAULT_INDEX, options.remote);
    Frost::String keyVaultPath = options.keyVault ? options.keyVault : DEFAULT_KEYVAULT;

    // Then open the index file
    Frost::String result = Frost::Helpers::indexFile.readFile(FrostFSOps::indexFilePath, false);
    if (result) { fprintf(stderr, "Can't read the index file given %s: %s\n", (const char*)FrostFSOps::indexFilePath, (const char*)result); return 1; }

    Frost::String pass = options.password ? options.password : "";
    if (!pass)
    {
        // Ask for password
        char password[256];
        size_t passLen = ArrSz(password);
        if (!Platform::queryHiddenInput("Password:", password, passLen))
        {
            fprintf(stderr, "Can't query a password, do you have a terminal or console running ?\n");
            return 1;
        }

        pass = password;
        memset(password, 0, passLen);
    }

    Frost::MemoryBlock cipheredMasterKey = Frost::Helpers::indexFile.getCipheredMasterKey();
    if (!cipheredMasterKey.getSize()) { fprintf(stderr, "Bad readback of ciphered master key\n"); return 1; }

    // Ok, now, we have successfully read the master key, let's open the index file
    FrostFSOps::maxRevisionID = Frost::Helpers::indexFile.getCurrentRevision();
    result = Frost::getKeyFactory().loadPrivateKey(keyVaultPath, cipheredMasterKey, pass, "");
    if (result) { fprintf(stderr, "Can't read the private key from the given keyvault %s: %s\n", (const char*)keyVaultPath, (const char*)result); return 1; }

    // Read all filter argument to find out the maximum size for the multichunks (this is used for setting up the cache per thread)
    const Frost::FileFormat::FilterArguments & fa = Frost::Helpers::indexFile.getFilterArguments();
    for (size_t i = 0; i < fa.arguments.getSize(); i++)
    {
        uint32 maxSize = fa.arguments[i];
        if (FrostFSOps::maxMultichunkSize < maxSize) FrostFSOps::maxMultichunkSize = maxSize;
    }
    // Then read and store all file trees for all revisions (cache mode)
    const Frost::FileFormat::Catalog * c = 0;
    for (uint32 rev = 1; rev <= FrostFSOps::maxRevisionID; rev++)
    {
        c = Frost::Helpers::indexFile.getCatalogForRevision(rev);
        if (!c) { fprintf(stderr, "No catalog found for revision %u\n", rev); return 1; }
        Frost::FileFormat::FileTree * ft = new Frost::FileFormat::FileTree(rev, true);
        if (!ft) { fprintf(stderr, "Not enough memory\n"); return 1; }
        if (!Frost::Helpers::indexFile.Load(*ft, c->fileTree)) { fprintf(stderr, "No file tree found for revision %u\n", rev); return 1; }
        FrostFSOps::fileTrees.storeValue(rev, ft);
    }

    // Ok, we are ready to run
    fprintf(stdout, "Let's go!\n");
    (void)FrostFSOps::getTLS();

    int ret = fuse_main(args.argc, args.argv, &frost_oper, 0);
    // Clean TLS stuff now
    FrostFSOps::cleanTLS();
    return ret;
}
#else
int main(int argc, char ** argv)
{
  #ifdef _POSIX
     // Install memory signal handler
     signal(SIGUSR2, Frost::asyncProcess);
     signal(SIGINT, Frost::asyncProcess);
#endif

    // Build the options array
    Strings::StringArray options(argv, (size_t)argc);
    if (options.getSize() < 2)
        return showHelpMessage();

    Frost::Helpers::compressor = Frost::Helpers::ZLib;

    Logger::ConsoleSink debugSink(~0);
    Frost::dumpLevel = options.indexOf("--verbose") != options.getSize() || options.indexOf("-v") != options.getSize();
    Frost::dumpLevel += options.indexOf("-vv") != options.getSize() ? 2 : 0;
    if (Frost::dumpLevel) Logger::setDefaultSink(&debugSink);

    // Check if we need to encrypt the index file once the backup is done
    Frost::safeIndex = options.indexOf("--safeindex") != options.getSize();

    // This also works for tests, so test it before entering any tests
    if (checkOption(options, "compression") == EXIT_SUCCESS) return EXIT_SUCCESS;
    if (checkOption(options, "entropy") == EXIT_SUCCESS) return EXIT_SUCCESS;
    // Check for bsc selection
    if (optionsMap["compression"] && *optionsMap["compression"] == "bsc")
    {   // Remember the compressor selected
        Frost::Helpers::compressor = Frost::Helpers::BSC;
        File::MultiChunk::setMaximumSize(25*1024*1024);
        optionsMap.storeValue("multichunk", new Strings::FastString("25600K"), true);
    }

    // Check for bsc selection
    if (optionsMap["entropy"])
    {
        if (optionsMap["entropy"]->invFindAnyChar(".0123456789") != -1)
            return showHelpMessage("Bad argument for entropy, should be a decimal number like 0.95");

        Frost::Helpers::entropyThreshold = (double)*optionsMap["entropy"];
    }

    // Test mode first
    int tested = checkTests(options);
    if (tested != BailOut) return tested == EXIT_SUCCESS ? EXIT_SUCCESS : EXIT_FAILURE;

    Strings::StringArray params;
    if (getOptionParameters(options, "help", params))
    {
        if (params.getSize() && params[0] == "security") return showSecurityMessage();
        if (params.getSize() && params[0] == "regex") return showRegExMessage();
        return showHelpMessage();
    }

    // Optional first
    if (checkOption(options, "cache", true) == EXIT_SUCCESS) return EXIT_SUCCESS;
    if (checkOption(options, "overwrite") == EXIT_SUCCESS) return EXIT_SUCCESS;
    if (checkOption(options, "strategy") == EXIT_SUCCESS) return EXIT_SUCCESS;
    if (checkOption(options, "keyid") == EXIT_SUCCESS) return EXIT_SUCCESS;
    if (checkOption(options, "exclude") == EXIT_SUCCESS) return EXIT_SUCCESS;
    if (checkOption(options, "include") == EXIT_SUCCESS) return EXIT_SUCCESS;
    if (checkOption(options, "multichunk", true) == EXIT_SUCCESS) return EXIT_SUCCESS;
    if (checkOption(options, "password") == EXIT_SUCCESS) return EXIT_SUCCESS;

    if (optionsMap["exclude"])
        Frost::Helpers::excludedFilePath = *optionsMap["exclude"];
    if (optionsMap["include"])
    {
        if (!optionsMap["exclude"]) return showHelpMessage("Include list can only be used if an exclusion list is used");
        Frost::Helpers::includedFilePath = *optionsMap["include"];
    }

    if (optionsMap["multichunk"])
        File::MultiChunk::setMaximumSize((uint32)parseNumericSuffixed(*optionsMap["multichunk"]));


    if (optionsMap["overwrite"]
        && *optionsMap["overwrite"] != "yes"
        && *optionsMap["overwrite"] != "no"
        && *optionsMap["overwrite"] != "update")
        return showHelpMessage("Bad argument for overwrite (none of: yes, no, update)");

    if (optionsMap["strategy"]
        && *optionsMap["strategy"] != "slow"
        && *optionsMap["strategy"] != "fast")
        return showHelpMessage("Bad argument for strategy (none of: slow, fast)");

    int remoteOpt = checkOption(options, "remote");
    if (remoteOpt == EXIT_SUCCESS) return EXIT_SUCCESS;
    if (remoteOpt == 1 && !Frost::safeIndex) // Found a remote, set the default index position
        optionsMap.storeValue("index", new Strings::FastString(*optionsMap["remote"]));

    if (checkOption(options, "index") == EXIT_SUCCESS) return EXIT_SUCCESS;

    optionsMap.storeValue("keyvault", new Strings::FastString(DEFAULT_KEYVAULT));
    if (checkOption(options, "keyvault") == EXIT_SUCCESS) return EXIT_SUCCESS;

    if (!optionsMap["cache"])
        optionsMap.storeValue("cache", new Strings::FastString("64M"));


    // Test for actions now
    int ret = 0;
    if ((ret = handleAction(options, "list")) != BailOut) return ret;
    if ((ret = handleAction(options, "filelist")) != BailOut) return ret;
    if ((ret = handleAction(options, "cat")) != BailOut) return ret;
    if ((ret = handleAction(options, "purge")) != BailOut) return ret;
    if ((ret = handleAction(options, "backup")) != BailOut) return ret;
    if ((ret = handleAction(options, "restore")) != BailOut) return ret;
    if ((ret = handleAction(options, "decryptindex"))   != BailOut) return ret;
    if ((ret = handleAction(options, "dump"))       != BailOut) return ret;

    return showHelpMessage("Either backup, purge or restore mode required");
}
#endif
