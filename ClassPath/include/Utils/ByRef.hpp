#ifndef hpp_ByRef_hpp
#define hpp_ByRef_hpp


// If we need to pass a value by reference
template <class T>
class ByRef
{
public:
    /** Access the data by reference */
	operator T& () const { return ref; }
	operator const T & () const { return ref; }

	/** Construct the objects */
	ByRef(T & _ref): ref(_ref) {}

private:
	/** No copy allowed */
	ByRef & operator = (const ByRef & obj);
	/** The reference itself */
	T & ref;
};

/** The helper function */
template <class T>
ByRef<T>	byRef(T & ref) { return ByRef<T>(ref); }

#endif
