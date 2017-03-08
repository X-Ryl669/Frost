#ifndef hpp_ScopeGuard_hpp
#define hpp_ScopeGuard_hpp

// From an idea of Andrei Alexandrescu
// We need ByRef objects
#include "ByRef.hpp"

/** This implement the scope guard */
class ScopeGuardImplBase
{

public:
    /** Don't perform the clean-up */
    void Dismiss() const throw()  { dismissed = true;    }

    // Construction and destruction is not allowed except for child classes
protected:
    /** Default constructor */
    ScopeGuardImplBase() : dismissed(false)    {}
    /** Copy constructor, undo the initial scope guard behavior */
    ScopeGuardImplBase(const ScopeGuardImplBase& other) : dismissed(other.dismissed) { other.Dismiss(); }
    /** Destructor 
        It is not virtual because from point of declaration to point to destruction
        it is a child class which is created and the compiler directly call the right 
        child destructor (it also avoid setting up and using virtual function pointer) */
    ~ScopeGuardImplBase() {}

    // Member
protected:    
    /** When set to true, the destructor ignore the asked clean up */
    mutable bool dismissed;

private:
    /** Scope guard are not moveable */
    ScopeGuardImplBase& operator= (const ScopeGuardImplBase&);
};
typedef const ScopeGuardImplBase & ScopeGuard;

/** Real implementation for a function with no parameter */
template <typename Fun>
class ScopeGuardImpl0 : public ScopeGuardImplBase
{
    // Construction and destruction
public:
    /** Unique constructor */
    ScopeGuardImpl0(const Fun& fun) : function(fun) {}
    /** Destructor - catch any pending exception to continue cleaning for any other guard */
    ~ScopeGuardImpl0()
    { if (!dismissed) try{ function(); } catch (...) {} }

    // Members
private:
    /** The function to call on exit */
    Fun             function;

};


/** Real implementation for a function with 1 parameter */
template <typename Fun, typename Parm>
class ScopeGuardImpl1 : public ScopeGuardImplBase
{
    // Construction and destruction
public:
    /** Unique constructor */
    ScopeGuardImpl1(const Fun& fun, const Parm& parm)   : function(fun), parameter(parm) {}
    /** Destructor - catch any pending exception to continue cleaning for any other guard */
    ~ScopeGuardImpl1()
    { if (!dismissed) try{ function(parameter); } catch (...) {} }

    // Members
private:
    /** The function to call on exit */
    Fun             function;
    /** The parameter for the specified function */
    const Parm      parameter;
};

/** Real implementation for a function with 2 parameters */
template <typename Fun, typename Parm, typename Parm2>
class ScopeGuardImpl2 : public ScopeGuardImplBase
{
    // Construction and destruction
public:
    /** Unique constructor */
    ScopeGuardImpl2(const Fun& fun, const Parm& parm, const Parm2& parm2)   : function(fun), parameter(parm), parameter2(parm2) {}
    /** Destructor - catch any pending exception to continue cleaning for any other guard */
    ~ScopeGuardImpl2()
    { if (!dismissed) try{ function(parameter, parameter2); } catch (...) {} }

    // Members
private:
    /** The function to call on exit */
    Fun             function;
    /** The parameter for the specified function */
    const Parm      parameter;
    /** The parameter for the specified function */
    const Parm2     parameter2;
};

/** Real implementation for a function with 3 parameters */
template <typename Fun, typename Parm, typename Parm2, typename Parm3>
class ScopeGuardImpl3 : public ScopeGuardImplBase
{
    // Construction and destruction
public:
    /** Unique constructor */
    ScopeGuardImpl3(const Fun& fun, const Parm& parm, const Parm2& parm2, const Parm3& parm3)   : function(fun), parameter(parm), parameter2(parm2), parameter3(parm3) {}
    /** Destructor - catch any pending exception to continue cleaning for any other guard */
    ~ScopeGuardImpl3()
    { if (!dismissed) try{ function(parameter, parameter2, parameter3); } catch (...) {} }

    // Members
private:
    /** The function to call on exit */
    Fun             function;
    /** The parameter for the specified function */
    const Parm      parameter;
    /** The parameter for the specified function */
    const Parm2     parameter2;
    /** The parameter for the specified function */
    const Parm3     parameter3;
};


/** Real implementation for a function with 4 parameters */
template <typename Fun, typename Parm, typename Parm2, typename Parm3, typename Parm4>
class ScopeGuardImpl4 : public ScopeGuardImplBase
{
    // Construction and destruction
public:
    /** Unique constructor */
    ScopeGuardImpl4(const Fun& fun, const Parm& parm, const Parm2& parm2, const Parm3& parm3, const Parm4& parm4)   : function(fun), parameter(parm), parameter2(parm2), parameter3(parm3), parameter4(parm4) {}
    /** Destructor - catch any pending exception to continue cleaning for any other guard */
    ~ScopeGuardImpl4()
    { if (!dismissed) try{ function(parameter, parameter2, parameter3, parameter4); } catch (...) {} }

    // Members
private:
    /** The function to call on exit */
    Fun             function;
    /** The parameter for the specified function */
    const Parm      parameter;
    /** The parameter for the specified function */
    const Parm2     parameter2;
    /** The parameter for the specified function */
    const Parm3     parameter3;
    /** The parameter for the specified function */
    const Parm4     parameter4;
};


template <typename Fun>
inline ScopeGuardImpl0<Fun*>                                MakeGuard(const Fun& fun)   { return ScopeGuardImpl0<Fun*>(fun); }
template <typename Fun, typename Parm>
inline ScopeGuardImpl1<Fun*, Parm>                          MakeGuard(const Fun& fun, const Parm& parm) { return ScopeGuardImpl1<Fun*, Parm>(fun, parm); }
template <typename Fun, typename Parm, typename Parm2>
inline ScopeGuardImpl2<Fun*, Parm, Parm2>                   MakeGuard(const Fun& fun, const Parm& parm, const Parm2& parm2) { return ScopeGuardImpl2<Fun*, Parm, Parm2>(fun, parm, parm2); }
template <typename Fun, typename Parm, typename Parm2, typename Parm3>
inline ScopeGuardImpl3<Fun*, Parm, Parm2, Parm3>            MakeGuard(const Fun& fun, const Parm& parm, const Parm2& parm2, const Parm3& parm3) { return ScopeGuardImpl3<Fun*, Parm, Parm2, Parm3>(fun, parm, parm2, parm3); }
template <typename Fun, typename Parm, typename Parm2, typename Parm3, typename Parm4>
inline ScopeGuardImpl4<Fun*, Parm, Parm2, Parm3, Parm4>     MakeGuard(const Fun& fun, const Parm& parm, const Parm2& parm2, const Parm3& parm3, const Parm4& parm4) { return ScopeGuardImpl4<Fun*, Parm, Parm2, Parm3, Parm4>(fun, parm, parm2, parm3, parm4); }

/** Same scope guard for objects, with no parameter */
template <class Obj, typename MemFun>
class ObjScopeGuardImpl0 : public ScopeGuardImplBase
{
    // Construction and destruction
public:
    /** Constructor */
    ObjScopeGuardImpl0(Obj& obj, MemFun memFun) : object(obj), method(memFun)  {}
    /** Destructor */
    ~ObjScopeGuardImpl0() {  if (!dismissed) try {(object.*method)(); } catch(...) {};  }

    // Members
private:
    /** Object to work on */
    Obj& object;
    /** Method to call */
    MemFun method;
};

/** Scope guard for objects with 1 parameter */
template <class Obj, typename MemFun, typename Parm1>
class ObjScopeGuardImpl1 : public ScopeGuardImplBase
{
    // Construction and destruction
public:
    /** Constructor */
    ObjScopeGuardImpl1(Obj& obj, MemFun memFun, const Parm1& parm1) : object(obj), method(memFun), parameter1(parm1)  {}
    /** Destructor */
    ~ObjScopeGuardImpl1() {  if (!dismissed) try {(object.*method)(parameter1); } catch(...) {};  }

    // Members
private:
    /** Object to work on */
    Obj& object;
    /** Method to call */
    MemFun method;
    /** The parameter for the specified function */
    const Parm1     parameter1;
};

/** Scope guard for objects with 2 parameters */
template <class Obj, typename MemFun, typename Parm1, typename Parm2>
class ObjScopeGuardImpl2 : public ScopeGuardImplBase
{
    // Construction and destruction
public:
    /** Constructor */
    ObjScopeGuardImpl2(Obj& obj, MemFun memFun, const Parm1& parm1, const Parm2& parm2) : object(obj), method(memFun), parameter1(parm1), parameter2(parm2)  {}
    /** Destructor */
    ~ObjScopeGuardImpl2() {  if (!dismissed) try {(object.*method)(parameter1, parameter2); } catch(...) {};  }

    // Members
private:
    /** Object to work on */
    Obj& object;
    /** Method to call */
    MemFun method;
    /** The parameter for the specified function */
    const Parm1     parameter1;
    /** The parameter for the specified function */
    const Parm2     parameter2;
};

/** Scope guard for objects with 3 parameters */
template <class Obj, typename MemFun, typename Parm1, typename Parm2, typename Parm3>
class ObjScopeGuardImpl3 : public ScopeGuardImplBase
{
    // Construction and destruction
public:
    /** Constructor */
    ObjScopeGuardImpl3(Obj& obj, MemFun memFun, const Parm1& parm1, const Parm2& parm2, const Parm3& parm3) : object(obj), method(memFun), parameter1(parm1), parameter2(parm2), parameter3(parm3)  {}
    /** Destructor */
    ~ObjScopeGuardImpl3() {  if (!dismissed) try {(object.*method)(parameter1, parameter2, parameter3); } catch(...) {};  }

    // Members
private:
    /** Object to work on */
    Obj& object;
    /** Method to call */
    MemFun method;
    /** The parameter for the specified function */
    const Parm1     parameter1;
    /** The parameter for the specified function */
    const Parm2     parameter2;
    /** The parameter for the specified function */
    const Parm3     parameter3;
};

/** Scope guard for objects with 4 parameters */
template <class Obj, typename MemFun, typename Parm1, typename Parm2, typename Parm3, typename Parm4>
class ObjScopeGuardImpl4 : public ScopeGuardImplBase
{
    // Construction and destruction
public:
    /** Constructor */
    ObjScopeGuardImpl4(Obj& obj, MemFun memFun, const Parm1& parm1, const Parm2& parm2, const Parm3& parm3, const Parm4& parm4) : object(obj), method(memFun), parameter1(parm1), parameter2(parm2), parameter3(parm3), parameter4(parm4)  {}
    /** Destructor */
    ~ObjScopeGuardImpl4() {  if (!dismissed) try {(object.*method)(parameter1, parameter2, parameter3, parameter4); } catch(...) {};  }

    // Members
private:
    /** Object to work on */
    Obj& object;
    /** Method to call */
    MemFun method;
    /** The parameter for the specified function */
    const Parm1     parameter1;
    /** The parameter for the specified function */
    const Parm2     parameter2;
    /** The parameter for the specified function */
    const Parm3     parameter3;
    /** The parameter for the specified function */
    const Parm4     parameter4;
};

template <class Obj, typename Fun>
inline ObjScopeGuardImpl0<Obj, Fun>                                 MakeObjGuard(Obj & obj, const Fun& fun) { return ObjScopeGuardImpl0<Obj, Fun>(obj, fun); }
template <class Obj, typename Fun, typename Parm>
inline ObjScopeGuardImpl1<Obj, Fun, Parm>                           MakeObjGuard(Obj & obj, const Fun& fun, const Parm& parm) { return ObjScopeGuardImpl1<Obj, Fun, Parm>(obj, fun, parm); }
template <class Obj, typename Fun, typename Parm, typename Parm2>
inline ObjScopeGuardImpl2<Obj, Fun, Parm, Parm2>                    MakeObjGuard(Obj & obj, const Fun& fun, const Parm& parm, const Parm2& parm2) { return ObjScopeGuardImpl2<Obj, Fun, Parm, Parm2>(obj, fun, parm, parm2); }
template <class Obj, typename Fun, typename Parm, typename Parm2, typename Parm3>
inline ObjScopeGuardImpl3<Obj, Fun, Parm, Parm2, Parm3>             MakeObjGuard(Obj & obj, const Fun& fun, const Parm& parm, const Parm2& parm2, const Parm3& parm3) { return ObjScopeGuardImpl3<Obj, Fun, Parm, Parm2, Parm3>(obj, fun, parm, parm2, parm3); }
template <class Obj, typename Fun, typename Parm, typename Parm2, typename Parm3, typename Parm4>
inline ObjScopeGuardImpl4<Obj, Fun, Parm, Parm2, Parm3, Parm4>      MakeObjGuard(Obj & obj, const Fun& fun, const Parm& parm, const Parm2& parm2, const Parm3& parm3, const Parm4& parm4) { return ObjScopeGuardImpl4<Obj, Fun, Parm, Parm2, Parm3, Parm4>(obj, fun, parm, parm2, parm3, parm4); }

/** The auto clean structure (clean at end of scope) */
template <typename T>
struct Clean : public ScopeGuardImplBase
{
    T *& obj;
    Clean(T *& value) : obj(value) {}
    ~Clean() { if (!dismissed) { delete obj; obj = 0; } }
};

/** The auto clean structure (clean at end of scope) */
template <typename T>
struct Freer : public ScopeGuardImplBase
{
    T *& obj;
    Freer(T *& value) : obj(value) {}
    ~Freer() { if (!dismissed) { free(obj); obj = 0; } }
};

/** The auto clean array structure (clean at end of scope) */
template <typename T>
struct CleanArray : public ScopeGuardImplBase
{
    T *& obj;
    CleanArray(T *& value) : obj(value) {}
    ~CleanArray() { if (!dismissed) { delete[] obj; obj = 0; } }
};

template <class T>
inline Clean<T>                                                     MakeCleaner(T* & t) { return Clean<T>(t); }
template <class T>
inline Freer<T>                                                     MakeFreer(T* & t) { return Freer<T>(t); }
template <class T>
inline CleanArray<T>                                                MakeArrayCleaner(T* & t) { return CleanArray<T>(t); }

// Some macro trickery
#define ConcatDoubleImpl(T, U)  T##U
#define ConcatDouble(X, Y)  ConcatDoubleImpl(X, Y)
#define Concat(X)  ConcatDouble(X, __LINE__)

#define DeleteOnScopeExit(X)        ScopeGuard Unused(Concat(_improbable)) = MakeCleaner(X)
#define FreeOnScopeExit(X)          ScopeGuard Unused(Concat(_improbable)) = MakeFreer(X)
#define DeleteArrayOnScopeExit(X)   ScopeGuard Unused(Concat(_improbable_)) = MakeArrayCleaner(X)

#define CleanUpOnScopeExit      ScopeGuard Unused(Concat(_improbable_)) = MakeGuard
#define CleanUpObjOnScopeExit   ScopeGuard Unused(Concat(_improbable_)) = MakeObjGuard

// Delete template helper
template <typename T>   void destroy(T*& t) { delete t; t = 0; }
template <typename T>   void destroyArray(T*& t) { delete[] t; t = 0; }


#endif
