#ifndef hpp_RobinHoodHashTable_hpp
#define hpp_RobinHoodHashTable_hpp

namespace Container
{
    /** Default function for uint32 */
    uint32 hashIntegerKey(uint32 x) { x = ((x >> 16) ^ x) * 0x45d9f3b; x = ((x >> 16) ^ x) * 0x45d9f3b; x = ((x >> 16) ^ x); return x ? (uint32)x : (uint32)1; } // Avoid 0 as it's reserved
    /** Default function for uint32 */
    uint32 hashIntegerKey(uint64 x) { x = ((x >> 32) ^ x) * 0x45d9f3b; x = ((x >> 16) ^ x) * 0x45d9f3b; x = ((x >> 16) ^ x) * 0x45d9f3b; x = ((x >> 16) ^ x); return x ? (uint32)x : (uint32)1; }
    /** Default function for uint32 */
    uint32 hashIntegerKey(uint16 x) { uint32 r = x * 0x45d9f3b; r = ((r >> 16) ^ r); return r ? r : 1; }
    /** Default function for any other integer type is to avoid hashing */
    template <typename T> uint32 hashIntegerKey(T x) { return x; }


    /** Implementation of the hashing policies for integers.
        @param T    must be an integer type
        You must provide a uint32 hashIntegerKey(T) overload for this to work */
    template <typename T>
    struct IntegerHashingPolicy
    {
        /** The type for the hashed key */
        typedef uint32 HashKeyT;
        /** Allow compiletime testing of the default value */
        enum { DefaultAreZero = 1 };

        /** Check if the initial keys are equal */
        static bool isEqual(const T key1, const T key2) { return key1 == key2; }
        /** Compute the hash value for the given input */
        static inline HashKeyT Hash(T x) { return hashIntegerKey(x); }
        /** Get the default hash value (this is stored by default in the buckets), it's the value that's means that the hash is not computed yet. */
        static inline HashKeyT defaultHash() { return 0; }
        /** Reset the key value to default */
        static inline void resetKey(T & key) { key = 0; }
    };

    /** The internal table is made of these. We use the same optimization as described in RobinHood's paper */
    template <typename T, typename Key, typename HashPolicy = IntegerHashingPolicy<Key> >
    struct Bucket
    {
        /** The hask key type we need */
        typedef typename HashPolicy::HashKeyT HashKeyT;

        /** This bucket data */
        T           data;

        /** Get the hash for this bucket */
        inline HashKeyT getHash(void *) const { return hash; }
        /** Get the key for this bucket */
        inline Key getKey(void *) const { return key; }
        /** Set the hask for this bucket */
        inline void setHash(const HashKeyT h, void *) { hash = h; }
        /** Set the key for this bucket */
        inline void setKey(const Key & k, void *) { key = k; }
        /** Reset the key to the default value */
        inline void resetKey(void *) { HashPolicy::resetKey(key); }
 
        /** Swap the content of this bucket with the given parameters.
            @return On output, this key is set to k, this hash is set to h and data is set to value, and k is set to the previous key, h to the previous hash, value to the previous data */
        inline void swapBucket(Key & k, HashKeyT & h, T & value)
        {
            HashKeyT tmpH = hash; hash = h; h = tmpH;
            Key  tmpK = key; key = k; k = tmpK;
            T tmpD = data; data = value; value = tmpD;
        }
        /** Swap the bucket with another one */
        inline void swapBucket(Bucket & o) { swapBucket(o.key, o.hash, o.data); }


        Bucket() : hash(HashPolicy::defaultHash()) { HashPolicy::resetKey(key); }

    private:
        /** The hashed key */
        HashKeyT    hash;
        /** The key itself */
        Key         key;
    };


    /** This class implements a Hopscotch hash table.
        For usage, @sa Tests::RobinHoodHashTableTests
     
        @warning The type must be a Plain Old Data (with no destructor), since no destructor are called, and data is moved
        @param T            The data type. Must be very small and POD
        @param Key          The key type.
        @param HashPolicy   Defaults to integer based key (should be small and POD too)
        @param Bucket       The bucket type to use (default to a very simple bucket type, but can be made more compact by template specialization) */
    template <typename T, typename Key = uint32, typename HashPolicy = IntegerHashingPolicy<Key>, typename Bucket = Bucket<T, Key, HashPolicy> >
    class RobinHoodHashTable
    {
        // Type definitions and enumerations
    public:
        /** The hash key type we are using */
        typedef typename HashPolicy::HashKeyT HashKeyT;

        /** Constants used in this table */
        enum
        {
            GrowthRate = 2, //!< The default exponential grow rate
        };
        /** The generator callback function used in resize */
        typedef bool (*ResizeGenFunc)(size_t index, T & t, Key & k, void * token);

        // Members
    private:
        /** The hash table storage */
        Bucket *        table;
        /** The load factor */
        const float     loadFactor;
        /** The current count of items in the table */
        size_t          count;
        /** The current table allocation size */
        size_t          allocSize;
        /** The maximum probing size */
        size_t          probingMaxSize;

        /** The opaque cross-bucket object */
        void *          opaque;


        // Helpers
    private:
        /** Compute the distance to the initial value */
        size_t computeDistToInit(size_t index) const
        {
            const HashKeyT hash = table[index].getHash(opaque);
            if (hash == HashPolicy::defaultHash()) return allocSize; // Impossible distance
            size_t init = table[index].getHash(opaque) % allocSize;
            return init <= index ? index - init : index + allocSize - init; // Warp the index
        }

        // Interface
    public:
        /** Clear the table */
        void Clear(const size_t newSize = 0, void * newOpaque = 0)
        {
            opaque = newOpaque;
            count = 0;
            if (newSize == allocSize)
            {
                for (size_t i = 0; i < allocSize; i++) { table[i].~Bucket(); new (&table[i]) Bucket(); }
                return;
            }
            // Destruct
            for (size_t i = 0; i < allocSize; i++) table[i].~Bucket();
            allocSize = newSize;
            probingMaxSize = allocSize;
            
            free(table);
            if (newSize)
            {   // And reconstruct
                table = (Bucket*)malloc(allocSize * sizeof(Bucket));
                for (size_t i = 0; i < allocSize; i++) new (&table[i]) Bucket();
            }
        }
    
        /** Check if this table contains the given key */
        bool containsKey(const Key & key) { return getValue(key) != 0; }

        /** Get the value for the given key */
        T * getValue(const Key & key) const
        {
            const HashKeyT hash = HashPolicy::Hash(key);
            size_t initPos = hash % allocSize;

            for (size_t i = 0; i < probingMaxSize; i++)
            {
                size_t current = (initPos + i) % allocSize;
                size_t probeDist = computeDistToInit(current);
                if (probeDist == allocSize || i > probeDist) break; // Not found within the given distance

                if (HashPolicy::isEqual(key, table[current].getKey(opaque))) return &table[current].data;
            }
            return 0;
        }

        /** Store a value in the table */
        bool storeValue(Key key, T data)
        {
            if ((count+1) == allocSize * loadFactor) return false; // Not enough space to worth appending it

            HashKeyT hash = HashPolicy::Hash(key);
            size_t initPos = hash % allocSize;
            size_t current = 0, probeCurrent = 0;

            // Search the insertion position
            for (size_t i = 0; i < probingMaxSize; i++)
            {
                current = (initPos + i) % allocSize;
                if (table[current].getHash(opaque) == HashPolicy::defaultHash())
                {
                    table[current].data = data;
                    table[current].setKey(key, opaque);
                    table[current].setHash(hash, opaque);
                    break;
                }
                size_t probeDist = computeDistToInit(current); // If the bucket is empty, the previous test used it already, so this can not happen here
                if (probeCurrent > probeDist)
                {   // Swap the current bucket with the one to insert
                    table[current].swapBucket(key, hash, data);
                    probeCurrent = probeDist;
                }
                probeCurrent++;
            }
            count++;
            return true;
        }

        /** Extract a value from the table. The value is forget from the table (done by swapping 0 with the value) */
        T extractValue (const Key & key)
        {
            const HashKeyT hash = HashPolicy::Hash(key);
            size_t initPos = hash % allocSize;
            size_t current = 0, probeCurrent = 0;
            T ret = 0;

            for(size_t i = 0; i < probingMaxSize; i++)
            {
                current = (initPos + i) % allocSize;
                size_t probeDist = computeDistToInit(current);
                if (probeDist == allocSize || i > probeDist) return ret; // Not found within the given distance

                if (HashPolicy::isEqual(key, table[current].getKey(opaque)))
                {
                    // Delete entry now
                    table[current].resetKey(opaque);
                    table[current].setHash(HashPolicy::defaultHash(), opaque);
                    ret = table[current].data;
                    table[current].data = 0;

                    // Backshift happens here
                    for (size_t j = 1; j < allocSize; j++)
                    {
                        size_t prev = (current + j - 1) % allocSize;
                        size_t swap = (current + j) % allocSize;
                        if (table[swap].getHash(opaque) == HashPolicy::defaultHash())
                        {
                            table[prev].data = 0;
                            break;
                        }
                        size_t probeDist = computeDistToInit(swap);
                        if (probeDist == allocSize) return 0; // Error in computing the probing distance
                        if (probeDist == 0)
                        {
                            table[prev].data = 0;
                            break;
                        }
                        // Swap buckets now
                        table[prev].swapBucket(table[swap]);
                    }
                    count--;
                    return ret;
                }
            }
            // Not found
            return ret;
        }

        /** This is used only in test to ensure coherency of the table */
        size_t computeSize()
        {
            size_t counter = 0;
            for(uint32 i = 0; i < allocSize; ++i)
                counter += table[i].getHash(opaque) != HashPolicy::defaultHash();
            return counter;
        }
        /** Get the current table's items count */
        size_t getSize() const { return count; }
        /** Get the current memory usage for this table (in bytes) */
        size_t getMemUsage() const { return sizeof(*this) + allocSize * sizeof(*table); }

        /** Check if the table need resizing */
        bool shouldResize() const { return (count + 1) >= allocSize * loadFactor; }

        /** Resize the table.
            There are two possibilities here, here it's done automatically by running other the current set, or by external process.
            The implementation here is local (so not optimal because it can use 50% more memory). To use the second method, use the move method
            Resizing is done by using the growth rate defined */
        bool resize(ResizeGenFunc generator = 0, void * token = 0 )
        {
            if (!generator)
            {
                // We create a new table that's twice the size and insert all items in there, then swap all values with it.
                RobinHoodHashTable other(allocSize * GrowthRate, opaque);
                for(size_t i = 0; i < allocSize; ++i)
                    if (table[i].getHash(opaque) != HashPolicy::defaultHash())
                    {
                        if (!other.storeValue(table[i].getKey(opaque), table[i].data)) return false;
                    }


                // Then we need to move the data from other to use
                free(table); table = other.table; other.table = 0;
                allocSize = other.allocSize;
                probingMaxSize = other.probingMaxSize;

                return true;
            }
            else
            {
                // To avoid allocating twice the size here, we can add 100% to the current allocation with realloc.
                // However, no other thread should be processing the table while doing so, and we don't support this yet (RW lock will be added later on)
                // The given generator will be called to refill the table once reallocated.
                // At least on linux, realloc for big area does not make any copy so it's ok to call realloc here
                size_t oldCount = count;
                size_t newAllocSize = allocSize * GrowthRate;
                Bucket * prev = (Bucket*)realloc(table, newAllocSize * sizeof(Bucket));
                if (!prev) return false; // Failed to realloc larger
                table = prev;
                allocSize = newAllocSize;
                probingMaxSize = newAllocSize;
                count = 0;
                // Call constructors now
                for (size_t i = 0; i < allocSize; i++) new(&table[i]) Bucket();
                // Then fill the table
                T item; Key k;
                for (size_t i = 0; i < oldCount; i++)
                {
                    if (!generator(i, item, k, token)) return false;
                    if (!storeValue(k, item)) return false;
                }
                return true;
            }
            return false;
        }

        // Construction and destruction
    public:
        /** Construct a RobinHood hash table */
        RobinHoodHashTable(const size_t allocSize, void * opaque = 0)
            :  table((Bucket*)malloc(allocSize * sizeof(*table))), loadFactor(0.80), count(0),
                allocSize(allocSize), probingMaxSize(allocSize), opaque(opaque)
        {
            // We want to use realloc here, so new[] and delete[] are inaccessible here
            for (size_t i = 0; i < allocSize; i++) new(&table[i]) Bucket();
        }

        /** Default destructor */
        ~RobinHoodHashTable()
        {
            // We want to use realloc here, so new[] and delete[] are inaccessible here
            Clear();
        }
    };

}


#endif
