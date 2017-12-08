// Include helper code
#include "UTIImpl.inc"

namespace Type
{
    /** Use this if you want to pass default parameters to functions */
    VarT<ObjectCopyPolicy> EmptyVar;
    /** Use this if you want to pass default parameters to functions */
    VarT<ObjectPtrPolicy> EmptyRef;
    /** The default error handling */
    ErrorCallback defaultHandling;
#if (WantDynamicEngine == 1)
    /** The default error on throwing */
    DynObj::ThrowOnError defaultThrow;
#endif
}

namespace UniversalTypeIdentifier
{
    // We use string from FastString lib here
    typedef Strings::FastString String;
    using namespace Type;

    TypeFactory * getTypeFactory() { static TypeFactory theUniqueFactoryInstance; return &theUniqueFactoryInstance; }
    template <typename T>
        // If compilation breaks here it is because you are trying to get a type id of a const type
        // Try without const
        TypeID getTypeIDImpl(T*, Bool2Type< true > * ) { static TypeIDImpl< typename WithoutConst<T>::type > staticType; return &staticType; }
    template <typename T>
//      If compilation break here, it is because you've forgotten to add this
//      in your code (must be in global namespace):
//        RegisterClassForVariant(YourClassHere, SomeUniqueID1, SomeUniqueID2, SomeUniqueID3, SomeUniqueID4);
//        RegisterClassFunctions(YourClassHere, GetCode, SetCode);
        TypeID getTypeIDImpl(T*, Bool2Type< false > * ) { typedef typename WarpType<T>::type type; static TypeIDImpl< type > staticType; return &staticType; }

    /** Some specializations for plain old data
        Those types never exists in run time (they are only compile time based)
        They don't take any space.

        If they need to be exported, use the specialized POD2Type template
    */
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
    MakePODHolder(long long,          PF_LLD, 0x00000007);
    MakePODHolder(unsigned long long, PF_LLU, 0x00000008);
#endif
#if !defined(_MAC)
    MakePODHolder(int64,          PF_LLD, 0x00000009);
    MakePODHolder(uint64,         PF_LLU, 0x0000000A);
#endif
    MakePODHolder(void *,          "%p",  0x0000000B);
    MakePODHolder(double,          "%lf", 0x0000000C);
    MakePODHolder(long double,     "%Lf", 0x0000000D);
    MakePODHolder(float,           "%f",  0x0000000E);
    MakePODHolder(bool,            "%d",  0x0000000F);
#undef MakePODHolder

}


/** Needed for registration */ 
/*
RegisterClassForVariant(::VarT<ObjectCopyPolicy>::Empty , 0x00000000, 0x00000000, 0x00000001, 0x00000000)
RegisterClassFunctions(::VarT<ObjectCopyPolicy>::Empty  , ("<Value></Value>"), ; )
RegisterClassForVariant(::VarT<ObjectPtrPolicy>::Empty  , 0x00000000, 0x00000000, 0x00000002, 0x00000000)
RegisterClassFunctions(::VarT<ObjectPtrPolicy>::Empty   , ("<Value></Value>"), ; )
RegisterClassForVariant(Bstrlib::String                 , 0x00000000, 0x00000000, 0x00000000, 0xc34def32)
RegisterClassFunctions(Bstrlib::String  , ("<Value>%s</Value>", (const char *)*(Bstrlib::String*)pData), *(Bstrlib::String*)pData = sSrc )

// Database stuff is here
#include "Database.hpp"
RegisterClassForVariant(Database::Index, 0x00000000, 0x00000000, 0x00000000, 0xf4e3de23)
RegisterClassFunctions(Database::Index, ("<Value>%u</Value>", ((Database::Index*)pData)->index), sSrc.Scan("%u", &((Database::Index*)pData)->index))
RegisterClassForVariant(Database::ReferenceBase, 0x00000000, 0x00000000, 0x00000000, 0xc3e5dd01)
RegisterClassFunctions(Database::ReferenceBase, ("<Value>%u</Value>", ((Database::ReferenceBase*)pData)->ref), sSrc.Scan("%u", &((Database::ReferenceBase*)pData)->ref))
RegisterClassForVariant(Database::TableDefinition*, 0x00000000, 0x00000000, 0x00000000, 0xdeadf00d)
RegisterClassFunctions(Database::TableDefinition*, ("<Value></Value>"), Throw(ConversionNotAllowed()); )
  
*/
#ifndef DOXYGEN

    typedef Type::VarT<Type::ObjectCopyPolicy>::Empty VarEmpty;
    typedef Type::VarT<Type::ObjectPtrPolicy>::Empty RefEmpty;

    typedef Container::WithCopyConstructor<Strings::FastString>::Array StringArray;

    RegisterClassForVariantImpl(VarEmpty , 0x00000000, 0x00000000, 0x00000001, 0x00000000, ("<Value></Value>"), ; )
    RegisterClassForVariantImpl(RefEmpty , 0x00000000, 0x00000000, 0x00000002, 0x00000000, ("<Value></Value>"), ; )
    RegisterClassForVariantImpl(Type::NamedFunc             , 0x00000000, 0x00000000, 0x00000003, 0x00000000, ("<Value>[native func at %p]</Value>", (void*)pData), sSrc.fromFirst("at ").Scan("%p", &pData) )
    RegisterClassForVariantImpl(Type::NamedFuncRef          , 0x00000000, 0x00000000, 0x00000003, 0x00000001, ("<Value>[native func at %p]</Value>", (void*)pData), sSrc.fromFirst("at ").Scan("%p", &pData) )
    RegisterClassForVariantImpl(Strings::FastString         , 0x00000000, 0x00000000, 0x00000000, 0xc34def32, ("<Value>%s</Value>", (const char *)*(Strings::FastString*)pData), *(Strings::FastString*)pData = sSrc)
#if (WantDynamicEngine == 1)
    RegisterClassForVariantImpl(Type::DynObj                , 0x00000000, 0x00000000, 0x00000000, 0x0b3ec1d1, ("<Value></Value>"), ; )
    RegisterClassForVariantImpl(Type::RefObj                , 0x00000000, 0x00000000, 0x00000000, 0x0b3ec1d2, ("<Value></Value>"), ; )
#endif
    RegisterClassForVariantImpl(Type::GetterSetter          , 0x00000000, 0x00000000, 0x00000003, 0x00000002, ("<Value>[native gs at %p,%p]</Value>", ((GetterSetter*)pData)->getter, ((GetterSetter*)pData)->setter), sSrc.fromFirst("at ").Scan("%p", &((GetterSetter*)pData)->getter); sSrc.fromFirst(",").Scan("%p", &((GetterSetter*)pData)->setter) )
    RegisterClassForVariantImpl(Type::GetterSetterRef       , 0x00000000, 0x00000000, 0x00000003, 0x00000003, ("<Value>[native gs at %p,%p]</Value>", ((GetterSetterRef*)pData)->getter, ((GetterSetterRef*)pData)->setter), sSrc.fromFirst("at ").Scan("%p", &((GetterSetterRef*)pData)->getter); sSrc.fromFirst(",").Scan("%p", &((GetterSetterRef*)pData)->setter) )

#if WantDatabase == 1
    // Database stuff is here
    #include "../../include/Database/Database.hpp"
    RegisterClassForVariantImpl(Database::Index, 0x00000000, 0x00000000, 0x00000000, 0xf4e3de23, ("<Value>%u</Value>", ((Database::Index*)pData)->index), sSrc.Scan("%u", &((Database::Index*)pData)->index))
    RegisterClassForVariantImpl(Database::LongIndex, 0x00000000, 0x00000000, 0x00000000, 0xf4e3de24, ("<Value>" PF_LLU "</Value>", ((Database::LongIndex*)pData)->index), sSrc.Scan(PF_LLU, &((Database::LongIndex*)pData)->index))
    //RegisterClassForVariantImpl(Database::ReferenceBase, 0x00000000, 0x00000000, 0x00000000, 0xc3e5dd01, ("<Value>%u</Value>", ((Database::ReferenceBase*)pData)->ref->like((unsigned int)0)), unsigned int u; sSrc.Scan("%u", &u); ((Database::ReferenceBase*)pData)->ref = u )
    //RegisterClassForVariantImpl(Database::TableDefinition*, 0x00000000, 0x00000000, 0x00000000, 0xdeadf00d, ("<Value></Value>"), Throw(ConversionNotAllowed()) )
    RegisterClassForVariantImpl(Database::UnescapedString , 0x00000000, 0x00000000, 0x00000000, 0xc34def33, ("<Value>%s</Value>", (const char *)*(Database::UnescapedString*)pData), *(Strings::FastString*)pData = sSrc)
    RegisterClassForVariantImpl(Database::Blob , 0x00000000, 0x00000000, 0x00000000, 0xc34def35, ("<Value>"); Database::SQLFormat::serializeBlob((Database::Blob*)pData, sSrc); sSrc+= "</Value>", Database::SQLFormat::unserializeBlob((Database::Blob*)pData, sSrc))

    RegisterClassForVariantImpl(Database::NotNullString     , 0x00000000, 0x00000000, 0x00000007, 0xc34def32, ("<Value></Value>"), Throw(ConversionNotAllowed()) )
    RegisterClassForVariantImpl(Database::NotNullUniqueString, 0x00000000, 0x00000000, 0x00000007, 0xc34def34, ("<Value></Value>"), Throw(ConversionNotAllowed()) )
    RegisterClassForVariantImpl(Database::NotNullInt        , 0x00000000, 0x00000000, 0x00000007, 0x00000005, ("<Value></Value>"), Throw(ConversionNotAllowed()) )
    RegisterClassForVariantImpl(Database::NotNullUnsigned   , 0x00000000, 0x00000000, 0x00000007, 0x00000006, ("<Value></Value>"), Throw(ConversionNotAllowed()) )
    RegisterClassForVariantImpl(Database::NotNullLongInt    , 0x00000000, 0x00000000, 0x00000007, 0x00000009, ("<Value></Value>"), Throw(ConversionNotAllowed()) )
    RegisterClassForVariantImpl(Database::NotNullUnsignedLongInt, 0x00000000, 0x00000000, 0x00000007, 0x0000000A, ("<Value></Value>"), Throw(ConversionNotAllowed()) )
    RegisterClassForVariantImpl(Database::NotNullDouble     , 0x00000000, 0x00000000, 0x00000007, 0x0000000C, ("<Value></Value>"), Throw(ConversionNotAllowed()) )
#endif

    RegisterClassForVariantImpl(StringArray                 , 0x00000000, 0x00000000, 0x00000008, 0xc34def32, ("<Value>"); for(uint32 iter = 0; iter < ((StringArray*)pData)->getSize(); iter++) sSrc+="<l>"+((*(StringArray*)pData)[iter])+"</l>"; sSrc+="</Value>", \
                                ((StringArray*)pData)->Clear(); while(sSrc.getLength()) {sSrc = sSrc.fromFirst("<l>"); (*(StringArray*)pData).Append(sSrc.upToFirst("</l>")); sSrc = sSrc.fromFirst("</l>"); })
    RegisterClassForVariantImpl(VarArray                    , 0x00000000, 0x00000000, 0x00000009, 0x00000000, ("<Value>"); for(uint32 iter = 0; iter < ((VarArray*)pData)->getSize(); iter++) \
                                { sSrc+="<l>"; DataSource * lds = ((VarArray*)pData)->getElementAtUncheckedPosition(iter).getDataSource(); Strings::FastString tmp; if (lds) lds->getValue().extractTo(tmp); sSrc+=tmp; sSrc+= "</l>"; delete lds; } sSrc+="</Value>", \
                                ((VarArray*)pData)->Clear(); while(sSrc.getLength()) {sSrc = sSrc.fromFirst("<l>"); Var tmp; tmp.setDataSource(new TextDataSource(sSrc.upToFirst("</l>"))); (*(VarArray*)pData).Append(tmp); sSrc = sSrc.fromFirst("</l>"); })
    RegisterClassForVariantImpl(RefArray                    , 0x00000000, 0x00000000, 0x0000000A, 0x00000000, ("<Value>"); for(uint32 iter = 0; iter < ((RefArray*)pData)->getSize(); iter++) \
                                { sSrc+="<l>"; DataSource * lds = ((RefArray*)pData)->getElementAtPosition(iter).getDataSource(); Strings::FastString tmp; if (lds) lds->getValue().extractTo(tmp); sSrc+=tmp; sSrc+= "</l>"; delete lds; } sSrc+="</Value>", \
                                ((RefArray*)pData)->Clear(); while(sSrc.getLength()) {sSrc = sSrc.fromFirst("<l>"); Ref * tmp = new Ref; if (!tmp) break; tmp->setDataSource(new TextDataSource(sSrc.upToFirst("</l>"))); (*(RefArray*)pData).Append(tmp); sSrc = sSrc.fromFirst("</l>"); })
#endif // !DOXYGEN
