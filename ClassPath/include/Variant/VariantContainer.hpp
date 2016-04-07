#ifndef hpp_HPP_VariantContainer_HPP_hpp
#define hpp_HPP_VariantContainer_HPP_hpp

// We need Variant
#include "Variant.hpp"
// We need containers
#include "../Container/Container.hpp"

namespace Type
{
    /** The Variant array class we are using */
    typedef Container::WithCopyConstructor<VarT<ObjectCopyPolicy> >::Array VarArray;
    /** The Variant array class we are using */
    typedef Container::NotConstructible<VarT<ObjectPtrPolicy> >::IndexList RefArray;
}






#endif