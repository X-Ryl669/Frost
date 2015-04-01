#ifndef hpp_ScopedPointer_hpp
#define hpp_ScopedPointer_hpp

// We need ForcedInline declaration
#include "../Types.hpp"

namespace Utils
{
    /** Usual scope pointer class with operator overloading.
        Unlike the ScopeGuard, this changes the declaration line of the pointer, but can be passed around.

        We found from our experience that reference counting was not an efficient way to deal with pointers for automatic resource management.
        RefCounting can create loops, requires an additional counter storage (since the ClassPath compiles on 32bits systems, we don't expect
        high bits in the pointer to be available for storing a counter).
        Most of the time, the reference counting never goes above 1, so this is a sign that a more efficient solution could be used.
        We present ScopePtr and OwnPtr here. A ScopePtr stored a pointer and delete it when it's deleted (usually when it goes out of scope).
        OwnPtr is like a ScopePtr except that it tracks if it owns the pointer to the object. It can be sold to become a WeakPtr.
        @sa OwnPtr

        There is nothing special with this, usage is quite
        transparent (use like a pointer) thanks to operator overloading.
        @warning Copy constructor use move semantics, so this works as intended:
                 ScopePtr<A> a = new A(); */
    template <class T>
    class ScopePtr
    {
        // Members
    private:
        /** The pointer object*/
        T *         pointer;

        // Helpers
    private:
        /** Because we have overloaded & operator, we need this to get the object pointer itself */
        ForcedInline(const ScopePtr* pThis() const) { return this; }
        /** The forget implementation */
        ForcedInline(T* const Forget(T * const other)) { T * const p = pointer; pointer = other; return p; }

        // Operators
    public:
        /** Assignment operator */
        ForcedInline(ScopePtr& operator = (ScopePtr& other))
        {
            if (other.pThis() != this)
            {
                delete Forget(other.pointer);
                other.pointer = 0;
            }
            return *this;
        }

        /** Assignment operator
            @warning This own the object passed in */
        ForcedInline(ScopePtr& operator = (T* const ownPtr))
        {
            if (pointer != ownPtr) delete Forget(ownPtr);
            return *this;
        }
        /** All classical pointer overload */
        ForcedInline(operator T*() const throw())                              { return pointer; }
        ForcedInline(T& operator*() const throw())                             { return *pointer; }
        ForcedInline(T* operator->() const throw())                            { return pointer; }
        ForcedInline(T* const* operator&() const throw())                      { return (T* const*) (&pointer); }
        ForcedInline(T** operator&() throw())                                  { return (T**) (&pointer); }


        // Interface
    public:
        /** Forget the object owned (don't delete it)
            The internal pointer is reset to 0.
            @return The pointer to the object */
        ForcedInline(T * Forget()) { return Forget(0); }

        // Construction
    public:
        /** Own the given object */
        ForcedInline(ScopePtr(T* const ownPtr = 0)) : pointer (ownPtr)    { }
        /** Copy constructor with moveable semantic (require for function returning this object) */
        ForcedInline(ScopePtr(const ScopePtr& other)) : pointer (other.pointer) { const_cast<ScopePtr&>(other).pointer = 0; }
        /** Destructor */
        ForcedInline(~ScopePtr())                                         { delete pointer; }
    };

    template <class T> ForcedInline(bool operator== (const ScopePtr<T> & ptr1, const T* const ptr2));
    template <class T> bool operator== (const ScopePtr<T> & ptr1, const T* const ptr2) { return (T*)ptr1 == ptr2; }
    template <class T> ForcedInline(bool operator!= (const ScopePtr<T> & ptr1, const T* const ptr2));
    template <class T> bool operator!= (const ScopePtr<T> & ptr1, const T* const ptr2) { return !(ptr1 == ptr2);  }

    /** This is like a scoped pointer class, except that the ownership
        is specified at construction, and can be changed during lifetime with the sold() method.
        You can only sell a pointer that's owned, obviously.

        Typically, OwnPtr is a reference counted smart pointer for the poor, except that it
        doesn't come with the pain/overload of reference counting. In most use case, references in such smart pointer
        never overcome 2, so it's often easier to just design the code with OwnPtr and transfer ownership explicitely
        when the refCount would reach 2 with a shared pointer (likely at a single place in the code).

        The interface using a reference &, like operator = and constructor won't own the object by default, while the other will.

        @warning Copy constructor use move semantics, so this works as intended:
                 OwnPtr<A> a = new A(); */
    template <class T>
    class OwnPtr
    {
        // Members
    private:
        /** The pointer object*/
        T *         pointer;
        /** Check if we own the object */
        bool        own;

        // Helpers
    private:
        /** Because we have overloaded & operator, we need this to get the object pointer itself */
        ForcedInline(const OwnPtr* pThis() const) { return this; }
        /** The forget implementation */
        ForcedInline(T* const Forget(T * const other)) { T * const p = pointer; pointer = other; return p; }

        // Operators
    public:
        /** Assignment operator */
        ForcedInline(OwnPtr& operator = (OwnPtr& other))
        {
            if (other.pThis() != this)
            {
                T * const prev = Forget(other.pointer);
                if (own) delete prev;
                other.pointer = 0;
                own = other.own;
            }
            return *this;
        }

        /** Assignment operator
            @warning This own the object passed in */
        ForcedInline(OwnPtr& operator = (T* const ownPtr))
        {
            if (pointer != ownPtr)
            {
                T * const prev = Forget(ownPtr);
                if (own) delete prev;
                own = true; // By default it owns the pointer given in.
            }
            return *this;
        }
        /** Assignment operator
            @warning This doesn't own the object passed in */
        ForcedInline(OwnPtr& operator = (T& ownPtr))
        {
            if (pointer != &ownPtr)
            {
                T * const prev = Forget(&ownPtr);
                if (own) delete prev;
                own = false; // By default it does not own the pointer given in.
            }
            return *this;
        }

        /** All classical pointer overload */
        ForcedInline(operator T*() const throw())                              { return pointer; }
        ForcedInline(T& operator*() const throw())                             { return *pointer; }
        ForcedInline(T* operator->() const throw())                            { return pointer; }
        ForcedInline(T* const* operator&() const throw())                      { return (T* const*) (&pointer); }
        ForcedInline(T** operator&() throw())                                  { return (T**) (&pointer); }
        /** Sell the object. Once this is done, we don't own it anymore (and you can not sell something you don't own).
            Unlike Forget, this actually keep the pointer valid in this object. */
        ForcedInline(void sold() throw())                                      { own = false; }

        // Interface
    public:
        /** Forget the object owned (don't delete it).
            The internal pointer is reset to 0.
            @return The pointer to the object */
        ForcedInline(T * Forget()) { return Forget(0); }

        // Construction
    public:
        /** Own the given object */
        ForcedInline(OwnPtr(T* const ownPtr = 0, const bool own = true)) : pointer (ownPtr), own(own)    { }
        /** Doesn't own the given object */
        ForcedInline(OwnPtr(T& ownPtr)) : pointer (&ownPtr), own(false)    { }
        /** Copy constructor with moveable semantic (require for function returning this object) */
        ForcedInline(OwnPtr(const OwnPtr& other)) : pointer (other.pointer), own(other.own) { const_cast<OwnPtr&>(other).pointer = 0; }
        /** Copy constructor with moveable semantic (require for function returning this object) */
        ForcedInline(OwnPtr(ScopePtr<T>& other)) : pointer (other.Forget()), own(true)     { }
        /** Destructor */
        ForcedInline(~OwnPtr())                                         { if (own) delete pointer; }
    };

    template <class T> ForcedInline(bool operator== (const OwnPtr<T> & ptr1, const T* const ptr2));
    template <class T> bool operator== (const OwnPtr<T> & ptr1, const T* const ptr2) { return (T*)ptr1 == ptr2; }
    template <class T> ForcedInline(bool operator!= (const OwnPtr<T> & ptr1, const T* const ptr2));
    template <class T> bool operator!= (const OwnPtr<T> & ptr1, const T* const ptr2) { return !(ptr1 == ptr2);  }

}


#endif
