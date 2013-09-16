// Include the variant type declaration
#include "../../include/Variant/UTIImpl.hpp"


namespace Type
{
    /** Use this if you want to pass default parameters to functions */
    VarT<ObjectCopyPolicy> EmptyVar;
    /** Use this if you want to pass default parameters to functions */
    VarT<ObjectPtrPolicy> EmptyRef;
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

    class TextDataSource : public DataSource
    {
    private:
        String     sourceHolder;

    public:
        void        setValue(const Var & var) { var.extractTo(sourceHolder); }
        Var         getValue() const          { Var var(sourceHolder); return var; }
        TextDataSource(const String & source) : sourceHolder(source) {}
    };
    /** Some specializations for plain old data
        Those types never exists in run time (they are only compile time based)
        They don't take any space.

        If they need to be exported, use the specialized POD2Type template
    */
    /** The macro below avoid making error while copy & pasting */

#define MakePODHolder(SType, As, Value) \
    template <> struct TypeIDImpl<SType> : public ModifiableTypeID  { enum { ID1 = PODBaseID, ID2 = PODBaseID, ID3 = PODBaseID, ID4 = Value }; typedef SType Type; TypeIDImpl(); \
	virtual const uint32 getID1() const { return ID1; } \
	virtual const uint32 getID2() const { return ID2; } \
	virtual const uint32 getID3() const { return ID3; } \
	virtual const uint32 getID4() const { return ID4; } }; \
    template <> struct CMImpl<SType> : public CreationMethods       { static Var * createDefaultObject() { return new Var(( SType ) 0); } static TypeID registerObjectUTI(void) { return new TypeIDImpl<SType>(); } static DataSource* getDataSource(const void * pData)\
    {   String sSrc;\
        sSrc.Format("<Former>\n\t<Type>%s</Type>\n\t<Value>" As "</Value>\n</Former>", CMImpl<SType> :: getTypeName(), *(SType*)pData);\
        return new TextDataSource(sSrc); \
    }\
    static void setDataSource(DataSource* pDS, void * pData) \
    {   Var var = pDS->getValue(); String sSrc; var.extractTo(sSrc); \
        int pos = sSrc.Find("<Value>"); *(SType*)pData = (SType)0; if (pos != -1) { sSrc = sSrc.midString(pos + 7, sSrc.Find("</Value>") - pos - 7); sSrc.Scan(As, pData); }; \
        delete pDS; \
    }\
    static const char * getTypeName(void) { return #SType; } CMImpl< SType >() : CreationMethods(CMImpl< SType >::createDefaultObject, CMImpl< SType >::registerObjectUTI, CMImpl< SType >::getDataSource, CMImpl< SType >::setDataSource, CMImpl< SType >::getTypeName) {} }; \
    TypeIDImpl<SType>::TypeIDImpl() {} \
    static AutoRegister autoR_##Value(new CMImpl<SType>()); \
    template <> TypeID getTypeIDImpl(SType *, Bool2Type< false > *) { static TypeIDImpl< SType > staticType; return &staticType; }




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


/** Macro used to register a class to the factory */
#define RegisterClassForVariant(TYPE, Id1, Id2, Id3, Id4) \
    namespace UniversalTypeIdentifier \
    {  \
        template <> struct TypeIDImpl< TYPE > : public ModifiableTypeID \
        { \
            enum { ID1 = Id1, ID2 = Id2, ID3 = Id3, ID4 = Id4 };  \
            typedef TYPE Type;  \
            const uint32 getID1() const { return ID1; }  \
            const uint32 getID2() const { return ID2; }  \
            const uint32 getID3() const { return ID3; }  \
            const uint32 getID4() const { return ID4; }  \
	    TypeIDImpl (); \
        }; \
        template <> struct CMImpl< TYPE > : public CreationMethods \
        { \
            static Var * createDefaultObject() { return new Var(0, *( TYPE * ) 0); }  \
            static TypeID registerObjectUTI(void) { return new TypeIDImpl<TYPE>(); } \
            static const char * getTypeName(void) { return #TYPE; } \
            static DataSource* getDataSource(const void *); \
            static void setDataSource(DataSource *, void *); \
            CMImpl< TYPE >() : CreationMethods(CMImpl< TYPE >::createDefaultObject, CMImpl< TYPE >::registerObjectUTI, CMImpl< TYPE >::getDataSource, CMImpl< TYPE >::setDataSource, CMImpl< TYPE >::getTypeName) {} \
        }; \
        static AutoRegister autoR_##Id1##_##Id2##_##Id3##_##Id4 (new CMImpl< TYPE > ()); \
        TypeIDImpl< TYPE >::TypeIDImpl() : ModifiableTypeID() { } \
        template <> TypeID getTypeIDImpl( TYPE * , Bool2Type< false > *) { static TypeIDImpl< TYPE > staticType; return &staticType; } \
    }

/** Macro used to auto create the type registering functions */
#define RegisterClassFunctions(TYPE, GET, SET) \
    namespace UniversalTypeIdentifier \
    {   \
        DataSource* CMImpl< TYPE >::getDataSource(const void * pData) \
        {   String sSrc;\
            sSrc.Format GET ; \
            return new TextDataSource(String("<Former>\n\t<Type>") + CMImpl<TYPE> :: getTypeName() + String("</Type>\n\t") + sSrc + "\n</Former>"); \
        }\
        void CMImpl< TYPE >::setDataSource(DataSource* pDS, void * pData) \
        {   Var var = pDS->getValue(); String sSrc; var.extractTo(sSrc); \
            int pos = sSrc.Find("<Value>"); if (pos != -1) { sSrc = sSrc.midString(pos + 7, sSrc.Find("</Value>") - pos - 7); SET ; } \
            delete pDS; \
        }\
    }

#define RegisterClassForVariantImpl(TYPE, Id1, Id2, Id3, Id4, GET, SET) \
    namespace UniversalTypeIdentifier \
    {  \
        template <> struct TypeIDImpl< TYPE > : public ModifiableTypeID \
        { \
            enum { ID1 = Id1, ID2 = Id2, ID3 = Id3, ID4 = Id4 };  \
            typedef TYPE Type;  \
            const uint32 getID1() const { return ID1; }  \
            const uint32 getID2() const { return ID2; }  \
            const uint32 getID3() const { return ID3; }  \
            const uint32 getID4() const { return ID4; }  \
            TypeIDImpl (); \
        }; \
        template <> struct CMImpl< TYPE > : public CreationMethods \
        { \
            static Type::VarT<Type::ObjectCopyPolicy> * createDefaultObject() { return new Type::VarT<Type::ObjectCopyPolicy>(0, *( TYPE * ) 0); }  \
            static TypeID registerObjectUTI(void) { return new TypeIDImpl<TYPE>(); } \
            static const char * getTypeName(void) { return #TYPE; } \
            static Type::DataSource* getDataSource(const void * pData) \
            {   String sSrc;\
                sSrc.Format GET ; \
                return new TextDataSource(String("<Former>\n\t<Type>") + CMImpl<TYPE> :: getTypeName() + String("</Type>\n\t") + sSrc + "\n</Former>"); \
            }\
            static void setDataSource(Type::DataSource* pDS, void * pData) \
            {   Type::Var var = pDS->getValue(); String sSrc; var.extractTo(sSrc); \
                int pos = sSrc.Find("<Value>"); if (pos != -1) { sSrc = sSrc.midString(pos + 7, sSrc.Find("</Value>") - pos - 7); try{ SET ; } catch(...) { delete pDS; throw; } } \
                delete pDS; \
            }\
            CMImpl< TYPE >() : CreationMethods(CMImpl< TYPE >::createDefaultObject, CMImpl< TYPE >::registerObjectUTI, CMImpl< TYPE >::getDataSource, CMImpl< TYPE >::setDataSource, CMImpl< TYPE >::getTypeName) {} \
        }; \
        static AutoRegister autoR_##Id1##_##Id2##_##Id3##_##Id4 (new CMImpl< TYPE > ()); \
        TypeIDImpl< TYPE >::TypeIDImpl() : ModifiableTypeID() { } \
        template <> TypeID getTypeIDImpl( TYPE * , Bool2Type< false > *) { static TypeIDImpl< TYPE > staticType; return &staticType; } \
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
RegisterClassFunctions(Database::TableDefinition*, ("<Value></Value>"), throw ConversionNotAllowed(); )
  
*/
#ifndef DOXYGEN

    typedef Type::VarT<Type::ObjectCopyPolicy>::Empty VarEmpty;
    typedef Type::VarT<Type::ObjectPtrPolicy>::Empty RefEmpty;

    typedef Container::WithCopyConstructor<Strings::FastString>::Array StringArray;

    RegisterClassForVariantImpl(VarEmpty , 0x00000000, 0x00000000, 0x00000001, 0x00000000, ("<Value></Value>"), ; )
    RegisterClassForVariantImpl(RefEmpty , 0x00000000, 0x00000000, 0x00000002, 0x00000000, ("<Value></Value>"), ; )
    RegisterClassForVariantImpl(Strings::FastString         , 0x00000000, 0x00000000, 0x00000000, 0xc34def32, ("<Value>%s</Value>", (const char *)*(Strings::FastString*)pData), *(Strings::FastString*)pData = sSrc)

#ifndef DontHaveDatabaseCode
    // Database stuff is here
    #include "../../include/Database/Database.hpp"
    RegisterClassForVariantImpl(Database::Index, 0x00000000, 0x00000000, 0x00000000, 0xf4e3de23, ("<Value>%u</Value>", ((Database::Index*)pData)->index), sSrc.Scan("%u", &((Database::Index*)pData)->index))
    RegisterClassForVariantImpl(Database::LongIndex, 0x00000000, 0x00000000, 0x00000000, 0xf4e3de24, ("<Value>" PF_LLU "</Value>", ((Database::LongIndex*)pData)->index), sSrc.Scan(PF_LLU, &((Database::LongIndex*)pData)->index))
    //RegisterClassForVariantImpl(Database::ReferenceBase, 0x00000000, 0x00000000, 0x00000000, 0xc3e5dd01, ("<Value>%u</Value>", ((Database::ReferenceBase*)pData)->ref->like((unsigned int)0)), unsigned int u; sSrc.Scan("%u", &u); ((Database::ReferenceBase*)pData)->ref = u )
    //RegisterClassForVariantImpl(Database::TableDefinition*, 0x00000000, 0x00000000, 0x00000000, 0xdeadf00d, ("<Value></Value>"), throw ConversionNotAllowed() )
    RegisterClassForVariantImpl(Database::UnescapedString , 0x00000000, 0x00000000, 0x00000000, 0xc34def33, ("<Value>%s</Value>", (const char *)*(Database::UnescapedString*)pData), *(Strings::FastString*)pData = sSrc)
    RegisterClassForVariantImpl(Database::Blob , 0x00000000, 0x00000000, 0x00000000, 0xc34def35, ("<Value>"); Database::SQLFormat::serializeBlob((Database::Blob*)pData, sSrc); sSrc+= "</Value>", Database::SQLFormat::unserializeBlob((Database::Blob*)pData, sSrc))

    RegisterClassForVariantImpl(Database::NotNullString     , 0x00000000, 0x00000000, 0x00000007, 0xc34def32, ("<Value></Value>"), throw ConversionNotAllowed() )
    RegisterClassForVariantImpl(Database::NotNullUniqueString, 0x00000000, 0x00000000, 0x00000007, 0xc34def34, ("<Value></Value>"), throw ConversionNotAllowed() )
    RegisterClassForVariantImpl(Database::NotNullInt        , 0x00000000, 0x00000000, 0x00000007, 0x00000005, ("<Value></Value>"), throw ConversionNotAllowed() )
    RegisterClassForVariantImpl(Database::NotNullUnsigned   , 0x00000000, 0x00000000, 0x00000007, 0x00000006, ("<Value></Value>"), throw ConversionNotAllowed() )
    RegisterClassForVariantImpl(Database::NotNullLongInt    , 0x00000000, 0x00000000, 0x00000007, 0x00000009, ("<Value></Value>"), throw ConversionNotAllowed() )
    RegisterClassForVariantImpl(Database::NotNullUnsignedLongInt, 0x00000000, 0x00000000, 0x00000007, 0x0000000A, ("<Value></Value>"), throw ConversionNotAllowed() )
    RegisterClassForVariantImpl(Database::NotNullDouble     , 0x00000000, 0x00000000, 0x00000007, 0x0000000C, ("<Value></Value>"), throw ConversionNotAllowed() )
#endif

    RegisterClassForVariantImpl(StringArray                 , 0x00000000, 0x00000000, 0x00000008, 0xc34def32, ("<Value>"); for(uint32 iter = 0; iter < ((StringArray*)pData)->getSize(); iter++) sSrc+="<l>"+((*(StringArray*)pData)[iter])+"</l>"; sSrc+="</Value>", \
                                while(sSrc.getLength()) {sSrc = sSrc.fromFirst("<l>"); (*(StringArray*)pData).Append(sSrc.upToFirst("</l>")); sSrc = sSrc.fromFirst("</l>"); })

#endif // !DOXYGEN

