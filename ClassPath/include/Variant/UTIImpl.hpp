#ifndef hpp_CPP_UTIImplementation_CPP_hpp
#define hpp_CPP_UTIImplementation_CPP_hpp

// Include the type engine declaration
#include "../../include/Variant/UTI.hpp"
// Include the variant type declaration
#include "../../include/Variant/Variant.hpp"
// We need container
#include "../../include/Container/Container.hpp"
// We need strings
#include "../../include/Strings/Strings.hpp"
// We need dynamic objects too
#include "../../include/Types/DynamicObject.hpp"

//   !!!!!!! WARNING !!!!!!!!
//   If you modify something here you have to report the modification in UTIImpl.cpp
//   !!!!!!! WARNING !!!!!!!!


namespace UniversalTypeIdentifier
{
    // We use string from FastString lib here
    typedef Strings::FastString String;
    using namespace Type;
    
    // Internal implementation for storing types
    class TextDataSource : public DataSource
    {
        String     sourceHolder;
    public:
        void       setValue(const Var & var) { var.extractTo(sourceHolder); }
        Var        getValue() const          { Var var(sourceHolder); return var; }
        TextDataSource(const String & source) : sourceHolder(source) {}
    };
    

    /** Some specializations for plain old data
        Those types never exists in run time (they are only compile time based)
        They don't take any space.

        If they need to be exported, use the specialized POD2Type template
    */
    /** The macro below avoid making error while copy & pasting */

#define MakePODHolder(SType, As, Value) template <> struct TypeIDImpl<SType>;
    MakePODHolder(signed char,     "%c",  0x00000001);
    MakePODHolder(unsigned char,   "%c",  0x00000002);
    MakePODHolder(signed short,    "%hd", 0x00000003);
    MakePODHolder(unsigned short,  "%hu", 0x00000004);
    MakePODHolder(signed int,      "%d",  0x00000005);
    MakePODHolder(unsigned int,    "%u",  0x00000006);
#ifndef _LP64
    MakePODHolder(signed long,     "%ld", 0x00000007);
    MakePODHolder(unsigned long,   "%lu", 0x00000008);
#else
    MakePODHolder(long long,      PF_LLD, 0x00000007);
#endif
    MakePODHolder(int64,          PF_LLD, 0x00000009);
    MakePODHolder(uint64,         PF_LLU, 0x0000000A);
    MakePODHolder(void *,          "%p",  0x0000000B);
    MakePODHolder(double,          "%lf", 0x0000000C);
    MakePODHolder(long double,     "%Lf", 0x0000000D);
    MakePODHolder(float,           "%f",  0x0000000E);
    MakePODHolder(bool,            "%d",  0x0000000F);
#undef MakePODHolder

}


#define RegisterClassForVariantDecl(TYPE) \
    namespace UniversalTypeIdentifier \
    {  \
        template <> struct TypeIDImpl< TYPE >; \
    }


#ifndef DOXYGEN

    typedef Type::VarT<Type::ObjectCopyPolicy>::Empty VarEmpty;
    typedef Type::VarT<Type::ObjectPtrPolicy>::Empty RefEmpty;
    typedef Container::NotConstructible<Type::VarT<Type::ObjectPtrPolicy> >::IndexList RefArray;
    typedef Container::WithCopyConstructor<Type::VarT<Type::ObjectCopyPolicy> >::Array VarArray;

    typedef Container::WithCopyConstructor<Strings::FastString>::Array StringArray;

    RegisterClassForVariantDecl(VarEmpty)
    RegisterClassForVariantDecl(RefEmpty)
    RegisterClassForVariantDecl(Strings::FastString)
    RegisterClassForVariantDecl(NamedFunc);
    RegisterClassForVariantDecl(NamedFuncRef);
    RegisterClassForVariantDecl(DynObj);
    RegisterClassForVariantDecl(RefObj);
    RegisterClassForVariantDecl(VarArray);
    RegisterClassForVariantDecl(RefArray);
    RegisterClassForVariantDecl(GetterSetter);
    RegisterClassForVariantDecl(GetterSetterRef);


#ifndef DontHaveDatabaseCode
    // Database stuff is here
    #include "../../include/Database/Database.hpp"
    RegisterClassForVariantDecl(Database::Index)
    RegisterClassForVariantDecl(Database::LongIndex)
    //RegisterClassForVariantDecl(Database::ReferenceBase)
    //RegisterClassForVariantDecl(Database::TableDefinition)
    RegisterClassForVariantDecl(Database::UnescapedString)
    RegisterClassForVariantDecl(Database::Blob)

    RegisterClassForVariantDecl(Database::NotNullString)
    RegisterClassForVariantDecl(Database::NotNullUniqueString)
    RegisterClassForVariantDecl(Database::NotNullInt)
    RegisterClassForVariantDecl(Database::NotNullUnsigned)
    RegisterClassForVariantDecl(Database::NotNullLongInt)
    RegisterClassForVariantDecl(Database::NotNullUnsignedLongInt)
    RegisterClassForVariantDecl(Database::NotNullDouble)
#endif

    RegisterClassForVariantDecl(StringArray)

#endif // !DOXYGEN

#endif
