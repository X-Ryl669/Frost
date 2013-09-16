#ifndef hpp_CPP_Comparator_CPP_hpp
#define hpp_CPP_Comparator_CPP_hpp

#include "../Types.hpp"

/** When using sorted tree, it's required to sort your key, using a Comparable.
    For plain old type, the default comparator will likely work out of the box.
    However, when using complex keys, you'll preferable specialize the Comparable template or define a policy to compare your keys  */
namespace Comparator
{
	/** The comparison result */
	enum CompareType
	{
		Less	= -1,
		Equal	=  0,
		Greater =  1
	};

    /** This is a default comparator using operator < */
	struct DefaultComparator
	{
		template <typename T>
			inline static bool LessThan(const T & a, const T & b) { return (bool)(a < b); }
		template <typename T>
			inline static bool Equal(const T & a, const T & b) { return (bool)(a == b); }
	};

	/** This is a very simple class used to compare against a given (templated) key
	    at runtime.
	    @code
			// Let's use long as keys
			Comparable<long> a(0), b(1);
			if (a.Compare(b) == Comparable::Equal)
				// Values are equal
				printf("Equal!\n");
            else if (a.Compare(b) == Comparable::Less)
				// In this example, we should get here as a is less than b
				printf("Less!\n");
			else
				printf("Greater!\n");
		@endcode

        While it may seems overkill for simple types like long, it's a very convenient
        class for type that aren't obvious to compare (like a String or a Variant)
	*/
	template <class KeyType, class Policy = DefaultComparator>
	class Comparable
	{
	public:
    	typedef CompareType Result;

		// Interface
	public:
		/** Compare this item against the given key */
		inline Result Compare(const KeyType & _key) const { return Policy::Equal(_key, key) ? Equal : (Policy::LessThan(_key, key) ? Less : Greater);	}

		/** Return the key in this comparator */
		inline KeyType Key() const { return Key; }

		// Constructors
	public:
		/** No default constructor, must provide a key */
		Comparable(KeyType  _key) : key(_key) {}

	private:
		/** The key used in this comparator */
		KeyType  key;
	};
}

#endif
