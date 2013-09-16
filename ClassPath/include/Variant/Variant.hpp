#ifndef hpp_Variant_hpp
#define hpp_Variant_hpp

// We need string to make type conversion
#include "../Strings/Strings.hpp"
// We need placement new too
#include <new>
// Need Universal Type Identifier
#include "UTI.hpp"


/** Manipulate any type generically.
    This namespace main contribution is the variant type called Var (or Ref).
    Unlike the commonly found variant class, this class isn't limited in the type it can support.
    This is achieved with a very convenient "static virtual table" idea. This idea allow efficient 
    data storage (on the stack or on the heap depending on the stored size), and very high performance.
    <br>
    Also unlike other variant class, you can convert between any type provided the type you are using
    is registered in the UniversalTypeIdentifier table.
    <br>
    Using the variant is quite straightforward see VarT
    
    @warning The object file compiled from the CPP file including UTIImpl.hpp must be the first in the link order
*/
namespace Type
{
    /** The variant type interface declaration.

        This class can store unknown type in a type safe way.
        It can extract data if the type match directly (as fast as a pointer dereferencing), or 
        perform a conversion. 

        Conversion is usually called heavy.
        Any type you want to convert must register a DataSource class (used to serialize the type to XML).
        Then type are first converted to a DataSource and DataSource are converted back to the final desired type.


        Using this class is very straightforward:
        @code
        // Create a empty variant 
        Var a;
        // Store an int in the variant
        a = 3;
        // You can read it back in two way
        int i = 0;
        a.extractTo(i); // This will work as a is an int
        const int b = a.like(b); // This will work too a can be seen as an int

        // You can test variant type easily
        bool isDouble = a.isExactly((double*)0); // false, of course

        // It's not because it's an int that it can't be converted to a double
        double c = 0;
        // This will return false, and c will still be zero, a is an int, and can't be extracted to c
        a.extractTo(c);
        // But, using like, you'll call heavy conversion if the type doesn't match
        c = a.like(c); // c = 3.0 after conversion
        // You shouldn't write Var t = "text" as it's treated as a pointer, not a string
        Var t = (String)"3.45";
        // This will (heavy) convert y to an int
        int y = t.like(y);      // y = 3
        double z = t.like(z);   // z = 3.45

        // You can check if a variant is assigned
        t.isEmpty(); // false
        @endcode */
    template <typename Policy>
    class VarT
    {
	    // Variant requires String for datasource handling
	    typedef Strings::FastString String;

    public:
	    /** Is the variant empty ? */
	    struct Empty {};

	    /** Not constant exception */
	    struct NotConstException {};

        //	RegisterClassForVariant(Empty, 0x00000000, 0x00000000, 0x00000000, 0x00000000);
	    /** This is the maximum variant size before it is stored automatically in the heap */
	    enum { MaximumStaticSize = sizeof(long double) };

    private:
        /** Spot POD type */
        static bool _isPOD() { return true; }
        /** Spot non POD type */
        static bool _isNotPOD() { return false; } 

	    /** The union itself (to allow the static & dynamic behaviour) */
	    union InternalUnion
	    {
		    uint8	Buffer[MaximumStaticSize];
		    void *  Pointer;

		    /** This is not truly portable, we should ensure the alignment with a type traits in a ideal case */
		    long double used_for_alignment;
		    long used_for_alignmentInOtherCase;
	    };
	    
	    /** The operation on the unions */
	    struct VirtualTable 
	    {
		    UniversalTypeIdentifier::TypeID	        (*getTypeID)();
		    void*									(*getPointer)(InternalUnion&);
		    const void*								(*getConstPointer)(const InternalUnion&);
		    void									(*Destructor)(InternalUnion&);
		    void									(*Deleter)(InternalUnion&);
		    void									(*Clone)(InternalUnion&, const InternalUnion&);
		    DataSource *							(*getDataSource)(const InternalUnion&);
		    void									(*setDataSource)(DataSource *, InternalUnion&);
            bool                                    (*PODDiscriminant)();
	    };

    public:
    #ifndef _MSC_VER
	    /** Specialization for stack based storage */
	    template <typename T, bool U> 
	    struct VirtualTableImpl
	    {
		    /** Remember we are optimized */
		    enum { Optimized = true };

		    static UniversalTypeIdentifier::TypeID	getTypeID()			{ return UniversalTypeIdentifier::getTypeID((T*)0); }
		    static void* getPointer(InternalUnion& x)					{ return x.Buffer; } 
		    static const void* getConstPointer(const InternalUnion& x)	{ return x.Buffer; } 
		    static void  Destructor(InternalUnion& x)					{ Cast(x)->~T();  }
		    static void  Deleter(InternalUnion& x)						{ Destructor(x); x.Pointer = NULL; }
		    static void  Clone(InternalUnion& x, const InternalUnion& y){ new(x.Buffer) T(*Cast(y)); }

		    static T* Cast(InternalUnion& x)							{ return reinterpret_cast<T*>(x.Buffer); }
		    static const T* Cast(const InternalUnion& x)				{ return reinterpret_cast<const T*>(x.Buffer); }

		    static DataSource * getDataSource(const InternalUnion& x)		{ UniversalTypeIdentifier::PGetDataSourceFunc pFunc = UniversalTypeIdentifier::getTypeFactory()->getDataSourceOutFunc(getTypeID()); if (pFunc != NULL) return (*pFunc)(getConstPointer(x)); else return NULL;	}
		    static void setDataSource(DataSource * pxDS, InternalUnion& x)	{ UniversalTypeIdentifier::PSetDataSourceFunc pFunc = UniversalTypeIdentifier::getTypeFactory()->getDataSourceInFunc(getTypeID()); if (pFunc != NULL) (*pFunc)(pxDS, getPointer(x)); }
	    };

	    /** Specialization for pointer based storage */
	    template<typename T>
	    struct VirtualTableImpl<T, false > 
	    {
		    /** Remember we are optimized */
		    enum { Optimized = false };

		    static UniversalTypeIdentifier::TypeID	getTypeID()			{ return UniversalTypeIdentifier::getTypeID((T*)0); }
		    static void* getPointer(InternalUnion& x)					{ return x.Pointer; } 
		    static const void* getConstPointer(const InternalUnion& x)	{ return x.Pointer; } 
		    static void  Destructor(InternalUnion& x)					{ Cast(x)->~T();  }
		    static void  Deleter(InternalUnion& x)						{ delete(Cast(x)); }
		    static void  Clone(InternalUnion& x, const InternalUnion& y){ x.Pointer = new T(*Cast(y)); }

		    static T* Cast(InternalUnion& x)							{ return reinterpret_cast<T*>(x.Pointer); }
		    static const T* Cast(const InternalUnion& x)				{ return reinterpret_cast<const T*>(x.Pointer); }

		    static DataSource * getDataSource(const InternalUnion& x)		{ UniversalTypeIdentifier::PGetDataSourceFunc pFunc = UniversalTypeIdentifier::getTypeFactory()->getDataSourceOutFunc(getTypeID()); if (pFunc != NULL) return (*pFunc)(getConstPointer(x)); else return NULL;	}
		    static void setDataSource(DataSource * pxDS, InternalUnion& x)	{ UniversalTypeIdentifier::PSetDataSourceFunc pFunc = UniversalTypeIdentifier::getTypeFactory()->getDataSourceInFunc(getTypeID()); if (pFunc != NULL) (*pFunc)(pxDS, getPointer(x)); }
	    };  

	    /** Now, select which of the function best match for the given type */
	    template<typename T> 
	    static VirtualTable* getTable(T*)  
	    {
		    /** Selection is done here at compile time */
		    enum { Optimize = sizeof(T) <= MaximumStaticSize };

		    /** Okay, get the pointers */
		    static VirtualTable virtualTable = 
		    {
		        &VirtualTableImpl<T, Optimize >::getTypeID
		      , &VirtualTableImpl<T, Optimize >::getPointer
		      , &VirtualTableImpl<T, Optimize >::getConstPointer
		      , &VirtualTableImpl<T, Optimize >::Destructor
		      , &VirtualTableImpl<T, Optimize >::Deleter
		      , &VirtualTableImpl<T, Optimize >::Clone
		      , &VirtualTableImpl<T, Optimize >::getDataSource
		      , &VirtualTableImpl<T, Optimize >::setDataSource
              , (IsPOD<T>::result || UniversalTypeIdentifier::IsEnum<T>::Result) ? &VarT<Policy>::_isPOD : &VarT<Policy>::_isNotPOD

		    };
		    return & virtualTable;
	    }	

    #else
	    /** Specialization for stack based storage */
	    template <typename T> 
	    struct VirtualTableBigImpl
	    {
		    template <typename U> struct Inner;
		    template <> struct Inner< Bool2Type<true> >
		    {
			    static UniversalTypeIdentifier::TypeID	getTypeID()			{ return UniversalTypeIdentifier::getTypeID((T*)0); }
			    static void* getPointer(InternalUnion& x)					{ return x.Buffer; } 
			    static const void* getConstPointer(const InternalUnion& x)	{ return x.Buffer; } 
			    static void  Destructor(InternalUnion& x)					{ Cast(x)->~T();  }
			    static void  Deleter(InternalUnion& x)						{ Destructor(x); x.Pointer = NULL; }
			    static void  Clone(InternalUnion& x, const InternalUnion& y){ new(x.Buffer) T(*Cast(y)); }

			    static T* Cast(InternalUnion& x)							{ return reinterpret_cast<T*>(x.Buffer); }
			    static const T* Cast(const InternalUnion& x)				{ return reinterpret_cast<const T*>(x.Buffer); }

			    static DataSource * getDataSource(const InternalUnion& x)		{ UniversalTypeIdentifier::PGetDataSourceFunc pFunc = UniversalTypeIdentifier::getTypeFactory()->getDataSourceOutFunc(getTypeID()); if (pFunc != NULL) return (*pFunc)(getConstPointer(x)); else return NULL;	}
			    static void setDataSource(DataSource * pxDS, InternalUnion& x)	{ UniversalTypeIdentifier::PSetDataSourceFunc pFunc = UniversalTypeIdentifier::getTypeFactory()->getDataSourceInFunc(getTypeID()); if (pFunc != NULL) (*pFunc)(pxDS, getPointer(x)); }
		    };

		    template <> struct Inner< Bool2Type<false> >
		    {
			    static UniversalTypeIdentifier::TypeID	getTypeID()			{ return UniversalTypeIdentifier::getTypeID((T*)0); }
			    static void* getPointer(InternalUnion& x)					{ return x.Pointer; } 
			    static const void* getConstPointer(const InternalUnion& x)	{ return x.Pointer; } 
			    static void  Destructor(InternalUnion& x)					{ Cast(x)->~T();  }
			    static void  Deleter(InternalUnion& x)						{ delete(Cast(x)); }
			    static void  Clone(InternalUnion& x, const InternalUnion& y){ x.Pointer = new T(*Cast(y)); }

			    static T* Cast(InternalUnion& x)							{ return reinterpret_cast<T*>(x.Pointer); }
			    static const T* Cast(const InternalUnion& x)				{ return reinterpret_cast<const T*>(x.Pointer); }

			    static DataSource * getDataSource(const InternalUnion& x)		{ UniversalTypeIdentifier::PGetDataSourceFunc pFunc = UniversalTypeIdentifier::getTypeFactory()->getDataSourceOutFunc(getTypeID()); if (pFunc != NULL) return (*pFunc)(getConstPointer(x)); else return NULL;	}
			    static void setDataSource(DataSource * pxDS, InternalUnion& x)	{ UniversalTypeIdentifier::PSetDataSourceFunc pFunc = UniversalTypeIdentifier::getTypeFactory()->getDataSourceInFunc(getTypeID()); if (pFunc != NULL) (*pFunc)(pxDS, getPointer(x)); }
		    };
	    };

	    template<typename T> 
	    static VirtualTable* getTable(T*)  
	    {
		    /** Selection is done here at compile time */
		    enum { Optimize = sizeof(T) <= MaximumStaticSize };

		    /** Okay, get the pointers */
		    static VirtualTable virtualTable = 
		    {
			    &VirtualTableBigImpl<T>::Inner< Bool2Type<Optimize> >::getTypeID
		      , &VirtualTableBigImpl<T>::Inner< Bool2Type<Optimize> >::getPointer
		      , &VirtualTableBigImpl<T>::Inner< Bool2Type<Optimize> >::getConstPointer
		      , &VirtualTableBigImpl<T>::Inner< Bool2Type<Optimize> >::Destructor
		      , &VirtualTableBigImpl<T>::Inner< Bool2Type<Optimize> >::Deleter
		      , &VirtualTableBigImpl<T>::Inner< Bool2Type<Optimize> >::Clone
		      , &VirtualTableBigImpl<T>::Inner< Bool2Type<Optimize> >::getDataSource
		      , &VirtualTableBigImpl<T>::Inner< Bool2Type<Optimize> >::setDataSource
              , (IsPOD<T>::result || UniversalTypeIdentifier::IsEnum<T>::Result) ? &VarT<Policy>::_isPOD : &VarT<Policy>::_isNotPOD
		    };
		    return & virtualTable;
	    }	
    #endif



	    // Interface
    public:
	    // Construction / Destruction
	    /** Default constructor */
	    VarT() : table(getTable((Empty*)0)) { inner.Pointer = NULL; }
	    /** String constructor (this is a hack, but a really useful hack) */
        VarT(const char * x) : table(getTable((Empty*)0)) { inner.Pointer = NULL; Init(Strings::FastString(x)); }
	    /** Value constructor */
	    template <typename T>
		    VarT(const T & x) : table(getTable((Empty*)0)) { inner.Pointer = NULL; Init(x); }
	    /** Value constructor */
	    template <typename T>
		    VarT(int, const T & x) : table(getTable((Empty*)0)) { inner.Pointer = NULL; InitDefault(x); }
	    /** Copy constructor */
	    VarT(const VarT & x) : table(getTable((Empty*)0)) { inner.Pointer = NULL; Set(x); }

	    /** Default destructor */
	    ~VarT()  { Reset();	}    

	    // Helpers methods 
    public:
	    /** Initialization to a specified type at runtime */
	    template<typename T>
	    void Init(const T& x) 
	    {
		    table = getTable((T*)0);
		    if (Policy::makeCopy)
		    {
			    if (sizeof(T) <= MaximumStaticSize)	new(inner.Buffer) T(x);
			    else 								inner.Pointer = new T(x); 
		    }
		    else inner.Pointer = reinterpret_cast<void*>(const_cast<T*>(&x));
	    }
	    /** Initialization to a specified type at runtime using default constructor */
	    template<typename T>
	    void InitDefault(const T& x) 
	    {
		    table = getTable((T*)0);
		    if (Policy::makeCopy)
		    {
			    if (sizeof(T) <= MaximumStaticSize)	new(inner.Buffer) T;
			    else 								inner.Pointer = new T; 
		    }
		    else inner.Pointer = reinterpret_cast<void*>(const_cast<T*>(&x));
	    }

	    VarT& Set(const VarT & x) 
	    {
		    Reset();				table = x.table;	  
		    if (Policy::makeCopy)	table->Clone(inner, x.inner);
		    else 					inner.Pointer = x.inner.Pointer;
		    return *this;
	    }

	    template<typename T>
	    inline VarT& operator=(const T& x)			{ return Set(VarT(x)); }
    /**
	    VarT& operator=(const char* x) 
	    {
		    return Set(VarT(cstring(x)));
	    }
    */
	    inline VarT& operator=(const VarT& x)	{ return Set(x);	}

    public:
	    /** Allow conversion to a datasource */
	    inline DataSource * getDataSource() const			{ return table->getDataSource(inner); }
	    /** Allow conversion to a datasource */
	    inline void setDataSource(DataSource * xDS) 		{ table->setDataSource(xDS, inner); }


	    /** Get the UTI (universal type ID) for the contained type */
	    inline UniversalTypeIdentifier::TypeID  getUTI() const { return table->getTypeID(); }
	    /** Check against a compile-time verifiable type */
	    template<typename T>
		    inline bool isExactly(T*) const { return getUTI()->isEqual(UniversalTypeIdentifier::getTypeID((T*)0));	}
	    /** Check against a run-time verifiable type */
	    inline bool isExactly(UniversalTypeIdentifier::TypeID  type) const { return getUTI()->isEqual(type); }

	    /** Try to implicitly convert the type 
	        @return false if the conversion doesn't apply (wanted type is different from internal type)
	    */	
	    template<typename T>
	    inline bool extractTo(T& out) const {	if (! this->isExactly((T*)0)) return false; toRef(out); return true; }

    //	template<typename T>
    //	inline bool convertTo(T*) {	if (! this->isExactly((T*)0)) return false; toRef(out); return true; }
	    
	    //	template<typename T>
    //	inline const T & like(const T &) const { if (!isExactly((T*)0)) throw NotConstException(); return *toPointer((const T*)0); }
	    /** Explicitly convert the type to the requested type 
	        If the requested type doesn't match the internal type, an heavy conversion is performed.
		    Heavy conversions requires creating a DataSource object dynamically and can fail
		    @throw NotConstException on failure
		    @return the converted and requested type
	    */
	    template<typename T>
	    inline const T like(const T *) const { if (!isExactly((T*)0)) return heavyConversionTo((T*)0); return *toPointer((const T*)0); }

	    template<typename T>
	    inline const T heavyConversionTo(T*) const { T temporary; VarT<Policy> tmp = temporary; tmp.setDataSource(getDataSource()); if (!tmp.extractTo(temporary)) throw NotConstException(); return temporary; }

        template<typename T>
	    inline bool convertInto(T& value) const { VarT<Policy> tmp = value; tmp.setDataSource(getDataSource()); return tmp.extractTo(value); }

	    template<typename T>
	    inline T* toPointer(T*) 
	    {	if (Policy::makeCopy)	return reinterpret_cast<T*>(table->getPointer(inner));
		    else 					return reinterpret_cast<T*>(inner.Pointer);	}

	    template<typename T>
	    inline const T* toPointer(const T *) const 
	    {	if (Policy::makeCopy)	return reinterpret_cast<const T*>(table->getConstPointer(inner));
		    else					return reinterpret_cast<const T*>(inner.Pointer);	}

	    template<typename T>
	    inline void toRef(T& t) 
	    {	if (Policy::makeCopy)	t = *reinterpret_cast<T*>(table->getPointer(inner));
		    else 					t = *reinterpret_cast<T*>(inner.Pointer);	}

	    template<typename T>
	    inline void toRef(const T & t) const 
	    {	if (Policy::makeCopy)	*const_cast<T*>(&t) = *reinterpret_cast<const T*>(table->getConstPointer(inner));
		    else					*const_cast<T*>(&t) = *reinterpret_cast<const T*>(inner.Pointer);	}


	    /** Check if this variant is currently empty */
	    inline bool isEmpty() const { return table == getTable((Empty*)0); }
        /** Check if this variant is a plain old data type (POD) */
        inline bool isPOD() const { return table->PODDiscriminant(); }

	    /** Reset the variant */
	    inline void Reset() 
	    {
		    if (isEmpty()) return; 
		    if (Policy::makeCopy)	table->Deleter(inner);
		    table = getTable((Empty*)0);
	    }

	    VarT* operator->() 
	    {
		    return this;
	    }

    // This operator has been removed because it is hazardous to use 
    #if 0
	    template <typename T>
		    operator T() { return like(*(T*)0);	}
    #endif

	    /** Container */
    private:
	    /** The type to object mapper functions */
	    VirtualTable * table;
	    /** The internal union */
	    InternalUnion inner;
    };

    struct ObjectPtrPolicy 
    {
	    enum { makeCopy = false }; 
    };

    struct ObjectCopyPolicy 
    {
	    enum { makeCopy = true }; 
    };

    /** Use this if you want to pass default variant parameters to functions */
    extern VarT<ObjectCopyPolicy> EmptyVar;
    /** Use this if you want to pass default variant parameters to functions */
    extern VarT<ObjectPtrPolicy> EmptyRef;
}

#endif
