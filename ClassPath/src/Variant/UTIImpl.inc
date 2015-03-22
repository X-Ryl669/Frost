// Include the variant type declaration
#include "../../include/Variant/UTIImpl.hpp"

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

