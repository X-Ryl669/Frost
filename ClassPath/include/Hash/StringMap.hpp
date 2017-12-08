#ifndef hpp_StringMap_hpp
#define hpp_StringMap_hpp

// We obviously need strings
#include "../Strings/Strings.hpp"
// We need hash table too
#include "HashTable.hpp"

namespace Strings
{
    /** The StringMap is a hash table O(1) with a fast hashing function in O(N) */
    typedef Container::HashTable<FastString, FastString, Container::HashKey<FastString>, Container::DeletionWithDelete<FastString>, false, true > StringMap;
}

#endif
