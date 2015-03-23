#ifndef hpp_UTI_hpp
#define hpp_UTI_hpp

// Need general type definitions
#include "../Types.hpp"
// Need Data Source declarations
#include "DataSource.hpp"
// We need AVL tree to store UIDs
#include "../Tree/AVL.hpp"



/** The universal type identifier - should be the same on all platform
    They are based stored on 128 bits
    @warning The object file compiled from the CPP file including UTIImpl.hpp must be the first in the link order */
namespace UniversalTypeIdentifier
{
    /** Runtime interface */
    struct ModifiableTypeID
    {
        virtual const uint32 getID1() const = 0;
        virtual const uint32 getID2() const = 0;
        virtual const uint32 getID3() const = 0;
        virtual const uint32 getID4() const = 0;
        inline bool isEqual(ModifiableTypeID const * other) const { if (other) return getID1() == other->getID1() && getID2() == other->getID2() && getID3() == other->getID3() && getID4() == other->getID4(); return false; }
        virtual ~ModifiableTypeID(){}
    };
    typedef const ModifiableTypeID  ConstTypeID;
    typedef ConstTypeID *           TypeID;

    /** Declare the Comparable policy for the type ID */
    struct TypeIDComparator
    {
        /** TypeID comparators */
        inline static bool LessThan(TypeID a, TypeID b)
        {
            return (bool)
                (a->getID1() == b->getID1() ?
                    (a->getID2() == b->getID2() ?
                        (a->getID3() == b->getID3() ?
                            (a->getID4() == b->getID4() ?
                                false
                                : a->getID4() < b->getID4())
                            : a->getID3() < b->getID3())
                        : a->getID2() < b->getID2())
                    : a->getID1() < b->getID1()
                );
        }
        inline static bool Equal(TypeID a, TypeID b) { return (bool)(a->getID1() == b->getID1() && a->getID2() == b->getID2() && a->getID3() == b->getID3() && a->getID4() == b->getID4()); }
    };

    /** Generic implementation for a TypeID
        This should be used as a reference for manual implementation.
        The implemented behavior here is to not compile
        (there is no default constructor and const members)
    */
    template <typename T>
    struct TypeIDImpl : public ModifiableTypeID
    {
        const uint32  &  ID1;
        const uint32  &  ID2;
        const uint32  &  ID3;
        const uint32  &  ID4;

        typedef Type::Var type;

        /** Runtime interface */
        const uint32 getID1() const { return ID1; }
        const uint32 getID2() const { return ID2; }
        const uint32 getID3() const { return ID3; }
        const uint32 getID4() const { return ID4; }

        // If the compilation stops here, it's because you must specialize the type ID for the type you are using
        // If it breaks for usual type, you've probably forgotten to include UTIImpl.hpp
    private:
        TypeIDImpl();
    };
    
    /** The type ID parser */
    class TypeIDParser
    {
        // Members
    private:
        uint32 ID1, ID2, ID3, ID4;

        // Construction and destruction
    private:
        /** Private default constructor with identifiers */
        TypeIDParser(const uint32 _ID1 = 0, const uint32 _ID2 = 0, const uint32 _ID3 = 0, const uint32 _ID4 = 0) : ID1(_ID1), ID2(_ID2), ID3(_ID3), ID4(_ID4) {}

    public:
        /** The minimal TypeID length in a string */
        enum { MinimumTypeIDLength = 35 };
        /** The only allowed construction is through char array */
        TypeIDParser(const char * typeName) : ID1(0), ID2(0), ID3(0), ID4(0)
        {
            if (typeName && strlen(typeName) == 35)
            {
                if (sscanf(typeName, "%08X-%08X-%08X-%08X", &ID1, &ID2, &ID3, &ID4) != 4)
                    ID1 = ID2 = ID3 = ID4 = 0;
            }
        }
        /** Or other TypeID */
        TypeIDParser(TypeID type) : ID1(type ? type->getID1() : 0), ID2(type ? type->getID2() : 0), ID3(type ? type->getID3() : 0), ID4(type ? type->getID4() : 0) {}
        /** Save the current TypeID identifier to a char array */
        inline bool saveTo(char * typeName, const uint32 typeSize)
        {
            if (typeSize < 36 || !typeName) return false;
            return sprintf(typeName, "%08X-%08X-%08X-%08X", ID1, ID2, ID3, ID4) > 0;
        }
        /** Check if the read TypeID is valid */
        inline bool isValid() { return !(ID1 == 0 && ID2 == 0 && ID3 == 0 && ID4 == 0); }
        /** Check if the parsed type is the same as the one given */
        inline const bool operator == (TypeID type) const { if (!type) return false; return type->getID1() == ID1 && type->getID2() == ID2 && type->getID3() == ID3 && type->getID4() == ID4; }
        /** Check if the parsed type is the same as the one given */
        inline const bool operator == (const char * typeName) const { if (!typeName) return false; TypeIDParser other(typeName); return ID1 == other.ID1 && ID2 == other.ID2 && ID3 == other.ID3 && ID4 == other.ID4; }
    };
    

    /** Qualifier cleaning template
        @cond Private
    */
    namespace Private
    {
        // Type1 has sizeof != sizeof(Type2)
        typedef char Type1;
        typedef struct { char some[2]; } Type2;

        // Our type differentiator
        Type1 isConst(const volatile void *);
        Type2 isConst(volatile void *);
        template<typename A, typename B>
        struct isSame { };

        template<typename A>
        struct isSame<A, A> { typedef void type; };

        template<typename> struct toVoid { typedef void type; };

        template<typename T, typename = void>
        struct isClass { enum { Result = false }; };

        template<typename C>
        struct isClass<C, typename toVoid<int C::*>::type> { enum { Result = true }; };

        template<typename T>
        struct isFloat { enum { Result = false }; typedef Type2 Type;  };

        template<> struct isFloat<float> { enum { Result = true }; typedef Type1 Type; };
        template<> struct isFloat<double> { enum { Result = true }; typedef Type1  Type; };
        template<> struct isFloat<long double> { enum { Result = true }; typedef Type1 Type; };

        // Check for enum type, typically anything that's not a class, function, reference, array, but can be converted to int
        template<typename E, typename = void>
        struct isEnum
        {
            struct nullsink {};
            static Type2 checkI(nullsink*); // accept null pointer constants
            static Type1 checkI(...);

            static Type1 checkE(int);
            static Type2 checkE(...);

            static Type2 checkFISink(const Type1 * );
            static Type1 checkFISink(const Type2 * );

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4244)
#endif
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion-null"
#endif

            enum { Result = sizeof(checkI(E())) == sizeof(Type1) 
                         && sizeof(checkFISink((typename isFloat<E>::Type*)0)) == sizeof(Type1) 
                         && sizeof(checkE(E())) == sizeof(Type1) };
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

        };

        // class
        template<typename E>
        struct isEnum<E, typename toVoid<int E::*>::type> { enum { Result = false }; };

        // reference
        template<typename R>
        struct isEnum<R&, void> { enum { Result = false }; };

        // function (FuntionType() will error out).
        template<typename F>
        struct isEnum<F, typename isSame<void(F), void(F*)>::type> { enum { Result = false }; };

        // array (ArrayType() will error out)
        template<typename E>
        struct isEnum<E[], void> { enum { Result = false }; };
        template<typename E, int N>
        struct isEnum<E[N], void> { enum { Result = false }; };
	}
	/** @endcond*/

    // Check for enum type, typically anything that's not a class, function, reference, array, but can be converted to int
    template<typename E, typename V = void>
    struct IsEnum
    {
        enum { Result = Private::isEnum<E, V>::Result };
    };

    // Extract and test for enum value
    template<typename E, typename V = void>
    class IfEnum
    { 
        struct ImpossibleTypeToInstantiate {};
        template <bool, int N> struct Test { typedef E Type; };
        template <int N> struct Test<false, N> { typedef ImpossibleTypeToInstantiate Type; };
    public:
        typedef typename Test< IsEnum<E, V> :: Result, 0> :: Type Type;
    };

    template <typename T>
    struct IsConst
    {

        // Avoid building T
        // (this will simply avoid compile error when T's constructor is private)
        static T* t;

        // The selector
        enum { Result = (sizeof(Private::Type1) == sizeof(Private::isConst( t )) )};
    };

    template <typename T>
    struct WithoutConst
    {
        struct PleaseDontTryToRegisterConstType;
        typedef PleaseDontTryToRegisterConstType type;
    };

    template <typename T>
    struct WarpType
    {
        typedef T type;
    };

    /** The binder static function that create the type ID at compile time. */
    template <typename T>
        // If compilation breaks here it is because you are trying to get a type id of a const type
        // Try without const
        TypeID getTypeIDImpl(T*, Bool2Type< true > * );
    template <typename T>
//      If compilation break here, it is because you've forgotten to add this
//      in your code (must be in global namespace):
//        RegisterClassForVariant(YourClassHere, SomeUniqueID1, SomeUniqueID2, SomeUniqueID3, SomeUniqueID4);
//        RegisterClassFunctions(YourClassHere, GetCode, SetCode);
        TypeID getTypeIDImpl(T*, Bool2Type< false > * );
    
    // If you get linker error related to getTypeIDImpl, it can be hard to spot where the given type is referenced in your code
    // In that case, you can use this macro, on the FIRST line of the file to force the compiler to break on a specific implementation type T
    // So, for example, if you get Undefined reference error for type A in file X.o
    // Put this at the very top of the X.cpp file:
    // #include "path/to/UTI.hpp"
    // PreventTypeIDImpl(A)
    // Then compile. This will result in compiler noise, from which you extract the location where the symbol is being used (in the previous example,
    // it gives:
    // path/to/X.cpp:580:80: note: in instantiation of function template specialization 'Type::VarT<Type::ObjectCopyPolicy>::extractIf<A>' requested here
    //                        Reference * _ref = initialiser->getResult(environment).extractIf((A*)0);
#define PreventTypeIDImpl(T) \
    namespace UniversalTypeIdentifier { void getTypeID(T *); }
#define PreventTypeIDImplForwardDecl(N,type,T) \
    namespace N { type T; } \
    namespace UniversalTypeIdentifier { void getTypeID(N::T *); }
    
    
    template <typename T>
    static TypeID getTypeID(T* t) { return getTypeIDImpl(t, (Bool2Type< IsConst<T>::Result > *)0); }

    enum { PODBaseID = 0x00000000, };


    /** An item in the type factory */
    struct CreationMethods
    {
        /** This method create the default object as a Var variable */
        Type::Var * (*createDefaultObject)(void);
        /** Return the Object's Universal Type Identifier */
        TypeID      (*registerObjectUTI)(void);
        /** Get the data source for this object */
        Type::DataSource* (*getDataSource)(const void *);
        /** Set the data source for this object */
        void        (*setDataSource)(Type::DataSource *, void *);
        /** Return the type name */
        const char * (*getTypeName)(void);

        CreationMethods(Type::Var * (*pFunc1)(), TypeID (*pFunc2)(), Type::DataSource * (*pFunc3)(const void *), void (*pFunc4)(Type::DataSource *, void*), const char* (*pFunc5)()) : createDefaultObject(pFunc1), registerObjectUTI(pFunc2), getDataSource(pFunc3), setDataSource(pFunc4), getTypeName(pFunc5) {}
        CreationMethods(const CreationMethods & xCM) : createDefaultObject(xCM.createDefaultObject), registerObjectUTI(xCM.registerObjectUTI), getDataSource(xCM.getDataSource), setDataSource(xCM.setDataSource), getTypeName(xCM.getTypeName) {}
    };

    typedef const CreationMethods   ConstCreationMethods;
    typedef ConstCreationMethods *  PCreationMethods;

    struct ConstCreationMethodsDeleter
    {
        /** Used for cleaning const object */
        inline static void deleter(PCreationMethods pObj, TypeID tID)  { delete const_cast<CreationMethods *>(pObj); delete const_cast<ModifiableTypeID*>(tID); }
    };


    template <typename T>
    struct CMImpl : public CreationMethods
    {
    };


    /** The data source type definition */
    typedef Type::DataSource * (*PGetDataSourceFunc)(const void *);
    typedef void (*PSetDataSourceFunc)(Type::DataSource *, void *);

    /** The factory itself */
    class TypeFactory
    {
    private:
        /** Private shortcut to the tree type */
        typedef Tree::AVL::Tree<PCreationMethods, TypeID, TypeIDComparator, ConstCreationMethodsDeleter>    TreeT;
        /** The factory holder */
        TreeT   mlTree;


    public:
        /** Not found enum */
        enum { NotFound = 0 };

        /** Add a type to the factory
            @return true on success, false if already exists */
        bool registerType(TypeID typeID, PCreationMethods pMethod)
        {
            return mlTree.insertObject(pMethod, typeID);
        }

        /** Find a type from the factory
            @return The creation methods or NULL on failure */
        inline PCreationMethods findType(TypeID typeID) const
        {
            // Find the object in the tree
            TreeT::IterT iter = mlTree.searchFor(typeID);
            if (iter.isValid()) return (PCreationMethods)*iter;
            return NULL;
        }

        /** Check if this type is already registered.
            Compile-time version */
        template <typename T>
            inline bool isRegistered(T* t) const { return isRegistered(UniversalTypeIdentifier::getTypeID(t)); }

        /** Check if this type is already registered.
            Run-time version */
        inline bool isRegistered(TypeID typeID) const { return (findType(typeID) != (PCreationMethods)NotFound);    }

        /** Get the type name.
            Run-time version */
        inline const char * getTypeName(TypeID typeID) const { PCreationMethods ptr = findType(typeID); return ptr ? (*ptr->getTypeName)() : ""; }

        /** Return the data source export and import function */
        inline PGetDataSourceFunc getDataSourceOutFunc(TypeID typeID) const
        {
            PCreationMethods pxCM = findType(typeID);
            if (pxCM == NULL) return NULL;
            return pxCM->getDataSource;
        }

        /** Return the data source export and import function */
        inline PSetDataSourceFunc getDataSourceInFunc(TypeID typeID) const
        {
            PCreationMethods pxCM = findType(typeID);
            if (pxCM == NULL) return NULL;
            return pxCM->setDataSource;
        }

    };

    /** The factory singleton */
    extern TypeFactory * getTypeFactory();

    /** The automatic register object at startup */
    struct AutoRegister
    {
        PCreationMethods mpCM;
#ifdef DelayTypeRegistering
        // Debug here
        // Delay registering
        static Stack::PlainOldData::FIFO<PCreationMethods> mlFifo;

        AutoRegister(const PCreationMethods & pCM) : mpCM(pCM) { mlFifo.Push(pCM); }
        // Register all the pending registration
        static void registerAllTypeAtOnce()
        {
            while (mlFifo.getSize())
            {
                const PCreationMethods & pCM = mlFifo.Pop();
                getTypeFactory()->registerType(pCM->registerObjectUTI(), pCM);
            }
        }
#else
        AutoRegister(const PCreationMethods & pCM) : mpCM(pCM) { getTypeFactory()->registerType(pCM->registerObjectUTI(), pCM); }
#endif
//      ~AutoRegister() { delete const_cast<CreationMethods*>(mpCM); }
    };

/* You'll need to enable this if you want to delay type registering
namespace UniversalTypeIdentifier
{
#ifdef DelayTypeRegistering
    Stack::PlainOldData::FIFO<PCreationMethods> AutoRegister::mlFifo;
#endif
}
*/

}

#endif
