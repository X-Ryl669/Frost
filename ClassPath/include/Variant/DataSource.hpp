#ifndef hpp_DataSource_hpp
#define hpp_DataSource_hpp

namespace Type
{
    /** Forward declarations */
    template <typename Policy>
    class VarT;
    struct ObjectCopyPolicy;
    struct ObjectPtrPolicy;
    /** The main variant type copies the internal data when copied */
    typedef VarT<ObjectCopyPolicy> Var;
    /** This variant type only transmit pointers to internal data when copied */
    typedef VarT<ObjectPtrPolicy>  Ref;
    
    /** Thrown when the conversion is not allowed */
    struct ConversionNotAllowed {};

    /** Define the data source interface.
        If you want to use variant with your own type, and allow code conversion with supported type,
        you must provide a DataSource to serialize your type to / from XML. */
    class DataSource
    {
	    /** The data source interface */
    public:
	    /** Get the data source value */
	    virtual Var 	getValue() const = 0;
	    /** Set the data source value */
	    virtual void	setValue(const Var &) = 0;

	    /** The destructor is virtual to allow correct behavior */
	    virtual ~DataSource() {};
    };
}


#endif
