#ifndef hpp_CPP_Database_CPP_hpp
#define hpp_CPP_Database_CPP_hpp

// We need strings
#include "../Strings/Strings.hpp"
// We need universal type specifier
#include "../Variant/UTI.hpp"
// We need common SQL stuff
#include "SQLFormat.hpp"
// We need variants
#include "../Variant/Variant.hpp"
// We need containers too
#include "../../include/Container/Container.hpp"
// We need Static Assert
#include "../Utils/StaticAssert.hpp"


/** A database can be seen as 2D matrix with variable-typed cells.
    Usually the columns are named and typed, and all cells in a column
    must match the type of the column
    <br>
    A query is usually retrieving a subset of this matrix.
    <br>
    The result on this query is usually one or more rows of cells called field.
    <br>
    As a first version, it is required to describe the database format.
    The library provides simple macros to ease format description.
    <br>
    The power of this library is in its easy usage (very very limited typing
    required, as everything is very well thought since beginning) and yet it is
    near optimal (algorithms used here are O(1) or O(n) and never more).
    <br>
    Macro are not evil here, as they are really, really simple and are just here
    to avoid copy and paste error (1st source of bugs).
    <br>
    Please notice that this declaration declares both the members
    and the table model in one line. (Previous library usually required 2 different
    declarations which are a source of bug, due to maintainance).
    <br>
    Database format declaration is like this:
    <br>
    For a database like this
    @verbatim
    Table "Description"         => "ID" (INTEGER PRIMARY KEY), "Description" (VARCHAR)
    Table "FieldDescription"    => "ID" (INTEGER PRIMARY KEY), "IDDescription" (INTEGER), "IDExtra" (INTEGER)
    Table "RecordItem"          => "ID" (INTEGER PRIMARY KEY), "StartTime" (INTEGER), "StopTime" (INTEGER), "IDExtra" (INTEGER), "IDFile" (INTEGER)
    @endverbatim
    <br>
    Writing those classes is done like this:
    @code
    // Now link the data in the database with a data holder struct
    struct Description : public Database::Table<Description>
    {
        BeginFieldDeclaration(Description)
            DeclareField(ID, Database::Index);
            DeclareField(Description, String);
        EndFieldDeclaration
    };
    @endcode
    And using is done like this:
    @code
        Description desc;
        // Select user with ID = 3 from DB and retrieve all the other members (if it doesn't exist in DB, it's created)
        desc.ID = 3;
        // Change the name of the user
        desc.Description = "First example";
                    // Remark, the line above actually perform "UPDATE Description SET(Description) VALUES("First example") WHERE ID=3"

        // Delete the user now from the database
        desc.Delete();
                    // Remark, the line above actually perform "DELETE FROM Description WHERE ID=3"
    @endcode
    <br>
    Please refer to TableDescription for a more complete example.
    <br>
    <br>
    Also, the current database drivers are limited in terms of types they can handle (it's
    the minimum common denominator). It's safe to use:<br>
    @verbatim
    int, unsigned int, String, Blob, int64, Index, double
    @endverbatim
    <br>
    You can use NotNull of any of the previous type like:<br>
    @verbatim
    NotNull<int>, NotNull<unsigned int>, ...
    @endverbatim
    (except blob and index, blob are always default null), and this will
    generate model with TYPE NOT NULL in the database schema).
    <br>
    <br>
    If you will be using multiple database (at the same time), please have a look to MultipleDatabaseConnection
    <br>
    In order to initialize the database system, you must call SQLFormat::initialize (respectively SQLFormat::finalize).
    Any call to the database function out of the initialize/finalize pair gives undefined behaviour.
*/
namespace Database
{
    // Only import what we really use to avoid name collision
    typedef UniversalTypeIdentifier::TypeID TypeID;
    using UniversalTypeIdentifier::getTypeID;
    using UniversalTypeIdentifier::IfEnum;
    typedef Type::Var Var;
    typedef Type::Ref Ref;
    typedef Strings::FastString String;

    /** The index is usually a primary key in a database table */
    struct Index
    {
        unsigned int index;
        operator unsigned int&() { return index; }
        operator const unsigned int&() const { return index; }

        Index(unsigned int index = DelayAction) : index(index) {}
        static const unsigned int WantNewIndex;
        static const unsigned int DelayAction;
    };
    /** The index is usually a primary key in a database table.
        You can also use long integer is most database */
    struct LongIndex
    {
        uint64 index;
        operator uint64&() { return index; }
        operator const uint64&() const { return index; }

        LongIndex(uint64 index = DelayAction) : index(index) {}
        static const uint64 WantNewIndex;
        static const uint64 DelayAction;
    };

    /** The Blob structure */
    struct Blob
    {
        String innerData;

        /** Receive deserialized data */
        void setData(const void * data, const int length)
        {
            char * blob = innerData.Alloc(length);
            if (blob) memcpy(blob, data, length);
            innerData.releaseLock(length);
        }

        Blob() {};
    };

#ifndef DOXYGEN
    struct UniqueString {};

    // The special type used to create not null item in a table
    template <class T>
    struct NotNull : public T
    {
    private:
        // If compilation breaks here, it's because you can't use a NotNull type for any other type than those
        // listed below
        NotNull();
    };

    // Default implementation for TEXT NOT NULL
    template <> struct NotNull<String>          { typedef String Type; };
    // Default implementation for INTEGER NOT NULL
    template <> struct NotNull<int>             { typedef int Type; };
    // Default implementation for UNSIGNED NOT NULL
    template <> struct NotNull<unsigned int>    { typedef unsigned int Type; };
    // Default implementation for REAL NOT NULL
    template <> struct NotNull<double>          { typedef double Type; };
    // Default implementation for BIGINT NOT NULL
    template <> struct NotNull<int64>           { typedef int64 Type; };
    // Default implementation for BIGINT UNSIGNED NOT NULL
    template <> struct NotNull<uint64>          { typedef uint64 Type; };
    // Default implementation for TEXT NOT NULL UNIQUE
    template <> struct NotNull<UniqueString>    { typedef UniqueString Type; };

    // Shortcut, it's shorter to type this
    // Use this in your field declaration if you want for INTEGER NOT NULL. C++ usage use an int
    typedef NotNull<int>            NotNullInt;
    // Use this in your field declaration if you want for INTEGER UNSIGNED NOT NULL. C++ usage use an unsigned int
    typedef NotNull<unsigned int>   NotNullUnsigned;
    // Use this in your field declaration if you want for REAL NOT NULL. C++ usage use a double
    typedef NotNull<double>         NotNullDouble;
    // Use this in your field declaration if you want for BIGINT NOT NULL. C++ usage use a int64
    typedef NotNull<int64>          NotNullLongInt;
    // Use this in your field declaration if you want for BIGINT NOT NULL. C++ usage use a uint64
    typedef NotNull<uint64>         NotNullUnsignedLongInt;
    // Use this in your field declaration if you want for TEXT NOT NULL. C++ usage use a String
    typedef NotNull<String>         NotNullString;
    // Use this in your field declaration if you want for TEXT NOT NULL UNIQUE. C++ usage use a String
    typedef NotNull<UniqueString>         NotNullUniqueString;

    namespace Private
    {
        // This class is used to remove the Not null qualified when using the type
        template <class T> struct NotNullRemove { typedef T Type; };

        // Implementation shortcut
        template <> struct NotNullRemove<NotNullInt>        { typedef int Type; };
        template <> struct NotNullRemove<NotNullUnsigned>   { typedef unsigned int Type; };
        template <> struct NotNullRemove<NotNullDouble>     { typedef double Type; };
        template <> struct NotNullRemove<NotNullLongInt>    { typedef int64 Type; };
        template <> struct NotNullRemove<NotNullString>     { typedef String Type; };
        template <> struct NotNullRemove<NotNullUniqueString>       { typedef String Type; };
        template <> struct NotNullRemove<NotNullUnsignedLongInt>    { typedef uint64 Type; };
    }
#else
    // Shortcut, it's shorter to type this
    /** Use this in your field declaration if you want for INTEGER NOT NULL. C++ usage use an int */
    typedef int                     NotNullInt;
    /** Use this in your field declaration if you want for INTEGER UNSIGNED NOT NULL. C++ usage use an unsigned int */
    typedef unsigned int            NotNullUnsigned;
    /** Use this in your field declaration if you want for READ NOT NULL. C++ usage use a double */
    typedef double                  NotNullDouble;
    /** Use this in your field declaration if you want for BIGINT NOT NULL. C++ usage use a int64 */
    typedef int64                   NotNullLongInt;
    /** Use this in your field declaration if you want for BIGINT NOT NULL. C++ usage use a int64 */
    typedef uint64                  NotNullUnsignedLongInt;
    /** Use this in your field declaration if you want for TEXT NOT NULL. C++ usage use a String */
    typedef String                  NotNullString;
    /** Use this in your field declaration if you want for TEXT NOT NULL. C++ usage use a String */
    typedef String                  NotNullUniqueString;
#endif

    /** This mapper allow compile time selection.
        This convert an integer to a compile time type */
    template <int val> struct Int2Type{ enum { Value = val }; };

    // This is used to mark a specific type as bool, and then later
    // avoid search for index in the table, search is done at compile time
    // So accessing the index is O(1) not O(N) as for any other type
    namespace Private
    {
        template <typename T> struct TypeSelect
        {
            template <int val> struct Inner
            {
                typedef Int2Type<val> Type;
            };
        };
        template <> struct TypeSelect<Database::Index>
        {
            template <int val> struct Inner
            {
                typedef bool Type;
            };
        };
        template <> struct TypeSelect<Database::LongIndex>
        {
            template <int val> struct Inner
            {
                typedef bool Type;
            };
        };
    }

    /** The table definition listener */
    struct TableDefinitionListener
    {
        virtual void hasBeenModified(const uint32 index, const Var & ref) = 0;
        virtual bool selectWhere(const uint32 index, const Var & ref) = 0;
        virtual ~TableDefinitionListener() {}
    };

    /** This callback is used to prevent when user apply a modification to the derived class */
    struct ModifiedCallback
    {
        /** The listener used when modified */
        TableDefinitionListener *       tdl;

        void setTDL(TableDefinitionListener * newTdl, const String & defVal) { tdl = newTdl; if (defVal.getLength()) setDefaultValue(defVal); }
        // Used for operator =
        void setTDL(TableDefinitionListener * newTdl, ModifiedCallback * other) { tdl = newTdl; if (other) setValueDirect(other->asVariant().like((String*)0)); }

        // Don't allow access to this
    protected:
        virtual void setValueDirect(const Var & value) = 0;
        virtual void setDefaultValue(const Var & value) = 0;
        virtual Var asVariant() const = 0;
        virtual bool isInit() const = 0;
        virtual bool find(const Var & value) = 0;
        // But still allow table description to call us
        friend struct TableDescription;
    };

    /** This structure map a column name and its associated type */
    struct FieldDescription
    {
        String              columnName;
        String              help;
        String              defaultValue;
        bool                isIndex;
        bool                isUnique;
        TypeID              value;
        /** The offset from the base address to the ModifiedCallback object */
        uint32              offset;

        /** Get the value position */
        ModifiedCallback *  getCB(void * table) const { return (ModifiedCallback*)((char*)table + offset); }

        FieldDescription(const String & name, TypeID typeID, uint32 offset, const String & defaultValue = "", const String & _help = "", bool isIndex = false, bool isUnique = false) : columnName(name), help(_help), defaultValue(defaultValue), value(typeID), offset(offset), isIndex(isIndex), isUnique(isUnique) {}
    };

    template <typename U, int position>
    class WriteMonitored : public ModifiedCallback
    {
        typedef typename Private::NotNullRemove<U>::Type T;
        // Members
    public:
        /** The protected data */
        T       ref;
        /** The index of this field */
        enum {  index = position };
        /** Is the object initialized (set to something interesting) */
        bool    init;

        // Interface
    public:
        /** Access this object as the template argument itself */
        inline operator const T () const { return ref; }
        /** Allow external access too */
        WriteMonitored & operator = (const T & value)
        {
            ref = value;
            init = true;
            if (tdl) tdl->hasBeenModified((uint32)index, ref);
            return *this;
        }
        /** Allow external access too */
        WriteMonitored & operator = (const WriteMonitored & value)
        {
            ref = value.ref;
            init = value.init;
            if (init && tdl) tdl->hasBeenModified((uint32)index, ref);
            return *this;
        }
        
        // Comparison operators
        bool operator == (const WriteMonitored & value) const { return ref == value.ref; }
        bool operator != (const WriteMonitored & value) const { return ref != value.ref; }
        bool operator > (const WriteMonitored & value) const { return ref > value.ref; }
        bool operator < (const WriteMonitored & value) const { return ref < value.ref; }
        bool operator >= (const WriteMonitored & value) const { return ref >= value.ref; }
        bool operator <= (const WriteMonitored & value) const { return ref <= value.ref; }
        // Comparison operators
        bool operator == (const T & value) const { return ref == value; }
        bool operator != (const T & value) const { return ref != value; }
        bool operator > (const T & value) const { return ref > value; }
        bool operator < (const T & value) const { return ref < value; }
        bool operator >= (const T & value) const { return ref >= value; }
        bool operator <= (const T & value) const { return ref <= value; }

        /** Check if this proxy is valid */
        bool isValid() const { return index >= 0; }
        /** Set the value directly.
            Don't use this as it breaks DB synchronization */
        virtual void setValueDirect(const Var & value)
        {
            ref = value.like(&ref);
            init = true;
        }
        /** Set the default value.
            Don't use this as it breaks DB synchronization */
        virtual void setDefaultValue(const Var & value) { ref = value.like(&ref); }
        /** Get the value as a variant */
        virtual Var asVariant() const { return Var(ref); }
        /** Check if a field is initialized */
        virtual bool isInit() const { return init; }
        /** Load the row where the value is like the one given.
            @warning If there are more than a row with the given value, this only loads the first one. You better use Database::find instead
            @return false if the value was not found or on error */
        virtual bool find(const Var & value) { return tdl ? tdl->selectWhere(index, value) : false; }


        // Constructor
    public:
        /** The only constructor */
        WriteMonitored() : init(false)  { this->tdl = 0; }
        /** Copying the object mutate the copied object to non functional (move semantics) */
        WriteMonitored(const WriteMonitored & common): ref(common.ref), init(common.init) { this->tdl = common.tdl; }
    };
#if !defined (_WIN32) || _MSC_VER > 1200
    template <int position>
    class WriteMonitored<unsigned int, position> : public ModifiedCallback
    {
        // Members
    public:
        /** The protected data */
        unsigned int            ref;
        /** The index of this field */
        enum { index = position };
        /** Is the object initialized (set to something interesting) */
        bool                   init;

        // Interface
    public:
        /** Access this object as the template argument itself */
        inline operator const unsigned int () const { return ref; }
        /** Allow external access too */
        WriteMonitored & operator = (const unsigned int value)
        {
            ref = value;
            init = true;
            if (tdl) tdl->hasBeenModified((uint32)index, ref);
            return *this;
        }
        /** Allow external access too */
        WriteMonitored & operator = (const Index value)
        {
            ref = (const unsigned int)value;
            init = true;
            if (tdl) tdl->hasBeenModified((uint32)index, ref);
            return *this;
        }
        /** Allow external access too */
        WriteMonitored & operator = (const WriteMonitored & value)
        {
            ref = value.ref;
            init = value.init;
            if (init && tdl) tdl->hasBeenModified((uint32)index, ref);
            return *this;
        }
        /** Allow external access too */
        template <int otherPosition>
        WriteMonitored & operator = (const WriteMonitored<Index, otherPosition> & value)
        {
            ref = value.ref;
            init = value.init;
            if (init && tdl) tdl->hasBeenModified((uint32)index, ref);
            return *this;
        }
        /*
        // Comparison operators 
        bool operator == (const WriteMonitored & value) const { return ref == value.ref; }
        bool operator != (const WriteMonitored & value) const { return ref != value.ref; }
        bool operator > (const WriteMonitored & value) const { return ref > value.ref; }
        bool operator < (const WriteMonitored & value) const { return ref < value.ref; }
        bool operator >= (const WriteMonitored & value) const { return ref >= value.ref; }
        bool operator <= (const WriteMonitored & value) const { return ref <= value.ref; }
        // Comparison operators
        bool operator == (const unsigned int value) const { return ref == value; }
        bool operator != (const unsigned int value) const { return ref != value; }
        bool operator > (const unsigned int value) const { return ref > value; }
        bool operator < (const unsigned int value) const { return ref < value; }
        bool operator >= (const unsigned int value) const { return ref >= value; }
        bool operator <= (const unsigned int value) const { return ref <= value; }        
        // Comparison operators
        bool operator == (const Index value) const { return ref == (uint32)value; }
        bool operator != (const Index value) const { return ref != (uint32)value; }
        bool operator > (const Index value) const { return ref > (uint32)value; }
        bool operator < (const Index value) const { return ref < (uint32)value; }
        bool operator >= (const Index value) const { return ref >= (uint32)value; }
        bool operator <= (const Index value) const { return ref <= (uint32)value; }
        // Enum checking 
        template <typename E> bool operator == (const typename IfEnum<E>::Type & value) const { return ref == (unsigned int)value; }
        template <typename E> bool operator != (const typename IfEnum<E>::Type & value) const { return ref != (unsigned int)value; }
        template <typename E> bool operator > (const typename IfEnum<E>::Type & value) const { return ref > (unsigned int)value; }
        template <typename E> bool operator < (const typename IfEnum<E>::Type & value) const { return ref < (unsigned int)value; }
        template <typename E> bool operator >= (const typename IfEnum<E>::Type & value) const { return ref >= (unsigned int)value; }
        template <typename E> bool operator <= (const typename IfEnum<E>::Type & value) const { return ref <= (unsigned int)value; }
*/
        /** Check if this proxy is valid */
        bool isValid() const { return index >= 0; }
        /** Set the value directly.
            Don't use this as it breaks DB synchronization */
        virtual void setValueDirect(const Var & value)
        {
            ref = value.like(&ref);
            init = true;
        }
        /** Set the default value.
            Don't use this as it breaks DB synchronization */
        virtual void setDefaultValue(const Var & value) { ref = value.like(&ref); }
        /** Get the value as a variant */
        virtual Var asVariant() const { return Var(ref); }
        /** Check if a field is initialized */
        virtual bool isInit() const { return init; }
        /** Load the row where the value is like the one given.
            @warning If there are more than a row with the given value, this only loads the first one. You better use Database::find instead
            @return false if the value was not found or on error */
        virtual bool find(const Var & value) { return tdl ? tdl->selectWhere(index, value) : false; }


        // Constructor
    public:
        /** The only constructor */
        WriteMonitored() : init(false)  { this->tdl = 0; }
        /** Copying the object mutate the copied object to non functional (move semantics) */
        WriteMonitored(const WriteMonitored & common): ref(common.ref), init(common.init) { this->tdl = common.tdl; }
    };
    template <int position>
    class WriteMonitored<int, position> : public ModifiedCallback
    {
        // Members
    public:
        /** The protected data */
        int            ref;
        /** The index of this field */
        enum { index = position };
        /** Is the object initialized (set to something interesting) */
        bool                   init;

        // Interface
    public:
        /** Access this object as the template argument itself */
        inline operator const int () const { return ref; }
        /** Allow external access too */
        WriteMonitored & operator = (const int value)
        {
            ref = value;
            init = true;
            if (tdl) tdl->hasBeenModified((uint32)index, ref);
            return *this;
        }
        /** Allow external access too */
        WriteMonitored & operator = (const Index value)
        {
            ref = (const unsigned int)value;
            init = true;
            if (tdl) tdl->hasBeenModified((uint32)index, ref);
            return *this;
        }
        /** Allow external access too */
        WriteMonitored & operator = (const WriteMonitored & value)
        {
            ref = value.ref;
            init = value.init;
            if (init && tdl) tdl->hasBeenModified((uint32)index, ref);
            return *this;
        }
        /** Allow external access too */
        template <int otherPosition>
        WriteMonitored & operator = (const WriteMonitored<Index, otherPosition> & value)
        {
            ref = value.ref;
            init = value.init;
            if (init && tdl) tdl->hasBeenModified((uint32)index, ref);
            return *this;
        }
        /*
        // Comparison operators 
        bool operator == (const WriteMonitored & value) const { return ref == value.ref; }
        bool operator != (const WriteMonitored & value) const { return ref != value.ref; }
        bool operator > (const WriteMonitored & value) const { return ref > value.ref; }
        bool operator < (const WriteMonitored & value) const { return ref < value.ref; }
        bool operator >= (const WriteMonitored & value) const { return ref >= value.ref; }
        bool operator <= (const WriteMonitored & value) const { return ref <= value.ref; }
        // Comparison operators
        bool operator == (const int value) const { return ref == value; }
        bool operator != (const int value) const { return ref != value; }
        bool operator > (const int value) const { return ref > value; }
        bool operator < (const int value) const { return ref < value; }
        bool operator >= (const int value) const { return ref >= value; }
        bool operator <= (const int value) const { return ref <= value; }        
        // Comparison operators
        bool operator == (const Index value) const { return ref == (uint32)value; }
        bool operator != (const Index value) const { return ref != (uint32)value; }
        bool operator > (const Index value) const { return ref > (uint32)value; }
        bool operator < (const Index value) const { return ref < (uint32)value; }
        bool operator >= (const Index value) const { return ref >= (uint32)value; }
        bool operator <= (const Index value) const { return ref <= (uint32)value; }
        // Enum checking 
        template <typename E> bool operator == (const typename IfEnum<E>::Type & value) const { return ref == (int)value; }
        template <typename E> bool operator != (const typename IfEnum<E>::Type & value) const { return ref != (int)value; }
        template <typename E> bool operator >  (const typename IfEnum<E>::Type & value) const { return ref > (int)value; }
        template <typename E> bool operator <  (const typename IfEnum<E>::Type & value) const { return ref < (int)value; }
        template <typename E> bool operator >= (const typename IfEnum<E>::Type & value) const { return ref >= (int)value; }
        template <typename E> bool operator <= (const typename IfEnum<E>::Type & value) const { return ref <= (int)value; }
        */

        /** Check if this proxy is valid */
        bool isValid() const { return index >= 0; }
        /** Set the value directly.
            Don't use this as it breaks DB synchronization */
        virtual void setValueDirect(const Var & value)
        {
            ref = value.like(&ref);
            init = true;
        }
        /** Set the default value.
            Don't use this as it breaks DB synchronization */
        virtual void setDefaultValue(const Var & value) { ref = value.like(&ref); }
        /** Get the value as a variant */
        virtual Var asVariant() const { return Var(ref); }
        /** Check if a field is initialized */
        virtual bool isInit() const { return init; }
        /** Load the row where the value is like the one given.
            @warning If there are more than a row with the given value, this only loads the first one. You better use Database::find instead
            @return false if the value was not found or on error */
        virtual bool find(const Var & value) { return tdl ? tdl->selectWhere(index, value) : false; }


        // Constructor
    public:
        /** The only constructor */
        WriteMonitored() : init(false)  { this->tdl = 0; }
        /** Copying the object mutate the copied object to non functional (move semantics) */
        WriteMonitored(const WriteMonitored & common): ref(common.ref), init(common.init) { this->tdl = common.tdl; }
    };

    template <int position>
    class WriteMonitored<Index, position> : public ModifiedCallback
    {
        // Members
    public:
        /** The protected data */
        Index        ref;
        /** The index of this field */
        enum { index = position };
        /** Is the object initialized (set to something interesting) */
        bool                   init;

        // Interface
    public:
        /** Access this object as the template argument itself */
        inline operator const unsigned int () const { return (unsigned int)ref; }
        /** Access this object as the template argument itself */
        inline operator const Index & () const { return ref; }

        /** Allow external access too */
        WriteMonitored & operator = (const unsigned int value)
        {
            ref = value;
            init = true;
            if (tdl) tdl->hasBeenModified((uint32)index, ref);
            return *this;
        }
        /** Allow external access too */
        WriteMonitored & operator = (const Index value)
        {
            ref = (const unsigned int)value;
            init = true;
            if (tdl) tdl->hasBeenModified((uint32)index, ref);
            return *this;
        }
        /** Allow external access too */
        WriteMonitored & operator = (const WriteMonitored & value)
        {
            ref = value.ref;
            init = value.init;
            if (init && tdl) tdl->hasBeenModified((uint32)index, ref);
            return *this;
        }
        /** Allow external access too */
        template <int otherPosition>
        WriteMonitored & operator = (const WriteMonitored<Index, otherPosition> & value)
        {
            ref = value.ref;
            init = value.init;
            if (init && tdl) tdl->hasBeenModified((uint32)index, ref);
            return *this;
        }
        // Comparison operators
        bool operator == (const WriteMonitored & value) const { return ref == value.ref; }
        bool operator != (const WriteMonitored & value) const { return ref != value.ref; }
        bool operator > (const WriteMonitored & value) const { return ref > value.ref; }
        bool operator < (const WriteMonitored & value) const { return ref < value.ref; }
        bool operator >= (const WriteMonitored & value) const { return ref >= value.ref; }
        bool operator <= (const WriteMonitored & value) const { return ref <= value.ref; }
        // Comparison operators
        bool operator == (const unsigned int value) const { return ref == value; }
        bool operator != (const unsigned int value) const { return ref != value; }
        bool operator > (const unsigned int value) const { return ref > value; }
        bool operator < (const unsigned int value) const { return ref < value; }
        bool operator >= (const unsigned int value) const { return ref >= value; }
        bool operator <= (const unsigned int value) const { return ref <= value; }        
        // Comparison operators
        bool operator == (const Index value) const { return ref == (uint32)value; }
        bool operator != (const Index value) const { return ref != (uint32)value; }
        bool operator > (const Index value) const { return ref > (uint32)value; }
        bool operator < (const Index value) const { return ref < (uint32)value; }
        bool operator >= (const Index value) const { return ref >= (uint32)value; }
        bool operator <= (const Index value) const { return ref <= (uint32)value; }

        /** Check if this proxy is valid */
        bool isValid() const { return index >= 0; }
        /** Set the value directly.
        Don't use this as it breaks DB synchronization */
        virtual void setValueDirect(const Var & value)
        {
            ref = value.like(&ref);
            init = true;
        }
        /** Set the default value.
            Don't use this as it breaks DB synchronization */
        virtual void setDefaultValue(const Var & value) { ref = value.like(&ref); }
        /** Get the value as a variant */
        virtual Var asVariant() const { return Var(ref); }
        /** Check if a field is initialized */
        virtual bool isInit() const { return init; }
        /** Load the row where the value is like the one given.
        @warning If there are more than a row with the given value, this only loads the first one. You better use Database::find instead
        @return false if the value was not found or on error */
        virtual bool find(const Var & value) { return tdl ? tdl->selectWhere(index, value) : false; }


        // Constructor
    public:
        /** The only constructor */
        WriteMonitored() : init(false)  { this->tdl = 0; }
        /** Copying the object mutate the copied object to non functional (move semantics) */
        WriteMonitored(const WriteMonitored & common): ref(common.ref), init(common.init) { this->tdl = common.tdl; }
    };

    template <int position>
    class WriteMonitored<uint64, position> : public ModifiedCallback
    {
        // Members
    public:
        /** The protected data */
        uint64                  ref;
        /** The index of this field */
        enum { index = position };
        /** Is the object initialized (set to something interesting) */
        bool                    init;

        // Interface
    public:
        /** Access this object as the template argument itself */
        inline operator const uint64 () const { return ref; }
        /** Allow external access too */
        WriteMonitored & operator = (const uint64 value)
        {
            ref = value;
            init = true;
            if (tdl) tdl->hasBeenModified((uint32)index, ref);
            return *this;
        }
        /** Allow external access too */
        WriteMonitored & operator = (const LongIndex value)
        {
            ref = (const uint64)value;
            init = true;
            if (tdl) tdl->hasBeenModified((uint32)index, ref);
            return *this;
        }
        /** Allow external access too */
        WriteMonitored & operator = (const WriteMonitored & value)
        {
            ref = value.ref;
            init = value.init;
            if (init && tdl) tdl->hasBeenModified((uint32)index, ref);
            return *this;
        }
        /** Allow external access too */
        template <int otherPosition>
        WriteMonitored & operator = (const WriteMonitored<LongIndex, otherPosition> & value)
        {
            ref = value.ref;
            init = value.init;
            if (init && tdl) tdl->hasBeenModified((uint32)index, ref);
            return *this;
        }
        /*

        // Comparison operators 
        bool operator == (const WriteMonitored & value) const { return ref == value.ref; }
        bool operator != (const WriteMonitored & value) const { return ref != value.ref; }
        bool operator > (const WriteMonitored & value) const { return ref > value.ref; }
        bool operator < (const WriteMonitored & value) const { return ref < value.ref; }
        bool operator >= (const WriteMonitored & value) const { return ref >= value.ref; }
        bool operator <= (const WriteMonitored & value) const { return ref <= value.ref; }
        // Comparison operators
        bool operator == (const uint64 value) const { return ref == value; }
        bool operator != (const uint64 value) const { return ref != value; }
        bool operator > (const uint64 value) const { return ref > value; }
        bool operator < (const uint64 value) const { return ref < value; }
        bool operator >= (const uint64 value) const { return ref >= value; }
        bool operator <= (const uint64 value) const { return ref <= value; }        
        // Comparison operators
        bool operator == (const LongIndex value) const { return ref == (uint64)value; }
        bool operator != (const LongIndex value) const { return ref != (uint64)value; }
        bool operator > (const LongIndex value) const { return ref > (uint64)value; }
        bool operator < (const LongIndex value) const { return ref < (uint64)value; }
        bool operator >= (const LongIndex value) const { return ref >= (uint64)value; }
        bool operator <= (const LongIndex value) const { return ref <= (uint64)value; }
        // Enum checking 
        template <typename E> bool operator == (const typename IfEnum<E>::Type & value) const { return ref == (uint64)value; }
        template <typename E> bool operator != (const typename IfEnum<E>::Type & value) const { return ref != (uint64)value; }
        template <typename E> bool operator >  (const typename IfEnum<E>::Type & value) const { return ref > (uint64)value; }
        template <typename E> bool operator <  (const typename IfEnum<E>::Type & value) const { return ref < (uint64)value; }
        template <typename E> bool operator >= (const typename IfEnum<E>::Type & value) const { return ref >= (uint64)value; }
        template <typename E> bool operator <= (const typename IfEnum<E>::Type & value) const { return ref <= (uint64)value; }
        */

        /** Check if this proxy is valid */
        bool isValid() const { return index >= 0; }
        /** Set the value directly.
            Don't use this as it breaks DB synchronization */
        virtual void setValueDirect(const Var & value)
        {
            ref = value.like(&ref);
            init = true;
        }
        /** Set the default value.
            Don't use this as it breaks DB synchronization */
        virtual void setDefaultValue(const Var & value) { ref = value.like(&ref); }
        /** Get the value as a variant */
        virtual Var asVariant() const { return Var(ref); }
        /** Check if a field is initialized */
        virtual bool isInit() const { return init; }
        /** Load the row where the value is like the one given.
            @warning If there are more than a row with the given value, this only loads the first one. You better use Database::find instead
            @return false if the value was not found or on error */
        virtual bool find(const Var & value) { return tdl ? tdl->selectWhere(index, value) : false; }

        // Constructor
    public:
        /** The only constructor */
        WriteMonitored() : init(false)  { this->tdl = 0; }
        /** Copying the object mutate the copied object to non functionnal (move semantics) */
        WriteMonitored(const WriteMonitored & common): ref(common.ref), init(common.init) { this->tdl = common.tdl; }
    };

    template <int position>
    class WriteMonitored<LongIndex, position> : public ModifiedCallback
    {
        // Members
    public:
        /** The protected data */
        LongIndex        ref;
        /** The index of this field */
        enum { index = position };
        /** Is the object initialized (set to something interesting) */
        bool                   init;

        // Interface
    public:
        /** Access this object as the template argument itself */
        inline operator const uint64 () const { return (const uint64)ref; }
        /** Access this object as the template argument itself */
        inline operator const LongIndex & () const { return ref; }

        /** Allow external access too */
        WriteMonitored & operator = (const unsigned int value)
        {
            ref = value;
            init = true;
            if (tdl) tdl->hasBeenModified((uint32)index, ref);
            return *this;
        }
        WriteMonitored & operator = (const uint64 value)
        {
            ref = value;
            init = true;
            if (tdl) tdl->hasBeenModified((uint32)index, ref);
            return *this;
        }
        /** Allow external access too */
        WriteMonitored & operator = (const LongIndex & value)
        {
            ref = (const unsigned int)value;
            init = true;
            if (tdl) tdl->hasBeenModified((uint32)index, ref);
            return *this;
        }
        /** Allow external access too */
        WriteMonitored & operator = (const WriteMonitored & value)
        {
            ref = value.ref;
            init = value.init;
            if (init && tdl) tdl->hasBeenModified((uint32)index, ref);
            return *this;
        }

        // Comparison operators
        bool operator == (const WriteMonitored & value) const { return ref == value.ref; }
        bool operator != (const WriteMonitored & value) const { return ref != value.ref; }
        bool operator > (const WriteMonitored & value) const { return ref > value.ref; }
        bool operator < (const WriteMonitored & value) const { return ref < value.ref; }
        bool operator >= (const WriteMonitored & value) const { return ref >= value.ref; }
        bool operator <= (const WriteMonitored & value) const { return ref <= value.ref; }
        // Comparison operators 
        bool operator == (const uint64 value) const { return ref == value; }
        bool operator != (const uint64 value) const { return ref != value; }
        bool operator > (const uint64 value) const { return ref > value; }
        bool operator < (const uint64 value) const { return ref < value; }
        bool operator >= (const uint64 value) const { return ref >= value; }
        bool operator <= (const uint64 value) const { return ref <= value; }        
        // Comparison operators
        bool operator == (const LongIndex value) const { return ref == (uint64)value; }
        bool operator != (const LongIndex value) const { return ref != (uint64)value; }
        bool operator > (const LongIndex value) const { return ref > (uint64)value; }
        bool operator < (const LongIndex value) const { return ref < (uint64)value; }
        bool operator >= (const LongIndex value) const { return ref >= (uint64)value; }
        bool operator <= (const LongIndex value) const { return ref <= (uint64)value; }
        /** Check if this proxy is valid */
        bool isValid() const { return index >= 0; }
        /** Set the value directly.
        Don't use this as it breaks DB synchronization */
        virtual void setValueDirect(const Var & value)
        {
            ref = value.like(&ref);
            init = true;
        }
        /** Set the default value.
            Don't use this as it breaks DB synchronization */
        virtual void setDefaultValue(const Var & value) { ref = value.like(&ref); }
        /** Get the value as a variant */
        virtual Var asVariant() const { return Var(ref); }
        /** Check if a field is initialized */
        virtual bool isInit() const { return init; }
        /** Load the row where the value is like the one given.
            @warning If there are more than a row with the given value, this only loads the first one. You better use Database::find instead
            @return false if the value was not found or on error */
        virtual bool find(const Var & value) { return tdl ? tdl->selectWhere(index, value) : false; }


        // Constructor
    public:
        /** The only constructor */
        WriteMonitored() : init(false)  { this->tdl = 0; }
        /** Copying the object mutate the copied object to non functional (move semantics) */
        WriteMonitored(const WriteMonitored & common): ref(common.ref), init(common.init) { this->tdl = common.tdl; }
    };

#endif

    /** This structure holds a unique table data */
    struct TableDeclaration;

    /** Let's say you have a table declared like this:
        @code
        CREATE TABLE [Users] ([ID] INTEGER AUTO_INCREMENT PRIMARY KEY,
                              [Name] TEXT NOT NULL UNIQUE,
                              [Real] TEXT NOT NULL,
                              [Password] TEXT NOT NULL,
                              [Rights] INTEGER DEFAULT 0,
                              [Lang] TEXT NOT NULL);
        @endcode

        You want to use an object like this:
        @code
            // Usage is as simple as :
            Users user;
            // Select user with ID = 3 from DB and retrieve all the other members (if it doesn't exist in DB, it's created)
            user.ID = 3;
            // Change the name of the user
            user.Name = "John Doe";
                        // Remark, the line above actually perform "UPDATE Users SET(Name) VALUES("John Doe") WHERE ID=3"

            // Change the password of the user
            user.Password = "Secret";
                        // Remark, the line above actually perform "UPDATE Users SET(Password) VALUES("Secret") WHERE ID=3"

            // Delete the user now from the database
            user.Delete();
                        // Remark, the line above actually perform "DELETE FROM Users WHERE ID=3"

            // While the above code tries to always be synchronized with the database, you might prefer to cache
            // the value until you're done with the object. It's possible only if the object has an Index field
            Users anotherUser;
            anotherUser.Name = "Joe Black";
                        // Remark, the line above doesn't perform anything as the index is unknown

            anotherUser.Password = "DontTellAnyone";
                        // Remark, the line above doesn't perform anything as the index is unknown

            anotherUser.ID = Database::Index::WantNewIndex;
                        // This actually perform "INSERT INTO Users SET(Name = "Joe Black" ...

            // If you had done this instead:
            anotherUser.ID = 3;
                        // This would have performed "UPDATE Users SET(Name ... Pass ... ) WHERE ID = 3", "SELECT * FROM Users WHERE ID = 3", and if previous command doesn't return anything, "INSERT INTO Users ..."

            // Last but not least, if you had done this instead:
            anotherUser.ID = Database::Index::DelayAction;
                        // Nothing would happen until you're actually querying a new index, or setting a physical index

        @endcode

        So, as you've understood now, the state of the object is tracked to best match the database state,
        and the opposite is true too.
        <br>
        This flexibility allow very efficient DB transfer (can be limited to the strict minimum).
        This also allow simple coding style (a developper could even "ignore" the DB abstraction below), and use
        the object like a native one.
        <br>
        For advanced developer (the one who read the documentation ;-), you'll even get the theorical minimum
        DB communication provided you use your object in a clever way, ie, setting the index at the final step.
        <br>
        Let's see how this is implemented.
        <br>
        You'll write:
        @code
        struct Users : public Table<Users>
        {
            // Database index are INTEGER PRIMARY KEY AUTOINCREMENT in all drivers
            // Beware not to insert anything between each line below
            // You could use BeginFieldDeclarationEx, if you don't intend to write a specialized constructor
            BeginFieldDeclaration(Users)
                DeclareField(ID, Database::Index);
                DeclareField(Name, String);
                DeclareField(Real, String);
                DeclareField(Password, String);
                DeclareField(Rights, int);
                DeclareField(Lang, String);
            EndFieldDeclaration

            // You can put methods here, but see below
        };
        @endcode

        The macros will expand to:
        @code
        struct Users : public Table<Users>
        {
            enum { __StartPos__ = __LINE__ }; typedef Users TableType;
                enum { __ID__ = __LINE__ - __StartPos__ - 1 }; // Line split for readability, macro doesn't split
                WriteMonitored<Database::Index, __ID__> ID; // Line split for readability, macro doesn't split
                const Database::FieldDescription & _ID() { static const Database::FieldDescription field("ID", UniversalTypeIdentified::getTypeID(( Database::Index *)0), (uint32)((void *)&ID - (void*)this)); return field; }// Line split for readability, macro doesn't split
                const FieldDescription * __fromPosition(Int2Type<__ID__> *) { return &_ID(); } // Line split for readability, macro doesn't split
                const FieldDescription * __fromName(const String & name, Int2Type<__ID__> *) { return name == "ID" ? &_ID : 0; } // Line split for readability, macro doesn't split
                const FieldDescription * __fromType(TypeID type, Int2Type<__ID__> *) { return type == UniversalTypeIdentified::getTypeID(( Database::Index *)0) ? &_ID : 0; } // Line split for readability, macro doesn't split

                enum { __Name__ = __LINE__ - __StartPos__ - 1 }; // Line split for readability, macro doesn't split
                WriteMonitored<String, __Name__> Name; // Line split for readability, macro doesn't split
                const Database::FieldDescription & _Name() { static const Database::FieldDescription field("Name", UniversalTypeIdentified::getTypeID(( String *)0), (uint32)((void *)&Name - (void*)this)); return field; }// Line split for readability, macro doesn't split
                const FieldDescription * __fromPosition(Int2Type<__Name__> *) { return &_Name; } // Line split for readability, macro doesn't split
                const FieldDescription * __fromName(const String & name, Int2Type<__Name__> *) { return name == "Name" ? &_Name : 0; } // Line split for readability, macro doesn't split
                const FieldDescription * __fromType(TypeID type, Int2Type<__Name__> *) { return type == UniversalTypeIdentified::getTypeID(( String *)0) ? &_Name : 0; } // Line split for readability, macro doesn't split

                // Same thing for other field

                // Final line below, expanded
            enum { __EndPos__ = __LINE__, FieldCount = __EndPos__ - __StartPos__ - 1; }
                FieldDescription * fromPosition(int pos)
                {
                    // Make recursive call to overloaded __fromPosition
                    switch(pos)
                    {
                    case 0: return __fromPosition(Int2Type<0>);
                    case 1: return __fromPosition(Int2Type<1>);
                    ...
                    default: return 0;
                    }
                }
                FieldDescription * fromName(const String & name)
                {
                    // Make recursive call to overloaded __fromName
                    FieldDescription * desc = 0;
                    switch(pos)
                    {
                    case 0: desc = __fromName(name, Int2Type<0>); if (desc) return desc;
                    case 1: desc = __fromName(name, Int2Type<1>); if (desc) return desc;
                    ...
                    default: return 0;
                    }
                }
                FieldDescription * fromType(TypeID type)
                {
                    // Make recursive call to overloaded __fromType
                    FieldDescription * desc = 0;
                    switch(pos)
                    {
                    case 0: desc = __fromType(type, Int2Type<0>); if (desc) return desc;
                    case 1: desc = __fromType(type, Int2Type<1>); if (desc) return desc;
                    ...
                    default: return 0;
                    }
                }
                // Construct all members now
                void init()
                {
                    for (int = 0; i < FieldCount; i++)
                    {
                        FieldDescription * desc = __fromPosition(i);
                        if (desc) ((MonitoredCallback*)((char*)this + desc->offset))->setTLD(this);
                    }
                }
                // Required template class to answer for non existing members
                template <class T>
                FieldDescription * __fromPosition(T *) { return 0; }
        };
        @endcode
        Ugly, isn't it ?
        @warning If you use BeginFieldDeclaration (and not BeginFieldDeclarationEx that write the constructor too),
                 then you must call init() in your constructor for your object to work.
        @warning Due to the macro definition, you can only have 32 members (field) in your structure
        @warning You can only have 2^32 - 2 index in a table (but that should be ok for most use)<br><br>
        @warning A common pitfall happens when you are using table with no index and no default value.<br>
                 In that case, when you set a member X while another member Y is already set, you'll get queries like:<br>
                 [ UPDATE Table SET X = 'val' WHERE Y = 'prev' ] which modifies all rows where Y = 'prev'. There is no solution to this issue in SQL.<br>
                 You could use default value in your field declaration so, at least you'll get (for the same C++ code):<br>
                 [ UPDATE Table SET X = 'val' WHERE Y = 'prev' AND X = defaultValue ]. This query (provided you always fill the entire row each time) is safer.<br><br>
                 The best solution, in that case, is to declare the table as delayed (BeginFieldDeclarationDelayEx),
                 and when you've set all the field of the indexless table, you call 'object.synchronize("Y")' which insert the whole object atomically.
    */
    struct TableDescription
    {
        // Members
    public:
        /** The table name, the same as in the database, escaped */
        String          tableName;
        /** If set, a useful comment of the table utilization */
        String          help;
        /** Should we hold data (usually false), can be set to true on construction, or just after construction */
        bool            holdData;
        /** Set to true if the object was modified */
        bool            wasModified;
        /** The database index */
        const uint32    databaseIndex;

        /** Construction */
        TableDescription(const String & name, const uint32 dbIndex, const bool delayInsert = false, const String & _help = "") : tableName(SQLFormat::escapeString(name)), holdData(delayInsert), help(_help), databaseIndex(dbIndex), wasModified(false) {}
        virtual ~TableDescription() {}


        // Required implementation in child class
    public:
        /** Get the number of field in this table */
        virtual int getFieldCount() const = 0;
        /** Check if this table has an index */
        virtual int hasIndex() const = 0;
        /** Check if this table has a long index */
        virtual bool hasLongIndex() const = 0;
        /** Get the field instance at the given position */
        virtual ModifiedCallback * getFieldInstance(const int pos) = 0;
        /** Get the field instance at the given position */
        virtual String getFieldName(const int pos) const = 0;
        /** Get the field description at the given position */
        virtual const FieldDescription * fromPosition(const int pos) const = 0;
        /** Prevent inserting / updating data (can be used as a tip to reduce SQL communication).
            You better use index with your tables */
        virtual void preventSync() { holdData = true; }
        /** Update or insert now.
            You should specify a column to synchronize upon if you need to update instead of inserting.
            @param referenceColumn  The column to synchronize upon (typically, the value of this column will be used to figure out which row
                                    to synchronize, so it must be set before calling this method). If set to an empty string a complete row is appended instead. */
        virtual void synchronize(const String & referenceColumn = "") { if (holdData && wasModified) synchronizeAllFields(referenceColumn); }

        // Helper methods to avoid code duplication
    protected:
        /** Provides a faster access to SQL command INSERT INTO */
        bool insertInto(const String & fields, const String & values);

        /** Provides a faster access to SQL command UPDATE
            @warning fieldName and fieldValue are escaped inside this method, while whereName and whereValue are not */
        bool updateWhere(const String & fieldName, const String & fieldValue, const String & whereName, const String & whereValue = "");

        /** Provides a faster access to SQL command DELETE */
        bool deleteWhere(const String & name, const String & value = "");

        /** Build Where clause from existing field, ignoring the specified field
            @return the number of filled fields including the ignored field */
        int buildWhereClause(String & whereName, const String & fieldToIgnore = "");

        /** Not implemented in this version */
        void updateReferenceIfRequired(const String & name, const String & value);

        /** Update if any field has been modified */
        bool updateIfAnyFieldModified(unsigned int indexOfIndex, unsigned int indexValue, bool & otherFieldModified);
        /** Update if any field has been modified */
        bool updateIfAnyFieldModified(unsigned int indexOfIndex, uint64 indexValue, bool & otherFieldModified);

        /** Get an string array for all filled fields
            @param name will be delete[] if not 0, and reallocated with new[]
            @param value will be delete[] if not 0, and reallocated with new[]
            @return the array size */
        int getNotEmptyFieldsNameAndValueAsArray(String * & name, String * & value);

        /** Return the SQL escaped, comma separated not empty fields */
        void getNotEmptyFieldsNameAndValue(String & name, String & value);

        /** Retrieve all the fields
            @warning Doesn't work with indexless table and default argument */
        bool retrieveAllFields(unsigned int indexOfIndex = -1);

        /** Update the database here.
            @warning With indexless table, all the caching functionalities are disabled. */
        void hasBeenModifiedImpl(const uint32 indexOfField, const Var & value);

        /** Synchronize all the fields with the DB */
        void synchronizeAllFields(const String & referenceColumn = "");

        /** Append the default value to the where given clause */
        void appendDefaultValue(String & whereClause);

        /** Retrieve the first row where the given field has the given value.
            @warning Beware that this should not be used if multiple row could have field = value, use Database::find instead
            @warning The compiler will stop here if you tried to use an invalid field */
        bool selectWhereImpl(const uint32 indexOfField, const Var & value);


        // Read-Write access
    public:
        /** Delete the field here
            @warning Works on tables without Index */
        void Delete();

        /** Reset this row */
        void Reset();

        /** Returns the index name
            @warning On indexless table, it returns empty string */
        inline String getIndexName() const { return getFieldName(hasIndex()); }

        /** Returns the index value
            @warning On indexless table, it returns -1 (not defined) or 0 (not set yet)
            @return -1 if the index field is not set yet (however this error is probably fatal, because it means that unprotected concurrent access are being made on the instance)
            @return 0 if the index is not currently valid (or not assigned)
        */
        unsigned int getIndex() const;
        /** Returns the index value
            @warning On indexless table, it returns -1 (not defined) or 0 (not set yet)
            @return -1 if the index field is not set yet (however this error is probably fatal, because it means that unprotected concurrent access are being made on the instance)
            @return 0 if the index is not currently valid (or not assigned)
        */
        uint64 getLongIndex() const;

        /** Set the field value without going through database
            @warning This defeat the whole system here, and can lead to database corruption if not used correctly */
        void setRowFieldsUnsafe(const SQLFormat::Results * res, const unsigned int index);
    };

    /** Derive from this class for implementing a table */
    template <class T>
    struct Table : public TableDescription, public TableDefinitionListener
    {
        Table(const String & name, const uint32 dbIndex, const bool delay = false, const String & help = "") : TableDescription(name, dbIndex, delay, help) {}

        void hasBeenModified(const uint32 index, const Var & ref) { this->hasBeenModifiedImpl(index, ref); }
        bool selectWhere(const uint32 index, const Var & ref) { return this->selectWhereImpl(index, ref); }
    };


#ifdef DOXYGEN
    /** Doesn't exist in reality (well, it's macro, remember?), but useful to understand how to write your object */
    namespace Macro
    {
        /** Documentation about how to declare your fields in the table.
            For example, if you want to declare a table like this:
            @code
            CREATE TABLE [Users] ([ID] INTEGER AUTO_INCREMENT PRIMARY KEY,
                                  [Name] TEXT NOT NULL,
                                  [Real] TEXT NOT NULL,
                                  [Password] TEXT,
                                  [Rights] INTEGER DEFAULT 0,
                                  [Lang] TEXT NOT NULL DEFAULT 'English');
            @endcode
            you'll write:
            @code
            // The name here must match the one in bracket above
            struct Users : public Table<Users>
            {
                // Database index are INTEGER PRIMARY KEY AUTO INCREMENT in all drivers
                // Beware not to insert anything between each line below
                // You could use BeginFieldDeclaration, if you intend to write a specialized constructor
                BeginFieldDeclarationEx(Users)
                    DeclareField(ID, Database::Index);
                    DeclareField(Name, NotNullString); // NotNullString is exactly like String
                    DeclareField(Real, NotNullString);
                    DeclareField(Password, String);
                    DeclareFieldEx(Rights, int, "0");
                    DeclareFieldEx(Lang, NotNullString, "English");
                EndFieldDeclaration

                // You can put methods here
            };
            @endcode

            Then, and due to the ease of macro, you'll get an magic object that'll reflect the change in the database and that you can use like:
            @code
            Users john;
            john.Name = "john";
            john.Real = "John Lennon";
            john.Right = 234;
            john.ID = Index::WantNewIndex;
            @endcode

        */
        namespace DeclaringFields
        {
    /** Start declaring the field of the table.
        The given name must be the struct name and the name seen in the database.
        @warning No default constructor is written, you must call init() inside your constructor.
        @warning Don't insert empty lines (or even comment), in the field declaration, as the macro is using __LINE__ for counting the fields
        @param X    The table name as seen in the database and the structure name
        @sa TableDescription for example use */
    MACRO BeginFieldDeclaration(Table X);
    /** Start declaring the field of the table.
        The given name must be the struct name and the name seen in the database.
        @warning A default constructor is written.
        @warning Don't insert empty lines (or even comment), in the field declaration, as the macro is using __LINE__ for counting the fields
        @param X    The table name as seen in the database and the structure name
        @sa TableDescription for example use */
    MACRO BeginFieldDeclarationEx(Table X);
    /** Start declaring the field of the table.
        The given name must be the struct name and the name seen in the database.
        The table isn't synchronized with the database until you call the synchronize(ColumnName) function.
        This is useful if your table doesn't have an index and you always fill all field in a block, you will then
        avoid the numerous UPDATE command for each field. You SHOULD call synchronize() when you've filled the field
        else no request will ever be made. If you have an index in the table, this delay is ignored, index are much safer.
        @warning A default constructor is written.
        @warning Don't insert empty lines (or even comment), in the field declaration, as the macro is using __LINE__ for counting the fields
        @param X    The table name as seen in the database and the structure name
        @sa TableDescription for example use */
    MACRO BeginFieldDeclarationDelayEx(Table X);
    /** Start declaring the field of the table.
        The given name must be the struct name and the name seen in the database.
        @warning A default constructor is written.
        @warning Don't insert empty lines (or even comment), in the field declaration, as the macro is using __LINE__ for counting the fields
        @param X        The table name as seen in the database and the structure name
        @param Name     The database name (must be the same as one used in the MultipleDatabaseConnection declaration
        @sa TableDescription for example use */
    MACRO BeginFieldDeclarationOnDatabase(Table X, Database Name);
    /** Start declaring the field of the table.
        The given name must be the struct name and the name seen in the database.
        The table isn't synchronized with the database until you call the synchronize(ColumnName) function.
        This is useful if your table doesn't have an index and you always fill all field in a block, you will then
        avoid the numerous UPDATE command for each field. You SHOULD call synchronize(ColumnName) when you've filled the field
        else no request will ever be made. If you have an index in the table, this delay is ignored, index are much safer.
        @warning A default constructor is written.
        @warning Don't insert empty lines (or even comment), in the field declaration, as the macro is using __LINE__ for counting the fields
        @param X    The table name as seen in the database and the structure name
        @param Name     The database name (must be the same as one used in the MultipleDatabaseConnection declaration
        @sa TableDescription for example use */
    MACRO BeginFieldDeclarationDelayOnDatabase(Table X, Database Name);
    /** Start declaring a field in the table.
        @param X    The given name will be the member name and the name seen in the database's table.
        @param TYPE The database type (guess from C++ type written here). You can use NotNullInt et al, or Index or Blob here
        @warning Don't insert empty lines (or even comment), in the field declaration, as the macro is using __LINE__ for counting the fields
        @sa TableDescription for example use */
    MACRO DeclareField(Field X, TYPE TYPE);
    /** Start declaring a field in the table. This field will be used as an index.
        You can have multiple indexes on a table. 
        If you're doing a lot of search on a table's field, then it's worth creating an index on it, else it'll slow down the insertion 
        operations, so avoid it.
        @param X        The given name will be the member name and the name seen in the database's table.
        @param TYPE     The database type (guess from C++ type written here). You can use NotNullInt et al, or Index or Blob here
        @param DFT      The default value of the field in the database (used as a string). If you're specifying a TEXT DEFAULT's value, you must escape the string with quotes
        @param UNIQUE   If set, this field must have unique value used in index
        @warning Don't insert empty lines (or even comment), in the field declaration, as the macro is using __LINE__ for counting the fields
        @sa TableDescription for example use */
    MACRO DeclareFieldWithIndex(Field X, TYPE TYPE, String DFT, bool UNIQUE);
    /** Start declaring a field in the table.
        @param X    The given name will be the member name and the name seen in the database's table.
        @param TYPE The database type (guess from C++ type written here). You can use NotNullInt et al, or Index or Blob here
        @param HLP  The help comment in the table field declaration
        @warning Don't insert empty lines (or even comment), in the field declaration, as the macro is using __LINE__ for counting the fields
        @sa TableDescription for example use */
    MACRO DeclareFieldHelp(Field X, TYPE TYPE, String HLP);
    /** Start declaring a field in the table with a default value.
        @param X    The given name will be the member name and the name seen in the database's table.
        @param TYPE The database type (guess from C++ type written here). You can use NotNullInt et al, or Index or Blob here
        @param DFT  The default value of the field in the database (used as a string). If you're specifying a TEXT DEFAULT's value, you must escape the string with quotes
        @warning Don't insert empty lines (or even comment), in the field declaration, as the macro is using __LINE__ for counting the fields
        @sa TableDescription for example use */
    MACRO DeclareFieldEx(Field X, TYPE TYPE, String DFT);
    /** Start declaring a field in the table with a default value.
        @param X    The given name will be the member name and the name seen in the database's table.
        @param TYPE The database type (guess from C++ type written here). You can use NotNullInt et al, or Index or Blob here
        @param DFT  The default value of the field in the database (used as a string). If you're specifying a TEXT DEFAULT's value, you must escape the string with quotes
        @param HLP  The help comment in the table field declaration
        @warning Don't insert empty lines (or even comment), in the field declaration, as the macro is using __LINE__ for counting the fields
        @sa TableDescription for example use */
    MACRO DeclareFieldExHelp(Field X, TYPE TYPE, String DFT, String HLP);
    /** The wrapper magic is defined here.
        @sa TableDescription for example use */
    MACRO EndFieldDeclaration();
        }
    }
#else

    #define WriteEqualOperator(X)           X & operator = (const X & __x) { for (int i = 0; i < __FieldCount__; i++) {\
                                                const ::Database::FieldDescription * ___a_desc = fromPosition(i); \
                                                if (___a_desc) { ::Database::ModifiedCallback * ___a_mc = (::Database::ModifiedCallback*)((char*)this + ___a_desc->offset); \
                                                ___a_mc->setTDL((::Database::TableDefinitionListener*)this, (::Database::ModifiedCallback*)((char*)&__x + ___a_desc->offset)); \
                                                }} return *this; }
    #define BeginFieldDeclaration(X)        enum { __DBIndex__ = 0, __StartPos__ = __LINE__ }; typedef X TableType; X() : ::Database::Table<X>(#X, __DBIndex__) { ___init(); }       WriteEqualOperator(X) X(const X & __x) : ::Database::Table<X>(#X, __DBIndex__) { ___init(); *this = __x; }       static const ::Database::String & getEscapedTableName() { static String ___a_name = ::Database::SQLFormat::escapeString(#X); return ___a_name; }
    #define BeginFieldDeclarationEx(X)      enum { __DBIndex__ = 0, __StartPos__ = __LINE__ }; typedef X TableType; X() : ::Database::Table<X>(#X, __DBIndex__) { ___init(); }       WriteEqualOperator(X) X(const X & __x) : ::Database::Table<X>(#X, __DBIndex__) { ___init(); *this = __x; }       static const ::Database::String & getEscapedTableName() { static String ___a_name = ::Database::SQLFormat::escapeString(#X); return ___a_name; }
    #define BeginFieldDeclarationDelayEx(X) enum { __DBIndex__ = 0, __StartPos__ = __LINE__ }; typedef X TableType; X() : ::Database::Table<X>(#X, __DBIndex__, true) { ___init(); } WriteEqualOperator(X) X(const X & __x) : ::Database::Table<X>(#X, __DBIndex__, true) { ___init(); *this = __x; } ~X() { synchronize(); } static const ::Database::String & getEscapedTableName() { static String ___a_name = ::Database::SQLFormat::escapeString(#X); return ___a_name; }
    // If compilation breaks here then it means that, you must declare the
    // MultipleDatabaseConnection object in the same namespace as your table, or at least,
    // inject the MultipleDBImpl object in your table's namespace using "typedef NamespaceContainingMultipleDB::MultipleDBImpl MutipleDBImpl;"
    #define BeginFieldDeclarationOnDatabase(X, Name)      enum { __DBIndex__ = MultipleDBImpl::__##Name##__, __StartPos__ = __LINE__ }; typedef X TableType; X() : ::Database::Table<X>(#X, __DBIndex__) { ___init(); }       WriteEqualOperator(X) X(const X & __x) : ::Database::Table<X>(#X, __DBIndex__) { ___init(); *this = __x; }       static const ::Database::String & getEscapedTableName() { static String ___a_name = ::Database::SQLFormat::escapeString(#X); return ___a_name; }
    #define BeginFieldDeclarationDelayOnDatabase(X, Name) enum { __DBIndex__ = MultipleDBImpl::__##Name##__, __StartPos__ = __LINE__ }; typedef X TableType; X() : ::Database::Table<X>(#X, __DBIndex__, true) { ___init(); } WriteEqualOperator(X) X(const X & __x) : ::Database::Table<X>(#X, __DBIndex__, true) { ___init(); *this = __x; } ~X() { synchronize(); } static const ::Database::String & getEscapedTableName() { static String ___a_name = ::Database::SQLFormat::escapeString(#X); return ___a_name; }

    #define DeclareField(X, TYPE)           enum { __##X##__ = __LINE__ - __StartPos__ - 1 }; ::Database::WriteMonitored<TYPE, __##X##__> X; \
                                            const ::Database::FieldDescription & _##X() const { static const ::Database::FieldDescription ___a_field(#X, ::UniversalTypeIdentifier::getTypeID(( TYPE *)0), (uint32)((const char *)&X - (const char*)this)); return ___a_field; } \
                                            static const ::Database::FieldDescription & _##X##_Incomplete() { static const ::Database::FieldDescription ___a_field(#X, ::UniversalTypeIdentifier::getTypeID(( TYPE *)0), 0); return ___a_field; } \
                                            static const ::Database::FieldDescription * __fromPositionIncomplete(::Database::Int2Type<__##X##__> *) { return &_##X##_Incomplete(); } \
                                            const ::Database::FieldDescription * __fromPosition(::Database::Int2Type<__##X##__> *) const { return &_##X(); } \
                                            const ::Database::FieldDescription * __fromName(const ::Database::String & ___a_name, ::Database::Int2Type<__##X##__> *) const { return ___a_name == #X ? &_##X() : 0; } \
                                            const ::Database::FieldDescription * __fromType(::UniversalTypeIdentifier::TypeID ___a_type, ::Database::Int2Type<__##X##__> *) const { return ___a_type == ::UniversalTypeIdentifier::getTypeID(( TYPE *)0) ? &_##X() : 0; }  \
                                            int __hasIndex(::Database::Private::TypeSelect<TYPE>::Inner<__##X##__>::Type * ) const { return (int)__##X##__; }

    #define DeclareFieldHelp(X, TYPE, HLP)  enum { __##X##__ = __LINE__ - __StartPos__ - 1 }; ::Database::WriteMonitored<TYPE, __##X##__> X; \
                                            const ::Database::FieldDescription & _##X() const { static const ::Database::FieldDescription ___a_field(#X, ::UniversalTypeIdentifier::getTypeID(( TYPE *)0), (uint32)((const char *)&X - (const char*)this), "", HLP); return ___a_field; } \
                                            static const ::Database::FieldDescription & _##X##_Incomplete() { static const ::Database::FieldDescription ___a_field(#X, ::UniversalTypeIdentifier::getTypeID(( TYPE *)0), 0); return ___a_field; } \
                                            static const ::Database::FieldDescription * __fromPositionIncomplete(::Database::Int2Type<__##X##__> *) { return &_##X##_Incomplete(); } \
                                            const ::Database::FieldDescription * __fromPosition(::Database::Int2Type<__##X##__> *) const { return &_##X(); } \
                                            const ::Database::FieldDescription * __fromName(const ::Database::String & ___a_name, ::Database::Int2Type<__##X##__> *) const { return ___a_name == #X ? &_##X() : 0; } \
                                            const ::Database::FieldDescription * __fromType(::UniversalTypeIdentifier::TypeID ___a_type, ::Database::Int2Type<__##X##__> *) const { return ___a_type == ::UniversalTypeIdentifier::getTypeID(( TYPE *)0) ? &_##X() : 0; }  \
                                            int __hasIndex(::Database::Private::TypeSelect<TYPE>::Inner<__##X##__>::Type * ) const { return (int)__##X##__; }

    #define DeclareFieldEx(X, TYPE, DFT)    enum { __##X##__ = __LINE__ - __StartPos__ - 1 }; ::Database::WriteMonitored<TYPE, __##X##__> X; \
                                            const ::Database::FieldDescription & _##X() const { static const ::Database::FieldDescription ___a_field(#X, ::UniversalTypeIdentifier::getTypeID(( TYPE *)0), (uint32)((const char *)&X - (const char *)this), DFT); return ___a_field; } \
                                            static const ::Database::FieldDescription & _##X##_Incomplete() { static const ::Database::FieldDescription ___a_field(#X, ::UniversalTypeIdentifier::getTypeID(( TYPE *)0), 0); return ___a_field; } \
                                            static const ::Database::FieldDescription * __fromPositionIncomplete(::Database::Int2Type<__##X##__> *) { return &_##X##_Incomplete(); } \
                                            const ::Database::FieldDescription * __fromPosition(::Database::Int2Type<__##X##__> *) const { return &_##X(); } \
                                            const ::Database::FieldDescription * __fromName(const ::Database::String & ___a_name, ::Database::Int2Type<__##X##__> *) const { return ___a_name == #X ? &_##X() : 0; } \
                                            const ::Database::FieldDescription * __fromType(::UniversalTypeIdentifier::TypeID ___a_type, ::Database::Int2Type<__##X##__> *) const { return ___a_type == ::UniversalTypeIdentifier::getTypeID(( TYPE *)0) ? &_##X() : 0; }  \
                                            int __hasIndex(::Database::Private::TypeSelect<TYPE>::Inner<__##X##__>::Type * ) const { return (int)__##X##__; }

    #define DeclareFieldExHelp(X, TYPE, DFT, HLP)\
                                            enum { __##X##__ = __LINE__ - __StartPos__ - 1 }; ::Database::WriteMonitored<TYPE, __##X##__> X; \
                                            const ::Database::FieldDescription & _##X() const { static const ::Database::FieldDescription ___a_field(#X, ::UniversalTypeIdentifier::getTypeID(( TYPE *)0), (uint32)((const char *)&X - (const char *)this), DFT, HLP); return ___a_field; } \
                                            static const ::Database::FieldDescription & _##X##_Incomplete() { static const ::Database::FieldDescription ___a_field(#X, ::UniversalTypeIdentifier::getTypeID(( TYPE *)0), 0, DFT, HLP); return ___a_field; } \
                                            static const ::Database::FieldDescription * __fromPositionIncomplete(::Database::Int2Type<__##X##__> *) { return &_##X##_Incomplete(); } \
                                            const ::Database::FieldDescription * __fromPosition(::Database::Int2Type<__##X##__> *) const { return &_##X(); } \
                                            const ::Database::FieldDescription * __fromName(const ::Database::String & ___a_name, ::Database::Int2Type<__##X##__> *) const { return ___a_name == #X ? &_##X() : 0; } \
                                            const ::Database::FieldDescription * __fromType(::UniversalTypeIdentifier::TypeID ___a_type, ::Database::Int2Type<__##X##__> *) const { return ___a_type == ::UniversalTypeIdentifier::getTypeID(( TYPE *)0) ? &_##X() : 0; }  \
                                            int __hasIndex(::Database::Private::TypeSelect<TYPE>::Inner<__##X##__>::Type * ) const { return (int)__##X##__; }

    #define DeclareFieldWithIndex(X, TYPE, DFT, UNIQUE)\
                                            enum { __##X##__ = __LINE__ - __StartPos__ - 1 }; ::Database::WriteMonitored<TYPE, __##X##__> X; \
                                            const ::Database::FieldDescription & _##X() const { static const ::Database::FieldDescription ___a_field(#X, ::UniversalTypeIdentifier::getTypeID(( TYPE *)0), (uint32)((const char *)&X - (const char *)this), DFT, "", true, UNIQUE); return ___a_field; } \
                                            static const ::Database::FieldDescription & _##X##_Incomplete() { static const ::Database::FieldDescription ___a_field(#X, ::UniversalTypeIdentifier::getTypeID(( TYPE *)0), 0, DFT, "", true, UNIQUE); return ___a_field; } \
                                            static const ::Database::FieldDescription * __fromPositionIncomplete(::Database::Int2Type<__##X##__> *) { return &_##X##_Incomplete(); } \
                                            const ::Database::FieldDescription * __fromPosition(::Database::Int2Type<__##X##__> *) const { return &_##X(); } \
                                            const ::Database::FieldDescription * __fromName(const ::Database::String & ___a_name, ::Database::Int2Type<__##X##__> *) const { return ___a_name == #X ? &_##X() : 0; } \
                                            const ::Database::FieldDescription * __fromType(::UniversalTypeIdentifier::TypeID ___a_type, ::Database::Int2Type<__##X##__> *) const { return ___a_type == ::UniversalTypeIdentifier::getTypeID(( TYPE *)0) ? &_##X() : 0; }  \
                                            int __hasIndex(::Database::Private::TypeSelect<TYPE>::Inner<__##X##__>::Type * ) const { return (int)__##X##__; }


    #define EndFieldDeclaration             enum { __EndPos__ = __LINE__, __FieldCount__ = __EndPos__ - __StartPos__ - 1 }; \
                                            const ::Database::FieldDescription * fromPosition(const int ___a_pos) const /* O(1) algorithm */ { switch(___a_pos) {\
                                                __Case(0, __fromPosition); __Case(1, __fromPosition); __Case(2, __fromPosition); __Case(3, __fromPosition); __Case(4, __fromPosition); __Case(5, __fromPosition); \
                                                __Case(6, __fromPosition); __Case(7, __fromPosition); __Case(8, __fromPosition); __Case(9, __fromPosition); __Case(10, __fromPosition); __Case(11, __fromPosition); \
                                                __Case(12, __fromPosition); __Case(13, __fromPosition); __Case(14, __fromPosition); __Case(15, __fromPosition); __Case(16, __fromPosition); __Case(17, __fromPosition); \
                                                __Case(18, __fromPosition); __Case(19, __fromPosition); __Case(20, __fromPosition); __Case(21, __fromPosition); __Case(22, __fromPosition); __Case(23, __fromPosition); \
                                                __Case(24, __fromPosition); __Case(25, __fromPosition); __Case(26, __fromPosition); __Case(27, __fromPosition); __Case(28, __fromPosition); __Case(29, __fromPosition); \
                                                __Case(30, __fromPosition); __Case(31, __fromPosition); default: return 0; } } \
                                            static const ::Database::FieldDescription * fromPositionIncomplete(const int ___a_pos) /* O(1) algorithm */ { switch(___a_pos) {\
                                                __Case(0, __fromPositionIncomplete); __Case(1, __fromPositionIncomplete); __Case(2, __fromPositionIncomplete); __Case(3, __fromPositionIncomplete); __Case(4, __fromPositionIncomplete); __Case(5, __fromPositionIncomplete); \
                                                __Case(6, __fromPositionIncomplete); __Case(7, __fromPositionIncomplete); __Case(8, __fromPositionIncomplete); __Case(9, __fromPositionIncomplete); __Case(10, __fromPositionIncomplete); __Case(11, __fromPositionIncomplete); \
                                                __Case(12, __fromPositionIncomplete); __Case(13, __fromPositionIncomplete); __Case(14, __fromPositionIncomplete); __Case(15, __fromPositionIncomplete); __Case(16, __fromPositionIncomplete); __Case(17, __fromPositionIncomplete); \
                                                __Case(18, __fromPositionIncomplete); __Case(19, __fromPositionIncomplete); __Case(20, __fromPositionIncomplete); __Case(21, __fromPositionIncomplete); __Case(22, __fromPositionIncomplete); __Case(23, __fromPositionIncomplete); \
                                                __Case(24, __fromPositionIncomplete); __Case(25, __fromPositionIncomplete); __Case(26, __fromPositionIncomplete); __Case(27, __fromPositionIncomplete); __Case(28, __fromPositionIncomplete); __Case(29, __fromPositionIncomplete); \
                                                __Case(30, __fromPositionIncomplete); __Case(31, __fromPositionIncomplete); default: return 0; } } \
                                            const ::Database::FieldDescription * fromName(const ::Database::String & ___a_name) const /* O(N) algorithm */ { const ::Database::FieldDescription * ___a_desc = 0; for (int ___a_pos = 0; ___a_pos < __FieldCount__; ___a_pos++) { switch(___a_pos) {\
                                                __CaseEx(0, __fromName, ___a_name); __CaseEx(1, __fromName, ___a_name); __CaseEx(2, __fromName, ___a_name); __CaseEx(3, __fromName, ___a_name); __CaseEx(4, __fromName, ___a_name); __CaseEx(5, __fromName, ___a_name); \
                                                __CaseEx(6, __fromName, ___a_name); __CaseEx(7, __fromName, ___a_name); __CaseEx(8, __fromName, ___a_name); __CaseEx(9, __fromName, ___a_name); __CaseEx(10, __fromName, ___a_name); __CaseEx(11, __fromName, ___a_name); \
                                                __CaseEx(12, __fromName, ___a_name); __CaseEx(13, __fromName, ___a_name); __CaseEx(14, __fromName, ___a_name); __CaseEx(15, __fromName, ___a_name); __CaseEx(16, __fromName, ___a_name); __CaseEx(17, __fromName, ___a_name); \
                                                __CaseEx(18, __fromName, ___a_name); __CaseEx(19, __fromName, ___a_name); __CaseEx(20, __fromName, ___a_name); __CaseEx(21, __fromName, ___a_name); __CaseEx(22, __fromName, ___a_name); __CaseEx(23, __fromName, ___a_name); \
                                                __CaseEx(24, __fromName, ___a_name); __CaseEx(25, __fromName, ___a_name); __CaseEx(26, __fromName, ___a_name); __CaseEx(27, __fromName, ___a_name); __CaseEx(28, __fromName, ___a_name); __CaseEx(29, __fromName, ___a_name); \
                                                __CaseEx(30, __fromName, ___a_name); __CaseEx(31, __fromName, ___a_name); default: return 0; } } return 0; } \
                                            const ::Database::FieldDescription * fromType(::UniversalTypeIdentifier::TypeID ___a_type) const /* O(N) algorithm */ { const ::Database::FieldDescription * ___a_desc = 0; for (int ___a_pos = 0; ___a_pos < __FieldCount__; ___a_pos++) { switch(___a_pos) {\
                                                __CaseEx(0, __fromType, ___a_type); __CaseEx(1, __fromType, ___a_type); __CaseEx(2, __fromType, ___a_type); __CaseEx(3, __fromType, ___a_type); __CaseEx(4, __fromType, ___a_type); __CaseEx(5, __fromType, ___a_type); \
                                                __CaseEx(6, __fromType, ___a_type); __CaseEx(7, __fromType, ___a_type); __CaseEx(8, __fromType, ___a_type); __CaseEx(9, __fromType, ___a_type); __CaseEx(10, __fromType, ___a_type); __CaseEx(11, __fromType, ___a_type); \
                                                __CaseEx(12, __fromType, ___a_type); __CaseEx(13, __fromType, ___a_type); __CaseEx(14, __fromType, ___a_type); __CaseEx(15, __fromType, ___a_type); __CaseEx(16, __fromType, ___a_type); __CaseEx(17, __fromType, ___a_type); \
                                                __CaseEx(18, __fromType, ___a_type); __CaseEx(19, __fromType, ___a_type); __CaseEx(20, __fromType, ___a_type); __CaseEx(21, __fromType, ___a_type); __CaseEx(22, __fromType, ___a_type); __CaseEx(23, __fromType, ___a_type); \
                                                __CaseEx(24, __fromType, ___a_type); __CaseEx(25, __fromType, ___a_type); __CaseEx(26, __fromType, ___a_type); __CaseEx(27, __fromType, ___a_type); __CaseEx(28, __fromType, ___a_type); __CaseEx(29, __fromType, ___a_type); \
                                                __CaseEx(30, __fromType, ___a_type); __CaseEx(31, __fromType, ___a_type); default: return 0; } } return 0; } \
                                            void ___init() { for (int i = 0; i < __FieldCount__; i++) {\
                                                const ::Database::FieldDescription * ___a_desc = fromPosition(i); \
                                                if (___a_desc) { ::Database::ModifiedCallback * ___a_mc = (::Database::ModifiedCallback*)((char*)this + ___a_desc->offset); \
                                                            ___a_mc->setTDL((::Database::TableDefinitionListener*)this, ___a_desc->defaultValue); \
                                                            if (___a_desc->value == ::UniversalTypeIdentifier::getTypeID((::Database::Index*)0)) holdData = false; \
                                                            if (___a_desc->value == ::UniversalTypeIdentifier::getTypeID((::Database::LongIndex*)0)) holdData = false; }}} \
                                            bool hasLongIndex() const { const ::Database::FieldDescription * ___a_desc = fromPosition(hasIndex()); return ___a_desc ? ___a_desc->value == ::UniversalTypeIdentifier::getTypeID((::Database::LongIndex*)0) : false; } \
                                            const ::Database::FieldDescription * __fromPosition(void *) const { return 0; } \
                                            static const ::Database::FieldDescription * __fromPositionIncomplete(void *) { return 0; } \
                                            const ::Database::FieldDescription * __fromName(const ::Database::String & , void *) const { return 0; } \
                                            const ::Database::FieldDescription * __fromType(::UniversalTypeIdentifier::TypeID, void *) const { return 0; } \
                                            int __hasIndex(void *) const { return -1; } int hasIndex() const { return __hasIndex((bool*)0); } \
                                            int getFieldCount() const { return __FieldCount__; } \
                                            ::Database::ModifiedCallback * getFieldInstance(const int ___a_pos) { const ::Database::FieldDescription * ___a_desc = fromPosition(___a_pos); \
                                                if (___a_desc) return (::Database::ModifiedCallback*)((char*)this + ___a_desc->offset); return 0; } \
                                            ::Database::String getFieldName(const int ___a_pos) const { const ::Database::FieldDescription * ___a_desc = fromPosition(___a_pos); \
                                                if (___a_desc) return ___a_desc->columnName; return ""; } \

#endif

    /** We don't instantiate the table description by using this class */
    struct AbstractTableDescription
    {
        /** The table name */
        String      tableName;
        /** The field count in the table */
        uint32      fieldCount;

        /** Get the abstract field description (no instance can be created from the returned object) */
        virtual const FieldDescription * getAbstractFieldDescription(const int pos) const = 0;

        AbstractTableDescription(const String & tableName, const uint32 fieldCount, const String & help = "") : tableName(tableName), fieldCount(fieldCount) {}
    };

    /** Simply refine this interface to a more defined class */
    template <typename T>
    struct AbstractTable : public AbstractTableDescription
    {
        const FieldDescription * getAbstractFieldDescription(const int pos) const
        {
            return T::fromPositionIncomplete(pos);
        }
        AbstractTable(const String & tableName, const uint32 fieldCount, const String & help = "") : AbstractTableDescription(tableName, fieldCount, help) {}
    };

    /** We want to describe the database like this:
        @code
        struct Photo : public Base<Photo>
        {
            BeginTableDeclaration(Photo) // No ; at the end, it would break compilation
                DeclareTable(Users)
                DeclareTable(Links)
            EndTableDeclaration()

            // No other member allowed
        };
        @endcode

        You can only use the object to create the model in SQLFormat::createDatabaseLikeModel,
        @cond
            And use it like this:
            @code
                Photo.connect(// The link);
                Photo.dropTables();
                Photo.createTables();
                // Remark Users and Links are not members of the struct in this case
            @endcode
        @endcond

        // Macro expands to:
        @code
        struct Photo : public Base<Photo>
        {
            typedef Photo DatabaseType; const int tableCount; Photo() : tableCount(0), Database<Photo>("Photo") {} const AbstractTableDescription * getAbstractTableArray() { static AbstractTableDescription * array = {
                &getAbstractDescription((AbstractTable<Users>*)0, "Users", tableCount++),
                &getAbstractDescription((AbstractTable<Link>*)0, "Links", tableCount++),
            }; return array; } \ // Line cut for readability, macro doesn't split
            template<class T> AbtractTableDescription & getAbstractDescription(AbstractTable<T> *, const String & name, int) const { static AbstractTable<T> item(name); return item; }
        }
        @endcode
        Not too bad ?
    */

    /** The database declaration interface */
    struct DatabaseDeclaration
    {
        virtual const AbstractTableDescription *  findTable(const uint32 index) const = 0;
        virtual const AbstractTableDescription *  findTable(const String & sName) const = 0;
        virtual uint32              getTableCount() const = 0;
        virtual const char *        getDatabaseName() const = 0;

        virtual ~DatabaseDeclaration(){}
    };

    template <typename T>
    struct Base : public DatabaseDeclaration
    {
        String name;
        String help;
        virtual const char * getDatabaseName() const { return T::databaseName(); }
        Base(const String & name, const String & help = "") : name(name), help(help) {}
    };

    /** When you declare a database with the BeginTableDeclaration macro,
        this registry is automatically filled with your database name.
        This is very useful if you intend to let the code build the database schema for you.
        You must however make sure the database name match with the one in the database connection for this to work.

        Provided you matched the database name with the declaration, creating the model on all your
        database is as easy as calling SQLFormat::createModelsForAllConnections()
    */
    struct DatabaseDeclarationRegistry
    {
        Container::WithCopyConstructor<String>::Array keys;
        Container::PlainOldData<DatabaseDeclaration *>::Array declarations;

        void registerDeclaration(const String & key, DatabaseDeclaration * decl) { keys.Append(key); declarations.Append(decl); }
        DatabaseDeclaration * getDeclaration(const String & key) { return declarations[keys.indexOf(key)]; } // Will return null pointer for bad keys
    };
    /** This is used internally to know which database model are declared.
        @sa Macro::DeclaringTables::EndTableDeclarationRegister */
    DatabaseDeclarationRegistry & getDatabaseRegistry();
    /** This one goes in pair with the DatabaseDeclarationRegistry.
        It's instantiated automatically when you use Macro::DeclaringTables::EndTableDeclarationRegister */
    struct AutoRegisterBase
    {
        DatabaseDeclaration * ptr;
        AutoRegisterBase(const String & key, DatabaseDeclaration * decl) : ptr(decl) { getDatabaseRegistry().registerDeclaration(key, decl); }
        ~AutoRegisterBase() { delete0(ptr); }
    };

#ifdef DOXYGEN
    namespace Macro
    {
        /** Documentation about how to declare your tables in the database.
            @code
            struct Photo : public Base<Photo>
            {
                BeginTableDeclaration(Photo) // No ; at the end, it would break compilation
                    DeclareTable(Users)
                    DeclareTable(Links)
                EndTableDeclaration()

                // No other member allowed
            };
            @endcode

            This is useful, only if you intend to use SQLFormat::createDatabaseLikeModel to let the code
            create the tables by itself.
        */
        namespace DeclaringTables
        {
    /** Start declaring the table in the database.
        The given name must be the struct name and the name seen in the database.
        A default constructor is written.
        @param X    The database name as seen in the database and the structure name
        @sa DatabaseDescription for example use */
    MACRO BeginTableDeclaration(Database X);
    /** Start declaring the table in the database.
        The given name must be the struct name and the name seen in the database.
        A default constructor is written.
        @param X    The database name as seen in the database and the structure name
        @param NAME The database name to use (for example if your database name can't be used as a C++ identifier)
        @sa DatabaseDescription for example use */
    MACRO BeginTableDeclaration(Database X, String NAME);
    /** Start declaring a table in the database.
        @param X    The given name will be the table name and the name seen in the database's table.
        @warning Don't put a ";" at the end of the declaration, as it breaks the array declaration the macro is writing
        @sa DatabaseDescription for example use */
    MACRO DeclareTable(Table X);
    /** Start declaring a field in the table with a default value.
        @param X    The given name will be the table name and the name seen in the database's table.
        @param HLP  The help comment in the table field declaration
        @warning Don't put a ";" at the end of the declaration, as it breaks the array declaration the macro is writing
        @sa DatabaseDescription for example use */
    MACRO DeclareTableEx(Table X, String HLP);
    /** The wrapper magic is defined here.
        @sa DatabaseDescription for example use */
    MACRO EndTableDeclaration();
    /** Auto register the base declaration to the registry.
        This is required if you intend to use SQLFormat::createModelsForAllConnections() method.<br>
        Basically, you should use this if the software is the only user of the database (or if it's the creator of the database).
        @sa DatabaseDescription for example use */
    MACRO EndTableDeclarationRegister(X);
        }
    }
#else
#define BeginTableDeclaration(X)    typedef X DatabaseType; static const char * databaseName() { return #X; } mutable int tableCount; X() : tableCount(0), ::Database::Base<X>(#X) {} \
                                        const ::Database::AbstractTableDescription ** getAbstractTableArray() const { static const ::Database::AbstractTableDescription * array[] = {
#define BeginTableDeclarationEx(X, DBName)  typedef X DatabaseType; static const char * databaseName() { return DBName; } const  mutable int tableCount; X() : tableCount(0), ::Database::Base<X>(#X) {} \
                                                const ::Database::AbstractTableDescription ** getAbstractTableArray() const { static const ::Database::AbstractTableDescription * array[] = {

#define DeclareTable(X)                 &getAbstractDescription((::Database::AbstractTable<X>*)0, #X, tableCount++),
#define DeclareTableEx(X, HLP)          &getAbstractDescription((::Database::AbstractTable<X>*)0, #X, tableCount++, HLP),
#define EndTableDeclaration         }; return array; } \
    template<class T> const ::Database::AbstractTableDescription & getAbstractDescription(::Database::AbstractTable<T> *, const ::Database::String & name, int, const ::Database::String & help = "") const { static ::Database::AbstractTable<T> item(name, T::__FieldCount__, help); return item; } \
                                    uint32 getTableCount() const { if (getAbstractTableArray()) return (uint32)tableCount; return 0; } \
                                    const ::Database::AbstractTableDescription * findTable(const uint32 index) const /** O(1) algorithm */{ const ::Database::AbstractTableDescription ** array = getAbstractTableArray(); if (index >= (uint32)tableCount) return 0; return array[index]; } \
                                    const ::Database::AbstractTableDescription * findTable(const ::Database::String & name) const /** O(N) algorithm */ { const ::Database::AbstractTableDescription ** array = getAbstractTableArray(); for (uint32 index = 0; index < (uint32)tableCount; index++) { if (array[index]->tableName == name) return array[index]; } return 0; }
#define EndTableDeclarationRegister(X)  }; return array; } \
    template<class T> const ::Database::AbstractTableDescription & getAbstractDescription(::Database::AbstractTable<T> *, const ::Database::String & name, int, const ::Database::String & help = "") const { static ::Database::AbstractTable<T> item(name, T::__FieldCount__, help); return item; } \
                                    uint32 getTableCount() const { if (getAbstractTableArray()) return (uint32)tableCount; return 0; } \
                                    const ::Database::AbstractTableDescription * findTable(const uint32 index) const /** O(1) algorithm */{ const ::Database::AbstractTableDescription ** array = getAbstractTableArray(); if (index >= (uint32)tableCount) return 0; return array[index]; } \
                                    const ::Database::AbstractTableDescription * findTable(const ::Database::String & name) const /** O(N) algorithm */ { const ::Database::AbstractTableDescription ** array = getAbstractTableArray(); for (uint32 index = 0; index < (uint32)tableCount; index++) { if (array[index]->tableName == name) return array[index]; } return 0; } \
                                    }; static ::Database::AutoRegisterBase __base_##X(#X, new X()); struct __dont_add_members_in_##X {
#endif

    /** When using multiple database at the same time,
        the usual drivers use multiple connection to the database.

        In that case, you'll like need to link which table correspond to which connection.
        While it can be error prone if done manually, the following code ensure the connection selection
        is done at compile time, and not at runtime. <br>
        This means that using tables as described in TableDescription, will still be possible, without writing "switching connection" code, or
        without having to deal with connections at all.
        <br>
        <br>
        So, a ugly code like:
        @code
            // Previous connection is on DB Two
            int data = tableInDBTwo.data;

            // Switch to DB One
            closeDBConnection(current);
            openDBConnection(DBOne);

            tableInDBOne.data = data;
        @endcode
        or
        @code
            int data = tableInDBTwo.attach(DBTwoConnection).fetchID(3).data;
            tableInDBOne.attach(DBOneConnection).newID().data = data;
        @endcode
        is avoided (it's very error prone).
        <br>
        You'll write this simple code instead:
        @code
            tableInDBOne.data = tableInDBTwo.data;
        @endcode
        <br>
        The system will automatically deals with connections by itself, restarting/destructing useless connection to ensure best resource usage and runtime speed.
        You'll have to declare which database you are using simultaneously (don't worry, you can list all your database, connections are created on the fly).
        Declaration is done with the macro described below.
        <br>
        <br>
        Then, when declaring tables and fields, simply use the Begin[...]DB macro specifying the database enum for the given table.
        <br>
        The system will then automatically select the right connection and database in O(1) time (so it can't be faster), and
        safely (with no specific code on your side) for this.
        <br>
        <br>
        <br>
        <br>
        You'll declare the multiple connection like this:
        @code
        // Beware, you can only declare this once in your code for all
        // You can't mix the connection drivers (yet), so if you're using for example, MySQL driver, you must use MySQL for all the connections
        // Don't insert empty lines (or even comment), in the declaration
        BeginDatabaseConnection
            DeclareDatabase(SetupDB, "bob:secret@192.168.1.7:3306")
            DeclareDatabase(OperatingDB, "user:password@192.168.0.234:3306")
        EndDatabaseConnection
        @endcode
        
        Even when using the MultipleDatabaseConnection, one has to call SQLFormat::initialize (probably also SQLFormat::createModelsForAllConnections), and 
        finally SQLFormat::finalize. 
        <br>
        <br>
        @warning The MultipleDatabaseConnection tell the system what database/table to use with what connection, but does not handle the system initialization
                 and finalization that still has to be done.
        @warning When using MultipleDatabaseConnection system, a static object will be instantiated before the main function is entered, so calling 
                 SQLFormat::initialize will be using the MultipleDatabaseConnection's connection factory (instead of the SingleConnection-default- one). This 
                 also means you can't have a static object doing SQLFormat::initialize in its constructor and SQLFormat::finalize in its destructor as this would 
                 probably break due to undefined static initialization order. */
    template <typename Decl>
    struct MultipleDatabaseConnection : public DatabaseConnection
    {
    private:
        void * connectionArray[Decl::__ConnectionCount__];
    public:

        /** Get the low level object used for the index-th connection */
        virtual void * getLowLevelConnection(const uint32 index = 0)
        {
            if (index >= Decl::__ConnectionCount__) return 0;
            return connectionArray[index];
        }
        /** Set the low level object used for the index-th connection */
        virtual bool setLowLevelConnection(const uint32 index, void * connection)
        {
            if (index >= Decl::__ConnectionCount__) return false;
            if (connectionArray[index]) SQLFormat::destructCreatedDatabaseConnection(connectionArray[index]);
            connectionArray[index] = connection;
            return true;
        }

        /** Get the connection parameters
            @param index    The index of the connection to retrieve
            @param dbName   On output, set to the database name to connect to
            @param dbURL    On output, set to the database URL to connect to (can contain username:password part)
            @return true on successful connection */
        virtual bool getDatabaseConnectionParameter(const uint32 index, String & dbName, String & dbURL) const
        {
            if (index >= Decl::__ConnectionCount__) return false;
            const Decl * const pT = (const Decl * const)(this);
            dbName = pT->getName((int)index);
            dbURL = pT->getURL((int)index);
            return true;
        }

        /** Create models on the database connections
            @return true on successful model creation */
        virtual bool createModels(const bool forceReinstall = false)
        {
            const Decl * const pT = (const Decl * const)(this);
            for(uint32 i = 0; i < Decl::__ConnectionCount__; i++)
            {
                const String & dbName = pT->getName((int)i);
                if (!SQLFormat::createDatabaseLikeModel(i, getDatabaseRegistry().getDeclaration(dbName), dbName, forceReinstall)) return false;
            }
            return true;
        }

        MultipleDatabaseConnection() { memset(connectionArray, 0, sizeof(connectionArray)); }
        ~MultipleDatabaseConnection() { for(uint32 i = 0; i < Decl::__ConnectionCount__; i++) { setLowLevelConnection(i, 0); } }
    };
#ifdef DOXYGEN
    /** Doesn't exist in reality (well, it's macro, remember?), but useful to understand how to write your object */
    namespace Macro
    {
        /** Documentation about how to declare multiple database connection.
            Please notice that if you're using a single database in your project (even in multiple thread), you still need to do
            this declaration if you need to create the tables in the database, or if the database name is not the same as the C++ identifier.
            <br>
            <br>
            This declaration is also useful if you plan to use multiple connection to different database (like for example,
            connecting to a development database and connecting to a production database).
            <br>
            <br>
            For example, if you want to declare a 2 connections, you'll write (in the same name as the table struct):
            @code
            BeginDatabaseConnection
                DeclareDatabase(SetupDB, "bob:secret@192.168.1.7:3306")
                DeclareDatabase(OperatingDB, "user:password@192.168.0.234:3306")
            EndDatabaseConnection
            @endcode
            This declares a unique MultipleDatabaseConnection object and instruct the DB driver to use this at runtime.
            <br>
            Don't forget to specify on which database your table are refering
            @sa Database::Macro::DeclaringFields::BeginFieldDeclarationOnDatabase
            @sa Database::MultipleDatabaseConnection
        */
        namespace DeclaringDBConnection
        {
        /** Start declaring the connections you'll need.
            A structure is created starting from this line.
            @sa MultipleDatabaseConnection for example use */
            MACRO BeginDatabaseConnection();
        /** Declare a connection to a database called name, accessible from the pointed URL.
            @param name     This is a C++ enum identifier so it can't contains invalid char for a C declaration. If your database is using a more complex name use DeclareDatabaseWithComplexDBName
            @param url      The database URL (should contains the username and password if required). This is a plain const char.
                            For SQLite, it's the path to the database file.
                            For server based database, it's the URL to the server, including port, username and password in the form "user:pw@dbIP:port"
            @note The name given is used in the following table declaration to link a table with a database connection.
            @note If the URL need to be dynamically generated (like read from a config file), you can use a const char array (or a FastString) that must be filled before the first database use is attempted.
            @sa DeclareDatabaseWithComplexDBNameDynURL for dynamic DB url
            @code
            const char basePathToDB[256] = "/path/to/DB";
            int main(...)
            {
                if (String(argv[1]) == "-config") strcpy(basePathToDB, argv[2]);
                [...]
            }

            // Then in your declaration
            [...]
                DeclareDatabase(DBOne, basePathToDB)
            [...]
            @endcode
            @sa MultipleDatabaseConnection for example use */
            MACRO DeclareDatabase(Name name, URL url);
        /** Declare a connection to a database called name, accessible from the pointed URL.
            @param name     This is a C++ enum identifier so it can't contains invalid char for a C declaration. It's used in later in the table declaration
            @param dbName   This is the real name as seen by the database (so typically the MySQL driver will emit "USE dbName;" command)
            @param url      The database URL (should contains the username and password if required). This is a plain const char.
            @note The name given is used in the following table declaration to link a table with a database connection.
            @note If the URL need to be dynamically generated (like read from a config file), you can use a const char array that must be filled before the first database use is attempted.
            @sa DeclareDatabaseWithComplexDBNameDynURL for dynamic DB url
            @sa MultipleDatabaseConnection for example use */
            MACRO DeclareDatabaseWithComplexDBName(Name name, Name dbName, URL url);
        /** Declare a connection to a database called name, accessible from the pointed dynamic URL.
            @param name     This is a C++ enum identifier so it can't contains invalid char for a C declaration. It's used in later in the table declaration
            @param dbName   This is the real name as seen by the database (so typically the MySQL driver will emit "USE dbName;" command)
            @param base     The base dynamic string that must be set up before using any database function
            @param url      The fixed-part URL (should contains the username and password if required). This is a plain const char.
                            The database URL that's used is base + url. This database URL can contain be modified in the SQL driver, following this scheme:
                            For SQLite, if the URL points to a database file, it's used directly, else if url points to a
                            directory, a file called <em>dbName</em>.db is created/used in that directory.
                            For server based database, it's the URL to the server, including port, username and password in the form "user:pw@base:port"
            @note The name given is used in the following table declaration to link a table with a database connection.
            @note If the URL need to be dynamically generated (like read from a config file), you can use a const char array (or a global FastString) that must be filled before the first database use is attempted.
            @sa MultipleDatabaseConnection for example use */
            MACRO DeclareDatabaseWithComplexDBNameDynURL(Name name, Name dbName, String base, URL url);
        /** Finish declaring the connections you'll need.
            The structure is closed here, and a static object is created to register this declaration in the DB driver.
            @sa MultipleDatabaseConnection for example use */
            MACRO EndDatabaseConnection();
        }
    }

#else
    // If compilation breaks here, it means that you've likely multiple BeginDatabaseConnection in your code (it's dangerous, and as such, forbidden)
    #define BeginDatabaseConnection         struct MultipleDBImplDecl { enum { __StartPos__ = __LINE__ };
    #define DeclareDatabase(X, URL)         enum { __##X##__ = __LINE__ - __StartPos__ - 1 }; static const char * __getName(::Database::Int2Type<__##X##__> *) { return #X; } \
                                            static const ::Database::String & __getURL(::Database::Int2Type<__##X##__> *) { static ::Database::String path = URL; return path; }
    #define DeclareDatabaseWithComplexDBName(X, TXT, URL)         \
                                            enum { __##X##__ = __LINE__ - __StartPos__ - 1 }; static const char * __getName(::Database::Int2Type<__##X##__> *) { return TXT; } \
                                            static const ::Database::String & __getURL(::Database::Int2Type<__##X##__> *) { static ::Database::String path = URL; return path; }
    #define DeclareDatabaseWithComplexDBNameDynURL(X, TXT, Base, URL)         \
                                            enum { __##X##__ = __LINE__ - __StartPos__ - 1 }; static const char * __getName(::Database::Int2Type<__##X##__> *) { return TXT; } \
                                            static const ::Database::String & __getURL(::Database::Int2Type<__##X##__> *) { static ::Database::String path = Base + URL; return path; }

    #define __Case(X, FUNC)                   case X: return FUNC((::Database::Int2Type<X>*)0);
    #define __CaseEx(X, FUNC, ARG)            case X: ___a_desc = FUNC(ARG, (::Database::Int2Type<X>*)0); if (___a_desc) return ___a_desc;

    #define EndDatabaseConnection           enum { __EndPos__ = __LINE__, __ConnectionCount__ = __EndPos__ - __StartPos__ - 1 }; \
                                            static const char * getName(const int pos) /* O(1) algorithm */ { switch(pos) {\
                                                __Case(0, __getName); __Case(1, __getName); __Case(2, __getName); __Case(3, __getName); __Case(4, __getName); __Case(5, __getName); \
                                                __Case(6, __getName); __Case(7, __getName); __Case(8, __getName); __Case(9, __getName); __Case(10, __getName); __Case(11, __getName); \
                                                __Case(12, __getName); __Case(13, __getName); __Case(14, __getName); __Case(15, __getName); __Case(16, __getName); __Case(17, __getName); \
                                                __Case(18, __getName); __Case(19, __getName); __Case(20, __getName); __Case(21, __getName); __Case(22, __getName); __Case(23, __getName); \
                                                __Case(24, __getName); __Case(25, __getName); __Case(26, __getName); __Case(27, __getName); __Case(28, __getName); __Case(29, __getName); \
                                                __Case(30, __getName); __Case(31, __getName); default: return 0; } } \
                                            static const ::Database::String & getURL(const int pos) /* O(1) algorithm */ { switch(pos) {\
                                                __Case(0, __getURL); __Case(1, __getURL); __Case(2, __getURL); __Case(3, __getURL); __Case(4, __getURL); __Case(5, __getURL); \
                                                __Case(6, __getURL); __Case(7, __getURL); __Case(8, __getURL); __Case(9, __getURL); __Case(10, __getURL); __Case(11, __getURL); \
                                                __Case(12, __getURL); __Case(13, __getURL); __Case(14, __getURL); __Case(15, __getURL); __Case(16, __getURL); __Case(17, __getURL); \
                                                __Case(18, __getURL); __Case(19, __getURL); __Case(20, __getURL); __Case(21, __getURL); __Case(22, __getURL); __Case(23, __getURL); \
                                                __Case(24, __getURL); __Case(25, __getURL); __Case(26, __getURL); __Case(27, __getURL); __Case(28, __getURL); __Case(29, __getURL); \
                                                __Case(30, __getURL); __Case(31, __getURL); default: return __getURL((void*)0); } } \
                                            static const char * __getName(void *) { return 0; } \
                                            static const ::Database::String & __getURL(void *) { static ::Database::String empty; return empty; } \
                                            }; struct MultipleDBImpl : public MultipleDBImplDecl, public ::Database::MultipleDatabaseConnection<MultipleDBImplDecl> { MultipleDBImpl() : ::Database::MultipleDatabaseConnection<MultipleDBImplDecl>() {} }; \
                                            struct MDBConnBuilder : public ::Database::BuildDatabaseConnection {  ::Database::DatabaseConnection * buildDatabaseConnection() const { return new MultipleDBImpl; } };\
                                            struct AutoRegisterMultipleDBConn { AutoRegisterMultipleDBConn() { static MDBConnBuilder builder; ::Database::SQLFormat::useDatabaseConnectionBuilder(&builder); } }; \
                                            static AutoRegisterMultipleDBConn __auto_register_multipleDBConn__;

#endif





    /** Instantiating this class enters a transaction-based query.
        Unless "shouldCommit" is called, the destructor undoes the transaction */
    class Transaction
    {
    private:
        /** Do we need to commit the transaction ? */
        bool    commit;
        /** The database index */
        uint32  index;

        /** No copy allowed */
        Transaction(const Transaction & trans);
        Transaction & operator =(const Transaction &);

        // Interface
    public:
        /** Construction start a transaction */
        Transaction (const bool & commitOnDestruction = false, const uint32 index = 0) : commit(commitOnDestruction), index(index) { SQLFormat::startTransaction(index); }
        /** Destruction commits or rollback the transaction */
        ~Transaction()  { commit ? SQLFormat::commitTransaction(index) : SQLFormat::rollbackTransaction(index); }
        /** Commit the transaction on destruction */
        inline void shouldCommit(const bool & commitOnDestruction = true)  { commit = commitOnDestruction; }
    };

    /** The special type used to avoid escaping
        @warning Don't use this, it's dangerous. */
    struct UnescapedString : public String
    {
        UnescapedString() {}
        UnescapedString(const String & str) : String(str) {}
    };
}

namespace UniversalTypeIdentifier 
{  
    // If the compiler stops here, it's because you are trying to store a table's field directly in a Variant, 
    // instead of the class itself. You should use table.field.asVariant() instead 
    template <class Type, int position> 
    TypeID Deprecated(getTypeIDImpl(Database::WriteMonitored<Type, position> * t , Bool2Type< false > *));

    template <class Type, int position> 
    TypeID getTypeIDImpl(Database::WriteMonitored<Type, position> * t , Bool2Type< false > *) 
    {
        // See above
        CompileTimeAssertFalse(Type);
        return 0;
    } 
}


#if 0
namespace UniversalTypeIdentifier
{
    // Visual studio 6.0 doesn't support template partial specialization
#ifndef _MSC_VER
    template <typename T, typename U>
    struct WarpType<Database::Reference<T,U> >
    {
        typedef Database::ReferenceBase type;
    };
#endif
}

// Aborted, it's too boring to write
#define AsPOD(X) \
    template <> \
    struct Wrapper<X> \
    { \
    private: \
        X  val; \
    public:\
        NotNull(X val) : val(val) {}\
        operator X & () { return val; }\
        operator const X & () const { return val; }\
        X operator ++() { val ++; }\
        X operator ++(int) { X tmp = val; ++val; return tmp; }\
        X operator --() { val --; }\
        X operator --(int) { int tmp = val; --val; return tmp; }\
        X operator = (const X t) { val = t; return val; }\
        inline X operator + (const X t) const { return val + t; } \
        inline X operator - (const X t) const { return val - t; } \
        inline X operator / (const X t) const { return val / t; } \
        inline X operator << (const X t) const { return val << t; } \
        inline X operator >> (const X t) const { return val >> t; } \
        inline X operator % (const X t) const { return val % t; } \
        inline X operator & (const X t) const { return val & t; } \
        inline X operator ^ (const X t) const { return val ^ t; } \
        inline X operator | (const X t) const { return val | t; } \
        inline bool operator == (const X t) const { return val == t; }\
        inline bool operator != (const X t) const { return val != t; }\
        inline bool operator < (const X t) const { return val < t; }\
        inline bool operator > (const X t) const { return val > t; }\
        inline bool operator <= (const X t) const { return val <= t; }\
        inline bool operator >= (const X t) const { return val >= t; }\
    };

//    AsPOD(int);
//    AsPOD(unsigned int);
//    AsPOD(double);
//    AsPOD(int64);
#undef AsPOD
#endif



#endif
