#include "../../include/Types.hpp"
#include "../../include/Strings/Strings.hpp"
#include "../../include/Hash/HashTable.hpp"
#include "../../include/Hash/StringHash.hpp"

namespace Container
{
    template <>
        uint32 HashKey<Strings::FastString>::hashKey(const Strings::FastString & initialKey)
    {
        return Hash::superFastHash(initialKey, initialKey.getLength());
    }
}

