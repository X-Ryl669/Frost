#ifndef hpp_Variant_hpp
#define hpp_Variant_hpp

// We need string to make type conversion
#include "../Strings/Strings.hpp"
// We need placement new too
#include <new>
// Need Universal Type Identifier
#include "UTI.hpp"
// Need exceptions too
#include "../Exceptions/BaseException.hpp"

#if __GNUC__ >= 6
  #ifndef __clang__
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wplacement-new"
  #endif
#endif

#if __GNUC__
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wuninitialized"
#endif

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
    namespace Private
    {
        
        template <class T>
        const long long operator == (const T &, const T &);
        
        template <class T>
        struct HasEqualOperator
        {
            static T makeT();
            enum { Result = (sizeof(makeT() == makeT()) != sizeof(long long)) };
        };
    };

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
        const int b = a.like(&b); // This will work too a can be seen as an int

        // You can test variant type easily
        bool isDouble = a.isExactly((double*)0); // false, of course

        // It's not because it's an int that it can't be converted to a double
        double c = 0;
        // This will return false, and c will still be zero, a is an int, and can't be extracted to c
        a.extractTo(c);
        // But, using like, you'll call heavy conversion if the type doesn't match
        c = a.like(c); // c == 3.0 after conversion
        // You can write Var t = "text" because we have an exception for const char * pointer (that's the only exception for types)
        Var t = "3.45";
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
	    struct Empty { };

	    /** Not constant exception */
	    struct NotConstException { virtual const char * what() const { return "Element is not const"; } };

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
            bool                                    (*NumberDiscriminant)();
            bool                                    (*Compare)(const InternalUnion &, const InternalUnion &);
	    };
        


        template <class T>
        static bool CompareStrict(const InternalUnion & a, const InternalUnion & b) { return *reinterpret_cast<const T*>(a.Buffer) == *reinterpret_cast<const T*>(b.Buffer); }
        template <class T>
        static bool CompareNonStrict(const InternalUnion & a, const InternalUnion & b) { return memcmp(a.Buffer, b.Buffer, sizeof(T)) == 0; }
        template <class T>
        static bool CompareStrictPtr(const InternalUnion & a, const InternalUnion & b) { return *reinterpret_cast<const T*>(a.Pointer) == *reinterpret_cast<const T*>(b.Pointer); }
        template <class T>
        static bool CompareNonStrictPtr(const InternalUnion & a, const InternalUnion & b) { return a.Pointer == b.Pointer; }
        

        template < typename T, bool U >
        struct EqualOperatorWrap
        {
            static inline bool Compare(const InternalUnion & a, const InternalUnion & b) { return CompareNonStrict<T>(a,b); }
            static inline bool ComparePtr(const InternalUnion & a, const InternalUnion & b) { return CompareNonStrictPtr<T>(a,b); }
        };
        
        template < typename T>
        struct EqualOperatorWrap<T, true>
        {
            static inline bool Compare(const InternalUnion & a, const InternalUnion & b) { return CompareStrict<T>(a,b); }
            static inline bool ComparePtr(const InternalUnion & a, const InternalUnion & b) { return CompareStrictPtr<T>(a,b); }
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
            
            static bool Compare(const InternalUnion & a, const InternalUnion & b) { return EqualOperatorWrap<T, Private::HasEqualOperator<T>::Result >::Compare(a, b); }
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

            static bool Compare(const InternalUnion & a, const InternalUnion & b) { return EqualOperatorWrap<T, Private::HasEqualOperator<T>::Result >::ComparePtr(a, b); }
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
              , (IsNumber<T>::result || UniversalTypeIdentifier::IsEnum<T>::Result) ? &VarT<Policy>::_isPOD : &VarT<Policy>::_isNotPOD
              , &VirtualTableImpl<T, Optimize >::Compare

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
                static bool Compare(const InternalUnion & a, const InternalUnion & b) { return EqualOperatorWrap<T, Private::HasEqualOperator<T>::Result >::Compare(a, b); }
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
                static bool Compare(const InternalUnion & a, const InternalUnion & b) { return EqualOperatorWrap<T, Private::HasEqualOperator<T>::Result >::ComparePtr(a, b); }
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
              , (IsNumber<T>::result || UniversalTypeIdentifier::IsEnum<T>::Result) ? &VarT<Policy>::_isPOD : &VarT<Policy>::_isNotPOD
              , &VirtualTableBigImpl<T>::Inner< Bool2Type<Optimize> >::Compare
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
        
        /** Comparison operator expect that both the stored type and the value to be the same.
            For loose comparison, use the similar member function instead */
        inline bool operator ==(const VarT& x) const { return table == x.table && table->Compare(inner, x.inner); }
        /** Comparison operator expect that both the stored type and the value to be the same.
            For loose comparison, use the similar member function instead */
        inline bool operator !=(const VarT& x) const { return table != x.table || !table->Compare(inner, x.inner); }

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
        
        /** Check and extract if it's the expected type */
        template<typename T>
        inline T * extractIf(T* t) { return isExactly(t) ? toPointer(t) : 0; }
        /** Check and extract if it's the expected type */
        template<typename T>
        inline const T * extractIf(const T* t) const { return isExactly(const_cast<T*>(t)) ? toPointer(t) : 0; }
        
	    /** Try to implicitly convert the type 
	        @return false if the conversion doesn't apply (wanted type is different from internal type)
	    */	
	    template<typename T>
	    inline bool extractTo(T& out) const {	if (! this->isExactly((T*)0)) return false; toRef(out); return true; }

    //	template<typename T>
    //	inline bool convertTo(T*) {	if (! this->isExactly((T*)0)) return false; toRef(out); return true; }
	    
	    //	template<typename T>
    //	inline const T & like(const T &) const { if (!isExactly((T*)0)) Throw(NotConstException()); return *toPointer((const T*)0); }
	    /** Explicitly convert the type to the requested type 
	        If the requested type doesn't match the internal type, an heavy conversion is performed.
		    Heavy conversions requires creating a DataSource object dynamically and can fail
		    @throw NotConstException on failure
		    @return the converted and requested type
	    */
	    template<typename T>
	    inline const T like(const T *) const { if (!isExactly((T*)0)) return heavyConversionTo((T*)0); return *toPointer((const T*)0); }

	    template<typename T>
	    inline const T heavyConversionTo(T*) const { T temporary; VarT<Policy> tmp = temporary; tmp.setDataSource(getDataSource()); if (!tmp.extractTo(temporary)) Throw(NotConstException()); return temporary; }

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
        /** Check if this variant is a number (int, double, float, long long, unsigned, etc) */
        inline bool isNumber() const { return table->NumberDiscriminant(); }
        /** Get the stored type name. 
            @warning Don't use that to compare Variant types, use isExactly() instead. */
        inline const char * getTypeName() const { return UniversalTypeIdentifier::getTypeFactory()->getTypeName(table->getTypeID()); }

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
        
        /** Check if the given parameter is similar to the current instance stored.
            This function is not necessarly transitive, that is x.similar(y) != y.similar(x) can be true.
            The function converts x to the type of y and compares it with y's internal == operator */
        template<typename T>
        const bool similar(const T & y) const
        {
            return (y == like(&y));
        }

    // This operator has been removed because it is hazardous to use until we have explicit type conversion operator in C++ standard
    #if 0
	    template <typename T>
		    explicit operator T() { return like(*(T*)0);	}
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
    
    /** This is used when modifying a variant has to be done through function calls.
        The underlying storage can be anything you want, provided you accept to and from Variant conversion. */
    template <typename Policy>
    struct GetterSetterT
    {
        /** Derive from this object to store your additional information */
        struct Opaque
        {
            const void * self;
            Opaque(const void * self = 0) : self(self) {}
            virtual Opaque * Clone() const { return new Opaque(self); }
            virtual ~Opaque() {  }
        };
        /** The getter function pointer */
        typedef VarT<Policy> (*GetterT)(const Opaque & self);
        /** The setter function pointer */
        typedef void (*SetterT)(Opaque & self, const VarT<Policy> &);
        
        /** The pointer on the getter function */
        GetterT     getter;
        /** The pointer on the setter function to use */
        SetterT     setter;
        /** The object this runs on */
        Opaque *    self;
        
        /** Default constructor */
        GetterSetterT() : getter(0), setter(0), self(0) {}
        /** Generic purpose constructor */
        GetterSetterT(const void * self, GetterT getter, SetterT setter) : getter(getter), setter(setter), self(new Opaque(self)) {}
        /** Copy constructor */
        GetterSetterT(const GetterSetterT & other) : getter(other.getter), setter(other.setter), self(other.self->Clone()) {}
        /** Copy operator */
        GetterSetterT & operator = (const GetterSetterT & other) { if (&other == this) return *this; this->~GetterSetterT(); return *new(this) GetterSetterT(other); }
        /** A constructor with an opaque member */
        GetterSetterT(Opaque * self, GetterT getter, SetterT setter) : getter(getter), setter(setter), self(self) {}
        
        ~GetterSetterT() { delete0(self); }
    };
    /** Use this if you want to store getters setters to an private value */
    typedef GetterSetterT<ObjectCopyPolicy> GetterSetter;
    /** Use this if you want to store getters setters to an private value */
    typedef GetterSetterT<ObjectPtrPolicy> GetterSetterRef;
    
    

    /** Error handler callback.
        If provided, and in case of invokation error, this is called before returning an empty variant.
        You can throw in the callback, the object does not catch any exception, so it'll bubble up your handling code, 
        or you can retry or escape or ... */
    struct ErrorCallback
    {
        /** The error type when invoking a method */
        enum ErrorType
        {
            Success = 0,            //!< No error is detected
            BadArgumentCount = 1,   //!< Not enough arguments provided
            BadArgumentType = 2,    //!< Bad argument type provided (and no conversion succeeded)
            MethodFailed = 3,       //!< Calling the method failed
            BadThisPointer = 4,     //!< The pointer to this object is not as expected.
            
            Unknown = 255,          //!< Unknown error
        };
        /** Will be called by the Dynamic object, providing the error type and a error message */
        virtual Var & errorDetected(Var & ret, const ErrorType type, const Strings::FastString & msg) const { return ret; }
        /** Syntactic sugar */
        inline Var & operator() (Var & ret, const ErrorType type, const Strings::FastString & msg) const { return errorDetected(ret, type, msg); }
        
        virtual ~ErrorCallback() {}
    };
    /** When none specified, this one is used and it does nothing. */
    extern ErrorCallback defaultHandling;
        
    
    /** This store the context and arguments for an unknown function call. */
    template <typename Policy>
    class FunctionArgsT
    {
        // Type definition and enumeration
    public:
        /** The type of object we are using */
        typedef VarT<Policy> Var;
        
        // Members
    public:
        /** A reference on this object if any (can be empty for a static function pointer) */
        void * thisObj;
        /** The argument list (not owned) */
        const Var * args;
        const size_t count;
        /** The error callback, if provided */
        const ErrorCallback & errorCb;
        
    private:
        // Prevent copy
        FunctionArgsT(const FunctionArgsT & other);
        FunctionArgsT & operator = (const FunctionArgsT & other);
        
        // Construction and destruction
    public:
        /** The object does not own the arguments array passed in. */
        FunctionArgsT(void * const thisObj, const Var * args, const size_t argsCount)
           : thisObj(thisObj), args(args), count(argsCount), errorCb(defaultHandling)
        {}
        /** The object does not own the arguments array passed in. */
        FunctionArgsT(void * const thisObj, const Var * args, const size_t argsCount, const ErrorCallback & cb)
           : thisObj(thisObj), args(args), count(argsCount), errorCb(cb)
        {}
    };

    /** The functions type.
        If you need to store a function pointer to a variant, you need to write a wrapper like this:
        @code
            void myFunc();
            int sum(int a, int b);
            
            // Write a wrapper like this:
            Var myFunc_wrap(const FunctionsArgs & args) { myFunc(); return Var(); }
            Var sum_wrap(const FunctionArgs & args) { int t; Var ret = sum(args.args[0].like(&t), args.args[1].like(&t)); return ret; }
            
            Var funcPtr = sum_wrap;
            // Call like this:
            int ret = Invoke(funcPtr)(1, 2).like(ret);
        @endcode */
    typedef VarT<ObjectCopyPolicy> (*NamedFunc)(const FunctionArgsT<ObjectCopyPolicy> & args);
    typedef VarT<ObjectPtrPolicy> (*NamedFuncRef)(const FunctionArgsT<ObjectPtrPolicy> & args);


    /** A class used to invoke a method from unknown object.
        If you need to store a function pointer to a variant, you need to write a wrapper like this:
        @code
            void myFunc();
            int sum(int a, int b);
            
            // Write a wrapper like this (error code omitted):
            Var myFunc_wrap(const FunctionsArgs & args) { myFunc(); return Var(); }
            Var sum_wrap(const FunctionArgs & args) { int t; Var ret = sum(args.args[0].like(&t), args.args[1].like(&t)); return ret; }
            
            Var funcPtr = sum_wrap;
            // Call like this:
            int ret = Invoke(funcPtr)(1, 2).like(ret);
        @endcode
        
        For object with methods, you'll write something like this:
        @code
            struct A
            {
                int sum(int a);
            };
            
            // Write a wrapper like this:
            Var sum_wrap(const FunctionArgs & args) { A * a = (A*)args.thisObj.toPointer((void**)0); int t; Var ret = a->sum(args.args[0].like(&t)); return ret; }
            // Call like this:
            A a;
            int ret = Invoke(&a, sum_wrap)(3);
        @endcode
        
        @warning If you don't want to register each class you want to invoke method from, then you must convert them to and from void *. 
                 To avoid strong mistakes, you might want to dynamic_cast the class to assert it's the type expected after void* downgrading (if RTTI is on).
    */
    template <typename Policy>
    class InvokeT
    {
        // Type definition and enumeration
    public:
        /** The Variant class we are using */
        typedef VarT<Policy> Var;
        /** The type of arguments we expect */
        typedef FunctionArgsT<Policy> Args;
        /** The function pointer type we expect */
        typedef Var (*FuncPtr)(const Args &);
        
        
        // Operators
    public:
        /** Basic 0 ary argument */
        inline Var operator()() const { return (*ptr)(Args(thisObj, 0, 0)); }
        /** Basic unary argument */
        inline Var operator()(const Var & arg) const { return (*ptr)(Args(thisObj, &arg, 1)); }
        /** Basic binary argument */
        inline Var operator()(const Var & arg1, const Var & arg2) const { const Var args[] = { arg1, arg2 }; return (*ptr)(Args(thisObj, args, 2)); }
        /** Basic ternary argument */
        inline Var operator()(const Var & arg1, const Var & arg2, const Var & arg3) const { const Var args[] = { arg1, arg2, arg3 }; return (*ptr)(Args(thisObj, args, 3)); }
        /** Basic four-ary argument */
        inline Var operator()(const Var & arg1, const Var & arg2, const Var & arg3, const Var & arg4) const { const Var args[] = { arg1, arg2, arg3, arg4 }; return (*ptr)(Args(thisObj, args, 4)); }
        /** Basic five-ary argument */
        inline Var operator()(const Var & arg1, const Var & arg2, const Var & arg3, const Var & arg4, const Var & arg5) const { const Var args[] = { arg1, arg2, arg3, arg4, arg5 }; return (*ptr)(Args(thisObj, args, 5)); }
        /** List based argument calling */
        inline Var operator()(const Var * args, const size_t count) const { return (*ptr)(Args(thisObj, args, count)); }
        
        // Construction and destruction
    public:
        /** Construct the invoker with a pointer to a function */
        InvokeT(FuncPtr ptr) : ptr(ptr), thisObj(0) {}
        /** Construct the invoker with a pointer to a method, it's not owned */
        InvokeT(void * thisObj, FuncPtr ptr) : ptr(ptr), thisObj(thisObj) {}
        
        // Members
    private:
        /** The function pointer to invoke */
        FuncPtr ptr;
        /** The object to use as context (might be empty) */
        void * thisObj;
    };
    
    /** The function args for copied parameters */
    typedef FunctionArgsT<ObjectCopyPolicy> FuncArgs;
    /** The function args for pointers parameters */
    typedef FunctionArgsT<ObjectPtrPolicy> FuncArgsRef;
    /** The invoker for copied parameters */
    typedef InvokeT<ObjectCopyPolicy> Invoke;
    /** The invoker for pointer parameters */
    typedef InvokeT<ObjectPtrPolicy> InvokeRef;
}

#ifdef __GNUC__
  #pragma GCC diagnostic pop
#endif

#if __GNUC__ >= 6
  #ifndef __clang__
  #pragma GCC diagnostic pop
  #endif
#endif

#endif
