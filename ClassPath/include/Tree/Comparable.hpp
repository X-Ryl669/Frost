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
        Less    = -1,           //!< The first value is less than the second one
        Equal   =  0,           //!< The first value is comparable with the second one
        Greater =  1,           //!< The first value is greater than the second one
        NotDecided = 0xBADC0DE, //!< There is not enough information to decide how the comparison would result
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
    struct Comparable
    {
        /** The result type we are using */
        typedef CompareType Result;

        /** Compare this item against the given key */
        inline Result Compare(const KeyType & _key) const { return BasicCompare(_key); }
        /** Basic compare that's never undecided */
        inline Result BasicCompare(const KeyType & _key) const { return Policy::Equal(_key, key) ? Equal : (Policy::LessThan(_key, key) ? Less : Greater); }
        /** Return the key in this comparator */
        inline KeyType Key() const { return key; }

        /** No default constructor, must provide a key */
        Comparable(KeyType  _key) : key(_key) {}

    private:
        /** The key used in this comparator */
        KeyType  key;
    };

    /** Comparable type with reserved key value.
        This is used to match unknown pattern at runtime. Obviously, the reserved key value will never be compared correctly.
        By default, this implementation supports two reserved pattern, '#' is used to match numbers-like (that is any sequence
        of these chars "-0123456789."), and '"' is used to match any text up to the delimiter. Finally, '*' is used as a catch-all since appearance.

        When used with URL, it allows O(log(N)) routing tables searching, with placeholder capture if any in the TS tree.
        If you need something else, just copy/paste and adapt this code */
    template <class KeyType, KeyType N = (KeyType)'#', KeyType T = (KeyType)'"', KeyType D = (KeyType)'/', KeyType C = (KeyType)'*', class Policy = DefaultComparator>
    struct ReservedComparable
    {
        typedef CompareType Result;
        typedef KeyType K;

        /** Compare this item against the given key */
        inline Result Compare(const KeyType & _key) const
        {
            if (Policy::Equal(key, N) &&
                (    Policy::Equal(_key, (K)'-') || Policy::Equal(_key, (K)'.') || Policy::Equal(_key, (K)'0') || Policy::Equal(_key, (K)'1') || Policy::Equal(_key, (K)'2')
                  || Policy::Equal(_key, (K)'3') || Policy::Equal(_key, (K)'4') || Policy::Equal(_key, (K)'5') || Policy::Equal(_key, (K)'6') || Policy::Equal(_key, (K)'7')
                  || Policy::Equal(_key, (K)'8') || Policy::Equal(_key, (K)'9')))
                return NotDecided;
            if (Policy::Equal(key, T) && !Policy::Equal(_key, D))
                return NotDecided;
            if (Policy::Equal(key, C))
                return NotDecided;
            return BasicCompare(_key);
        }
        /** Basic compare that's never undecided */
        inline Result BasicCompare(const KeyType & _key) const { return Policy::Equal(_key, key) ? Equal : (Policy::LessThan(_key, key) ? Less : Greater); }

        /** Return the key in this comparator */
        inline KeyType Key() const { return key; }

        /** No default constructor, must provide a key */
        ReservedComparable(KeyType key) : key(key) {}

    private:
        /** The key used in this comparator */
        KeyType  key;
    };

}

#endif
