#ifndef hpp_CPP_FIFO_CPP_hpp
#define hpp_CPP_FIFO_CPP_hpp


// Need realloc
#include <stdlib.h>
// Need operator new
#include <new>

// Need global definitions 
#include "../Types.hpp"
// Need assert too
#include "../Utils/Assert.hpp"
// Need debugger
#include "../Platform/Platform.hpp"

/** Stack implementations for both Plain Old Data and other objects */
namespace Stack
{
	/** Specialized stack implementations for Plain Old Data */
	namespace PlainOldData
	{
		/** Provides FIFO like stack functionalities only for POD objects
			It is recommended not to push heap stored objects (like new/malloced object)
			
			The stack takes care of memory management in its internal array when 
			destructed or reallocated (size zeros and grow again)

			@code
			Stack::WithClone::FIFO<double> stack;
			// Push some data onto this stack
			stack.Push(3.1);
			stack.Push(4);
			// Pop them
			double a = stack.Pop();
			// Don't delete the popped pointer
			double b = stack.Pop();
			// c will be default constructed object (probably 0xCDCDCDCDCDCDCDCD)
			double c = stack.Pop(); 
            // On leaving scope, the array will be destructed
			@endcode
		*/
		template <typename T> 
		class FIFO
		{
		public:
			/** Default Constructor */
			inline FIFO() : array(NULL), currentPos(0), currentSize(0), allocatedSize(0)	{ }
			/** Copy constructor */
			inline FIFO(const FIFO & other) : array(NULL), currentPos(0), currentSize(0), allocatedSize(0) { *this = other; }
			/** Destructor */
			~FIFO()	{ Clear(); }

		// Interface
		public:
			/** Reset the FIFO */
			inline void Reset() { currentSize = 0; allocatedSize = 0; currentPos = 0; array = NULL; }
			/** Pop an element from the list */
			inline T & Pop()	{ if (currentSize == 0) return defaultElement; T& ref = *(T*)&array[currentPos]; currentPos += sizeof(T); currentSize --; return ref; }
			/** Push an element to the list */
			inline void Push(const T & ref) 
			{
				// Check if the object can be freed
				if (currentSize == 0 && currentPos) Clear();
				if (currentSize >= allocatedSize || currentPos >= (allocatedSize - currentSize - 1) * sizeof(T))
				{
					// Need to realloc the data
					if (allocatedSize == 0) allocatedSize = 2; else allocatedSize *= 2;
					// This will not work with type other than POD
					array = (uint8*)Platform::safeRealloc(array, allocatedSize*sizeof(T));
					if (array == NULL) { Reset(); return; }
				}
				// Copy the data
				new(&array[currentSize * sizeof(T) + currentPos]) T(ref);
				currentSize ++;
			}

			/** Classic copy operator */
			inline const FIFO& operator = ( const FIFO <T>& fifo)
			{
				Clear();

				for (size_t i = 0; i < fifo.currentSize; i++)
					Push(*(T*)&fifo.array[i * sizeof(T) + fifo.currentPos]);

				return *this;
			}
			
			/** Access size member */
			inline size_t getSize() const { return currentSize; }
			/** Tell the stack it is no more required to delete the given pop'd object 
				@return false because it is not supported for POD
			*/
			inline bool Forget(const T& avoidDeleting) { return false;	}

        private:
            /** Clear the fifo */
            inline void Clear() 
            { 
                while (currentSize)	
                { 
                    currentSize --; 
                    T * pPtr = (T*)&array[currentSize * sizeof(T) + currentPos]; 
                    pPtr->~T(); 
                }
                while (currentPos)	
                { 
                    currentPos -= sizeof(T); 
                    T * pPtr = (T*)&array[currentPos]; 
                    pPtr->~T(); 
                }
                Platform::safeRealloc(array, 0);
                Reset(); 
            }
		private:
			/** The array holder */
			uint8 *													array;
			/** The current position (in uint8s) */
			size_t 													currentPos;
			/** The current size (in sizeof(T) unit) */
			size_t													currentSize;
			/** The allocated size (always more than currentSize, max currentSize * 2) */
			size_t													allocatedSize;
			/** The returned default element on error
			    Compilation will stop here when used on non POD data
			*/
			static T												defaultElement;
		};
		template <typename T> T FIFO<T>::defaultElement = 0;
	}

	/** Specialized stack implementations for object (with possible clone() method) */
	namespace WithClone
	{
		/** Provides FIFO like stack functionalities for any object
			It is recommended to push "new" created object
			
			The stack takes care of deleting the pushed pointer when 
			destructed or reallocated (size zeros and grow again)
			If you plan to use copy constructor or operator =, the 
			objects must provide a "clone" method.

			@code
			Stack::WithClone::FIFO<MyClass> stack;
			// Push some data onto this stack
			stack.Push(new MyClass(...));
			stack.Push(new MyDerivedClass(...));
			// Pop them
			MyClass * t = stack.Pop();
			// You must delete the popped pointer
			MyClass * t2 = stack.Pop();
			// t3 will be NULL, but t and t2 still points to valid memory
			MyClass * t3 = stack.Pop(); 
			// And so on...
			stack.Push(new MyChildClass(3));
			
			// This will use virtual MyClass::clone method
			Stack::WithClone::FIFO<MyClass> stack2 = stack; 
            // On leaving scope, the array will be destructed, so will MyChildClass instance.
			@endcode
		*/
		template <typename T> 
		class FIFO
		{
		private:
			enum { elementSize = sizeof(T*) };
		public:
			/** Default Constructor */
			inline FIFO() : array(NULL), currentPos(0), currentSize(0), allocatedSize(0)	{ }
			/** Copy constructor */
			inline FIFO(const FIFO & other) : array(NULL), currentPos(0), currentSize(0), allocatedSize(0) { *this = other; }
			/** Destructor */
			~FIFO()		{ Clear(); }

		// Interface
		public:
			/** Reset the FIFO */
			inline void Reset() { currentSize = 0; allocatedSize = 0; currentPos = 0; array = NULL; }
			/** Pop an element from the list */
			inline T * Pop()	{ Assert(currentSize + currentPos <= allocatedSize); if (currentSize == 0) return NULL; T* ref = array[currentPos]; array[currentPos] = 0; currentPos ++; currentSize --; return ref; }
			/** Push an element to the list */
			inline void Push(T * ref) 
			{
                Assert(!allocatedSize || currentSize + currentPos <= allocatedSize);
				// Check if the object can be freed
				if (currentSize == 0 && currentPos) Clear(ref);
				if (currentSize >= allocatedSize)
				{
					// Need to realloc the data
					if (allocatedSize == 0) allocatedSize = 2; else allocatedSize *= 2;
					// This will not work with type other than POD
					array = (T**)Platform::safeRealloc(array, allocatedSize*elementSize);
					if (array == NULL) { Reset(); return; }
				}
                if (currentSize + currentPos + 1 >= allocatedSize )
                {
                    // Need to move back the data to the top of the FIFO
                    memmove(array, &array[currentPos], currentSize * elementSize);
                    memset(&array[currentSize], 0, (allocatedSize - currentSize) * elementSize);
                    currentPos = 0;
                }
				// Copy the data
				array[currentSize + currentPos] = ref;
				currentSize ++;
                Assert(currentSize + currentPos <= allocatedSize);
			}

			/** Classic copy operator */
			inline const FIFO& operator = ( const FIFO <T>& fifo)
			{
				Clear();
				for (size_t i = 0; i < fifo.currentSize; i++) Push(fifo.array[i + fifo.currentPos]->clone());
				return *this;
			}
			
			/** Access size member */
			inline size_t getSize() const { return currentSize; }

		private:
			/** Clear the fifo 
				@param ref is checked for every item. If one of them matches, then it is not deleted
			*/
			inline void Clear(T * ref = NULL) 
			{	
                Assert(currentSize + currentPos <= allocatedSize);
                while (currentSize--) { T * pPtr = array[currentSize + currentPos]; if (pPtr != ref) delete pPtr; }
				while (currentPos--)  { T * pPtr = array[currentPos]; if (pPtr != ref) delete pPtr; }
                Platform::safeRealloc(array, 0);
                Reset();
            }
		private:
			/** The array holder */
			T **													array;
			/** The current position (in bytes) */
			size_t 													currentPos;
			/** The current size (in sizeof(T) unit) */
			size_t													currentSize;
			/** The allocated size (always more than currentSize, max currentSize * 2) */
			size_t													allocatedSize;
		};
	}
}


#endif
