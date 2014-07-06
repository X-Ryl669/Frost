#ifndef hpp_HashTable_hpp
#define hpp_HashTable_hpp

// We need types 
#include "../Types.hpp"
// We need containers
#include "../Container/Container.hpp"

namespace Container
{
    /** Hash the given key.
        The hasher interface transform a key to make searching faster.
        The identity transformer interface gives the same output as the input.
        @warning The HashKeyT must be a plain old data, or it will break, and it must support % operator
    */
    template <class KeyType>
    struct NoHashKey
    {
        /** The final hash type */
        typedef KeyType HashKeyT;
        /** The key hashing process */
        static inline HashKeyT hashKey(const KeyType & initialKey) { return initialKey; }
        /** Compare function */
        static inline bool     compareKeys(const KeyType & first, const KeyType & second) { return first == second; }
    };
        
    /** The hasher interface transform a key to make searching faster. */
    template <class KeyType>
    struct HashKey
    {
        /** The final hash type */
        typedef uint32 HashKeyT;
        
        /** The key hashing process */
        static HashKeyT hashKey(const KeyType & initialKey);
        /** Compare function */
        static inline bool compareKeys(const KeyType & first, const KeyType & second) { return first == second; }
    };

    /** Default deletion for node's objects doing nothing (no deletion) */
    template <typename T>
    struct NoDeletion
    {   inline static void deleter(T *) {} };

    /** Default true deletion for node's objects  */
    template <typename T>
    struct DeletionWithDelete
    {   inline static void deleter(T *& t) { delete t; t = 0; } };    

    /** Default true deletion for node's objects with free */
    template <typename T>
    struct DeletionWithFree
    {   inline static void deleter(T *& t) { free((void*)t); t = 0; } };    

#if (_MSC_VER > 1200 || !defined(_MSC_VER))
    // The selector based on POD key type, this clear speed up the hash table with POD is used for key
    template <bool param, class KeyType> 
    struct PODSelect 
    { 
        // The keys array (as keys might not be POD) 
        typedef typename Container::WithCopyConstructor<KeyType>::IndexList  KeyArrayT;
        typedef const KeyType * Type; 
        static bool saveKey(const KeyType & key, Type & nodeKey, KeyArrayT & keyArray)
        {
            KeyType * keyCopy = new KeyType(key);
            if (!keyCopy) return false; 
            keyArray.Append(keyCopy);
            nodeKey = keyCopy; 
            return true;
        }
        static bool removeKey(Type nodeKey, KeyArrayT & keyArray)
        {
            size_t keyIndex = keyArray.indexOf(const_cast<KeyType*>(nodeKey)); 
            if (keyIndex == keyArray.getSize()) return false; // Can't find the key
            keyArray.Remove(keyIndex);
            return true;
        }
        static const KeyType & accessKey(Type nodeKey) { return *nodeKey; }
        static bool isValid(Type nodeKey) { return nodeKey != 0; }
    };

    template <class KeyType> 
    struct PODSelect<true, KeyType> 
    { 
        // The keys array (as keys might not be POD) 
        typedef typename Container::WithCopyConstructor<KeyType>::IndexList  KeyArrayT;
        typedef KeyType Type; 
        static bool saveKey(const KeyType & key, Type & nodeKey, KeyArrayT &)
        {
            nodeKey = key;
            return true;
        }
        static bool removeKey(Type nodeKey, KeyArrayT & keyArray) { return true; }
        static const KeyType & accessKey(Type & nodeKey) { return nodeKey; }
        static bool isValid(Type nodeKey) { return true; }
    };

#endif

    /** The hash table class.
        This hash table is very simple to use.
        You'll use storeKey to store a T at the given key in the hash table.
        You'll use removeKey to remove a T at the given key in the hash table.
        You'll use getValue to get a pointer for the given key (or use the operator []).
        You'll use containsKey to check if a key exists.
        
        As for other container, you have a getSize() and isEmpty() method.
        You can iterate over the hash table's entries by calling iterateAllEntries, or using the getFirstIterator.
        
        @param T                The type to store in this table. The actual storage is a pointer to a T object that's owned
        @param KeyType          The key to use for locating values. The table detect for most Plain Old Data (POD) 
                                keys and perform an optimization in this case. If you're not using POD key, the keys are saved 
                                as pointers that are owned by the class.
        @param HashPolicy       The hash policy to use to map the KeyType to an integer. Usually you'll either use 
                                HashKey<KeyType> for transformation based policy (typically character based String to int hashing) or
                                NoHashKey<KeyType> if your key's type is already acting like an integer, and supports modulo operation
        @param DeletionPolicy   The deletion policy to use. You can either use NoDeletion<T>, DeletionWithDelete<T> or DeletionWithFree<T>
        @param withPODKey       Unless you're doing something very specific, you should not mess with that parameter (and in 
                                that case, you'll need to figure out the meaning by yourself). 
        @param hasCopyConst     If you intend to copy your table, the type to store in the table must have a copy constructor. 
                                Since it's not possible to detect this with C++ < C++0x, you have to specify this by hand. */
    template <typename T, typename KeyType, typename HashPolicy = NoHashKey<KeyType> , typename DeletionPolicy = DeletionWithDelete<T>, bool withPODKey = IsPOD<KeyType>::result != 0, bool hasCopyConst = IsPOD<T>::result != 0>
    class HashTable
    {
        // Required for copy constructor hackery
    private: struct Unbuildable {};
    
        // Type definition and enumeration
    public:
        /** The keys array (as keys might not be POD) */
        typedef typename Container::WithCopyConstructor<KeyType>::IndexList  KeyArrayT;

        template <bool val, class First, class Other> struct TypeSelect { typedef Other Type; };
        template <class First, class Other> struct TypeSelect<true, First, Other> { typedef First Type; };
        typedef typename TypeSelect<hasCopyConst, const HashTable &, Unbuildable>::Type CopyTypeT; 
        typedef typename TypeSelect<!hasCopyConst, const HashTable &, Unbuildable>::Type NonCopyTypeT; 

#if _MSC_VER && (_MSC_VER <= 1200)
        /** The selector based on POD key type */
        template <bool key> struct PODSelect { };

        template <> struct PODSelect<true> 
        { 
            typedef KeyType Type; 
            static bool saveKey(const KeyType & key, Type & nodeKey, KeyArrayT & keyArray)
            {
                nodeKey = key;
                return true;
            }
            static bool removeKey(Type nodeKey, KeyArrayT & keyArray) { return true; }
            static const KeyType & accessKey(Type & nodeKey) { return nodeKey; }
            static bool isValid(Type nodeKey) { return true; }
        };
        template <> struct PODSelect<false> 
        { 
            typedef const KeyType * Type; 
            static bool saveKey(const KeyType & key, Type & nodeKey, KeyArrayT & keyArray)
            {
                KeyType * keyCopy = new KeyType(key);
                if (!keyCopy) return false; 
                keyArray.Append(keyCopy);
                nodeKey = keyCopy; 
                return true;
            }
            static bool removeKey(Type nodeKey, KeyArrayT & keyArray)
            {
                uint32 keyIndex = keyArray.indexOf(const_cast<KeyType*>(nodeKey)); 
                if (keyIndex == keyArray.getSize()) return false; // Can't find the key
                keyArray.Remove(keyIndex);
                return true;
            }
            static const KeyType & accessKey(Type nodeKey) { return *nodeKey; }
            static bool isValid(Type nodeKey) { return nodeKey != 0; }
        };
        typedef PODSelect<withPODKey>  PODKey;
        typedef PODKey::Type           PODKeyT;
#else
        typedef typename Container::PODSelect<withPODKey, KeyType>::Type  PODKeyT;
        typedef Container::PODSelect<withPODKey, KeyType>                 PODKey;
#endif

        /** The entry item */
        struct Entry
        {
            /** The next entry (used if the bucket is already filled) */
            Entry *                     next;
            /** The entry final key index */
            PODKeyT                     key;
            /** The entry value */
            T  *                        value;
        };

        /** The hash table iterator */
        struct IterT
        {
            const HashTable * hash;
            mutable uint32      currentIndex; 
            mutable Entry *     currentEntry;

            IterT(const HashTable * hash, const uint32 index = 0, Entry * first = 0)
                : hash(hash), currentIndex(index), currentEntry(first) {}

            /** Increment the iterator */
            const IterT & operator++() const 
            {
                if (currentEntry && currentEntry->next)
                    currentEntry = currentEntry->next;
                else
                {
                    currentEntry = 0;
                    while (hash && !currentEntry && currentIndex < hash->capacity)
                        currentEntry = hash->entryTable[currentIndex++];
                }
                return *this;
            }
            /** Post-fix increment */
            inline IterT operator ++(int) const { IterT tmp(hash, currentIndex, currentEntry); ++(*this); return tmp; }

            /** Access the data */
            inline T * operator *() { return currentEntry ? currentEntry->value : 0; }
            /** Access the data */
            inline const T * operator *() const { return currentEntry ? currentEntry->value : 0; }
            /** Access the key */
            inline const KeyType * getKey() const { return currentEntry ? &PODKey::accessKey(currentEntry->key) : 0; }
            /** Check for object validity */
            inline bool isValid() const { return currentEntry != 0; }

            /** Comparison operator */
            inline bool operator == (const IterT & other) const { return other.currentEntry == currentEntry; } 
            /** Comparison operator */
            inline bool operator != (const IterT & other) const { return other.currentEntry != currentEntry; } 
            /** Copy operator */
            inline IterT & operator = (const IterT & other) { if (this != &other) new(this) IterT(other); return *this; }
        };

        /** Allow the iterator to access us directly */
        friend struct IterT;

        // Members
    protected:
        /** The hash table capacity (max number of entry) */
        uint32      capacity;
        /** The current hash table usage */
        uint32      count;
        /** The threshold before rehashing */
        uint32      threshold;
        /** The modification count */
        uint32      modCount;
        /** The allowed load factor */
        float       loadFactor;
        /** The hash table entries are stored here */
        Entry **    entryTable;
        /** The keys array */
        KeyArrayT   keyArray;

        // Construction / Destruction
    public:
        /** Default constructor that let specify the capacity of the hash table */
        HashTable(uint32 _capacity = 101, float _loadFactor = 0.75)
            : capacity(_capacity), count(0), modCount(0), loadFactor(_loadFactor)
        {
            if (loadFactor <= 0) { capacity = 0; return; }
            entryTable = (Entry **)malloc(capacity * sizeof(Entry*));
            if (!entryTable) { capacity = 0; return; }
            memset(entryTable, 0, capacity * sizeof(Entry*));

            threshold = (uint32)((float)capacity * loadFactor + 0.5f);
        }
        /** Try to build a copy constructor */
        HashTable(CopyTypeT other) 
            : capacity(other.capacity), count(0), threshold(other.threshold), modCount(0), loadFactor(other.loadFactor)
        {
            entryTable = (Entry**)malloc(capacity * sizeof(Entry*));
            if (!entryTable) { capacity = 0; return; }
            memset(entryTable, 0, capacity * sizeof(Entry*));

            for (uint32 index = 0; index < capacity; index++)
            {
                Entry * e = other.entryTable[index];
                while (e)
                {
                    if (!storeValue(PODKey::accessKey(e->key), new T(*e->value))) return;
                    e = e->next;
                }
            }
        }
        /** Destructor */
        ~HashTable()
        {
            clearTable(true);
        }

        // Interface 
    public:
        /** Clear the hash table */
        inline void    clearTable(const bool clean = false)
        {
            for (uint32 i = 0; i < capacity; i++)
            {
                Entry * current, * next;
                for (current = entryTable[i]; current; current = next)
                {
                    next = current->next;
                    DeletionPolicy::deleter(current->value);
                    free(current);
                }
            }
            free(entryTable); 
            if (!clean)
            {
                capacity = 101; 
                entryTable = (Entry **)malloc(capacity * sizeof(Entry*));
                if (!entryTable) { capacity = 0; return; }
                memset(entryTable, 0, capacity * sizeof(Entry*));
            }
            else 
            {
                capacity = 0;
                entryTable = 0;
            }

            threshold = (uint32)((float)capacity * loadFactor + 0.5f);
            modCount = count = 0;    
            if (!withPODKey) keyArray.Clear();
        } 
        /** Is the hash table empty ? */
        inline bool    isEmpty() const  { return count == 0; }
        /** Does the table contains the given key ? 
            @param key The key to look for 
            @return true if the get is in the hash table */
        inline bool    containsKey(const KeyType & key) const { return getHashEntry(key) != 0; }
        /** Get the value for the given key. 
            @param key The key to look for 
            @return The linked value with the key, or 0 if not found */
        inline T *     getValue(const KeyType & key) const { Entry * e = getHashEntry(key); return e ? e->value : 0; }
        /** Store a key & value in the table.
            @param key      The key for the new entry 
            @param value    The value for the new entry 
            @param update   If true, value is updated on collision
            @return true on success or if the key exist (in that case, the value is updated depending on update), false otherwise */
        bool storeValue(const KeyType & key, T * value, const bool update = false)
        {
            Entry * e = getHashEntry(key);
            if (e) 
            {
                if (update) { DeletionPolicy::deleter(e->value); e->value = value; }
                return true;
            }
            typename HashPolicy::HashKeyT hash = HashPolicy::hashKey(key);
            uint32 index = 0;
            modCount ++;
            if (count > threshold) rehashTable();

            index = hash % capacity;
            e = (Entry *)malloc(sizeof(Entry));
            if (!e) return false;

            // Append the key in the array
            if (!PODKey::saveKey(key, e->key, keyArray)) { free(e); return false; }
            e->value = value; e->next = entryTable[index]; entryTable[index] = e;
            count++; return true;
        }
        /** Get the hash table usage */
        uint32 getSize() const { return count; }

        /** Remove a key entry from the table.
            @param key The key to look for 
            @return The linked value with the key, or 0 if not found 
            @warning Removing is still a O(N) operation if the key type is not a POD, as we are scanning the key array to remove the entry key 
            @warning You must clean the returned object depending on the chosen hash policy */
        T *  extractValue(const KeyType & key) 
        {
            typename HashPolicy::HashKeyT hash = HashPolicy::hashKey(key);
            if (!count || !capacity) return 0;
            uint32 index = hash % capacity;
            Entry * e = entryTable[index], *prev = 0;

            // Find the entry
            for (; e; e = e->next)
            {
                if (!PODKey::isValid(e->key)) return 0;
                if (HashPolicy::hashKey(PODKey::accessKey(e->key)) == hash && HashPolicy::compareKeys(PODKey::accessKey(e->key), key))
                {
                    // Delete the key in the key array 
                    if (!PODKey::removeKey(e->key, keyArray)) return 0;

                    T * value;
                    modCount++;
                    if (prev) prev->next = e->next;
                    else entryTable[index] = e->next;

                    count --;
                    value = e->value;
                    
                    free(e);
                    return value;
                }
                prev = e;
            }
            return 0;
        }

        /** Remove a key from the table.
            @param key The key to look for 
            @return true on successful removing of the object, false if not found
            @warning Removing is still a O(N) operation if the key type is not a POD, as we are scanning the key array to remove the entry key */
        bool removeValue(const KeyType & key) 
        {
            typename HashPolicy::HashKeyT hash = HashPolicy::hashKey(key);
            if (!count || !capacity) return false;
            uint32 index = hash % capacity;
            Entry * e = entryTable[index], *prev = 0;

            // Find the entry
            for (; e; e = e->next)
            {
                if (!PODKey::isValid(e->key)) return false;
                if (HashPolicy::hashKey(PODKey::accessKey(e->key)) == hash && HashPolicy::compareKeys(PODKey::accessKey(e->key), key))
                {
                    // Delete the key in the key array 
                    if (!PODKey::removeKey(e->key, keyArray)) return false;

                    T * value;
                    modCount++;
                    if (prev) prev->next = e->next;
                    else entryTable[index] = e->next;

                    count --;
                    value = e->value;
                    
                    free(e);
                    DeletionPolicy::deleter(value);
                    return true;
                }
                prev = e;
            }
            return false;
        }

        /** Iterate the hash table calling the given callback method.
            If you have a class declared like:
            @code
                class A { public: int FuncToApplyToAllEntry(const KeyType & key, const T * t); };
            @endcode
            you will call this method like this :
            @code
                MyService.iterateAllEntries(AInstance, &A::FuncToApplyToAllEntry);
            @endcode

            @param obj  The object to call the method onto
            @param fun  The method to call 
            @return Your callback method can return 0 to stop processing, -1 to 
                    restart iterating (after a remove for example), anything else continue iteration */
        template <typename Obj, typename Process>
        void iterateAllEntries(Obj & obj, const Process & fun) 
        {
            for (uint32 index = 0; index < capacity; index++)
            {
                Entry * e = entryTable[index];
                while (e)
                {
                    int result = (obj.*fun)(PODKey::accessKey(e->key), e->value);
                    if (result == -1) { index = (uint32)-1; break; } // Restart iterating from the beginning
                    if (!result) return;
                    e = e->next;
                }
            }
        }

        /** Shortcut to use the [] operator to access data */
        inline T * operator[] (const KeyType & key) const { return getValue(key); }

        /** Get the first iterator */
        inline IterT getFirstIterator() { return ++IterT(this); }
        /** Get iterator on first object */
        inline const IterT getFirstIterator() const { return ++IterT(this); }


        // Helper methods
    protected:
        /** Rehash the table after a growing */
        bool rehashTable()
        {
            uint32 newCapacity = capacity * 2 + 1;
            Entry ** newTable = (Entry **)malloc(newCapacity * sizeof(Entry*));
            if (!newTable) { return false; }
            memset(newTable, 0, newCapacity * sizeof(Entry*));

            modCount++;
            threshold = (uint32)((float)newCapacity * loadFactor + 0.5f);
            
            for (uint32 i = 0; i<capacity; i++)
            {
                Entry * old, *e = 0;
                for (old = entryTable[i]; old;)
                {
                    e = old; old = old->next;
                    uint32 index = HashPolicy::hashKey(PODKey::accessKey(e->key)) % newCapacity;
                    e->next = newTable[index];
                    newTable[index] = e;
                }
            }
            free(entryTable);
            entryTable = newTable;
            capacity = newCapacity;

            return true;            
        }
        
        /** Get a hash entry from the table */
        inline struct Entry *  getHashEntry(const KeyType & key) const
        {
            typename HashPolicy::HashKeyT hash = HashPolicy::hashKey(key);
            if (!count || !capacity) return 0;
            uint32 index = hash % capacity;

            // Find the entry
            for (Entry * e = entryTable[index]; e; e = e->next)
            {
                if (HashPolicy::hashKey(PODKey::accessKey(e->key)) == hash && HashPolicy::compareKeys(PODKey::accessKey(e->key), key))
                    return e;
            }
            return 0;
        }
    public:
        /** Even if copying the object around is bad for performance reason, here an operator = defaulting to 
            calling the copy constructor. If the type T doesn't allow copy, you'll not end up here, 
            unless you set the last parameter of the template HashTable to true. */
        HashTable & operator = (CopyTypeT other)
        {
            if (this == &other) return *this;
            this->~HashTable();
            return *new (this) HashTable(other);
        }
    
    private:
        /** Prevent copying if you don't provide a copy constructor for the type T */
        HashTable & operator = (NonCopyTypeT other);
        /** When no copy constructor is available for type T, if you try to copy the hash table, you'll get a compile error here.
            If you think the type T has a copy constructor, then change the last parameter of the HashTable template to yes */
        HashTable(NonCopyTypeT other);
    };

}

#ifdef hpp_Strings_hpp
namespace Strings
{
    typedef Container::HashTable<FastString, FastString, Container::HashKey<FastString>, Container::DeletionWithDelete<FastString>, false, true > StringMap;
}
#endif
    
#endif
