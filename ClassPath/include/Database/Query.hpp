#ifndef hpp_Query_hpp
#define hpp_Query_hpp

// We need database declaration
#include "Database.hpp"
// We also need constraint declaration
#include "Constraints.hpp"

namespace Database
{
/** When the Constraint code is not flexible enough for your request, you might want to deal with low level SQL code.
    This implies that your requests are no more compile-time checked for correctness, and that the database driver might 
    fail to understand your requests.
    
    This system uses Argument Dependent Lookup to inject the required types into your code, so it's a very bad idea to 
    inject the whole namespace into yours. */
namespace Query
{
    /** The unsafe row iterator.
        This is used to access the results of a Select query in an unsafe way. 
        Beware that this class is using move semantics for fast access to the query results and low memory usage.
        This means that you can not re-use the object if you copy it (with copy constructor or operator =), the initial 
        object is changed and the result pool is moved to the destination.
        
        You can access the row from your request using the operator[] like this:
        @code
            Database::UnsafeRowIterator iter = Database::Query::Select("ID", "Age").From("Car");
            while (iter) 
            { 
                printf("%s | %s\n", (const char*)iter["ID"], (const char*)iter["Age”]);
                ++iter; 
            }
        @endcode
        */
    class UnsafeRowIterator
    {
        const SQLFormat::Results * res;
        unsigned int rowIndex; 
    public:
        /** Main access operator */
        String operator [] (const String & fieldName) const { Var result; if (SQLFormat::getResults(res, result, rowIndex, fieldName)) return result.like((String*)0); const_cast<unsigned int &>(rowIndex) = (unsigned int)-1; return ""; }
        /** Main access operator */
        String operator [] (const char * fieldName) const { Var result; if (SQLFormat::getResults(res, result, rowIndex, fieldName)) return result.like((String*)0); const_cast<unsigned int &>(rowIndex) = (unsigned int)-1; return ""; }
        /** Iterator syntax */
        UnsafeRowIterator & operator ++() { Var result; if (SQLFormat::getResults(res, result, rowIndex+1, "")) ++rowIndex; else rowIndex = (unsigned int)-1; return *this; }
        /** Check if it's valid */
        operator bool() const { return (int)rowIndex >= 0; }
        /** Similar method for checking operator validity */
        inline bool isValid() const { return (int)rowIndex >= 0; }
        /** Transitive move */
        UnsafeRowIterator & operator = (const UnsafeRowIterator & other)
        {
            if (&other != this)
            {
                SQLFormat::cleanResults(res);
                res = other.res;
                rowIndex = other.rowIndex;
                const_cast<UnsafeRowIterator&>(other).res = 0;
            }
            return *this;
        }
    
        /** Construction */
        UnsafeRowIterator(const SQLFormat::Results * res) : res(res), rowIndex(0) { Var result; if (!SQLFormat::getResults(res, result, 0, "")) rowIndex = (unsigned int)-1; }
        /** Move semantics */
        UnsafeRowIterator(const UnsafeRowIterator & other) : res(other.res), rowIndex(other.rowIndex) { const_cast<UnsafeRowIterator&>(other).res = 0; }
        /** Destructor */
        ~UnsafeRowIterator() { SQLFormat::cleanResults(res); }
    };

    namespace Private
    {
        /** An operator plus argument */
        struct OpArg { String op, arg; OpArg(const String & op = "", const String & arg = "") : op(op), arg(arg) {} };
        /** The current query */
        typedef Container::WithCopyConstructor<OpArg>::Array QueryArrayT;

        /** Check if a variant can be expressed as a basic type requiring no escaping. */
        static inline String escapeField(const Var & value)
        {
            String str = value.like(&str);
            /*
            // Try integer type first
#define     TestPOD(X) value.isExactly((X*)0) || value.isExactly((const X*)0)
#define     TestIntPOD(X) TestPOD(signed X) || TestPOD(unsigned X)

            if (TestIntPOD(int) || TestIntPOD(long) || TestIntPOD(long long) || TestIntPOD(short) || TestIntPOD(char) 
                || TestPOD(bool) || TestPOD(double) || TestPOD(float))
                */
            if (value.isPOD())
                return SQLFormat::escapeString(str);
            else 
                return SQLFormat::escapeString(str, '\'');
        }
        
        static void addCondOp(QueryArrayT & query, const String & field, const String & alias)
        {
            if (query.getSize() && query[query.getSize() - 1].op == "")
                query.Append(OpArg("", ", " + SQLFormat::escapeString(field) + " AS " + SQLFormat::escapeString(alias)));
            else
                query.Append(OpArg("", SQLFormat::escapeString(field) + " AS " + SQLFormat::escapeString(alias)));
        }
        
        static void addOp(QueryArrayT & query, const String & op, const String & val)
        {
            query.Append(OpArg(op, SQLFormat::escapeString(val)));
        }
        static void addOp(QueryArrayT & query, const String & op, const String & val, bool parenthesis)
        {
            if (query.getSize() && query[query.getSize() - 1].op == "")
                query.Append(OpArg("", ", " + op + SQLFormat::escapeString(val) + ") "));
            else
                query.Append(OpArg("", op + SQLFormat::escapeString(val) + ") "));
        }
        static void addOp(QueryArrayT & query, const String & op, const String & val, const String & as)
        {
            if (query.getSize() && query[query.getSize() - 1].op == "")
                query.Append(OpArg("", ", " + op + SQLFormat::escapeString(val) + ") AS " + SQLFormat::escapeString(as)));
            else
                query.Append(OpArg("", op + SQLFormat::escapeString(val) + ") AS " + SQLFormat::escapeString(as)));
        }
        static void addOp(QueryArrayT & query, const String & op, const String & val, const String & delim, const String & val2)
        {
            query.Append(OpArg(op, SQLFormat::escapeString(val) + delim + SQLFormat::escapeString(val2)));
        }
                
        static void addOp(QueryArrayT & query, const String & op, const Var & value)
        {
            query.Append(OpArg(op, escapeField(value)));
        }

        static void addOp(QueryArrayT & query, const String & op, const Var & value, const String & delim, const Var & val2)
        {
            String val2Str = val2.like(&val2Str);
            query.Append(OpArg(op, escapeField(value) + (val2Str ? (delim + escapeField(val2)) : String(""))));
        }

        
        static void getFinalText(String & result, const QueryArrayT & query, const String & actionName, const size_t fromPos, const size_t wherePos, const String & tableName)
        {
            result = actionName;
            bool fromInc = false;
            for (size_t i = 0; i < query.getSize(); i++)
            {
                const OpArg & op = query.getElementAtPosition(i);
                if ((fromPos == (size_t)-1 && i == wherePos) || i == fromPos)
                {
                    result += " FROM " + tableName + " ";
                    fromInc = true;
                }
                result += op.op + op.arg;
            }
            if ((fromPos == (size_t)-1 || fromPos == query.getSize()) && tableName && !fromInc)
            {
                result += " FROM " + tableName + " ";
            }
        }
        static void getFinalCountText(String & result, const QueryArrayT & query, const String & actionName, const size_t fromPos, const size_t wherePos, const String & tableName)
        {
            String subQuery;
            getFinalText(subQuery, query, actionName, fromPos, wherePos, tableName);
            result = "SELECT COUNT(*) AS _X_countRows FROM (" + subQuery + ") ";
        }
        static bool getFinalTextWithCount(String & result, const QueryArrayT & query, const String & actionName, const size_t fromPos, const size_t wherePos, const String & tableName)
        {
            if (actionName != "SELECT ") return false;
            
            String subQuery;
            getFinalText(subQuery, query, actionName, fromPos, wherePos, tableName);
            
            result = "SELECT ";
            bool fromInc = false;
            for (size_t i = 0; i < query.getSize(); i++)
            {
                const OpArg & op = query.getElementAtPosition(i);
                if ((fromPos == (size_t)-1 && i == wherePos) || i == fromPos)
                {
                    result += ", (SELECT COUNT(*) FROM (" + subQuery + ")) AS xZ_X_Count_T823 FROM " + tableName + " ";
                    fromInc = true;
                }
                    
                result += op.op + op.arg;
            }
            if ((fromPos == (size_t)-1 || fromPos == query.getSize()) && tableName && !fromInc)
                result += ", (SELECT COUNT(*) FROM (" + subQuery + ")) AS xZ_X_Count_T823 FROM " + tableName + " ";
            return true;
        }
    }
    
    /** This is used to avoid escaping the fields in comparison operators */
    struct FieldString
    {
        String val;
        explicit FieldString(const String & val) : val(val) {}
        const String & toString() const { return val; }
    };
/** This macro is used to avoid injecting the FieldString object when non escaped strings are required */
#define _U(Field) ::Database::Query::FieldString(Field)
 
    /** This is used in the templated scheme below to avoid duplicating the code */
    template <class T>
    class SelectBase
    {
        // Type definition and enumeration
    protected:
        
        // Members
    protected:
        /** The current query */
        Private::QueryArrayT query;
        /** The table name to select from */
        String tableName;
        /** The where position in the argument list */
        size_t wherePos;
        /** The from position in the argument list */
        size_t fromPos;
        /** Requiring unsafe iteration */
        bool unsafeIteration;

        // Base interface
    protected:
        /** The default operator name (can be SELECT or DELETE) */
        const String getActionName() const { return "SELECT "; }
        /** Making the statement out of the operation stack */
        ForcedInline(String getFinalText() const) { String result; Private::getFinalText(result, query, ((T*)this)->getActionName(), this->fromPos, this->wherePos, this->tableName); return result; }
        /** Making the statement out of the operation stack */
        ForcedInline(String getFinalTextWithCount() const) { String result; Private::getFinalTextWithCount(result, query, ((T*)this)->getActionName(), this->fromPos, this->wherePos, this->tableName); return result; }
        /** Changing the code a little bit to count the results */
        ForcedInline(String getCountText() const) { String result; Private::getFinalCountText(result, query, ((T*)this)->getActionName(), this->fromPos, this->wherePos, this->tableName); return result; }
        /** Allow all select to access our method */
        template<class U> friend class SelectBase;
        /** Allow the create temporary table must access the compiled statement */
        friend class CreateTempTable;

        // Interface
    public:
        /** The equal operator.
            Beware that this only works if you start with a Select statement */
        ForcedInline(T & operator == (const Var & value)) { Private::addOp(query, " = ", value); return (T&)*this; }
        /** The different operator.
            Beware that this only works if you start with a Select statement */
        ForcedInline(T & operator != (const Var & value)) { Private::addOp(query, " <> ", value); return (T&)*this; }
        /** The less or equal operator.
            Beware that this only works if you start with a Select statement */
        ForcedInline(T & operator <= (const Var & value)) { Private::addOp(query, " <= ", value); return (T&)*this; }
        /** The greater or equal operator.
            Beware that this only works if you start with a Select statement */
        ForcedInline(T & operator >= (const Var & value)) { Private::addOp(query, " >= ", value); return (T&)*this; }
        /** The less operator.
            Beware that this only works if you start with a Select statement */
        ForcedInline(T & operator < (const Var & value)) { Private::addOp(query, " < ", value); return (T&)*this; }
        /** The greater operator.
            Beware that this only works if you start with a Select statement */
        ForcedInline(T & operator > (const Var & value)) { Private::addOp(query, " > ", value); return (T&)*this; }
        
        /** The equal operator.
            Beware that this only works if you start with a Select statement */
        ForcedInline(T & operator == (const FieldString & value)) { Private::addOp(query, " = ", value.toString()); return (T&)*this; }
        /** The different operator.
            Beware that this only works if you start with a Select statement */
        ForcedInline(T & operator != (const FieldString & value)) { Private::addOp(query, " <> ", value.toString()); return (T&)*this; }
        /** The less or equal operator.
            Beware that this only works if you start with a Select statement */
        ForcedInline(T & operator <= (const FieldString & value)) { Private::addOp(query, " <= ", value.toString()); return (T&)*this; }
        /** The greater or equal operator.
            Beware that this only works if you start with a Select statement */
        ForcedInline(T & operator >= (const FieldString & value)) { Private::addOp(query, " >= ", value.toString()); return (T&)*this; }
        /** The less operator.
            Beware that this only works if you start with a Select statement */
        ForcedInline(T & operator < (const FieldString & value)) { Private::addOp(query, " < ", value.toString()); return (T&)*this; }
        /** The greater operator.
            Beware that this only works if you start with a Select statement */
        ForcedInline(T & operator > (const FieldString & value)) { Private::addOp(query, " > ", value.toString()); return (T&)*this; }

        
        /** The bit and operator.
            Beware that this only works if you start with a Select statement */
        ForcedInline(T & operator & (const Var & value)) { Private::addOp(query, " & ", value);  return (T&)*this; }
        /** The bit or operator.
            Beware that this only works if you start with a Select statement */
        ForcedInline(T & operator | (const Var & value)) { Private::addOp(query, " | ", value);  return (T&)*this; }
        /** The bit xor operator.
            Beware that this only works if you start with a Select statement */
        ForcedInline(T & operator ^ (const Var & value)) { Private::addOp(query, " ^ ", value);  return (T&)*this; }
        /** The arithmetic operators.
            Beware that this only works if you start with a Select statement */
        ForcedInline(T & operator + (const Var & value)) { Private::addOp(query, " + ", value);  return (T&)*this; }
        ForcedInline(T & operator - (const Var & value)) { Private::addOp(query, " - ", value);  return (T&)*this; }
        ForcedInline(T & operator / (const Var & value)) { Private::addOp(query, " / ", value);  return (T&)*this; }
        ForcedInline(T & operator * (const Var & value)) { Private::addOp(query, " * ", value);  return (T&)*this; }
        /** The not operator.
            Beware that this only works if you start with a Select statement */
        ForcedInline(T & operator ! ()) { Private::addOp(query, " NOT ", String("")); return (T&)*this; }
        
        // Name based operators
        /** The column operator.
            Beware that this only works if you start with a Select statement.
            @warning This is only used for specifying fields.
                     Typically, this will work as expected unless you pass binary data here */
        ForcedInline(T & Field (const String & name)) { Private::addOp(query, "", ", " + name); return (T&)*this; }
        /** The alias operator.
            Beware that this only works if you start with a Select statement.
            @warning This is only used for specifying fields */
        ForcedInline(T & Alias (const String & name, const String & alias)) { Private::addCondOp(query, name, alias); return (T&)*this; }
        /** FROM Table selection */
        ForcedInline(T & From(const String & name)) { fromPos = query.getSize(); tableName = name; return (T&)*this; }
        /** FROM clause for another select statement. */
        template<class U>
        T & From(const SelectBase<U> & statement) { fromPos = query.getSize(); tableName = "(" + statement.getFinalText() + ")"; return (T&)*this; }
        /** WHERE clause */
        ForcedInline(T & Where(const String & name)) { wherePos = query.getSize(); Private::addOp(query, " WHERE ", name); return (T&)*this; }
        /** WHERE clause */
        template<class U>
        T & Where(const SelectBase<U> & statement) { wherePos = query.getSize(); query.Append(Private::OpArg(" WHERE (" + statement.getFinalText() +") ")); return (T&)*this; }
        /** DISTINCT clause.
            @warning If you need to call use this, then the SelectBase first field must be empty */
        ForcedInline(T & Distinct(const String & name)) { Private::addOp(query, " DISTINCT ", name); return (T&)*this; }
        /** MAX clause.
            @warning If you use this, then you must use an UnsafeRowIterator */
        ForcedInline(T & Max(const String & name)) { unsafeIteration = true; Private::addOp(query, " MAX( ", name, true); return (T&)*this; }
        /** MIN clause.
            @warning If you use this, then you must use an UnsafeRowIterator */
        ForcedInline(T & Min(const String & name)) { unsafeIteration = true; Private::addOp(query, " MIN( ", name, true); return (T&)*this; }
        /** COUNT clause.
            @warning If you use this, then you must use an UnsafeRowIterator */
        ForcedInline(T & Count(const String & name)) { unsafeIteration = true; Private::addOp(query, " COUNT( ", name, true); return (T&)*this; }
        /** MAX clause used like "MAX(name) AS as". */
        ForcedInline(T & Max(const String & name, const String & as)) { Private::addOp(query, " MAX( ", name, as); return (T&)*this; }
        /** MIN clause used like "MIN(name) AS as". */
        ForcedInline(T & Min(const String & name, const String & as)) { Private::addOp(query, " MIN( ", name, as); return (T&)*this; }
        /** COUNT clause used like "COUNT(name) AS as". */
        ForcedInline(T & Count(const String & name, const String & as)) { Private::addOp(query, " COUNT( ", name, as); return (T&)*this; }
        /** GROUP BY clause. */
        ForcedInline(T & GroupBy(const String & name)) { Private::addOp(query, " GROUP BY ", name); return (T&)*this; }
        /** HAVING clause.*/
        ForcedInline(T & Having(const String & name)) { Private::addOp(query, " HAVING ", name); return (T&)*this; }
        /** LIMIT clause. */
        ForcedInline(T & Limit(const Var & value, const Var & offset = "")) { Private::addOp(query, " LIMIT ", value, ", ", offset); return (T&)*this; }
        /** ORDER BY clause. */
        ForcedInline(T & OrderBy(const String & name, const bool ascending)) { Private::addOp(query, " ORDER BY ", name + (ascending ? " ASC " : " DESC")); return (T&)*this; }
        /** ORDER BY clause. */
        ForcedInline(T & OrderBy(const String & name, const bool ascending, const String & otherField, const bool otherAscending)) { Private::addOp(query, " ORDER BY ", name + (ascending ? " ASC " : " DESC"), ", ", otherField + (otherAscending ? " ASC " : " DESC")); return (T&)*this; }
        /** LIKE clause. */
        ForcedInline(T & Like(const String & name)) { Private::addOp(query, " LIKE ", name); return (T&)*this; }
        /** NOT LIKE clause. */
        ForcedInline(T & NotLike(const String & name)) { Private::addOp(query, " NOT LIKE ", name); return (T&)*this; }
        /** BETWEEN clause. */
        ForcedInline(T & Between(const Var & value, const Var & other)) { Private::addOp(query, " BETWEEN ", value, " AND ", other); return (T&)*this; }
        /** IS NULL clause. */
        ForcedInline(T & IsNull()) { Private::addOp(query, " IS NULL ", String("")); return (T&)*this; }
        /** IS NOT NULL clause. */
        ForcedInline(T & IsNotNull()) { Private::addOp(query, " IS NOT NULL ", String("")); return (T&)*this; }
        /** AND clause.
            @sa sP and eP for starting and ending parenthesis if the default operator precedence does not fit you */
        ForcedInline(T & And(const String & name)) { Private::addOp(query, " AND ", name); return (T&)*this; }
        /** OR clause.
            @sa sP and eP for starting and ending parenthesis if the default operator precedence does not fit you */
        ForcedInline(T & Or(const String & name)) { Private::addOp(query, " OR ", name); return (T&)*this; }
        /** AND clause.
            @sa sP and eP for starting and ending parenthesis if the default operator precedence does not fit you */
        template<class U>
        T & And(const SelectBase<U> & statement) { query.Append(Private::OpArg(" AND (", statement.getFinalText() + ") ")); return (T&)*this; }
        /** OR clause.
            @sa sP and eP for starting and ending parenthesis if the default operator precedence does not fit you */
        template<class U>
        T & Or(const SelectBase<U> & statement) { query.Append(Private::OpArg(" OR (", statement.getFinalText() + ") ")); return (T&)*this; }

        /** INNER JOIN clause.
            This is equivalent to selecting from multiple table, and doing a "WHERE tableA.field = tableB.otherField".
            The syntax however allow to separate real filtering conditions in the WHERE clause from the intersect-ing condition. 
            @sa On */
        ForcedInline(T & InnerJoin(const String & name)) { Private::addOp(query, " INNER JOIN ", name); return (T&)*this; }
        /** FULL OUTER JOIN clause. 
            This is equivalent to selecting from multiple table, and doing a "WHERE tableA.field = tableB.otherField" except 
            that if any field in tableA does not have a matching value in tableB, it's still returned (the missing field in table B 
            is replaced by NULL), and the same applies to tableA (null if field is missing).
            @sa On */
        ForcedInline(T & FullOuterJoin(const String & name)) { Private::addOp(query, " FULL OUTER JOIN ", name); return (T&)*this; }
        /** LEFT OUTER JOIN clause.
            This is equivalent to selecting from multiple table, and doing a "WHERE tableA.field = tableB.otherField" except 
            that if any field in tableA does not have a matching value in tableB, it's still returned (the missing field in table B 
            is replaced by NULL).
            @sa On */
        ForcedInline(T & LeftOuterJoin(const String & name)) { Private::addOp(query, " LEFT OUTER JOIN ", name); return (T&)*this; }
        
        /** ON clause. 
            This is to be used on joining tables.
            If you need to use the other operators, like = or != with field names (like in ON tableA.b_id = tableB.id) 
            then you will have to use the _U() macro to avoid escaping the operator value.
            @sa InnerJoin, FullOuterJoin, LeftOuterJoin */
        ForcedInline(T & On(const String & name)) { Private::addOp(query, " ON ", name); return (T&)*this; }
        
        
        /** Open parenthesis clause. */
        ForcedInline(T & sP()) { Private::addOp(query, "(", String("")); return (T&)*this; }
        /** Close parenthesis clause. */
        ForcedInline(T & eP()) { Private::addOp(query, ")", String("")); return (T&)*this; }
        


        /** IN clause. */
        ForcedInline(T & In(const Var & value)) { Private::addOp(query, " IN(", value, "", ") "); return (T&)*this; }
        /** IN clause for a fixed range. */
        ForcedInline(T & In(const Strings::StringArray & set)) { String array = set.Join(", "); Private::addOp(query, " IN(", array + ") "); return (T&)*this; }
        /** IN clause for another select statement. */
        template<class U>
        T & In(const SelectBase<U> & statement) { query.Append(Private::OpArg(" IN(", statement.getFinalText() + ") ")); return (T&)*this; }
        
        /** NOT IN clause. */
        ForcedInline(T & NotIn(const Var & value)) { Private::addOp(query, " NOT IN(", value, "", ") "); return (T&)*this; }
        /** NOT IN clause for a fixed range. */
        ForcedInline(T & NotIn(const Strings::StringArray & set)) { String array = set.Join(", "); Private::addOp(query, " NOT IN(", array + ") "); return (T&)*this; }
        /** NOT IN clause for another select statement. */
        template<class U>
        T & NotIn(const SelectBase<U> & statement) { query.Append(Private::OpArg(" NOT IN(", statement.getFinalText() + ") ")); return (T&)*this; }
   
        /** UNION clause for another select statement. */
        template<class U>
        T & Union(const SelectBase<U> & statement) { query.Append(Private::OpArg(" UNION ", statement.getFinalText() + " ")); return (T&)*this; }
        /** UNION ALL clause for another select statement. */
        template<class U>
        T & UnionAll(const SelectBase<U> & statement) { query.Append(Private::OpArg(" UNION ALL ", statement.getFinalText() + " ")); return (T&)*this; }
             
        
    
#ifndef DOXYGEN
        /** The equal operator.
            Beware that this only works if you start with a Select statement */
        template <typename U, int pos>
        T & operator == (const WriteMonitored<U, pos> & value) { Private::addOp(query, " = ", value.asVariant()); return (T&)*this; }
        /** The different operator.
            Beware that this only works if you start with a Select statement */
        template <typename U, int pos>
        T & operator != (const WriteMonitored<U, pos> & value) { Private::addOp(query, " <> ", value.asVariant()); return (T&)*this; }
        /** The less or equal operator.
            Beware that this only works if you start with a Select statement */
        template <typename U, int pos>
        T & operator <= (const WriteMonitored<U, pos> & value) { Private::addOp(query, " <= ", value.asVariant()); return (T&)*this; }
        /** The greater or equal operator.
            Beware that this only works if you start with a Select statement */
        template <typename U, int pos>
        T & operator >= (const WriteMonitored<U, pos> & value) { Private::addOp(query, " >= ", value.asVariant()); return (T&)*this; }
        /** The less operator.
            Beware that this only works if you start with a Select statement */
        template <typename U, int pos>
        T & operator < (const WriteMonitored<U, pos> & value) { Private::addOp(query, " < ", value.asVariant()); return (T&)*this; }
        /** The greater operator.
            Beware that this only works if you start with a Select statement */
        template <typename U, int pos>
        T & operator > (const WriteMonitored<U, pos> & value) { Private::addOp(query, " > ", value.asVariant()); return (T&)*this; }
        /** The bit and operator.
            Beware that this only works if you start with a Select statement */
        template <typename U, int pos>
        T & operator & (const WriteMonitored<U, pos> & value) { Private::addOp(query, " & ", value.asVariant());  return (T&)*this; }
        /** The bit or operator.
            Beware that this only works if you start with a Select statement */
        template <typename U, int pos>
        T & operator | (const WriteMonitored<U, pos> & value) { Private::addOp(query, " | ", value.asVariant());  return (T&)*this; }
        /** The bit xor operator.
            Beware that this only works if you start with a Select statement */
        template <typename U, int pos>
        T & operator ^ (const WriteMonitored<U, pos> & value) { Private::addOp(query, " ^ ", value.asVariant());  return (T&)*this; }
        /** The arithmetic operators.
            Beware that this only works if you start with a Select statement */
        template <typename U, int pos>
        T & operator + (const WriteMonitored<U, pos> & value) { Private::addOp(query, " + ", value.asVariant());  return (T&)*this; }
        template <typename U, int pos>
        T & operator - (const WriteMonitored<U, pos> & value) { Private::addOp(query, " - ", value.asVariant());  return (T&)*this; }
        template <typename U, int pos>
        T & operator / (const WriteMonitored<U, pos> & value) { Private::addOp(query, " / ", value.asVariant());  return (T&)*this; }
        template <typename U, int pos>
        T & operator * (const WriteMonitored<U, pos> & value) { Private::addOp(query, " * ", value.asVariant());  return (T&)*this; }
        /** LIMIT clause. */
        template <typename U, int pos>
        T & Limit(const WriteMonitored<U, pos> & value, const WriteMonitored<U, pos> & offset) { Private::addOp(query, " LIMIT ", value.asVariant(), ", ", offset.asVariant()); return (T&)*this; }
        /** BETWEEN clause. */
        template <typename U, int pos>
        T & Between(const WriteMonitored<U, pos> & value, const WriteMonitored<U, pos> & other) { Private::addOp(query, " BETWEEN ", value.asVariant(), " AND ", other.asVariant()); return (T&)*this; }
        /** IN clause. */
        template <typename U, int pos>
        T & In(const WriteMonitored<U, pos> & value) { Private::addOp(query, " IN(", value.asVariant(), "", ") "); return (T&)*this; }
        /** NOT IN clause. */
        template <typename U, int pos>
        T & NotIn(const WriteMonitored<U, pos> & value) { Private::addOp(query, " NOT IN(", value.asVariant(), "", ") "); return (T&)*this; }
#endif
        
        // Construction and destruction
    public:
        /** Basic construction with a single field */
        SelectBase(const String & fieldName = "") : wherePos((size_t)-1), fromPos((size_t)-1), unsafeIteration(false)
        {
            // Remember the field name 
            if (fieldName) query.Append(Private::OpArg("", SQLFormat::escapeString(fieldName)));
        }
        /** Two fields */
        SelectBase(const String & f1, const String & f2) : wherePos((size_t)-1), fromPos((size_t)-1), unsafeIteration(false)
        {
            // Remember the field name
            String fields = SQLFormat::escapeString(f1) + ", " + SQLFormat::escapeString(f2);
            query.Append(Private::OpArg("", fields));
        }
        /** Three fields */
        SelectBase(const String & f1, const String & f2, const String & f3) : wherePos((size_t)-1), fromPos((size_t)-1), unsafeIteration(false)
        {
            // Remember the field name
            String fields = SQLFormat::escapeString(f1) + ", " + SQLFormat::escapeString(f2) + ", " + SQLFormat::escapeString(f3);
            query.Append(Private::OpArg("", fields));
        }
        // For more fields, use the Field() method
    };
    

    /** The basic Select statement.
        The interface is using fluent mode (that is, each method returns an reference on the current instance, so 
        you can chain the code in an elegant manner).
        You typically write code like this:
        @code
            Database::Pool<Person> pool = (Database::Query::Select("ID", "Name").From("Person").Where("Age") > 34).And("City") == "Denver";
            // Or, if you want the very low level interface (beware, no escaping done):
            Database::UnsafeRowIterator iter = Database::Query::SelectRaw("SELECT ID, Name FROM Person WHERE Age > 34 AND City == Denver");
            
            // If you have variables however, then the former is safer, because it'll prepare a statement and escape everything:
            int minAge = 34;
            String city = "Denver'; DROP TABLE Person; --"; // Notice the SQL injection here
            Database::Pool<Person> pool = (Database::Query::Select("ID", "Name").From("Person").Where("Age") > minAge).And("City") == city;
            // Results in: "SELECT 'ID' FROM 'Person' WHERE 'Age' > 34 AND 'City' == 'Denver''; DROP TABLE Person; --';
            // Which is safe here, from the Database point of view
            
            // Beware of mismatch in the table selection and the object type
            Database::Pool<Person> pool = Database::Query::Select("ID").From("Car"); // Run-time error here, since the "Car" table will not fit into "Person" table.
            // You can feel more safe using the templated version
            Database::Pool<Person> pool = Database::Query::SelectT<Car>(ID); // Compile-time error "Can't convert from Pool<Car> to Pool<Person>"
            // Or you can parse the results yourself using the UnsafeRowIterator interface
            Database::UnsafeRowIterator iter = Database::Query::Select("ID", "Age").From("Car");
            while (iter) { printf("%s | %s;", iter["ID"], iter["Age”]); ++iter; }
        @endcode
        
        Few caveats through:
        @code 
            // If you need non-anonymous variable, you can NOT do this:
            Database::Query::Select oldGuy("ID", "Name").From("Person").Where("Age") > 34; // Error here, since you are in a declaration, you can't use the methods
            // You MUST not do this either:
            const Database::Query::Select & oldGuy = Database::Query::Select("ID", "Name").From("Person").Where("Age") > 34; // The reference will be bound to the first temporary that's already collected.
            // You SHOULD do this:
            Database::Query::Select oldGuy = Database::Query::Select("ID", "Name").From("Person").Where("Age") > 34; // A copy is made if required (most compiler will optimize it away)
        @endcode
        
        For a more detailled interface documentation, please refer to SelectBase
        @warning If you are using the non-specialized Select version, then only the first database connection is used (unless you specify it manually using setDBIndex).
        @sa SelectBase */
    template <typename Table>
    class SelectT : public SelectBase< SelectT<Table> >
    {
        // The logic operation is here
    public:
        /** This is used to extract the results directly as C++ objects 
            @warning If the table you pass in is not what the select refers to, then you'll get completely undefined results in your pool. */
        operator Database::Pool<Table>() const
        {
            // Need to perform the select here
            String base = this->getFinalTextWithCount();
            if (this->unsafeIteration || !base.getLength()) return Pool<Table>(0);
            base += ";";

            // The query retrieve all results at once
            const SQLFormat::Results * res = SQLFormat::sendQuery(Table::__DBIndex__, base);
            Var countT;
            if (res == NULL) return Pool<Table>(0);
            if (!SQLFormat::getResults(res, countT, 0, "xZ_X_Count_T823")) { SQLFormat::cleanResults(res); return Pool<Table>(0); }

            // Now, create the array of results
            int count = countT.like(&count);
            Pool<Table> results(count);
            // And store data in it
            for (unsigned int i = 0; (int)i < count; i++)
            {
                Table & row = results[i];
                row.setRowFieldsUnsafe(res, i);
            }

            SQLFormat::cleanResults(res);
            return results;
        }
        /** Get the result count out of a single request.
            This actually count the rows in the remote SQL engine, not locally, so it should be faster. */
        int getCount() const { return UnsafeRowIterator(SQLFormat::sendQuery(Table::__DBIndex__, this->getCountText() + ";"))["_X_countRows"]; }
        
        /** Get the result with an unsafe iterator */
        operator UnsafeRowIterator() const
        {
            // The query retrieve all results at once
            return UnsafeRowIterator(SQLFormat::sendQuery(Table::__DBIndex__, this->getFinalText() + ";"));
        }
        /** Refine the search fields */
        SelectT Refine(const String & fieldName) const
        {
            // We need to figure out the FROM positon, and rebuild from here
            SelectT copy(fieldName);
            // Check if we had a defined From yet or not
            size_t from = this->fromPos != (size_t)-1 ? this->fromPos : (this->wherePos != (size_t)-1) ? this->wherePos : this->query.getSize();
            copy.fromPos = copy.query.getSize();
            copy.tableName = this->tableName;
            copy.wherePos = (this->wherePos != (size_t)-1 ? 1 : (size_t)-1);
            copy.unsafeIteration = this->unsafeIteration;
            for (size_t i = from; i < this->query.getSize(); i++)
                copy.query.Append(this->query[i]);
            return copy;
        }
        /** Use this selection to delete the rows in the database */
        void Delete() const
        {
            SQLFormat::cleanResults(SQLFormat::sendQuery(Table::__DBIndex__, "DELETE " + Refine().getFinalText().fromFirst(this->getActionName()) + ";"));
        }
        
    
        // Construction
    public:
    
        SelectT(const String & fieldName = "") : SelectBase<SelectT>(fieldName) { this->tableName = Table::getEscapedTableName(); }
        SelectT(const String & f1, const String & f2) : SelectBase<SelectT>(f1, f2) { this->tableName = Table::getEscapedTableName(); }
        SelectT(const String & f1, const String & f2, const String & f3) : SelectBase<SelectT>(f1, f2, f3) { this->tableName = Table::getEscapedTableName(); }
    };
    
    template <>
    class SelectT<void> : public SelectBase< SelectT<void> >
    {
    private:
        uint32 dbIndex;
        
        // The logic operation is here
    public:
        /** Change the database connection index */
        SelectT & setDBIndex(const uint32 index) { dbIndex = index; return *this; }
        
        /** This is used to extract the results directly as C++ objects 
            @warning If the table you pass in is not what the select refers to, then you'll get completely undefined results in your pool. */
        template <typename Table>
        operator Database::Pool<Table>() const
        {
            // Need to perform the select here
            String base = this->getFinalTextWithCount();
            if (this->unsafeIteration || !base.getLength()) return Pool<Table>(0);
            base += ";";

            // The query retrieve all results at once
            const SQLFormat::Results * res = SQLFormat::sendQuery(Table::__DBIndex__, base);
            Var countT;
            if (res == NULL) return Pool<Table>(0);
            if (!SQLFormat::getResults(res, countT, 0, "xZ_X_Count_T823")) { SQLFormat::cleanResults(res); return Pool<Table>(0); }

            // Now, create the array of results
            int count = countT.like(&count);
            Pool<Table> results(count);
            // And store data in it
            for (unsigned int i = 0; (int)i < count; i++)
            {
                Table & row = results[i];
                row.setRowFieldsUnsafe(res, i);
            }

            SQLFormat::cleanResults(res);
            return results;
        }
        /** Get the result count out of a single request.
            This actually count the rows in the remote SQL engine, not locally, so it should be faster. */
        int getCount() const { return UnsafeRowIterator(SQLFormat::sendQuery(dbIndex, this->getCountText() + ";"))["_X_countRows"]; }
        /** Get the result with an unsafe iterator */
        operator UnsafeRowIterator() const
        {
            // The query retrieve all results at once
            return UnsafeRowIterator(SQLFormat::sendQuery(dbIndex, this->getFinalText() + ";"));
        }
        /** Refine the search fields */
        SelectT Refine(const String & fieldName) const
        {
            // We need to figure out the FROM positon, and rebuild from here
            SelectT copy(fieldName);
            // Check if we had a defined From yet or not
            size_t from = this->fromPos != (size_t)-1 ? this->fromPos : (this->wherePos != (size_t)-1) ? this->wherePos : this->query.getSize();
            copy.fromPos = copy.query.getSize();
            copy.tableName = this->tableName;
            copy.wherePos = (this->wherePos != (size_t)-1 ? 1 : (size_t)-1);
            copy.unsafeIteration = this->unsafeIteration;
            for (size_t i = from; i < this->query.getSize(); i++)
                copy.query.Append(this->query[i]);
            return copy;
        }
        /** Use this selection to delete the rows in the database */
        void Delete() const
        {
            SQLFormat::cleanResults(SQLFormat::sendQuery(dbIndex, "DELETE " + Refine("").getFinalText().fromFirst(this->getActionName()) + ";"));
        }
    
        // Construction
    public:
        SelectT(const String & fieldName = "") : SelectBase<SelectT>(fieldName), dbIndex(0) {}
        SelectT(const String & f1, const String & f2) : SelectBase<SelectT>(f1, f2), dbIndex(0) {}
        SelectT(const String & f1, const String & f2, const String & f3) : SelectBase<SelectT>(f1, f2, f3), dbIndex(0) {}
    };
    
    /** Inject the Select name inside the system */
    typedef SelectT<void> Select;
    
    /** Create a temporary table out of a selection.
        Typically, you'll write code like this:
        @code
            // Create a temporary table that's not auto dropped (you must call DropTable if you want to drop it)
            CreateTempTable("tempT1").As(Select("ID").From("Person").Where("FirstName") == "John");
            
            // Create a temporary table that's auto dropped when leaving the scope
            CreateTempTable tempT1 = CreateTempTable("tempT1", true).As(Select("ID").From("Person").Where("FirstName") == "John");
        @endcode 
    */
    class CreateTempTable
    {
    private:
        /** Temporary table name */
        const String tableName;
        /** The database connection to use */
        uint32 dbIndex;
        /** Whever to drop it when leaving the scope */
        mutable bool autoDrop;
        
    public:
        /** Change the database connection index */
        CreateTempTable & setDBIndex(const uint32 index) { dbIndex = index; return *this; }
        
    public:
        /** As clause for another select statement. */
        template<class U>
        CreateTempTable & As(const SelectBase<U> & statement)
        {
            SQLFormat::cleanResults(SQLFormat::sendQuery(dbIndex, "CREATE TEMPORARY TABLE " + SQLFormat::escapeString(tableName) + " AS " + statement.getFinalText() + ";"));
            return *this;
        }
    
        /** Create a temporary table  */
        CreateTempTable(const String & tableName, const bool autoDrop = false) : tableName(tableName), dbIndex(0), autoDrop(autoDrop) {}
        /** Copy constructor transfer the dropping behaviour */
        CreateTempTable(const CreateTempTable & other) : tableName(other.tableName), dbIndex(other.dbIndex), autoDrop(other.autoDrop) { other.autoDrop = false; }
        /** If specified auto drop the table */
        ~CreateTempTable()
        {
            if (autoDrop) SQLFormat::cleanResults(SQLFormat::sendQuery(dbIndex, "DROP TABLE " + SQLFormat::escapeString(tableName) + ";"));
        }
    };
    
    /** Used to drop a previously created temporary table if not asked to do it automatically.
        @sa CreateTempTable */
    struct DropTable
    {
        /** Drop a temporary table created earlier */
        DropTable(const String & tableName, const uint32 dbIndex = 0)
        {
            SQLFormat::cleanResults(SQLFormat::sendQuery(dbIndex, "DROP TABLE " + SQLFormat::escapeString(tableName) + ";"));
        }
    };
    
    /** Emit an completely unsafe raw request. 
        You have to specify the database connection index if you don't want to use the first one.
        @warning You should not use this code but use the Select method above */
    class SelectRaw 
    {
    private:
        uint32 dbIndex;
        const String & raw;
        
        // The logic operation is here
    public:
        /** Change the database connection index */
        SelectRaw & setDBIndex(const uint32 index) { dbIndex = index; return *this; }
        // The logic operation is here
    public:
        /** Get the result with an unsafe iterator */
        operator UnsafeRowIterator() const
        {
            // The query retrieve all results at once
            return UnsafeRowIterator(SQLFormat::sendQuery(dbIndex, raw + ";"));
        }
    
        // Construction
    public:
        SelectRaw(const String & raw) : dbIndex(0), raw(raw) {}
    };
    
    /** Delete from the database.
        This is exactly like Select class, but emit a DELETE request (and no results) */
    class Delete : public SelectBase<Delete>
    {
        uint32 dbIndex;
    public:
        virtual const String getActionName() const { return "DELETE "; }
        /** Change the database connection index */
        Delete & setDBIndex(const uint32 index) { dbIndex = index; return *this; }
        /** This is the main execution method that must be called explicitely */
        void execute() const { SQLFormat::cleanResults(SQLFormat::sendQuery(dbIndex, getFinalText() + ";")); }
    
        /** Default construction */
        Delete() : SelectBase<Delete>(""), dbIndex(0) {}
    };

}

}


#endif
