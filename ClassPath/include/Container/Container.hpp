#ifndef hpp_CPP_Container_CPP_hpp
#define hpp_CPP_Container_CPP_hpp

#include <new>
// We need general types
#include "../Types.hpp"
// We need Bool2Type declaration
#include "Bool2Type.hpp"

/** Containers (array, list, and so on) are declared here.

    @sa Container::PrivateGenericImplementation::Array
    @sa Container::PrivateGenericImplementation::IndexList
    @sa Container::PrivateGenericImplementation::ChainedList
    @sa Container::HashTable
    @sa Container::Algorithms

    You'll likely use the class from this namespace like this:
    @code
        // Build an array of my simple struct
        Container::PlainOldData<MyStruct>::Array arrayOfMyStruct;
        // Build an indexed list of my class
        Container::PlainOldData<MyStruct>::IndexList listOfMyStruct;
        // Build an double chained list of my class
        Container::PlainOldData<MyStruct>::ChainedList linkedListOfMyStruct;

        // If you're not using plain old data struct, but a class
        Container::WithCopyConstructor<MyClass>::Array arrayOfMyClass;
        // Build an indexed list of my class
        Container::WithCopyConstructor<MyClass>::IndexList listOfMyClass;
        // Build an double chained list of my class
        Container::WithCopyConstructor<MyClass>::ChainedList linkedListOfMyClass;

        // Now if your class is not constructible (no constructor or not accessible)
        Container::NotConstructible<UnbuildableClass>::IndexList    unbuildableClassArray;
        // Or a double chained list
        Container::NotConstructible<UnbuildableClass>::ChainedList  unbuildableClassList;
    @endcode
*/
namespace Container
{
    // Overload the min / max macro in our own namespace
    template <typename T> inline T min(T a, T b) { return a < b ? a : b; }
    // Overload the min / max macro in our own namespace
    template <typename T> inline T max(T a, T b) { return a > b ? a : b; }

#define MaxValStep(t)   ( ((t)1) << (CHAR_BIT * sizeof(t) - 1 - ((t)-1 < 1)) )
    /** This is the maximum position that can be reached by a container */
    const size_t MaxPositiveSize = (MaxValStep(size_t) - 1 + MaxValStep(size_t));
#undef MaxValStep

    namespace PrivateGenericImplementation
    {
        /** The memory policy interface */
        template <typename T>
        struct MemoryPolicy
        {
            /** This method copy (can also move depending on policy) the given data */
            static void Copy(T & dest, const T & src);
            /** This method copy the srcSize data from src to dest, but doesn't resizing dest if it's too small (Copy is then limited to destSize elements). */
            static void CopyArray(T * dest, const T * src, const size_t & destSize, const size_t srcSize);
            /** This method copy the srcSize data from src to dest, resizing dest if required.
                @warning dest must not be 0
            */
            static void CopyArray(T *& dest, const T * src, size_t & destSize, const size_t srcSize);
            /** This method returns a default constructed element or can throw */
            static T & DefaultElement(); // This method can throw
            /** This method search for the first element that match the given element and returns arraySize if not found */
            static size_t Search(const T* array, const size_t arraySize, const size_t from, const T & valueToLookFor);
            /** This method search for the last element that match the given element and returns arraySize if not found */
            static size_t ReverseSearch(const T* array, const size_t arraySize, const T & valueToLookFor);
            /** This method perform a non destructive allocation in place if possible or move the given array to a new place copying data in it */
            static T * NonDestructiveAlloc(const T* array, const size_t currentSize, const size_t newSize, const T & fillWith);
            /** This method perform a non destructive allocation in place if possible or move the given array to a new place copying data in it */
            static T * NonDestructiveAlloc(const T* array, const size_t currentSize, const size_t newSize,  T * const fillWith);
            /** This method perform a non destructive allocation in place if possible or move the given array to a new place copying data in it, plus inserting the given object at the given place and DefaultElement for empty places if any */
            static T * Insert(const T* array, const size_t currentSize, const size_t newSize, const size_t index, const T & elementToInsert);
            /** Delete the array */
            static void DeleteArray(const T * array, const size_t currentSize);
            /** Compare array */
            static bool CompareArray(const T * array1, const T * array2, const size_t commonSize);
        };

        /** The array takes ownership of the element passed in by copying the element in its own heap allocated array.
            <br>
            It deals with array growth and decrease by itself, and it's optimized to limit the copying to the minimum required.
            When using plain old data like int/float/etc..., it realloc its internal array for growing, only when absolutely required.<br>
            When using objects with copy constructor, it still realloc its array that stores the objects, and
            copy the given object to the un-allocated space with the copy constructor if possible.<br>
            This way, growing the array for an append only operation, calls the copy constructor once.<br>
            If the realloc fails / or will move the data, it calls the copy constructor for all previous object like any other dynamic array.
            <br>
            <br>
            Pros : Fastest possible access time once constructed (as fast as pointer dereferencing). Element order is always preserved. <br>
            Cons : Construction / Adding and Remove operations are slow because copying is done on per element basis
            <br>
            <br>
            For usage, @sa Tests::ContainerArrayTests
            <br>
            Policy must follow the MemoryPolicy interface
            @sa MemoryPolicy
        */
        template <typename T, class Policy = MemoryPolicy<T>, bool ExactSize = false>
        class Array
        {
            // Type definition and enumeration
        public:
            /** Define the internal stored type used for template */
            typedef T   TypeT;
            struct Internal
            {
                T    *      array;
                size_t      currentSize;
                size_t      allocatedSize;
            };

        private:
            enum { elementSize = sizeof(T) };


        public:
            /** Default Constructor */
            inline Array() : array(0), currentSize(0), allocatedSize(0)  { }
            /** Copy constructor */
            inline Array(const Array & other) : array(0), currentSize(0), allocatedSize(0) { *this = other; }
            /** Move constructor. Use getMovable() to move the array */
            inline Array(const Internal & intern) : array(intern.array), currentSize(intern.currentSize), allocatedSize(intern.allocatedSize) {}
            /** Destructor */
            ~Array()        { Clear(); }


            // Interface
        public:
            /** Clear the array, and destruct any remaining objects */
            inline void Clear() { Clear(0); }

            /** Append an element to the end of the array */
            inline void Append(const T & ref) throw() { Add(ref, (Bool2Type<ExactSize> * )0); }
            /** Append an element only if not present.
                Usually you'll store objects in an array if they don't exist already in the array,
                and you'll likely want the position of the object if it exists to use it directly.
                @param ref The object to be appended if not found in the array.
                @return The position of the searched item if found, or getSize() - 1 if not and it was appended. */
            inline size_t appendIfNotPresent(const T & ref) { size_t pos = indexOf(ref); if (pos == getSize()) Append(ref); return pos; }
            /** Grow this array by (at least) the given number of elements.
                With non exact sized array (likely PlainOldData), this set up the allocation size to, at least, currentSize + count.
                @param elements    A pointer to the elements to append (they are copied, can be 0)
                @param count       How many elements to copy from the given array */
            inline void Grow(const size_t count, T * const elements) throw() { Add(elements, count, (Bool2Type<ExactSize>*)0); }
            /** Insert an element just before the given index
                @param index    Zero based index of the element once inserted
                @param ref      The element*/
            inline void insertBefore(size_t index, const T & ref) throw() { Insert(index, ref, (Bool2Type<ExactSize> * )0); }
            /** Remove an object from the array
                @param index Zero based index of the object to remove */
            inline void Remove(size_t index) throw() { Remove(index, (Bool2Type<ExactSize> * )0); }
            /** Forget an object from the array
                @param index Zero based index of the object to remove */
            inline void Forget(size_t index) throw() { Forget(index, (Bool2Type<ExactSize> * )0); }
            /** Swap operator
                @param index1   The index to the first object to swap
                @param index2   The index to the second object to swap
                @warning Nothing is done if any index is out of range */
            inline void Swap(const size_t index1, const size_t index2) { if (index1 < currentSize && index2 < currentSize) { T tmp = array[index1]; array[index1] = array[index2]; array[index2] = tmp; } }


            /** Classic copy operator */
            inline const Array& operator = (const Array & other)
            {
                if (&other == this) return *this;
                Policy::CopyArray(array, other.array, allocatedSize, other.currentSize);
                currentSize = other.currentSize;
                return *this;
            }

            /** Access size member */
            inline size_t getSize() const { return currentSize; }
            /** Access operator
                @param index The position in the array
                @return the index-th element if index is in bound, or Policy::DefaultElement() else
            */
            inline T & operator [] (size_t index) { return index < currentSize ? array[index] : Policy::DefaultElement(); }
            /** Access operator
                @param index The position in the array
                @return the index-th element if index is in bound, or Policy::DefaultElement() else
            */
            inline const T & operator [] (size_t index) const { return index < currentSize ? array[index] : Policy::DefaultElement(); }

            /** Get element at position
                @param index The position in the array
                @return the index-th element if index is in bound, or Policy::DefaultElement() else
            */
            inline const T & getElementAtPosition (size_t index) const { return index < currentSize ? array[index] : Policy::DefaultElement(); }

            /** Search operator in O(N)
                @param objectToSearch   The object to look for in the array
                @param startPos         The position to start searching from
                @return the element index of the given element or getSize() if not found. */
            inline size_t indexOf(const T & objectToSearch, const size_t startPos = 0) const { return Policy::Search(array, currentSize, startPos, objectToSearch); }
            /** Check whether this contains contains the given object.
                Search is O(N)
                @param objectToSearch   The object to look for in the array
                @param startPos         The position to start searching from
                @return true if found. */
            inline bool Contains(const T & objectToSearch, const size_t startPos = 0) const { return Policy::Search(array, currentSize, startPos, objectToSearch) != currentSize; }
            /** Search operator in O(log(N)) mode
                @warning This method only works if the container is sorted.
                @param objectToSearch   The object to look for in the array
                @param startPos         The position to start searching from
                @return the element index of the given element or getSize() if not found. */
            inline size_t indexOfSorted(const T & objectToSearch, const size_t startPos = 0) const { return Policy::SearchSorted(array, currentSize, startPos, objectToSearch); }

            /** Reverse search operator
                @param objectToSearch   The object to look for in the array
                @param startPos         The position to start searching from (usually end of array)
                @return the element index of the given element or getSize() if not found.
            */
            inline size_t lastIndexOf(const T & objectToSearch, const size_t startPos = MaxPositiveSize) const { return Policy::ReverseSearch(array, min(currentSize, startPos), objectToSearch); }
            /** Fast access operator, but doesn't check the index given in */
            inline T & getElementAtUncheckedPosition(size_t index) { return array[index]; }
            /** Compare operator */
            inline bool operator == (const Array & other) const { return other.currentSize == currentSize && Policy::CompareArray(array, other.array, currentSize); }


            /** Move strategy is explicit with this intermediate object */
            Internal getMovable() { Internal intern = { array, currentSize, allocatedSize }; Reset(); return intern; }
            /** Copy strategy is explicit with this intermediate object. Beware, don't use this as a Array constructor */
            Internal getCopyable() const { Internal intern = { array, currentSize, allocatedSize }; return intern; }
            /** When using move strategy, this is the default value to initialize the object */
            static Internal emptyInternal() { Internal intern = { 0, 0, 0 }; return intern; }

        private:
            /** Add an object to the array when the exact size must be respected */
            inline void Add(const T & ref, Bool2Type<true> *) throw()
            {
                array = Policy::NonDestructiveAlloc(array, currentSize, currentSize+1, ref);
                if (array == 0) { Reset(); return; }
                allocatedSize = ++currentSize;
            }
            /** Add an object to the array when the array can be larger than the current size */
            inline void Add(const T & ref, Bool2Type<false> *) throw()
            {
                // Check if the array needs reallocation
                if (currentSize >= allocatedSize)
                {
                    // Need to realloc the data
                    if (allocatedSize == 0) allocatedSize = 2; else allocatedSize += (allocatedSize>>1);
                    array = Policy::NonDestructiveAlloc(array, currentSize, allocatedSize, Policy::DefaultElement());
                    if (array == 0) { Reset(); return; }
                }
                // Copy the data
                Policy::Copy(array[currentSize], ref);
                currentSize ++;
            }
            /** Add an object to the array when the exact size must be respected */
            inline void Add(T * const elements, const size_t count, Bool2Type<true> *) throw()
            {
                array = Policy::NonDestructiveAlloc(array, currentSize, currentSize+count, elements);
                if (array == 0) { Reset(); return; }
                currentSize += count;
                allocatedSize = currentSize;
            }
            /** Add an object to the array when the array can be larger than the current size */
            inline void Add(T * const elements, const size_t count, Bool2Type<false> *) throw()
            {
                // Check if the array needs reallocation
                if (currentSize + count >= allocatedSize)
                {
                    // Need to realloc the data
                    allocatedSize = currentSize + count;
                    array = Policy::NonDestructiveAlloc(array, currentSize, currentSize + count, elements);
                    if (array == 0) { Reset(); return; }
                    currentSize += count;
                    allocatedSize = currentSize;
                    return;
                }
                // Copy the data
                if (elements) Policy::CopyArray(&array[currentSize], elements, (allocatedSize - currentSize), count);
                currentSize += count;
            }

            /** Insert an object into the array when the exact size must be respected */
            inline void Insert(size_t index, const T & ref, Bool2Type<true> * t) throw()
            {
                if (index >= currentSize) { Add(ref, t); return; }
                array = Policy::Insert(array, currentSize, currentSize+1, index, ref);
                if (array == 0) { Reset(); return; }
                allocatedSize = ++currentSize;
            }
            /** Insert an object to the array when the array can be larger than the current size */
            inline void Insert(size_t index, const T & ref, Bool2Type<false> * t) throw()
            {
                if (index >= currentSize) { Add(ref, t); return; }
                // Check if the array needs reallocation
                if (currentSize >= allocatedSize)
                {
                    // Need to realloc the data
                    if (allocatedSize == 0) allocatedSize = 2; else allocatedSize += (allocatedSize>>1);
                    array = Policy::Insert(array, currentSize, allocatedSize, index, ref);
                    if (array == 0) { Reset(); return; }
                } else
                {   // Need to move the data
                    size_t i = currentSize;
                    for (; i > index; i--)
                        Policy::Copy(array[i], array[i - 1]);
                    Policy::Copy(array[i], ref);
                }
                currentSize ++;
            }

            /** Remove an object from the array when the array should shrink to the new size */
            inline void Remove(size_t index, Bool2Type<true> *) throw()
            {
                if (index == 0 && currentSize == 1)
                    Clear();
                else
                {
                    if (index < currentSize)
                    {
                        T * newArray = Policy::NonDestructiveAlloc(0, 0, currentSize - 1, Policy::DefaultElement());
                        if (newArray == 0) return;
                        // Copy the data before the index
                        Policy::CopyArray(newArray, array, currentSize - 1, index);
                        // Copy the data after the index
                        Policy::CopyArray(&newArray[index], &array[index+1], currentSize - 1, currentSize - index - 1);
                        // Then switch the pointers
                        allocatedSize = --currentSize;
                        // Delete the previous array
                        Policy::DeleteArray(array, allocatedSize + 1);

                        array = newArray;
                    }
                }

            }

            /** Remove an object from the array when the array can stay the size it is currently */
            inline void Remove(size_t index, Bool2Type<false> *) throw()
            {
                if (index < currentSize)
                {
                    // Copy the data after the index
                    Policy::CopyArray(&array[index], &array[index+1], currentSize - 1, currentSize - index - 1);
                    // Then switch the pointers
                    --currentSize;
                }
            }
            /** Forget an object from the array when the array should shrink to the new size */
            inline void Forget(size_t index, Bool2Type<true> *) throw()
            {
                if (index < currentSize)
                {
                    Swap(index, currentSize - 1);
                    array[currentSize - 1] = Policy::DefaultElement();
                    Remove(currentSize - 1, (Bool2Type<true>*)0);
                }
            }

            /** Forget an object from the array when the array can stay the size it is currently */
            inline void Forget(size_t index, Bool2Type<false> *) throw()
            {
                if (index < currentSize)
                {
                    // Copy the data after the index
                    Policy::CopyArray(&array[index], &array[index+1], currentSize - 1, currentSize - index - 1);
                    // Then switch the pointers
                    --currentSize;
                    // Zero the last element
                    array[currentSize] = Policy::DefaultElement();
                }
            }


            /** Reset the vector to its initial state (doesn't perform destruction of data) */
            inline void Reset() { currentSize = 0; allocatedSize = 0; array = 0; }
            /** Clear the array
                @param ref is checked for every item. If one of them matches, then it is not deleted */
            inline void Clear(T *) { Policy::DeleteArray(array, allocatedSize);  Reset(); }
        private:
            /** The array holder */
            T *                                                     array;
            /** The current size (in sizeof(T) unit) */
            size_t                                                  currentSize;
            /** The allocated size (always more than currentSize, max currentSize * 1.5) */
            size_t                                                  allocatedSize;
        };

        /** @cond */
        /** The memory policy interface for index list */
        template <typename T, bool WithDeref = true>
        struct MemoryListPolicy
        {
            /** This is used to select the [] return type */
            enum { CanDereference = WithDeref };
            /** This method copy (can also move depending on policy) the given data */
            static void CopyPtr(T* & dest, T* const & src);
            /** This method copy the srcSize data from src to dest, but doesn't resizing dest if it's too small (Copy is then limited to destSize elements). */
            static void CopyArray(T** dest,  T** const src, const size_t & destSize, const size_t srcSize);
            /** This method copy the srcSize data from src to dest, resizing dest if required.
                @warning dest must not be 0
            */
            static void CopyArray(T**& dest,  T** const src, size_t & destSize, const size_t srcSize);
            /** This method duplicate array and clone every member in it resizing dest if required.
                @warning dest must not be 0
            */
            static void DuplicateArray(T**& dest, T** const src, size_t & destSize, const size_t srcSize);
            /** This method search for the first element that match the given element and returns arraySize if not found
                The type must have a defined "== operator"
            */
            static size_t Search(T** const array, const size_t arraySize, const size_t from, T* const & valueToLookFor);
            /** This method search for the last element that match the given element and returns arraySize if not found */
            static size_t ReverseSearch(T** const array, const size_t arraySize, T* const & valueToLookFor);
            /** This method search for the given element and returns arraySize if not found */
            static size_t SearchRef(const T** array, const size_t arraySize, const size_t from, const T & valueToLookFor);
            /** This method returns a default constructed element or can throw */
            static T& DefaultSubElement();
            /** This method returns a default constructed element */
            static T* DefaultElement();
            static T** NonDestructiveAlloc(T** const array, const size_t currentSize, const size_t newSize, T* const & fillWith);
            static T** NonDestructiveAlloc(T** const array, const size_t currentSize, const size_t newSize, T** const fillWith);
            static T* * Insert(T** const array, const size_t currentSize, const size_t newSize, const size_t index, T* const & elementToInsert);
            /** Delete the array */
            static void DeleteArray(T* * const array, const size_t currentSize);
            /** Remove on item from the array */
            static void Remove(T * const data);
            /** Compare array */
            static bool CompareArray(T** const array1, T** const array2, const size_t commonSize);
        };
        /** @endcond */

        /** The IndexList holds pointers to object passed in.<br>
            <br>
            Pros : Almost as fast as array's access time once constructed. Element order is always preserved (except after explicit Swap or Sort calls). <br>
            Cons : Construction / Adding and Remove operations are slow (but faster than array) because of some copying is done for each pointer in the list <br>
            <br>
            This class only deals with an array of pointers to the given objects. It never move them, but own them (on destruction, it delete them, so they must be allocated with new).<br>
            <br>
            It deals with array growth and decrease by itself, and it's optimized to limit the copying to the minimum required.<br>
            It realloc its internal array for growing, only when absolutely required, and never reallocate the owned object<br>
            <br>
            When copying, it duplicates the pointer array by calling the copy constructor of the objects<br>
            <br>
            It's best if used like
            @code
            // Create an index list
            IndexList<MyObj> myList;
            myList.Append(new MyObj(something));
            // This will delete the inserted object by using the selected policy (delete in this example)
            myList.Remove(0);
            myList.insertBefore(2, new MyObj(foo)); // Index can be out of range
            myList.insertBefore(0, new MyObj(bar));
            // Get a reference on the list element
            const MyObj & ref = myList[1];
            // Search for an element based on its address
            assert(1 == myList.indexOf(&ref));
            // Search for an element based on its value (much slower)
            assert(1 == myList.indexOfMatching(ref));
            @endcode


            @warning You can't store array of objects, as the destructor doesn't call delete[].
            @warning If you want to use it with copy-constructible objects, you'll have to provide a default constructable element returned when using the [] operator

            Policy must follow the MemoryPolicy interface
            @sa MemoryPolicy
        */
        template <typename TElem, class Policy = MemoryListPolicy<TElem, true>, bool ExactSize = false>
        class IndexList
        {
            // Type definition and enumeration
        public:
            /** Define the internal stored type used for template */
            typedef TElem   TypeT;
            struct Internal
            {
                TElem   **  array;
                size_t      currentSize;
                size_t      allocatedSize;
            };

            // Type definition and enumeration
        private:
            /** The element size (required for position computation) */
            enum { elementSize = sizeof(TElem) };
            /** The pointer type used */
            typedef TElem * TPtr;

        public:
            /** Default Constructor */
            inline IndexList() : array(0), currentSize(0), allocatedSize(0)  { }
            /** Copy constructor */
            inline IndexList(const IndexList & other) : array(0), currentSize(0), allocatedSize(0) { *this = other; }
            /** Move constructor. Use getMovable() to move the array */
            inline IndexList(const Internal & intern) : array(intern.array), currentSize(intern.currentSize), allocatedSize(intern.allocatedSize) {}
            /** Destructor */
            ~IndexList()        { Clear(); }


        // Interface
        public:
            /** Clear the array, and destruct any remaining objects */
            inline void Clear() { Clear(0); }

            /** Append an element to the end of the array */
            inline void Append(const TPtr & ref) throw() { Add(ref, (Bool2Type<ExactSize> * )0); }
            /** Append an element only if not present.
                The object itself is compared, not the pointer.
                Usually you'll store objects in an array if they don't exist already in the array,
                and you'll likely want the position of the object if it exists to use it directly.
                @param ref              The pointer to a new allocated object to be appended if not found in the array.
                @param deleteIfPresent  If true, and the object is already found in the array, the given pointed object is deleted.
                @return The position of the searched item if found (you might need to delete the element in that case if you don't want it done here), or getSize() - 1 if not and it was appended. */
            inline size_t appendIfNotPresent(const TPtr & ref, const bool deleteIfPresent = true) { size_t pos = indexOf(ref); if (pos == getSize()) { Append(ref); return pos; } if (deleteIfPresent) delete ref; return pos; }
            /** Grow this array by (at least) the given number of elements
                This set up the allocation size to, at least, currentSize + count.
                The elements are owned by the array.
                @param elements    A pointer to the elements to append (they are copied, can be 0)
                @param count       How many elements to copy from the given array */
            inline void Grow(const size_t count, TPtr * const elements) throw() { Add(elements, count, (Bool2Type<ExactSize>*)0); }
            /** Insert an element just before the given index
                @param index    Zero based index of the element once inserted
                @param ref      The element to insert
                @warning If index is out of range, the element is appended instead. */
            inline void insertBefore(size_t index, const TPtr & ref) throw() { Insert(index, ref, (Bool2Type<ExactSize> * )0); }
            /** Remove an object from the array
                @param index Index of the object to remove */
            inline void Remove(size_t index) throw() { Remove(index, (Bool2Type<ExactSize> * )0); }
            /** Forget an object from the array
                @param index Zero based index of the object to remove */
            inline void Forget(size_t index) throw() { Forget(index, (Bool2Type<ExactSize> * )0); }
            /** Swap operator
                @param index1   The index to the first object to swap
                @param index2   The index to the second object to swap
                @warning Nothing is done if any index is out of range */
            inline void Swap(const size_t index1, const size_t index2) { if (index1 < currentSize && index2 < currentSize) { TPtr tmp = array[index1]; array[index1] = array[index2]; array[index2] = tmp; } }


            /** Classic copy operator */
            inline const IndexList& operator = ( const IndexList & other)
            {
                if (&other == this) return *this;
                Policy::DuplicateArray(array, other.array, allocatedSize, other.currentSize);
                currentSize = other.currentSize;
                return *this;
            }

            /** Access size member */
            inline size_t getSize() const { return currentSize; }
            /** Access operator
                @param index The position in the array
                @return the index-th element if index is in bound, or Policy::DefaultElement() else */
            inline TElem & operator [] (const size_t index) { return getElementRefAtPosition(index, (Bool2Type<Policy::CanDereference> *)0); }
            /** Access operator
                @param index The position in the array
                @return the index-th element if index is in bound, or Policy::DefaultElement() else */
            inline const TElem & operator [] (const size_t index) const { return getElementAtPosition(index, (Bool2Type<Policy::CanDereference> *)0); }
            /** Get element at position
                @param index The position in the array
                @return the index-th element if index is in bound, or Policy::DefaultElement() else */
            inline const TElem & getElementAtPosition (size_t index) const { return getElementAtPosition(index, (Bool2Type<Policy::CanDereference> *)0); }
            /** Search operator
                @param objectToSearch   The object to look for in the array based on the given address
                @param startPos         The position to start searching from
                @return the element index of the given element or getSize() if not found. */
            inline size_t indexOf(const TPtr & objectToSearch, const size_t startPos = 0) const { return Policy::Search(array, currentSize, startPos, objectToSearch); }
            /** Check whether this contains contains the given object.
                Search is O(N)
                @param objectToSearch   The object to look for in the array
                @param startPos         The position to start searching from
                @return true if found. */
            inline bool Contains(const TPtr & objectToSearch, const size_t startPos = 0) const { return Policy::Search(array, currentSize, startPos, objectToSearch) != currentSize; }
            /** Reverse search operator
                @param objectToSearch   The object to look for in the array
                @param startPos         The position to start searching from
                @return the element index of the given element or getSize() if not found. */
            inline size_t lastIndexOf(const TPtr & objectToSearch, const size_t startPos = MaxPositiveSize) const { return Policy::ReverseSearch(array, min(currentSize, startPos), objectToSearch); }
            /** Search operator
                @param objectToSearch The object to look for in the array
                @return the element index of the given element or getSize() if not found. */
            inline size_t indexOfMatching(const TElem & objectToSearch, const size_t startPos = 0) const { return Policy::SearchRef(array, currentSize, startPos, objectToSearch); }
            /** Fast access operator, but doesn't check the index given in */
            inline TPtr & getElementAtUncheckedPosition(size_t index) { return array[index]; }
            /** Fast access operator, but doesn't check the index given in */
            inline const TPtr & getElementAtUncheckedPosition(size_t index) const { return array[index]; }
            /** Compare operator */
            inline bool operator == (const IndexList & other) const { return other.currentSize == currentSize && Policy::CompareArray(array, other.array, currentSize); }

            /** Move strategy is explicit with this intermediate object */
            Internal getMovable() { Internal intern = { array, currentSize, allocatedSize }; Reset(); return intern; }
            /** When using move strategy, this is the default value to initialize the object */
            static Internal emptyInternal() { Internal intern = { 0, 0, 0 }; return intern; }

        private:
            /** Add an object to the array when the exact size must be respected */
            inline void Add(const TPtr & ref, Bool2Type<true> *) throw()
            {
                array = Policy::NonDestructiveAlloc(array, currentSize, currentSize+1, ref);
                if (array == 0) { Reset(); return; }
                allocatedSize = ++currentSize;
            }
            /** Add an object to the array when the array can be larger than the current size */
            inline void Add(const TPtr & ref, Bool2Type<false> *) throw()
            {
                // Check if the array needs reallocation
                if (currentSize >= allocatedSize)
                {
                    // Need to realloc the data
                    if (allocatedSize == 0) allocatedSize = 2; else allocatedSize += (allocatedSize >> 1); // Growth factor of 1.5 allow reuse of memory deallocated
                    array = Policy::NonDestructiveAlloc(array, currentSize, allocatedSize, Policy::DefaultElement());
                    if (array == 0) { Reset(); return; }
                }
                // Copy the data
                Policy::CopyPtr(array[currentSize], ref);
                currentSize ++;
            }
            /** Add an object to the array when the exact size must be respected */
            inline void Add(TPtr * const elements, const size_t count, Bool2Type<true> *) throw()
            {
                array = Policy::NonDestructiveAlloc(array, currentSize, currentSize+count, elements);
                if (array == 0) { Reset(); return; }
                currentSize += count;
                allocatedSize = currentSize;
            }
            /** Add an object to the array when the array can be larger than the current size */
            inline void Add(TPtr * const elements, const size_t count, Bool2Type<false> *) throw()
            {
                // Check if the array needs reallocation
                if (currentSize + count >= allocatedSize)
                {
                    // Need to realloc the data
                    allocatedSize = currentSize + count;
                    array = Policy::NonDestructiveAlloc(array, currentSize, currentSize + count, elements);
                    if (array == 0) { Reset(); return; }
                    currentSize += count;
                    allocatedSize = currentSize;
                    return;
                }
                // Copy the data
                if (elements) Policy::CopyArray(&array[currentSize], elements, (allocatedSize - currentSize), count);
                currentSize += count;
            }

            /** Insert an object into the array when the exact size must be respected */
            inline void Insert(size_t index, const TPtr & ref, Bool2Type<true> * t) throw()
            {
                if (index >= currentSize) { Add(ref, t); return; }
                array = Policy::Insert(array, currentSize, currentSize+1, index, ref);
                if (array == 0) { Reset(); return; }
                allocatedSize = ++currentSize;
            }
            /** Insert an object to the array when the array can be larger than the current size */
            inline void Insert(size_t index, const TPtr & ref, Bool2Type<false> * t) throw()
            {
                if (index >= currentSize) { Add(ref, t); return; }
                // Check if the array needs reallocation
                if (currentSize >= allocatedSize)
                {
                    // Need to realloc the data
                    if (allocatedSize == 0) allocatedSize = 2; else allocatedSize += (allocatedSize >> 1); // Growth factor of 1.5 allow reuse of memory deallocated
                    array = Policy::Insert(array, currentSize, allocatedSize, index, ref);
                    if (array == 0) { Reset(); return; }
                } else
                {   // Need to move the data
                    size_t i = currentSize;
                    for (; i > index; i--)
                        Policy::CopyPtr(array[i], array[i - 1]);
                    Policy::CopyPtr(array[i], ref);
                }
                currentSize ++;
            }

            /** Remove an object from the array when the array should shrink to the new size */
            inline void Remove(size_t index, Bool2Type<true> *) throw()
            {
                if (index < currentSize)
                {
                    TPtr * newArray = Policy::NonDestructiveAlloc(0, 0, currentSize - 1, Policy::DefaultElement());
                    if (newArray == 0) return;
                    // Copy the data before the index
                    Policy::CopyArray(newArray, array, currentSize - 1, index);
                    // Copy the data after the index
                    Policy::CopyArray(&newArray[index], &array[index+1], currentSize - 1, currentSize - index - 1);
                    // Then switch the pointers
                    allocatedSize = --currentSize;
                    // Remove the data at the given index
                    Policy::Remove(array[index]);
                    // Delete the previous array
                    Policy::DeleteArray(array, allocatedSize + 1);

                    array = newArray;
                }
            }

            /** Remove an object from the array when the array can stay the size it is currently */
            inline void Remove(size_t index, Bool2Type<false> *) throw()
            {
                if (index < currentSize)
                {
                    // Remove the given data
                    Policy::Remove(array[index]);
                    Forget(index, (Bool2Type<false> *)0);
                }
            }

            /** Forget an object from the array when the array should shrink to the new size */
            inline void Forget(size_t index, Bool2Type<true> *) throw()
            {
                if (index < currentSize)
                {
                    Swap(index, currentSize - 1);
                    array[currentSize - 1] = Policy::DefaultElement();
                    Remove(currentSize - 1, (Bool2Type<true>*)0);
                }
            }

            /** Forget an object from the array when the array can stay the size it is currently */
            inline void Forget(size_t index, Bool2Type<false> *) throw()
            {
                if (index < currentSize)
                {
                    // Copy the data after the index
                    Policy::CopyArray(&array[index], &array[index+1], currentSize - 1, currentSize - index - 1);
                    // Then switch the pointers
                    --currentSize;
                    // Zero the last element
                    array[currentSize] = Policy::DefaultElement();
                }
            }

            /** Access operator
                @param index The position in the array
                @return the index-th element if index is in bound, or Policy::DefaultElement() else
            */
            inline TElem & getElementRefAtPosition (const size_t index, Bool2Type<true> *) { return index < currentSize ? *array[index] : Policy::DefaultSubElement(); }
            /** Get element at position
                @param index The position in the array
                @return the index-th element if index is in bound, or Policy::DefaultElement() else
            */
            inline const TElem & getElementAtPosition (size_t index, Bool2Type<true> *) const { return index < currentSize ? *array[index] : Policy::DefaultSubElement(); }
            /** Access operator
                @param index The position in the array
                @return the index-th element if index is in bound, or Policy::DefaultElement() else
                @warning If the application crash here, it's out-of-index exception
            */
            inline TElem & getElementRefAtPosition (const size_t index, Bool2Type<false> *) { return *array[index]; }
            /** Get element at position
                @param index The position in the array
                @return the index-th element if index is in bound, or Policy::DefaultElement() else
                @warning If the application crash here, it's out-of-index exception
            */
            inline const TElem & getElementAtPosition (size_t index, Bool2Type<false> *) const { return *array[index]; }


            /** Reset the vector to its initial state (doesn't perform destruction of data) */
            inline void Reset() { currentSize = 0; allocatedSize = 0; array = 0; }
            /** Clear the list
                @param ref is checked for every item. If one of them matches, then it is not deleted */
            inline void Clear(TPtr *) { Policy::DeleteArray(array, allocatedSize); Reset(); }
        private:
            /** The array holder */
            TElem **                                                array;
            /** The current size (in sizeof(TPtr) unit) */
            size_t                                                  currentSize;
            /** The allocated size (always more than currentSize, max currentSize * 1.5) */
            size_t                                                  allocatedSize;
        };



        /** The Search policy interface for ChainedList */
        template <typename T>
        struct SearchPolicyInterface
        {
            typedef T & Result;
            /** Returns the default value when not found */
            static Result DefaultValue();
            /** Convert the type to the required type for comparing */
            static Result From(T * x);
            /** Compare an item with a given item */
            static bool   Compare(const T & a, const T & b);

            /** The policy must provide this enum, set to true or false */
            enum { DefaultConstructibleAndCopyable };
        };

        /** @internal
            The declaration below are used to select which implementation to choose from */
        template <bool flag>
        struct SelectImpl
        {
            template <class T, class U>
            struct In
            {
                typedef T Result;
            };
        };

        /** @internal
            The declaration below are used to select which implementation to choose from */
        template <>
        struct SelectImpl<false>
        {
            template <class T, class U>
            struct In
            {
                typedef U Result;
            };
        };
        /** @internal
            The declaration below are used to select which implementation to choose from */
        template <bool flag, typename T, typename U>
        struct Select
        {
            typedef typename SelectImpl<flag>::template In<T, U>::Result Result;
        };


        /** ChainedList class usage<br>
            <br>
            ChainedList is a template double chained list that
            allow high efficiency access and advantages of chained list.<br>
            <br>
            In usual chained list, random access into the list is an O(N) operation.<br>
            In this list, random access into the list is an O(N/M) operation.
            <br>
            The list chains allocated blocks of M pointers.
            When accessing the i-th element, not all (i-1) elements
            are accessed, but only (j)/M elements are accessed with j
            being the minimum distance from both ends.<br>
            <br>
            (Example : To reach the 41st element in a usual list
            you have to cross the first 40 element. Here, if the
            array is 16 pointers wide, you will cross 2 array elements
            and 8 pointer elements, that is 10 operations, not 40)<br>
            <br>
            While appending data is almost a free operation [O(1)],
            inserting/removing data in such list impose accessing O(N)
            elements in the worst case in order to preserve list integrity.
            <br>
            If insertion and removing are to be frequent compared to
            indexed access, then consider using Insert and Remove method
            instead of Add and Sub.
            <br>
            Insert and Remove both loose list integrity, so next indexed
            access will be O(N) again, but then the list will act like any
            standard chained list.
            <br>
            <br>
            So, if you don't need to use these improvements, simply use the
            list in a usual way, via inserting and removing functions,
            it will act as an usual chained list.
            <br>
            Another improvement with this list are the adding functions.
            You could insert pointer to an element in the list, and they
            will be deleted when list is deleted (usual owning chained list),
            OR, you can insert a element in the list, and a copy of it
            will be insert in the list and deleted. No need to know if
            you should delete the object.<br>
            <br>
            This list also provide very fast parsing system.<br>
            If you want extremely fast access to the list call the parseList
            method, that return the next pointer.<br>
            <br>
            @code
            // Create a int based chained list
            ChainedList<int>     myList0;

            // Create a int chained list, with 8 = 2^3 pointer wide blocks
            ChainedList<int, 3>  myList1;

            // Add an element to list
            myList0.Add(3);
            // Or simply myList0.Add(new int(3));
            myList1 += 45;

            // Access the i-th element
            cout << myList0[i];

            // Remove the i-th list element
            myList0.Sub(i);
            myList0 -= 3;

            // To insert an element before the i-th position
            myList0.Insert(7896, i);

            // To swap 2 elements, at i and j position
            myList0.Swap(i,j);

            // Change an element value, in the i-th position
            myList0.Change(456,i);

            // To parse the list, first time, beginning with i-th
            // position
            int * a = myList0.Parse(i);
            // To parse the list, next time
            int * b = myList0.Parse();

            // To find the position of an inserted element
            int pos = myList0.Find(456);
            @endcode
        */
        template <class U, unsigned int pow2 = 4, class SearchPolicy = SearchPolicyInterface<U> >
        class ChainedList
        {
        public:
            /** Define the constant that the list can return */
            enum
            {
                ChainedEnd      = -1,           //!< This is used to add and or insert at end of list
                ChainedError    = ChainedEnd,   //!< Whenever an indexed error occurs, this is the returned value
                ChainedStart    = 0,            //!< This is used to insert data in the head of the list
                ChainedBS       = 1<<pow2,      //!< Process the asked pow once
            };

        private:
            /** The node base class */
            class NodeNotCopyable
            {
            public:
                /** The pointer on the previous object */
                NodeNotCopyable   * mpxPrevious;
                /** The pointer on the next object */
                NodeNotCopyable   * mpxNext;
                /** The pointer on the data */
                U *             mpuElement;


                /** Default constructors */
                NodeNotCopyable(U* t = 0) : mpxPrevious(0), mpxNext(0), mpuElement(t) {}
                /** The destructor always delete the object */
                ~NodeNotCopyable()
                {
                    delete mpuElement; mpuElement = 0;
                }

                /** Change the pointed object with pointer assignment */
                void    Set(U* t) { mpuElement = t; }
                /** Get a reference on the pointed object */
                U&      Get() { return (*mpuElement); }
            };

            /** The node class for copyable objects */
            class NodeCopyable
            {
            public:
                /** The pointer on the previous object */
                NodeCopyable   *    mpxPrevious;
                /** The pointer on the next object */
                NodeCopyable   *    mpxNext;
                /** The pointer on the data */
                U *             mpuElement;
            public:
                /** Default constructors */
                NodeCopyable(U* t = 0) : mpxPrevious(0), mpxNext(0), mpuElement(t) {}
                /** The destructor always delete the object */
                ~NodeCopyable()
                {
                    delete mpuElement; mpuElement = 0;
                }

                /** Change the pointed object with pointer assignment */
                void    Set(U* t) { mpuElement = t; }
                /** Get a reference on the pointed object */
                U&      Get() { return (*mpuElement); }

                // Add the copy interface
            public:
                /** Constructor using element's copy constructor */
                NodeCopyable(const U& t) : mpxPrevious(0), mpxNext(0) { mpuElement = new U(t); }
                /** Change the pointed object with copy constructor */
                void    Set(const U& t) { mpuElement = new U(t); }

            };

            typedef typename Select<SearchPolicy::DefaultConstructibleAndCopyable, NodeCopyable, NodeNotCopyable>::Result Node;

            /** Select the parameters type depending on constructible policy */
            typedef typename Select<SearchPolicy::DefaultConstructibleAndCopyable, U&, U*>::Result                  DefaultParamType;
            typedef typename Select<SearchPolicy::DefaultConstructibleAndCopyable, const U&, const U*>::Result      DefaultConstParamType;

        private:
            /** The memory array holding pointers */
            template <unsigned int size>
            class LinearBlock
            {
                // Members
            private:
                /** Pointer on previous memory block */
                LinearBlock *   spxNext;
                /** Pointer on next memory block */
                LinearBlock *   spxPrevious;
                /** The memory block (array of node pointers) with template's size : size */
                Node            sapxBlock[size];
                /** Used pointers count in array */
                unsigned char   soUsed;

                // Construction
            public:
                /** Default constructor */
                LinearBlock(LinearBlock * previous = 0, LinearBlock * next = 0) : spxNext(next), spxPrevious(previous) , soUsed(0)    {}
                /** Default destruction too : destruct object if needed */
                ~LinearBlock()
                {
                    if ( spxNext!=0 || spxPrevious!=0)
                    {
                        if (spxNext!=0)
                            spxNext->spxPrevious = spxPrevious;
                        if (spxPrevious!=0)
                            spxPrevious->spxNext = spxNext;

                        spxNext = spxPrevious = 0;
                    }
                }

                // Interface
            public:
                /** Connect to other nodes */
                inline void Connect(LinearBlock * previous, LinearBlock * next)     { spxPrevious = previous; spxNext = next; }
                /** Delete this object safely, reconnect other node if needed */
                inline void Delete()
                {
                    if (spxNext!=0)
                        spxNext->spxPrevious = spxPrevious;
                    if (spxPrevious!=0)
                        spxPrevious->spxNext = spxNext;

                    spxNext = spxPrevious = 0;
                    delete this;
                }

                /** Return first element in the list (O(n) speed) */
                inline LinearBlock<size> * GoFirst()    {  LinearBlock<size>* pNode = this; while (pNode->spxPrevious != 0) pNode = spxPrevious; return pNode; }
                /** Return last element in the list (O(n) speed) */
                inline LinearBlock<size> * GoLast()     {  LinearBlock<size>* pNode = this; while (pNode->spxNext != 0) pNode = spxNext; return pNode; }


                /** Append a new block to the end */
                inline bool CreateNewBlock()
                {
                    if (spxNext!=0) return false;
                    LinearBlock<size>* pNode = new LinearBlock<size>(this, 0);
                    if (pNode == 0) return false;
                    else               spxNext = pNode;
                    return true;
                }

                /** Get a pointer on data */
                inline Node * GetData()     {   return &sapxBlock[0]; }
                /** Find a node */
                LinearBlock<size> * Find(Node * pNode)
                {
                    if (pNode==0) return 0;
                    LinearBlock<size> * pBlock = GoFirst();
                    LinearBlock<size> * pEnd = GoLast();
                    for (; pBlock != pEnd; pBlock = pBlock->spxNext)
                        if ( pNode >= pBlock->GetData() && pNode <= pBlock->GetData() + size) return pBlock;
                    return 0;
                }

            private:
                /** Delete all list
                    Note : this list should never be deleted explicitely
                    Only when ChainedList<U>::Delete is called
                    This list can only grow (for memory saving purpose, there is a protection, when two nodes are
                    free of data pointers, last one is deleted
                */
                bool DeleteAll()
                {
                    LinearBlock<size>* pNode2, *pNode = GoFirst();
                    if (pNode == 0) return false;
                    if (pNode->spxNext == 0)
                    {
                        pNode->Delete();
                        pNode = 0;
                        return true;
                    }

                    // Goto second pointer until next pointer is different from 0 , advance to next
                    for (; pNode != 0; )
                    {
                        pNode2 = pNode->spxNext;
                        pNode->Delete();
                        pNode = pNode2;
                    }
                    return true;
                }
            public:
                friend class ChainedList<U, pow2, SearchPolicy>;
            };

            // Members
        private:
            /** Pointer on the first node */
            Node *          mpxFirst;
            /** Pointer on the last node */
            Node *          mpxLast;
            /** Pointer on the current node */
            mutable Node *  mpxCurrent;
            /** The node count */
            unsigned int    mlNumberOfNodes;

            /** Number of allocated blocks */
            unsigned int                        mlNumberOfBlocks;
            /** Block size = 2^(template)pow2 */
//          unsigned int                        mlBlockSize;
            /** Pointer on first memory block */
            LinearBlock<ChainedBS> *    mpxBlock;
            /** Pointer on last block memory block */
            LinearBlock<ChainedBS> *    mpxBLast;

        private:
            /** Should we use blocks ? (same thing as (template)pow2 = 0)
                If not, the list act like a standard linked list
            */
            bool                        mbUseBlocks;
            /** Is integrity preserved ? (meaning pNode++ == pNode->mpxNext) */
            bool                        mbIntegrity;

            // Properties
        public:
            /** Get the current used node number */
            inline unsigned int getSize() const { return mlNumberOfNodes; }

            // Interface
        public:
            /** Add a node to list
                @warning Take care that add function preserve list integrity
                whereas Insert function doesn't in most of case.
                Even if Add function can insert node in list just before the
                given position, it is slower than a Insert function.
                So, in realtime application, prefer adding node in a
                preprocessing step, or using Insert function...
                (Inserting with Add is O(Number of node - position))
                @return true if insertion was successful */
            bool Add(DefaultConstParamType, const unsigned int& = ChainedEnd);
            /** Add a node to list via pointers
                @sa virtual bool Add(const U&, const unsigned int &)
                @return true if insertion was successful */
            bool Add(U*, const unsigned int& = ChainedEnd);

            /** Take care that Insert doesn't preserve list integrity
                @sa Add function summary for details
                @return true if insertion was successful */
            bool Insert(DefaultConstParamType, const unsigned int& = ChainedStart);
            /** Subtract a node from list
                @warning Please note, that Sub conserve list integrity,
                whereas Remove function does not. However, Sub is slower
                than Remove, because of list reconstruction...
                So, in realtime, prefer using remove (that is faster)
                @return true if removing was successful */
            bool Sub(const unsigned int& = ChainedEnd);
            /** Remove a node from the list, but in most of case, it doesn't
                preserve list integrity
                @sa Sub method
                @return true if insertion was successful */
            bool Remove(const unsigned int& = ChainedStart);

            /** Find a node in the list
                If provided an operator == for U, then this method
                perform a O(N) search in the list
                @return The node index if found or ChainedError if not */
            unsigned int indexOf(DefaultConstParamType);
            /** Find the last matching node in the list
                If provided an operator == for U, then this method
                perform a O(N) search in the list
                @return The node index if found or ChainedError if not */
            unsigned int lastIndexOf(DefaultConstParamType);
            /** Swap two nodes, preserve list integrity for the given indexes
                @return true on successful swapping */
            bool Swap(const unsigned int &, const unsigned int &);

            // Helpers functions
        private:
            /** Create a node pointer in block array */
            inline Node * CreateNode();
            /** Goto the indexed node number */
            inline Node * Goto(const unsigned int &);
            /** Should we use blocks ? */
            inline bool UseBlocks(const bool &);
            /** Connect Nodes */
            bool Connect(Node *, Node *, Node *);

        public:
            /** Insert a list to this list
                @sa virtual bool Add(const U&, const unsigned int &)
                @return true on successful insertion    */
            bool Add(const ChainedList&, const unsigned int & = ChainedEnd);

            /** Delete the list
                @warning This function do delete nodes... (and data)
                @return true on successful deletion     */
            bool Free();
            /** Allow mutating a node data at the given index
                @return true on successful mutation     */
            bool Change(DefaultConstParamType, const unsigned int &);


            /** Iterate the list
                When starting set bInit = true, the method returns the first node, then return each time the next node
                @warning No other list function should be used between each parse list call (or use saveParsingStack / restoreParsingStack)
                @return the first node's data pointer when bInit is true, or the next parsed one else */
            U * parseList(const bool & bInit = false) const;
            /** Save the current parsing state (to restore it later on).
                You must call this method when you don't know what is going on until the parsing's end.
                @return a pointer on the list stack to restore later on with restoreParsingStack */
            const void * saveParsingStack() const;
            /** Save the current parsing state (to restore it later on).
                You must call this method when you don't know what is going on until the parsing's end.
                @param stack A pointer on the stack as returned by saveParsingStack
                @return a pointer on the list stack to restore later on with restoreParsingStack */
            void restoreParsingStack(const void * stack) const;
            /** Iterate the list from a given position
                When started (uiPos != ChainedEnd) return the requested node, else return each time the next node
                @sa ParseList */
            U * parseListStart(const unsigned int & uiPos = ChainedEnd) const;

            /** Reverse iterate the list
                When started (ie when bInit = true) return the last node, then return each time the previous node
                @warning No other list function should be used between each parse list call
                @return the last node's data pointer when bInit is true, or the previous parsed one else */
            U * reverseParseList(const bool & bInit = false) const;
            /** Reverse parse list from a given position
                When started (uiPos != ChainedEnd) return the requested node, else return each time the previous node
                @sa ReverseParseList */
            U * reverseParseListStart(const unsigned int & uiPos = ChainedEnd) const;


            // Operators
        public:
            /** Operator [] return U type at i-th position.
                @param i the indexed position to retrieve
                @return The returned type depends on the selected policy
            */
            inline typename SearchPolicy::Result operator [] (const unsigned int &i)
            {   Node * pNode = Goto(i);
                if (pNode == 0)    return SearchPolicy::DefaultValue();
                return SearchPolicy::From(pNode->mpuElement);
            }

            /** Append the given element to the list end */
            inline ChainedList& operator += (DefaultConstParamType a)       { this->Add(a); return (*this); }
            /** Remove the given element if found */
            inline ChainedList& operator -= (DefaultConstParamType a)       { unsigned int i = this->lastIndexOf(a); if (i!=ChainedEnd) this->Sub(i); return (*this); }

            /** The classic copy and modify operators */
            inline ChainedList operator + (DefaultConstParamType a) const   { return ChainedList(*this)+= a; }
            inline ChainedList operator - (DefaultConstParamType a) const   { return ChainedList(*this)-= a; }

            /** Equality operator */
            inline ChainedList& operator = (const ChainedList& );
            /** Append the given list to ours */
            inline ChainedList& operator += (const ChainedList& a)  { this->Add(a); return (*this); }

            /** It costs nothing... */
            inline ChainedList& operator + (const ChainedList& a) const { return ChainedList(*this)+= a; }

            /** Move the object from the given position to the given position.
                This method doesn't swap the two objects, but instead remove the first object and reinsert it later on.
                @param uiInitialPos     The position of the object to remove from the list
                @param uiFinalPos       The position to insert the object back
                @return true on success, false on memory error */
            bool moveObject(const unsigned int & uiInitialPos, const unsigned int & uiFinalPos = ChainedEnd);

            /** Move the list from a ChainedList into this one (the parameter's node are then 0ified)
                @warning the current nodes is this list are inserted to the beginning of this list
                @return true on success, false on error */
            inline bool moveAppendedList(ChainedList & a)
            {
                if (mlNumberOfNodes)
                {
                    if (!a.Add(*this, ChainedStart)) return false;
                }
                mpxBlock = a.mpxBlock;
                mpxBLast = a.mpxBLast;
                mpxFirst = a.mpxFirst;
                mpxLast = a.mpxLast;
                mpxCurrent = a.mpxCurrent;
                mlNumberOfNodes = a.mlNumberOfNodes;
                mlNumberOfBlocks = a.mlNumberOfBlocks;
                mbUseBlocks = a.mbUseBlocks;
                mbIntegrity = a.mbIntegrity;

                a.mpxBlock = a.mpxBLast = 0;
                a.mpxFirst = a.mpxLast = a.mpxCurrent = 0;
                a.mlNumberOfNodes = a.mlNumberOfBlocks = 0;
                a.mbIntegrity = true;
                return true;
            }

            // Construction
        public:
            /** Default constructor */
            ChainedList();
            /** Default destructor which destruct objects linked to list */
            ~ChainedList();

            /** Copy constructor */
            ChainedList(const ChainedList &);

        };


        // Inclusion for template definition, and implementation (it's too big to be implemented here)
        #include "template/ChainedList.hpp"
    }







    //////////////////////////////////////////////////////////////////////  Policy implementation below
    //// Plain Old Data
    /////////////////////////////////////////////////////////////////////////////////////////////////////

    /** @cond */
    /** Use containers from this namespace when you are dealing with Plain Old Data (like int, char, basic struct type with no pointers) */
    namespace PlainOldDataPolicy
    {
        /** The default memory policy interface for plain old data */
        template <typename T>
        struct DefaultMemoryPolicy
        {
            /** This method copy (can also move depending on policy) the given data */
            inline static void Copy(T & dest, const T & src) { dest = src; }

            /** This method copy the srcSize data from src to dest, not resizing dest even if required */
            inline static void CopyArray(T * dest, const T * src, const size_t & destSize, const size_t srcSize)
            {
                memmove(dest, src, Container::min(srcSize, destSize) * sizeof(T));
            }
            /** This method copy the srcSize data from src to dest, resizing dest if required */
            inline static void CopyArray(T *& dest, const T * src, size_t & destSize, const size_t srcSize)
            {
                if (destSize < srcSize)
                {
                    // Need to resize the array
                    T* tmp = (T*)realloc(0, srcSize * sizeof(T));
                    if (tmp == 0) { DeleteArray(dest, destSize); dest = 0; destSize = 0; return;  }

                    memcpy(tmp, src, srcSize * sizeof(T));

                    // Erase the previous array
                    DeleteArray(dest, destSize);
                    dest = tmp;

                    destSize = srcSize;
                } else memcpy(dest, src, srcSize * sizeof(T));
            }
            /** This method returns a default constructed element or can throw */
            inline static T& DefaultElement() { static T t(0); return t; }
            /** This method search for the given element and returns arraySize if not found */
            inline static size_t Search(const T* array, const size_t arraySize, const size_t from, const T & valueToLookFor) { size_t i = from; while (i < arraySize && array[i] != valueToLookFor) i++; return i; }
            /** This method search for the given element and returns arraySize if not found */
            inline static size_t SearchSorted(const T* array, const size_t arraySize, const size_t from, const T & valueToLookFor)
            {
                size_t highPos = arraySize, lowPos = from, i = 0;
                while (1)
                {
                    if (lowPos >= highPos) return arraySize;
                    else if (array[lowPos] == valueToLookFor) return lowPos;
                    else
                    {
                        i = (highPos + lowPos) / 2;
                        if (i == lowPos) return arraySize;
                        if (array[i] <= valueToLookFor) lowPos = i; else highPos = i;
                    }
                }
            }

            /** This method search for the last given element and returns arraySize if not found */
            inline static size_t ReverseSearch(const T* array, const size_t arraySize, const T & valueToLookFor) { size_t i = arraySize; while (i && array[i-1] != valueToLookFor) i--; return i ? i - 1 : arraySize; }
            /** This method perform a non destructive allocation in place if possible or move the given array to a new place copying data in it */
            inline static T * NonDestructiveAlloc(const T* array, const size_t currentSize, const size_t newSize, const T & fillWith)
            {
                T * ret = (T*)realloc((void*)array, newSize * sizeof(T));
                if (ret == 0) { free((void*)array); return 0; }
                size_t i = currentSize;
                for (; i < newSize; i++)
                    ret[i] = fillWith;

                return ret;
            }
            /** This method perform a non destructive allocation in place if possible or move the given array to a new place copying data in it */
            inline static T * NonDestructiveAlloc(const T* array, const size_t currentSize, const size_t newSize, T * const fillWith)
            {
                T * ret = (T*)realloc((void*)array, newSize * sizeof(T));
                if (ret == 0) { free((void*)array); return 0; }
                size_t i = currentSize;
                if (fillWith) for (; i < newSize; i++) ret[i] = fillWith[i - currentSize];
                else          for (; i < newSize; i++) ret[i] = DefaultElement();

                return ret;
            }
            /** This method perform a non destructive allocation in place if possible or move the given array to a new place copying data in it,
                plus inserting the given object at the given place and DefaultElement for empty places if any */
            static T * Insert(const T* array, const size_t currentSize, const size_t newSize, const size_t index, const T & elementToInsert)
            {
                T * ret = (T*)realloc((void*)array, newSize * sizeof(T));
                if (ret == 0) { free((void*)array); return 0; }

                size_t i = newSize - 1;
                for (; i > currentSize; i--)
                    ret[i] = DefaultElement();
                for (; i > index; i--)
                    ret[i] = ret[i-1];

                ret[index] = elementToInsert;

                return ret;
            }
            /** Delete the array */
            inline static void DeleteArray(const T * array, const size_t currentSize) { free((void*)array); }
            /** Compare array */
            inline static bool CompareArray(const T * array1, const T * array2, const size_t commonSize) { return memcmp(array1, array2, commonSize * sizeof(T)) == 0; }

        };

        template <typename T>
        struct CloneAsNew
        {
            inline static void Clone(T ** const dest, T ** const src, size_t size)
            {
                while (size --)
                    dest[size] = new T(*src[size]);
            }

            inline static void Delete(T * data)
            {
                delete data;
            }
        };

        /** The default memory policy interface for plain old data */
        template <typename T, bool WithDeref = true, typename CloneMixin = CloneAsNew<T> >
        struct DefaultMemoryListPolicy
        {
            /** This is used to select the [] return type */
            enum { CanDereference = WithDeref };

            typedef T * Tptr;
            typedef const Tptr * constPtr;

            inline static void CopyPtr(T* & dest, T* const & src)                                                 { DefaultMemoryPolicy<T*>::Copy(dest, src); }
            inline static void CopyArray(T** dest,  T** const src, const size_t & destSize, const size_t srcSize) { DefaultMemoryPolicy<T*>::CopyArray(dest, src, destSize, srcSize); }
            inline static void CopyArray(T**& dest,  T** const src, size_t & destSize, const size_t srcSize)      { DefaultMemoryPolicy<T*>::CopyArray(dest, src, destSize, srcSize); }
            inline static void DuplicateArray(T**& dest, T** const src, size_t & destSize, const size_t srcSize)
            {
                if (destSize < srcSize)
                {
                    // Need to resize the array
                    T** tmp = (T**)realloc(0, srcSize * sizeof(T*));
                    if (tmp == 0) { DeleteArray(dest, destSize); dest = 0; destSize = 0; return;  }

                    CloneMixin::Clone(tmp, src, srcSize);

                    // Erase the previous array
                    DeleteArray(dest, destSize);
                    dest = tmp;

                    destSize = srcSize;
                } else CloneMixin::Clone(dest, src, srcSize);
            }

            /** This method search for the first element that match the given element and returns arraySize if not found
                The type must have a defined "== operator"
            */
            inline static size_t Search(T** const array, const size_t arraySize, const size_t from, T* const & valueToLookFor)        { size_t i = from; while (i < arraySize && !(array[i] == valueToLookFor)) i++; return i; }
            /** This method search for the last element that match the given element and returns arraySize if not found
                The type must have a defined "== operator"
            */
            inline static size_t ReverseSearch(T** const array, const size_t arraySize, T* const & valueToLookFor) { size_t i = arraySize; while (i && !(array[i-1] == valueToLookFor)) i--; return i ? i - 1 : arraySize; }
            /** This method search for the given element and returns arraySize if not found */
            inline static size_t SearchRef(const Tptr* array, const size_t arraySize, const size_t from, const T & valueToLookFor) { size_t i = from; while (i < arraySize && *array[i] != valueToLookFor) i++; return i; }
            /** This method returns a default constructed element or can throw */
            inline static T& DefaultSubElement() { static T t; return t; }
            /** This method returns a default constructed element */
            inline static Tptr DefaultElement() { return 0; }
            inline static T** NonDestructiveAlloc(T** const array, const size_t currentSize, const size_t newSize, T* const & fillWith) { return DefaultMemoryPolicy<T*>::NonDestructiveAlloc(array, currentSize, newSize, fillWith); }
            inline static T** NonDestructiveAlloc(T** const array, const size_t currentSize, const size_t newSize, T** const fillWith)
            {
                T** ret = (T**)realloc((void*)array, newSize * sizeof(T*));
                if (ret == 0) { free((void*)array); return 0; }
                if (fillWith)   memmove(&ret[currentSize], fillWith, (newSize - currentSize) * sizeof(T*));
                else            memset(&ret[currentSize], 0, (newSize - currentSize) * sizeof(T*));
                return ret;
            }
            inline static T* * Insert(T** const array, const size_t currentSize, const size_t newSize, const size_t index, T* const & elementToInsert) { return DefaultMemoryPolicy<T*>::Insert(array, currentSize, newSize, index, elementToInsert); }
            /** Delete the array */
            inline static void DeleteArray(Tptr * const array, const size_t currentSize)
            {
                // Delete all pointers in the array
                for (size_t i = 0; i < currentSize; i++)
                {
                    Remove(array[i]); array[i] = 0;
                }
                free((void*)array);
            }
            /** Remove on item from the array */
            inline static void Remove(T * const data)             { CloneMixin::Delete(data); }

            /** Compare array
                The type must have a defined "== operator"            */
            inline static bool CompareArray(T** const array1, T** const array2, const size_t commonSize) { constPtr end = array1 + commonSize; constPtr it = array1, it2 = array2; while (it != end) { if (!(*(*it++) == *(*it2++))) return false; } return true; }
        };

        template <typename T>
        struct SearchPolicy
        {
            typedef T & Result;
            inline static Result DefaultValue() { static T t(0); return t; }
            inline static Result From(T * x) { return *x; }
            inline static bool   Compare(const T & a, const T & b) { return a == b; }

            enum { DefaultConstructibleAndCopyable = true };
        };

    }
    /** @endcond */

    /** The array for plain old data and movable objects */
    template <typename T, class Policy = PlainOldDataPolicy::DefaultMemoryPolicy<T> >
    struct PlainOldData
    {
        /** @copydoc Container::PrivateGenericImplementation::Array
            @sa Container::PrivateGenericImplementation::Array for interface description
        */
        typedef Container::PrivateGenericImplementation::Array<T, Policy, false> Array;
        /** @copydoc Container::PrivateGenericImplementation::IndexList
            @sa Container::PrivateGenericImplementation::IndexList for interface description
        */
        typedef Container::PrivateGenericImplementation::IndexList<T, PlainOldDataPolicy::DefaultMemoryListPolicy<T>, false> IndexList;
        /** @copydoc Container::PrivateGenericImplementation::ChainedList
            @sa Container::PrivateGenericImplementation::ChainedList for interface description
        */
        typedef Container::PrivateGenericImplementation::ChainedList<T, 4, PlainOldDataPolicy::SearchPolicy<T> > ChainedList;
    };












    //////////////////////////////////////////////////////////////////////  Policy implementation below
    //// Copy constructable object
    /////////////////////////////////////////////////////////////////////////////////////////////////////



    /** @cond */
    /** Use these containers when you are dealing with class with copy constructors and defined "!=" operator */
    namespace WithCopyConstructorPolicy
    {
        /** The default memory policy interface for plain old data */
        template <typename T, const bool WithDeref = true>
        struct DefaultMemoryPolicy
        {
            /** This is used to select the [] return type */
            enum { CanDereference = WithDeref };

            typedef const T * constPtr;
            typedef const T constT;
            /** This method copy (can also move depending on policy) the given data */
            inline static void Copy(T & dest, const T & src) { dest.~T(); new(&dest) T(src); }
            /** This method copy the srcSize data from src to dest, not resizing dest even if required */
            inline static void CopyArray(T * dest, const T * src, const size_t & destSize, const size_t srcSize)
            {
                constPtr it = src, end = &src[srcSize < destSize ? srcSize : destSize];
                T * itd = dest;
                while (it != end)
                {   // Destruct itd
                    itd->~T();
                    // Construct it again
                    new(itd) T(*it);
                    itd++, it++;
                }
            }
            /** This method copy the srcSize data from src to dest, resizing dest if required */
            inline static void CopyArray(T *& dest, const T * src, size_t & destSize, const size_t srcSize)
            {
                if (destSize < srcSize)
                {
                    // Need to resize the array
                    T* tmp = (T*)realloc(0, srcSize * sizeof(T));
                    if (tmp == 0) { DeleteArray(dest, destSize); dest = 0; destSize = 0; return;  }

                    size_t i = 0;
                    for (; i < srcSize; i++)
                        new (&tmp[i]) T(src[i]);

                    // Erase the previous array
                    DeleteArray(dest, destSize);
                    dest = tmp;

                    destSize = srcSize;
                } else CopyArray((T*)dest, src, srcSize, srcSize);
            }
            /** This method returns a default constructed element or can throw */
            inline static T & DefaultElement() { static T t; return t; }
            /** This method search for the given element and returns arraySize if not found
                The type must have a defined "!= operator"
            */
            inline static size_t Search(const T* array, const size_t arraySize, const size_t from, const T & valueToLookFor) { size_t i = from; while (i < arraySize && array[i] != valueToLookFor) i++; return i; }
            /** This method search for the last element that match the given element and returns arraySize if not found
                The type must have a defined "== operator"
            */
            inline static size_t ReverseSearch(const T * array, const size_t arraySize, const T & valueToLookFor) { size_t i = arraySize; while (i && array[i-1] != valueToLookFor) i--; return i ? i - 1 : arraySize; }
            /** This method perform a non destructive allocation in place if possible or move the given array to a new place copying data in it */
            inline static T * NonDestructiveAlloc(const T* array, const size_t currentSize, const size_t newSize, const T & fillWith)
            {
                // Realloc here will not work. So let's do it the old way.
                T * ret = newSize ? (T*)malloc(newSize * sizeof(T)) : 0;
                if (ret == 0) { DeleteArray(array, currentSize); return 0; }

                size_t i = 0;
                for (; i < currentSize; i++)
                    new (&ret[i]) T(array[i]);

                for (; i < newSize; i++)
                    new (&ret[i]) T(fillWith);

                // Ok, let's delete the previous array
                DeleteArray(array, currentSize);
                return ret;
            }
            /** This method perform a non destructive allocation in place if possible or move the given array to a new place copying data in it */
            inline static T * NonDestructiveAlloc(const T* array, const size_t currentSize, const size_t newSize, T * const fillWith)
            {
                // Realloc here will not work. So let's do it the old way.
                T * ret = newSize ? (T*)malloc(newSize * sizeof(T)) : 0;
                if (ret == 0) { DeleteArray(array, currentSize); return 0; }

                size_t i = 0;
                for (; i < currentSize; i++)
                    new (&ret[i]) T(array[i]);

                if (fillWith)   for (; i < newSize; i++) new (&ret[i]) T(fillWith[i - currentSize]);
                else            for (; i < newSize; i++) new (&ret[i]) T();

                // Ok, let's delete the previous array
                DeleteArray(array, currentSize);
                return ret;
            }
            /** This method perform a non destructive allocation in place if possible or move the given array to a new place copying data in it,
                plus inserting the given object at the given place and DefaultElement for empty places if any */
            static T * Insert(const T* array, const size_t currentSize, const size_t newSize, const size_t index, const T & elementToInsert)
            {
                T * ret = (T*)malloc(newSize * sizeof(T));
                if (ret == 0) { DeleteArray(array, currentSize); return 0; }

                size_t i = 0;
                for (; i < index; i++)
                    new (&ret[i]) T(array[i]);

                new (&ret[index]) T(elementToInsert);

                for (; i < currentSize; i++)
                    new (&ret[i+1]) T(array[i]);
                for (; i < newSize - 1; i++)
                    new (&ret[i+1]) T(DefaultElement());

                // Ok, let's delete the previous array
                DeleteArray(array, currentSize);
                return ret;
            }

            /** Delete the array */
            inline static void DeleteArray(const T * array, const size_t currentSize)
            {
                if (!currentSize) return;
                constPtr end = array + currentSize, it = array;
                for (; it != end; it++)
                    it->~T();

                free((void*)array);
            }
            /** Compare array
                The type must have a defined "!= operator"            */
            inline static bool CompareArray(const T * array1, const T * array2, const size_t commonSize) { constPtr end = array1 + commonSize; constPtr it = array1, it2 = array2; while (it != end) { if (*it++ != *it2++) return false; } return true; }
        };

        /** The default memory policy interface for plain old data */
        template <typename T, const bool WithDeref = true>
        struct SimplerMemoryPolicy
        {
            /** This is used to select the [] return type */
            enum { CanDereference = WithDeref };

            typedef const T * constPtr;
            inline static void Copy(T & dest, const T & src)                                                       { DefaultMemoryPolicy<T>::Copy(dest, src); }
            inline static void CopyArray(T * dest, const T * src, const size_t & destSize, const size_t srcSize)   { DefaultMemoryPolicy<T>::CopyArray(dest, src, destSize, srcSize); }
            inline static void CopyArray(T *& dest, const T * src, size_t & destSize, const size_t srcSize)        { DefaultMemoryPolicy<T>::CopyArray(dest, src, destSize, srcSize); }
            inline static T & DefaultElement()                                                                     { return DefaultMemoryPolicy<T>::DefaultElement(); }
            /** This method search for the given element and returns arraySize if not found
                The type must have a defined "== operator" */
            inline static size_t Search(const T* array, const size_t arraySize, const size_t from, const T & valueToLookFor)          { size_t i = from; while (i < arraySize && !(array[i] == valueToLookFor)) i++; return i; }
            /** This method search for the last element that match the given element and returns arraySize if not found
                The type must have a defined "== operator" */
            inline static size_t ReverseSearch(const T* array, const size_t arraySize, const T & valueToLookFor)   { size_t i = arraySize; while (i && !(array[i-1] == valueToLookFor)) i--; return i ? i - 1 : arraySize; }

            inline static T * NonDestructiveAlloc(const T* array, const size_t currentSize, const size_t newSize, const T & fillWith) { return DefaultMemoryPolicy<T>::NonDestructiveAlloc(array, currentSize, newSize, fillWith); }
            inline static T * NonDestructiveAlloc(const T* array, const size_t currentSize, const size_t newSize, T * const fillWith) { return DefaultMemoryPolicy<T>::NonDestructiveAlloc(array, currentSize, newSize, fillWith); }
            inline static T * Insert(const T* array, const size_t currentSize, const size_t newSize, const size_t index, const T & elementToInsert) { return DefaultMemoryPolicy<T>::Insert(array, currentSize, newSize, index, elementToInsert); }
            inline static void DeleteArray(const T * array, const size_t currentSize)                            { DefaultMemoryPolicy<T>::DeleteArray(array, currentSize); }
            /** Compare array
                The type must have a defined "== operator"            */
            inline static bool CompareArray(const T * array1, const T * array2, const size_t commonSize) { constPtr end = array1 + commonSize; constPtr it = array1, it2 = array2; while (it != end) { if (!(*it++ == *it2++)) return false; } return true; }
        };

        template <typename T>
        struct CloneAsNew
        {
            inline static void Clone(T ** const dest, T ** const src, size_t size)
            {
                while (size --)
                    dest[size] = new T(*src[size]);
            }

            inline static void Delete(T * data)
            {
                delete data;
            }
        };

        /** The default memory policy interface for plain old data */
        template <typename T, typename CloneMixin = CloneAsNew<T>, const bool WithDeref = true >
        struct DefaultMemoryListPolicy
        {
            /** This is used to select the [] return type */
            enum { CanDereference = WithDeref };

            typedef T * Tptr;
            typedef const Tptr * constPtr;

            inline static void CopyPtr(T* & dest, T* const & src)                                                 { DefaultMemoryPolicy<T*>::Copy(dest, src); }
            inline static void CopyArray(T** dest,  T** const src, const size_t & destSize, const size_t srcSize) { DefaultMemoryPolicy<T*>::CopyArray(dest, src, destSize, srcSize); }
            inline static void CopyArray(T**& dest,  T** const src, size_t & destSize, const size_t srcSize)      { DefaultMemoryPolicy<T*>::CopyArray(dest, src, destSize, srcSize); }
            inline static void DuplicateArray(T**& dest, T** const src, size_t & destSize, const size_t srcSize)
            {
                if (destSize < srcSize)
                {
                    // Need to resize the array
                    T** tmp = (T**)realloc(0, srcSize * sizeof(T*));
                    if (tmp == 0) { DeleteArray(dest, destSize); dest = 0; destSize = 0; return;  }

                    CloneMixin::Clone(tmp, src, srcSize);

                    // Erase the previous array
                    DeleteArray(dest, destSize);
                    dest = tmp;

                    destSize = srcSize;
                } else CloneMixin::Clone(dest, src, srcSize);
            }

            /** This method search for the given element and returns arraySize if not found
                The type must have a defined "== operator"
            */
            inline static size_t Search(T** const array, const size_t arraySize, const size_t from, T* const & valueToLookFor)        { size_t i = from; while (i < arraySize && !(array[i] == valueToLookFor)) i++; return i; }
            /** This method search for the last element that match the given element and returns arraySize if not found
                The type must have a defined "== operator"
            */
            inline static size_t ReverseSearch(T** const array, const size_t arraySize, T* const & valueToLookFor) { size_t i = arraySize; while (i && !(array[i-1] == valueToLookFor)) i--; return i ? i - 1 : arraySize; }
            /** This method search for the given element and returns arraySize if not found */
            inline static size_t SearchRef(const Tptr* array, const size_t arraySize, const size_t from, const T & valueToLookFor)    { size_t i = from; while (i < arraySize && *array[i] != valueToLookFor) i++; return i; }
            /** This method returns a default constructed element or can throw */
            inline static T& DefaultSubElement() { static T t; return t; }
            /** This method returns a default constructed element */
            inline static Tptr DefaultElement() { return 0; }
            inline static T** NonDestructiveAlloc(T** const array, const size_t currentSize, const size_t newSize, T* const & fillWith) { return DefaultMemoryPolicy<T*>::NonDestructiveAlloc(array, currentSize, newSize, fillWith); }
            inline static T** NonDestructiveAlloc(T** const array, const size_t currentSize, const size_t newSize, T** const fillWith) { return DefaultMemoryPolicy<T*>::NonDestructiveAlloc(array, currentSize, newSize, fillWith); }
            inline static T* * Insert(T** const array, const size_t currentSize, const size_t newSize, const size_t index, T* const & elementToInsert) { return DefaultMemoryPolicy<T*>::Insert(array, currentSize, newSize, index, elementToInsert); }
            /** Delete the array */
            inline static void DeleteArray(Tptr * const array, const size_t currentSize)
            {
                // Delete all pointers in the array
                for (size_t i = 0; i < currentSize; i++)
                {
                    Remove(array[i]); array[i] = 0;
                }
                free((void*)array);
            }
            /** Remove on item from the array */
            inline static void Remove(T * const data)             { CloneMixin::Delete(data); }
            /** Compare array
                The type must have a defined "== operator"            */
            inline static bool CompareArray(T** const array1, T** const array2, const size_t commonSize) { constPtr end = array1 + commonSize; constPtr it = array1, it2 = array2; while (it != end) { if (!(*(*it++) == *(*it2++))) return false; } return true; }
        };


        template <typename T>
        struct SearchPolicy
        {
            typedef T & Result;
            inline static Result DefaultValue() { static T t; return t; }
            inline static Result From(T * x) { return *x; }
            inline static bool   Compare(const T & a, const T & b) { return a == b; }

            enum { DefaultConstructibleAndCopyable = true };
        };

    }

    /** Use these containers when you are dealing with class that are not constructible */
    namespace NotConstructiblePolicy
    {
        template <typename T>
        struct SearchPolicy
        {
            typedef T * Result;
            inline static Result DefaultValue() { return 0; }
            inline static Result From(T * x) { return x; }
            inline static bool   Compare(const T & a, const T * b) { return &a == b; }

            enum { DefaultConstructibleAndCopyable = false };
        };
    }
    /** @endcond */

    /** The copy constructor implementation

        The given class must provide an explicit copy constructor
        (don't rely on compiler generated copy constructor, or use POD)
        and an "== operator" and "!= operator" too.

        @remark No "= operator" is required.
    */
    template <typename T, class Policy = WithCopyConstructorPolicy::DefaultMemoryPolicy<T> >
    struct WithCopyConstructorAndOperators
    {
        /** @copydoc Container::PrivateGenericImplementation::Array
            @sa Container::PrivateGenericImplementation::Array for interface description
        */
        typedef Container::PrivateGenericImplementation::Array<T, Policy, true> Array;
        /** @copydoc Container::PrivateGenericImplementation::IndexList
            @sa Container::PrivateGenericImplementation::IndexList for interface description
        */
        typedef Container::PrivateGenericImplementation::IndexList<T, WithCopyConstructorPolicy::DefaultMemoryListPolicy<T>, false> IndexList;
        /** @copydoc Container::PrivateGenericImplementation::ChainedList
            @sa Container::PrivateGenericImplementation::ChainedList for interface description
        */
        typedef Container::PrivateGenericImplementation::ChainedList<T, 4, WithCopyConstructorPolicy::SearchPolicy<T> > ChainedList;
    };

    /** The easiest implementation with only copy constructor and == operator

        @remark No "= operator" is required.
    */
    template <typename T, class Policy = WithCopyConstructorPolicy::SimplerMemoryPolicy<T> >
    struct WithCopyConstructor
    {
        /** @copydoc Container::PrivateGenericImplementation::Array
            @sa Container::PrivateGenericImplementation::Array for interface description
        */
        typedef Container::PrivateGenericImplementation::Array<T, Policy, true> Array;
        /** @copydoc Container::PrivateGenericImplementation::IndexList
            @sa Container::PrivateGenericImplementation::IndexList for interface description
        */
        typedef Container::PrivateGenericImplementation::IndexList<T, WithCopyConstructorPolicy::DefaultMemoryListPolicy<T>, false> IndexList;
        /** @copydoc Container::PrivateGenericImplementation::ChainedList
            @sa Container::PrivateGenericImplementation::ChainedList for interface description
        */
        typedef Container::PrivateGenericImplementation::ChainedList<T, 4, WithCopyConstructorPolicy::SearchPolicy<T> > ChainedList;
    };


    namespace PrivateNotConstructibleImplementation
    {
        /** ChainedList class usage

            @copydoc Container::PrivateGenericImplementation::ChainedList
        */
        template <class U, unsigned int pow2 = 4, class SearchPolicy = NotConstructiblePolicy::SearchPolicy<U> >
        class ChainedList
        {
        public:
            /** Define the constant that the list can return */
            enum
            {
                ChainedEnd      = -1,           //!< This is used to add and or insert at end of list
                ChainedError    = ChainedEnd,   //!< Whenever an indexed error occurs, this is the returned value
                ChainedStart    = 0,            //!< This is used to insert data in the head of the list
                ChainedBS       = 1<<pow2,      //!< Process the asked pow once
            };

        private:
            /** The node base class */
            class NodeNotCopyable
            {
            public:
                /** The pointer on the previous object */
                NodeNotCopyable   * mpxPrevious;
                /** The pointer on the next object */
                NodeNotCopyable   * mpxNext;
                /** The pointer on the data */
                U *             mpuElement;


                /** Default constructors */
                NodeNotCopyable(U* t = 0) : mpxPrevious(0), mpxNext(0), mpuElement(t) {}
                /** The destructor always delete the object */
                ~NodeNotCopyable()
                {
                    delete mpuElement; mpuElement = 0;
                }

                /** Change the pointed object with pointer assignment */
                void    Set(U* t) { mpuElement = t; }
                /** Get a reference on the pointed object */
                U&      Get() { return (*mpuElement); }
            };

            /** The node class for copyable objects */
            class NodeCopyable
            {
            public:
                /** The pointer on the previous object */
                NodeCopyable   *    mpxPrevious;
                /** The pointer on the next object */
                NodeCopyable   *    mpxNext;
                /** The pointer on the data */
                U *         mpuElement;
            public:
                /** Default constructors */
                NodeCopyable(U* t = 0) : mpxPrevious(0), mpxNext(0), mpuElement(t) {}
                /** The destructor always delete the object */
                ~NodeCopyable()
                {
                    delete mpuElement; mpuElement = 0;
                }

                /** Change the pointed object with pointer assignment */
                void    Set(U* t) { mpuElement = t; }
                /** Get a reference on the pointed object */
                U&      Get() { return (*mpuElement); }

                // Add the copy interface
            public:
                /** Constructor using element's copy constructor */
                NodeCopyable(const U& t) : mpxPrevious(0), mpxNext(0) { mpuElement = new U(t); }
                /** Change the pointed object with copy constructor */
                void    Set(const U& t) { mpuElement = new U(t); }

            };

            typedef typename Container::PrivateGenericImplementation::Select<SearchPolicy::DefaultConstructibleAndCopyable, NodeCopyable, NodeNotCopyable>::Result Node;

            /** Select the parameters type depending on constructible policy */
            typedef typename Container::PrivateGenericImplementation::Select<SearchPolicy::DefaultConstructibleAndCopyable, U&, U*>::Result                  DefaultParamType;
            typedef typename Container::PrivateGenericImplementation::Select<SearchPolicy::DefaultConstructibleAndCopyable, const U&, U* const>::Result      DefaultConstParamType;
        private:
            /** The memory array holding pointers */
            template <unsigned int size>
            class LinearBlock
            {
                // Members
            private:
                /** Pointer on previous memory block */
                LinearBlock *   spxNext;
                /** Pointer on next memory block */
                LinearBlock *   spxPrevious;
                /** The memory block (array of node pointers) with template's size : size */
                Node            sapxBlock[size];
                /** Used pointers count in array */
                unsigned char   soUsed;

                // Construction
            public:
                /** Default constructor */
                LinearBlock(LinearBlock * previous = 0, LinearBlock * next = 0) : spxPrevious(previous), spxNext(next) , soUsed(0)    {}
                /** Default destruction too : destruct object if needed */
                ~LinearBlock()
                {
                    if ( spxNext!=0 || spxPrevious!=0)
                    {
                        if (spxNext!=0)
                            spxNext->spxPrevious = spxPrevious;
                        if (spxPrevious!=0)
                            spxPrevious->spxNext = spxNext;

                        spxNext = spxPrevious = 0;
                    }
                }

                // Interface
            public:
                /** Connect to other nodes */
                inline void Connect(LinearBlock * previous, LinearBlock * next)     { spxPrevious = previous; spxNext = next; }
                /** Delete this object safely, reconnect other node if needed */
                inline void Delete()
                {
                    if (spxNext!=0)
                        spxNext->spxPrevious = spxPrevious;
                    if (spxPrevious!=0)
                        spxPrevious->spxNext = spxNext;

                    spxNext = spxPrevious = 0;
                    delete this;
                }

                /** Return first element in the list (O(n) speed) */
                inline LinearBlock<size> * GoFirst()    {  LinearBlock<size>* pNode = this; while (pNode->spxPrevious != 0) pNode = spxPrevious; return pNode; }
                /** Return last element in the list (O(n) speed) */
                inline LinearBlock<size> * GoLast()     {  LinearBlock<size>* pNode = this; while (pNode->spxNext != 0) pNode = spxNext; return pNode; }


                /** Append a new block to the end */
                inline bool CreateNewBlock()
                {
                    if (spxNext!=0) return false;
                    LinearBlock<size>* pNode = new LinearBlock<size>(this, 0);
                    if (pNode == 0) return false;
                    else               spxNext = pNode;
                    return true;
                }

                /** Get a pointer on data */
                inline Node * GetData()     {   return &sapxBlock[0]; }
                /** Find a node */
                LinearBlock<size> * Find(Node * pNode)
                {
                    if (pNode==0) return 0;
                    LinearBlock<size> * pBlock = GoFirst();
                    LinearBlock<size> * pEnd = GoLast();
                    for (; pBlock != pEnd; pBlock = pBlock->spxNext)
                        if ( pNode >= pBlock->GetData() && pNode <= pBlock->GetData() + size) return pBlock;
                    return 0;
                }

            private:
                /** Delete all list
                    Note : this list should never be deleted explicitely
                    Only when ChainedList<U>::Delete is called
                    This list can only grow (for memory saving purpose, there is a protection, when two nodes are
                    free of data pointers, last one is deleted
                */
                bool DeleteAll()
                {
                    LinearBlock<size>* pNode2, *pNode = GoFirst();
                    if (pNode == 0) return false;
                    if (pNode->spxNext == 0)
                    {
                        pNode->Delete();
                        pNode = 0;
                        return true;
                    }

                    // Goto second pointer until next pointer is different from 0 , advance to next
                    for (; pNode != 0; )
                    {
                        pNode2 = pNode->spxNext;
                        pNode->Delete();
                        pNode = pNode2;
                    }
                    return true;
                }
            public:
                friend class ChainedList<U, pow2, SearchPolicy>;
            };

            // Members
        private:
            /** Pointer on the first node */
            Node *          mpxFirst;
            /** Pointer on the last node */
            Node *          mpxLast;
            /** Pointer on the current node */
            mutable Node *  mpxCurrent;
            /** The node count */
            unsigned int    mlNumberOfNodes;

            /** Number of allocated blocks */
            unsigned int                        mlNumberOfBlocks;
            /** Block size = 2^(template)pow2 */
//          unsigned int                        mlBlockSize;
            /** Pointer on first memory block */
            LinearBlock<ChainedBS> *    mpxBlock;
            /** Pointer on last block memory block */
            LinearBlock<ChainedBS> *    mpxBLast;

        private:
            /** Should we use blocks ? (same thing as (template)pow2 = 0)
                If not, the list act like a standard linked list
            */
            bool                        mbUseBlocks;
            /** Is integrity preserved ? (meaning pNode++ == pNode->mpxNext) */
            bool                        mbIntegrity;

            // Properties
        public:
            /** Get the current used node number */
            inline unsigned int getSize() const { return mlNumberOfNodes; }

            // Interface
        public:
            /** Add a node to list
                @warning Take care that add function preserve list integrity
                whereas Insert function doesn't in most of case.
                Even if Add function can insert node in list just before the
                given position, it is slower than a Insert function.
                So, in realtime application, prefer adding node in a
                preprocessing step, or using Insert function...
                (Inserting with Add is O(Number of node - position))
                @return true if insertion was successful */
            bool Add(U*, const unsigned int& = ChainedEnd);

            /** Take care that Insert doesn't preserve list integrity
                @sa Add function summary for details
                @return true if insertion was successful */
            bool Insert(DefaultConstParamType, const unsigned int& = ChainedStart);
            /** Subtract a node from list
                @warning Please note, that Sub conserve list integrity,
                whereas Remove function does not. However, Sub is slower
                than Remove, because of list reconstruction...
                So, in realtime, prefer using remove (that is faster)
                @return true if removing was successful */
            bool Sub(const unsigned int& = ChainedEnd);
            /** Remove a node from the list, but in most of case, it doesn't
                preserve list integrity
                @sa Sub method
                @return true if insertion was successful */
            bool Remove(const unsigned int& = ChainedStart);

            /** Find a node in the list
                If provided an operator == for U, then this method
                perform a O(N) search in the list
                @return The node index if found or ChainedError if not */
            unsigned int indexOf(DefaultConstParamType);
            /** Find the last matching node in the list
                If provided an operator == for U, then this method
                perform a O(N) search in the list
                @return The node index if found or ChainedError if not */
            unsigned int lastIndexOf(DefaultConstParamType);
            /** Swap two nodes, preserve list integrity for the given indexes
                @return true on successful swapping */
            bool Swap(const unsigned int &, const unsigned int &);

            // Helpers functions
        private:
            /** Create a node pointer in block array */
            inline Node * CreateNode();
            /** Goto the indexed node number */
            inline Node * Goto(const unsigned int &);
            /** Should we use blocks ? */
            inline bool UseBlocks(const bool &);
            /** Connect Nodes */
            bool Connect(Node *, Node *, Node *);

        public:
            /** Insert a list to this list
                @sa virtual bool Add(const U&, const unsigned int &)
                @return true on successful insertion    */
            bool Add(const ChainedList&, const unsigned int & = ChainedEnd);

            /** Delete the list
                @warning This function do delete nodes... (and data)
                @return true on successful deletion     */
            bool Free();
            /** Allow mutating a node data at the given index
                @return true on successful mutation     */
            bool Change(DefaultConstParamType, const unsigned int &);


            /** Iterate the list
                When starting set bInit = true, the method returns the first node, then return each time the next node
                @warning No other list function should be used between each parse list call (or use saveParsingStack / restoreParsingStack)
                @return the first node's data pointer when bInit is true, or the next parsed one else */
            U * parseList(const bool & bInit = false) const;
            /** Save the current parsing state (to restore it later on).
                You must call this method when you don't know what is going on until the parsing's end.
                @return a pointer on the list stack to restore later on with restoreParsingStack */
            const void * saveParsingStack() const;
            /** Save the current parsing state (to restore it later on).
                You must call this method when you don't know what is going on until the parsing's end.
                @param stack A pointer on the stack as returned by saveParsingStack
                @return a pointer on the list stack to restore later on with restoreParsingStack */
            void restoreParsingStack(const void * stack) const;
            /** Iterate the list from a given position
                When started (uiPos != ChainedEnd) return the requested node, else return each time the next node
                @sa ParseList */
            U * parseListStart(const unsigned int & uiPos = ChainedEnd) const;

            /** Reverse iterate the list
                When started (ie when bInit = true) return the last node, then return each time the previous node
                @warning No other list function should be used between each parse list call
                @return the last node's data pointer when bInit is true, or the previous parsed one else */
            U * reverseParseList(const bool & bInit = false) const;
            /** Reverse parse list from a given position
                When started (uiPos != ChainedEnd) return the requested node, else return each time the previous node
                @sa ReverseParseList */
            U * reverseParseListStart(const unsigned int & uiPos = ChainedEnd) const;

            /** Move the object from the given position to the given position.
                This method doesn't swap the two objects, but instead remove the first object and reinsert it later on.
                @param uiInitialPos     The position of the object to remove from the list
                @param uiFinalPos       The position to insert the object back
                @return true on success, false on memory error */
            bool moveObject(const unsigned int & uiInitialPos, const unsigned int & uiFinalPos = ChainedEnd);

            /** Move the list from a ChainedList into this one (the parameter's node are then 0ified)
                @warning the operation will fail if the list is not empty (use transferList in that case)
                @return true on success, false on error or non empty list */
            inline bool moveList(ChainedList & a)
            {
                if (mlNumberOfNodes) return false;
                mpxBlock = a.mpxBlock;
                mpxBLast = a.mpxBLast;
                mpxFirst = a.mpxFirst;
                mpxLast = a.mpxLast;
                mpxCurrent = a.mpxCurrent;
                mlNumberOfNodes = a.mlNumberOfNodes;
                mlNumberOfBlocks = a.mlNumberOfBlocks;
                mbUseBlocks = a.mbUseBlocks;
                mbIntegrity = a.mbIntegrity;

                a.mpxBlock = a.mpxBLast = 0;
                a.mpxFirst = a.mpxLast = a.mpxCurrent = 0;
                a.mlNumberOfNodes = a.mlNumberOfBlocks = 0;
                a.mbIntegrity = true;
                return true;
            }
            /** Move the list from a ChainedList into this one (the parameter's node are then 0ified)
                @warning the current nodes is this list are inserted to the beginning of this list
                @return true on success, false on error */
            inline bool moveAppendedList(ChainedList & a)
            {
                if (mlNumberOfNodes)
                {
                    if (!a.Add(*this, ChainedStart)) return false;
                }
                mpxBlock = a.mpxBlock;
                mpxBLast = a.mpxBLast;
                mpxFirst = a.mpxFirst;
                mpxLast = a.mpxLast;
                mpxCurrent = a.mpxCurrent;
                mlNumberOfNodes = a.mlNumberOfNodes;
                mlNumberOfBlocks = a.mlNumberOfBlocks;
                mbUseBlocks = a.mbUseBlocks;
                mbIntegrity = a.mbIntegrity;

                a.mpxBlock = a.mpxBLast = 0;
                a.mpxFirst = a.mpxLast = a.mpxCurrent = 0;
                a.mlNumberOfNodes = a.mlNumberOfBlocks = 0;
                a.mbIntegrity = true;
                return true;
            }


            // Operators
        public:
            /** Operator [] return U type at i-th position.
                @param i the indexed position to retrieve
                @return The returned type depends on the selected policy
            */
            inline typename SearchPolicy::Result operator [] (const unsigned int &i)
            {   Node * pNode = Goto(i);
                if (pNode == 0)    return SearchPolicy::DefaultValue();
                return SearchPolicy::From(pNode->mpuElement);
            }

            /** Append the given element to the list end */
            inline ChainedList& operator += (DefaultConstParamType a)       { this->Add(a); return (*this); }
            /** Remove the given element if found */
            inline ChainedList& operator -= (DefaultConstParamType a)       { unsigned int i = this->lastIndexOf(a); if (i!=ChainedEnd) this->Sub(i); return (*this); }


            /** Append the given list to ours */
            inline ChainedList& operator += (const ChainedList& a)  { this->Add(a); return (*this); }


            // Disallowed here
        private:
            /** Copy constructor */
            ChainedList(const ChainedList &);
            /** Equality operator */
            inline ChainedList& operator = (const ChainedList& );
            /** It costs nothing... */
            inline ChainedList& operator + (const ChainedList& a) const;
            /** The classic copy and modify operators */
            inline ChainedList operator + (const U & a) const;
            inline ChainedList operator - (const U & a) const;
            bool Add(const U&, const unsigned int& = ChainedEnd);


            // Construction
        public:
            /** Default constructor */
            ChainedList();
            /** Default destructor which destruct objects linked to list */
            ~ChainedList();
        };


        // Inclusion for template definition, and implementation (it's too big to be implemented here)
        #define NotCopyable
#ifndef DOXYGEN
        #include "template/ChainedList.hpp"
#endif
        #undef NotCopyable
    }


    /** Not copyable objects */
    template <typename T>
    struct NotConstructible
    {
        /** @copydoc Container::PrivateGenericImplementation::IndexList
            @sa Container::PrivateGenericImplementation::IndexList for interface description
        */
        typedef Container::PrivateGenericImplementation::IndexList<T, PlainOldDataPolicy::DefaultMemoryListPolicy<T, false>, false> IndexList;
        /** @copydoc Container::PrivateGenericImplementation::ChainedList
            @sa Container::PrivateGenericImplementation::ChainedList for interface description

            @warning Dealing with unconstructible type is a pain for templated code, so you might get errors
        */
        typedef PrivateNotConstructibleImplementation::ChainedList<T, 4, NotConstructiblePolicy::SearchPolicy<T> > ChainedList;
    };

    /** Define some basic algorithms like sorting
        @param T    The container to apply algorithms on */
    template <typename T>
    struct Algorithms
    {
        /** Sort the given container using the comparator object.
            You can safely use this algorithm on the IndexList and ChainedList containers too.

            @param array            A reference on the container to sort. The container must have a Swap(index, index) method
            @param comparator       A object that must have a <pre>compareData</pre> method which compare 2 container's object.
                                    Typically with a IndexList&lt;float&gt;, the expected method signature will be "int compareData(const float & a, const float & b)".
                                    It can be static or const.<br>
                                    This method must return -1 if the first object should come before the second<br>
                                    This method must return 0 if the first object similar to the second<br>
                                    This method must return 1 if the first object should come after the second<br>
            @param sortSameElements If set to true, the similar object are sorted (and therefore can be moved from their current relative position).
                                    It's faster to set this to true, but it's sometimes better not to move relative position of similar objects
            @param firstIndex       The first index to start sorting with
            @param lastIndex        The last index to stop sorting with
            @return true if the sort is possible, false on error
            The algorithm is a mix of (quickSort and insertionSort) from http://www.seeingwithc.org/topic2html.html and http://www.rawmaterialsoftware.com/juce  */
        template <typename Comparator>
        static bool sortContainer(T & array, Comparator & comparator, const bool sortSameElements = true, uint32 firstIndex = 0, uint32 lastIndex = (uint32)-1)
        {
            (void) comparator;  // if you pass in an object with a static compareElements() method, this
                                // avoids getting warning messages about the parameter being unused

            if (!array.getSize()) return true;
            if (firstIndex > array.getSize()) return false;
            if (lastIndex > array.getSize()) lastIndex = array.getSize() - 1;
            if (lastIndex < firstIndex) return false;

            if (!sortSameElements)
            {
                // Use simple Bubble sort in that case, as we are not supposed to sort the same elements
                for (uint32 i = firstIndex; i < lastIndex; ++i)
                {
                    if (comparator.compareData (array[i], array [i + 1]) > 0)
                    {
                        array.Swap(i, i+1);
                        // Restart the algorithm as soon as we detected incoherencies
                        if (i > firstIndex) i -= 2;
                    }
                }
            }
            else
            {
                // Use Quicksort here
                enum
                {
                    StackSize = (sizeof(uint32) * 8), // Quicksort uses a stack size of O(log2(N)) for keeping the indexes to data. If you use more than 32bits indexes, increase this number
                };
                uint32 fromStack[StackSize], toStack[StackSize];
                int stackIndex = 0;

                while (1)
                {
                    const uint32 size = (lastIndex - firstIndex) + 1;

                    if (size <= 8)
                    {
                        // Quick sort is not efficient when the amount of data to sort is not that high, so simply
                        // perform a O(N*N) Insertion sort here (with N being 8 at max)
                        uint32 j = lastIndex;
                        uint32 maxIndex;

                        while (j > firstIndex)
                        {
                            maxIndex = firstIndex;
                            for (uint32 k = firstIndex + 1; k <= j; ++k)
                                if (comparator.compareData (array[k], array [maxIndex]) > 0)
                                    maxIndex = k;

                            array.Swap(maxIndex, j);
                            --j;
                        }

                        // Ok, we are done!
                    }
                    else
                    {
                        // The quick sort algorithm starts here
                        const uint32 mid = firstIndex + (size >> 1);
                        // Make sure the first index is in the middle place
                        array.Swap(mid, firstIndex);

                        uint32 i = firstIndex;
                        uint32 j = lastIndex + 1;

                        // Main compare loop
                        while (1)
                        {
                            // Make sure the element between i and j are all sorted
                            while (++i <= lastIndex && comparator.compareData (array[i], array [firstIndex]) <= 0);
                            while (--j > firstIndex && comparator.compareData (array[j], array [firstIndex]) >= 0);

                            // Ok, one of the element is not at the right place
                            if (j < i) break;
                            // Then swap them
                            array.Swap(i, j);
                        }
                        // So swap it
                        array.Swap(firstIndex, j);

                        // And order the stack of operations to perform
                        if (j - 1 - firstIndex >= lastIndex - i)
                        {
                            if (firstIndex + 1 < j)
                            {
                                fromStack [stackIndex] = firstIndex;
                                toStack [stackIndex] = j - 1;
                                ++stackIndex;
                            }

                            if (i < lastIndex)
                            {
                                firstIndex = i;
                                continue;
                            }
                        }
                        else
                        {
                            if (i < lastIndex)
                            {
                                fromStack [stackIndex] = i;
                                toStack [stackIndex] = lastIndex;
                                ++stackIndex;
                            }

                            if (firstIndex + 1 < j)
                            {
                                lastIndex = j - 1;
                                continue;
                            }
                        }
                    }

                    // Do we have remaining elements to sort ?
                    if (--stackIndex < 0) break;
                    // Are we overflowing the stack ?
                    if (stackIndex > StackSize) return false;

                    // Pop the stack
                    firstIndex = fromStack [stackIndex];
                    lastIndex = toStack [stackIndex];
                }
            }
            return true;
        }
        /** Search the given container using the comparator object.
            @warning This only works if the container is sorted.

            You can safely use this algorithm on the IndexList and ChainedList containers too.

            @param array            A reference on the container to sort. The container must have a Swap(index, index) method
            @param comparator       A object that must have a <pre>compareData</pre> method which compare 2 container's object.
                                    Typically with a IndexList&lt;float&gt;, the expected method signature will be "int compareData(const float & a, const float & b)".
                                    It can be static or const.<br>
                                    This method must return -1 if the first object should come before the second<br>
                                    This method must return 0 if the first object similar to the second<br>
                                    This method must return 1 if the first object should come after the second<br>
            @param value            The value to search for
            @param below            If the exact match isn't found, and if this is set to true, this
                                    will return the element that's just below the value to search for, else it returns the one just above
            @param firstIndex       The first index to start searching with
            @param lastIndex        The last index to stop searching with
            @return the index of the matched element, or the array size if not found (even concidering the "below" parameter) */
        template <typename Comparator, typename Val>
        static size_t searchContainer(T & array, Comparator & comparator, const Val & value, const bool below = true, size_t firstIndex = 0, size_t lastIndex = (size_t)-1)
        {
            (void) comparator; // Unused param

            if (!array.getSize()) return array.getSize();
            if (firstIndex >= array.getSize()) return array.getSize();
            if (lastIndex >= array.getSize()) lastIndex = array.getSize() - 1;
            if (lastIndex < firstIndex) return array.getSize();

            size_t pos = (lastIndex + firstIndex) / 2;
            while (pos < array.getSize())
            {
                int comparand = comparator.compareData(array[pos], value);
                if (comparand == 0)
                {
                    // Depending on below parameter, we'll return the exact value that's matching on the left or right side of the range
                    if (!below)
                        while (pos + 1 < array.getSize() && comparator.compareData(array[pos + 1], value) == 0) ++pos;
                    else
                        while (pos > 0 && comparator.compareData(array[pos - 1], value) == 0) --pos;

                    return pos;
                }
                if (comparand > 0)
                {
                    if (!pos) return below ? array.getSize() : 0;
                    if (lastIndex == pos)
                        // Can't find it in the current array
                        return below ? pos - 1 : array.getSize();
                    lastIndex = pos;
                    pos = (lastIndex + firstIndex) / 2;
                    continue;
                }

                if (firstIndex == lastIndex)
                    // Can't find it in the current array
                    return below ? array.getSize() - 1 : array.getSize();
                firstIndex = pos;
                pos = (pos + lastIndex + 1) / 2;
            }
            return pos;
        }
    };
}

#endif

