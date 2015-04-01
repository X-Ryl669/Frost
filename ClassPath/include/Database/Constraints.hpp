#ifndef hpp_CPP_Constraints_CPP_hpp
#define hpp_CPP_Constraints_CPP_hpp

// We need placement new operator
#include <new>
// We need database declarations
#include "Database.hpp"

namespace Database
{
    /** The pool of results after a query on the database.
        Pool use move semantics. It's transparent for you, you can safely write code like this:
        @code
        // Equivalent to Query(SELECT * FROM Table WHERE "ID" > 3).GetResults().CleanResults();
        Pool<Table> pool = find(Constraint<Table>("ID", Condition::GreaterThan(3)));
        // Only one copy of the pool of result is created (no temporary), unlike the previous pseudo code.
        @endcode
    */
    template <typename T>
    class Pool
    {
    private:
        T *     array;
        T       defaultT;

    public:
        /** The count member */
        const uint32  count;
        /** Access any element in the array */
        T & operator [](const unsigned int & i) { if (i < count) return array[i]; return defaultT; }
        /** Check validity of element */
        bool isValid(T& ref) { return &ref == &defaultT; }
        /** Merge with another pool (to mix results) (deep copy is made here) */
        bool mergeWith(const Pool & pool)
        {   if (pool.count == 0) return true;
            uint32 newCount = count + pool.count, i = 0;
            T * newArray = new T[newCount];
            if (newArray == NULL) return false;
            for (; i < count; i++) newArray[i] = array[i];
            for (; i < newCount; i++) newArray[i] = pool.array[i - count];
            delete[] array; array = newArray; const_cast<uint32&>(count) = newCount;
            return true;
        }

        /** Copy a pool of result by moving the data */
        inline Pool<T> & operator = ( const Pool & pool)
        {
            if (&pool == this) return *this;
            delete[] array;
            const_cast<uint32&>(count) = pool.count; array = pool.array;
            const_cast<Pool<T> *>(&pool)->array = 0; const_cast<uint32&>(const_cast<Pool<T> *>(&pool)->count) = 0;
            return *this;
        }

        /** Simple constructor with a count parameter for result pool creation */
        Pool(const uint32 & _count) : array(0), count(_count) { if (count) array = new T[count]; const_cast<uint32&>(count) = (array != 0) ? count : 0; }
        /** Copy constructor with previous pool destruction (transfer semantic) */
        Pool(const Pool & pool) : array(pool.array), count(pool.count) { const_cast<Pool<T> *>(&pool)->array = 0; const_cast<uint32&>(const_cast<Pool<T> *>(&pool)->count) = 0; }
        /** Destructor */
        ~Pool() { if (array != 0) delete[] array; array = 0; const_cast<uint32&>(count) = 0; }
    };

    /** The conditions you can refine a Constraint with.
        You can see the database as a giant map.
        A Constraint split the area, following the Conditions you'll use,
        and typically will be used to prepare a statement to get results from the database.
        <br>
        There is a lot of Conditions you can use:
        Max
    */
    namespace Conditions
    {
        /** The base condition */
        struct Condition
        {
            virtual bool getConditionAsString(const String & fieldName, String & constraintOnSelection, String & constraints, String & constraintOnPresentation) const = 0;
            virtual Condition * clone() const = 0;
            virtual bool update(const Var & = Var(), const Var &  = Var()) = 0;
            virtual bool isNoAry() const = 0;
            virtual ~Condition() {}
        };
        /** Condition that usually rewrite the selection.
            These conditions narrow the selection (for example 'SELECT MAX(column)' or 'SELECT COUNT(column)').
            Please notice that the selection change is aliased on the column name, so you'll have a the column
            acting like the condition (like in 'SELECT COUNT(id) as id') */
        struct NoAry : public Condition
        {
            virtual bool update(const Var &) { return false; }
            virtual bool update(const Var &, const Var &) { return false; }
            virtual bool isNoAry() const  { return true; }
        };
        /** Get the unique Maximum of the given field.
            The field itself must be sortable, and the maximum is aliased on the field itself.
            This usually produce "SELECT MAX(field) AS field" */
        struct Max : public NoAry
        {
            bool getConditionAsString(const String & fieldName, String & constraintOnSelection, String & constraints, String & constraintOnPresentation) const
            {
                if (constraintOnSelection.getLength()) constraintOnSelection += ", ";
                constraintOnSelection += " MAX( " + SQLFormat::escapeString(fieldName);
                constraintOnSelection += " ) AS ";
                constraintOnSelection += SQLFormat::escapeString(fieldName);
                return true;
            }
            virtual Condition * clone() const { return new Max(*this);  }
        };
        /** Get the unique Minimum of the given field
            The field itself must be sortable, and the minimum is aliased on the field itself.
            This usually produce "SELECT MIN(field) AS field" */
        struct Min : public NoAry
        {
            bool getConditionAsString(const String & fieldName, String & constraintOnSelection, String & constraints, String & constraintOnPresentation) const
            {
                if (constraintOnSelection.getLength()) constraintOnSelection += ", ";
                constraintOnSelection += " MIN( " + SQLFormat::escapeString(fieldName);
                constraintOnSelection += " ) AS ";
                constraintOnSelection += SQLFormat::escapeString(fieldName);
                return true;
            }
            virtual Condition * clone() const { return new Min(*this);  }
        };
        /** Get the unique number of item with the given field
            The count itself is returned as a the field (so you'll find the count value in the field, instead of the initial field value).
            This usually produce "SELECT COUNT(field) AS field"
            @warning Beware that the field type might interfer with the result, for example if a column is of type 'Blob', then the
                     aliasing will be interpreted as a binary blob. If it bothers you, check the Field structure */
        struct Count : public NoAry
        {
            bool getConditionAsString(const String & fieldName, String & constraintOnSelection, String & constraints, String & constraintOnPresentation) const
            {
                if (constraintOnSelection.getLength()) constraintOnSelection += ", ";
                constraintOnSelection += " COUNT( " + SQLFormat::escapeString(fieldName);
                constraintOnSelection += " ) AS ";
                constraintOnSelection += SQLFormat::escapeString(fieldName);
                return true;
            }
            virtual Condition * clone() const { return new Count(*this);  }
        };
        /** Get a specific field declaration.
            This appends a fieldName on the select query, like this "SELECT previousColumn, fieldName".
            This is used if you need to specify your own selection rule that's not interpreted by this system,
            like 'SELECT DATETIME(timeCol, "unixepoch")' when fieldName is 'DATETIME(timeCol, "unixepoch")'.
            Beware that this is unsafe, as no checking is done by this engine. */
        struct Field : public NoAry
        {
            bool getConditionAsString(const String & fieldName, String & constraintOnSelection, String & constraints, String & constraintOnPresentation) const
            {
                if (constraintOnSelection.getLength()) constraintOnSelection += ", ";
                constraintOnSelection += SQLFormat::escapeString(fieldName);
                return true;
            }
            virtual Condition * clone() const { return new Field(*this);  }
        };
        /** Get a specific distinct declaration.
            This appends a DISTINCT fieldName on the select query, like this "SELECT previousColumn, DISTINCT fieldName".
            This is supposed to be faster than using GROUP BY, althrough most SQL engine optimizer will emit the same code.
            Beware that this is unsafe, as no checking is done by this engine. */
        struct Distinct : public NoAry
        {
            bool getConditionAsString(const String & fieldName, String & constraintOnSelection, String & constraints, String & constraintOnPresentation) const
            {
                if (constraintOnSelection.getLength()) constraintOnSelection += ", ";
                constraintOnSelection += "DISTINCT " + SQLFormat::escapeString(fieldName);
                return true;
            }
            virtual Condition * clone() const { return new Distinct(*this);  }
        };
        /** Group results by the given field.
            This appends a "GROUP BY fieldName" to your query. */
        struct GroupBy : public NoAry
        {
            bool getConditionAsString(const String & fieldName, String & constraintOnSelection, String & constraints, String & constraintOnPresentation) const
            {
                if (constraintOnPresentation.getLength()) constraintOnPresentation += " ";
                constraintOnPresentation += " GROUP BY " + SQLFormat::escapeString(fieldName);
                return true;
            }
            virtual Condition * clone() const { return new GroupBy(*this);  }
        };
        /** Applied a final constraint on the returned result.
            This appends a "HAVING fieldName" to your query.
            @warning The fieldName here is not interpreted, nor filtered, so it's clearly unsafe to use with user generated data.

            @todo Improve this code to really use a Unary or Binary condition */
        struct Having : public NoAry
        {
            bool getConditionAsString(const String & fieldName, String & constraintOnSelection, String & constraints, String & constraintOnPresentation) const
            {
                if (constraintOnPresentation.getLength()) constraintOnPresentation += " ";
                constraintOnPresentation += " HAVING " + fieldName;
                return true;
            }
            virtual Condition * clone() const { return new Having(*this);  }
        };
        /** Check when a field is NULL.
            This appends a "(fieldName IS NULL)" to your where clause. */
        struct IsNull : public NoAry
        {
            bool getConditionAsString(const String & fieldName, String & constraintOnSelection, String & constraints, String & constraintOnPresentation) const
            {
                constraints += "(";
                constraints += SQLFormat::escapeString(fieldName);
                constraints += " IS NULL)";
                return true;
            }
            virtual Condition * clone() const { return new IsNull(*this);  }
        };
        /** Check when a field is NOT NULL.
            This appends a "(fieldName IS NOT NULL)" to your where clause. */
        struct IsNotNull : public NoAry
        {
            bool getConditionAsString(const String & fieldName, String & constraintOnSelection, String & constraints, String & constraintOnPresentation) const
            {
                constraints += "(";
                constraints += SQLFormat::escapeString(fieldName);
                constraints += " IS NOT NULL)";
                return true;
            }
            virtual Condition * clone() const { return new IsNotNull(*this);  }
        };
        /** This constraint completely ignore the fieldName and value. */
        struct Empty : public NoAry
        {
            bool getConditionAsString(const String & fieldName, String & constraintOnSelection, String & constraints, String & constraintOnPresentation) const
            {
                return true;
            }
            virtual Condition * clone() const { return new Empty(*this);  }
        };

        /** Unary conditions are these conditions that usually fit in the WHERE clause of the query */
        struct Unary : public Condition
        {
            // The value to test against
            Var requiredValue;
            bool getConditionAsString(const String & fieldName, String & constraintOnSelection, String & constraints, String & constraintOnPresentation) const
            {
                constraints += "(";
                constraints += SQLFormat::escapeString(fieldName);
                constraints += getConstraintOperator();
                try
                {
                    if (requiredValue.isExactly((UnescapedString*)0))
                        constraints += requiredValue.like(&constraints);
                    else
                        constraints += SQLFormat::escapeString(requiredValue.like(&constraints), '\'');
                } catch (...) { return false; }
                constraints += ")";
                return true;
            }
            virtual String getConstraintOperator() const = 0;
            Unary(const Var & _var) : requiredValue(_var){}

            // Proxy for using table's field directly
            template <class Type, int position>
            Unary(const WriteMonitored<Type, position>  & _var) : requiredValue(_var.asVariant()) {}

            virtual bool update(const Var & a) { requiredValue = a; return true; }
            virtual bool update(const Var & a, const Var & b) { requiredValue = a; return true; }
            virtual bool isNoAry() const  { return false; }

        };

        /** Limit the returned result to the given offset and count.
            This is a special condition type as it's handling both row result limiting and ordering. */
        struct Limit : public Condition
        {
            Var offset;
            Var count;

            bool getConditionAsString(const String & fieldName, String & constraintOnSelection, String & constraints, String & constraintOnPresentation) const
            {
                if (constraintOnPresentation.getLength()) constraintOnPresentation += " ";
                constraintOnPresentation += " LIMIT ";
                constraintOnPresentation += (offset.isEmpty() ? "" : (SQLFormat::escapeString(offset.like(&constraints), 0) + ", "));
                constraintOnPresentation += SQLFormat::escapeString(count.like(&constraints), 0);
                return true;
            }

            Limit(const Var & _count, const Var & _offset) : offset(_offset), count(_count) {}
            // Proxy for using table's field directly
            template <class Type, int position, class Type2, int position2>
            Limit(const WriteMonitored<Type, position>  & _count, const WriteMonitored<Type2, position2> & _offset)
                : offset(_offset.asVariant()), count(_count.asVariant()) {}

            virtual bool update(const Var & _count) { count = _count; return true; }
            // Proxy for using table's field directly
            template <class Type, int position>
            bool update(const WriteMonitored<Type, position>  & _count) { count = _count.asVariant(); return true; }


            virtual bool update(const Var & _count, const Var & _offset) { count = _count; offset = _offset; return true; }
            // Proxy for using table's field directly
            template <class Type, int position, class Type2, int position2>
            bool update(const WriteMonitored<Type, position>  & _count, const WriteMonitored<Type2, position2> & _offset)
            { count = _count.asVariant(); offset = _offset.asVariant(); return true; }

            virtual bool isNoAry() const  { return true; }
            virtual Condition * clone() const { return new Limit(*this);  }
        };
        /** Order the returned result pool.
            This appends a "ORDER BY fieldName ASC" or "DESC" depending on variable to your final query clause. */
        struct OrderBy : public Unary
        {
            enum OrderDirection { Ascending = 0, Descending };

            virtual String getConstraintOperator() const { return ""; }
            OrderBy(const Var & _var = (int)Ascending) : Unary(_var) { }

            bool getConditionAsString(const String & fieldName, String & constraintOnSelection, String & constraints, String & constraintOnPresentation) const
            {
                if (constraintOnPresentation.getLength()) constraintOnPresentation += " ";
                constraintOnPresentation += " ORDER BY " + SQLFormat::escapeString(fieldName);
                if (!requiredValue.isEmpty())
                {
                    OrderDirection dir = (OrderDirection)requiredValue.like((int*)0);
                    constraintOnPresentation += (dir != Descending ? " ASC " : " DESC ");
                }
                else
                    constraintOnPresentation += " ASC ";
                return true;
            }
            virtual Condition * clone() const { return new OrderBy(*this);  }

            // Proxy for using table's field directly
            template <class Type, int position>
            OrderBy(const WriteMonitored<Type, position>  & t) : Unary(t) {}

        };

        /** Get all the result where a field equal the given value.
            This appends "field = val" to the SQL "WHERE" clause. */
        struct Equal : public Unary
        {
            Equal(const Var & _var) : Unary(_var) { }

            // Proxy for using table's field directly
            template <class Type, int position>
            Equal(const WriteMonitored<Type, position>  & t) : Unary(t) {}

            String getConstraintOperator() const { return String(" = "); }
            virtual Condition * clone() const { return new Equal(*this);    }
        };
        /** Get all the result where a field isn't in the the given set.
            This appends "field NOT IN val" to the SQL "WHERE" clause. */
        struct NotInSet : public Unary
        {
            NotInSet(const Var & _var) : Unary(_var) { }

            // Proxy for using table's field directly
            template <class Type, int position>
            NotInSet(const WriteMonitored<Type, position>  & t) : Unary(t) {}

            String getConstraintOperator() const { return String(" NOT IN "); }
            virtual Condition * clone() const { return new NotInSet(*this);    }
        };
        /** Get all the result where a field is in the the given set.
            This appends "field IN val" to the SQL "WHERE" clause.
            A typical example of this is when using sub queries like this:
            @code
            Constraint<Color> color(ID, Condition::Field()).And(Constraint<Color>(ID, Condition::Greater(34)));
            Constraint<Car> cars(IDColor, Condition::InSet(color.createSubConstraintText()));
            // Generates: SELECT * from Car WHERE IDColor IN (SELECT ID FROM Color WHERE ID > 34)

            With macros, the code would read as
            BuildConstraint(Color, aaa, ID, _C::Field());
            BuildConstraint(Color, aab, ID, _C::Greater(34));
            BuildConstraint(Car, cars, IDColor, _C::InSet(aaa.And(aab).createSubConstraintText()));
            @endcode */
        struct InSet : public Unary
        {
            InSet(const Var & _var) : Unary(_var) { }

            // Proxy for using table's field directly
            template <class Type, int position>
            InSet(const WriteMonitored<Type, position>  & t) : Unary(t) {}

            String getConstraintOperator() const { return String(" IN "); }
            virtual Condition * clone() const { return new InSet(*this);    }
        };
        /** Get all the result where a field match binary AND operator.
            This appends "field & val" to the SQL "WHERE" clause. */
        struct BitAnd : public Unary
        {
            BitAnd(const Var & _var) : Unary(_var) {    }

            // Proxy for using table's field directly
            template <class Type, int position>
            BitAnd(const WriteMonitored<Type, position>  & t) : Unary(t) {}

            String getConstraintOperator() const { return String(" & "); }
            virtual Condition * clone() const { return new BitAnd(*this);   }
        };
        /** Get all the result where a field match binary OR operator.
            This appends "field | val" to the SQL "WHERE" clause. */
        struct BitOr : public Unary
        {
            BitOr(const Var & _var) : Unary(_var) { }

            // Proxy for using table's field directly
            template <class Type, int position>
            BitOr(const WriteMonitored<Type, position>  & t) : Unary(t) {}

            String getConstraintOperator() const { return String(" | "); }
            virtual Condition * clone() const { return new BitOr(*this);    }
        };
        /** Get all the result where a field match binary XOR operator.
            This appends "field ^ val" to the SQL "WHERE" clause. */
        struct BitXor : public Unary
        {
            BitXor(const Var & _var) : Unary(_var) {    }

            // Proxy for using table's field directly
            template <class Type, int position>
            BitXor(const WriteMonitored<Type, position>  & t) : Unary(t) {}

            String getConstraintOperator() const { return String(" ^ "); }
            virtual Condition * clone() const { return new BitXor(*this);   }
        };
        /** Get all the result where a field is like the given value.
            This appends "field LIKE val" to the SQL "WHERE" clause. */
        struct Like : public Unary
        {
            Like(const Var & _var) : Unary(_var) {  }

            // Proxy for using table's field directly
            template <class Type, int position>
            Like(const WriteMonitored<Type, position>  & t) : Unary(t) {}

            String getConstraintOperator() const { return String(" LIKE "); }
            virtual Condition * clone() const { return new Like(*this); }
        };
        /** Get all the result where a field is less than the given value.
            This appends "field < val" to the SQL "WHERE" clause. */
        struct Less : public Unary
        {
            Less(const Var & _var) : Unary(_var) {  }

            // Proxy for using table's field directly
            template <class Type, int position>
            Less(const WriteMonitored<Type, position>  & t) : Unary(t) {}

            String getConstraintOperator() const { return String(" < "); }
            virtual Condition * clone() const { return new Less(*this); }
        };
        /** Get all the result where a field is less or equal than the given value.
            This appends "field <= val" to the SQL "WHERE" clause. */
        struct LessOrEqual : public Unary
        {
            LessOrEqual(const Var & _var) : Unary(_var) {   }

            // Proxy for using table's field directly
            template <class Type, int position>
            LessOrEqual(const WriteMonitored<Type, position>  & t) : Unary(t) {}

            String getConstraintOperator() const { return String(" <= "); }
            virtual Condition * clone() const { return new LessOrEqual(*this);  }
        };
        /** Get all the result where a field is greater than the given value.
            This appends "field > val" to the SQL "WHERE" clause. */
        struct Greater : public Unary
        {
            Greater(const Var & _var) : Unary(_var) {   }

            // Proxy for using table's field directly
            template <class Type, int position>
            Greater(const WriteMonitored<Type, position>  & t) : Unary(t) {}

            String getConstraintOperator() const { return String(" > "); }
            virtual Condition * clone() const { return new Greater(*this);  }
        };
        /** Get all the result where a field is greater or equal than the given value.
            This appends "field >= val" to the SQL "WHERE" clause. */
        struct GreaterOrEqual : public Unary
        {
            GreaterOrEqual(const Var & _var) : Unary(_var) {    }

            // Proxy for using table's field directly
            template <class Type, int position>
            GreaterOrEqual(const WriteMonitored<Type, position>  & t) : Unary(t) {}

            String getConstraintOperator() const { return String(" >= "); }
            virtual Condition * clone() const { return new GreaterOrEqual(*this);   }
        };
        /** Get all the result where a field is not equal to the given value.
            This appends "field <> val" to the SQL "WHERE" clause. */
        struct NotEqual : public Unary
        {
            NotEqual(const Var & _var) : Unary(_var) {  }

            // Proxy for using table's field directly
            template <class Type, int position>
            NotEqual(const WriteMonitored<Type, position>  & t) : Unary(t) {}

            String getConstraintOperator() const { return String(" <> "); }
            virtual Condition * clone() const { return new NotEqual(*this); }
        };
        /** Get all the result where a field is not like the given value.
            This appends "field NOT LIKE val" to the SQL "WHERE" clause. */
        struct NotLike : public Unary
        {
            NotLike(const Var & _var) : Unary(_var) {   }

            // Proxy for using table's field directly
            template <class Type, int position>
            NotLike(const WriteMonitored<Type, position>  & t) : Unary(t) {}

            String getConstraintOperator() const { return String(" NOT LIKE "); }
            virtual Condition * clone() const { return new NotLike(*this);  }
        };

        /** Binary conditions are conditions with 2 arguments, like range based querying */
        struct Binary : public Condition
        {
            // The first required value to test against
            Var value1;
            Var value2;

            bool getConditionAsString(const String & fieldName, String & constraintOnSelection, String & constraints, String & constraintOnPresentation) const
            {
                constraints += "(";
                constraints += SQLFormat::escapeString(fieldName);
                constraints += getConstraintOperator();
                try
                {
                    if (value1.isExactly((UnescapedString*)0))
                        constraints += value1.like(&constraints);
                    else
                        constraints += SQLFormat::escapeString(value1.like(&constraints), '\'');
                    constraints += " AND ";

                    if (value2.isExactly((UnescapedString*)0))
                        constraints += value2.like(&constraints);
                    else
                        constraints += SQLFormat::escapeString(value2.like(&constraints), '\'');
                } catch (...) { return false; }
                constraints += ")";
                return true;
            }
            virtual String getConstraintOperator() const = 0;
            Binary(const Var & _var1, const Var & _var2) : value1(_var1), value2(_var2) {}

            // Proxy for using table's field directly
            template <class Type, int position, class Type2, int position2>
            Binary(const WriteMonitored<Type, position>  & _var1, const WriteMonitored<Type2, position2> & _var2)
                : value1(_var1.asVariant()), value2(_var2.asVariant()) {}

            virtual bool update(const Var & a) { value1 = a; return true; }
            virtual bool update(const Var & a, const Var & b) { value1 = a; value2 = b; return true; }
            virtual bool isNoAry() const  { return false; }

        };

        /** Get all the result where a field is between the two given values.
            This appends "field BETWEN val1 AND val2" to the SQL "WHERE" clause. */
        struct Between : public Binary
        {
            Between(const Var & _var1, const Var & _var2) : Binary(_var1, _var2) {  }

            // Proxy for using table's field directly
            template <class Type, int position, class Type2, int position2>
            Between(const WriteMonitored<Type, position>  & _var1, const WriteMonitored<Type2, position2> & _var2) : Binary(_var1, _var2) {}

            String getConstraintOperator() const { return String(" BETWEEN "); }
            virtual Condition * clone() const { return new Between(*this);  }
        };
    }

    /** This structure adds a constraint on a model for retrieving results.
        The idea of using Constraint is to avoid SQL error, leading to vulnerabilities, at compile time.
        You'll build a Constraint at compile time, and it'll be compiled at compile time.
        This constraint can then be used safely on a database (whatever the driver).
        <br>
        You can see the entries in a database as a giant map, like this:
        @verbatim
        /-----------------------------------------------\
        |     x (ID=3, Value=Blue)   x (ID=6, Value=Red)|
        |                    x (ID = 2, Value=Yellow)   |
        |                                               |
        | x (ID=34, ColorID=6)                          |
        \-----------------------------------------------/
        @endverbatim

        If you create a contraint like this:
        Constraint<Color>("ID", Condition::Equal(34));
        You'll partitionate the result field like this:

        @verbatim
        /-----------------------------------------------\
        |     x (ID=3, Value=Blue)   x (ID=6, Value=Red)|
        |                    x (ID = 2, Value=Yellow)   |
        |-----------------------.                       |
        | x (ID=34, ColorID=6)  |                       |
        \-----------------------------------------------/
        @endverbatim

        Please notice that the Constraint isn't "applied to"
        the database until you actually call Database::find
        <br>
        <br>
        Building a constraint is very fast, and doesn't imply
        querying the database (this is possible because of the
        way the database model is declared).
        Usually, in SQL terms, you're preparing a statement.
        But unlike SQL, the statement you are preparing will be
        verified at compile time.
        <br>
        <br>
        This means that this SQL statement would compile:
        @code
        SELECT * FROM Color WHERE Value=%d, 34
        @endcode
        (but would fail at runtime)
        <br>
        <br>
        But this constraint won't compile:
        @code
        Constraint<Color>(Value, Condition::Equal(34)); // Error "Value" doesn't exist in Table<Color>
        @endcode
        <br>
        <br>
        Of course, you can combine constraints to further reduce the result set, by doing something like this:
        @code
        Constraint<Color>(Value, Condition::Less(21)).And(Constraint<Color>(Value, Condition::Greater(45)))
        or
        Constraint<Color>(Value, Condition::Less(21)).Or(Constraint<Color>(Value, Condition::Greater(45)))
        @endcode

        @warning    Usually, some people do "SELECT * FROM table" (with no limit on the result pool).
                    With a Constraint, this proves difficult because, well:<br>
                    -# This request can exhaust memory (if the database is larger thant the available memory), there is no way to know beforehand the size of the result pool
                    -# This means that the powerful SQL engine isn't used and the developer intend to filter the result himself (which is prone to error)
                    Anyway, if you really, really need to do such bad code, you can use Limit (recommended), or ID.GreaterOrEqual(1) which is similar.
        @warning    Constraints are using move semantics, for performance reason. You will likely ignore this, unless you are reusing Constraint in different chains.
                    In that case, you can use the clone() method, and AndConst/OrConst method for safety
    */
    template <typename T>
    struct Constraint
    {
        /** Set the field with the constraint */
        String                  constrainedField;
        /** The condition on the constraint itself */
        Conditions::Condition * condition;

        /** Chain constraints in a AND like manner.
            This does not modify the passed parameter unlike the And method */
        Constraint & AndConst(const Constraint & t)
        {
            if (next != 0) next->And(t);
            else { next = t.clone(); type = (LinkType)((int)type | _And); }
            return *this;
        }

        /** Chain constraints in a OR like manner
            This does not modify the passed parameter unlike the And method */
        Constraint & OrConst(const Constraint & t)
        {
            if (next != 0) next->Or(t);
            else { next = t.clone(); type = (LinkType)((int)type | _Or); }
            return *this;
        }
        /** Chain constraints in a AND like manner */
        Constraint & And(const Constraint & t)
        {
            if (next != 0) next->And(t);
            else { next = new Constraint(t); type = (LinkType)((int)type | _And); }
            return *this;
        }

        /** Chain constraints in a OR like manner */
        Constraint & Or(const Constraint & t)
        {
            if (next != 0) next->Or(t);
            else { next = new Constraint(t); type = (LinkType)((int)type | _Or); }
            return *this;
        }

        /** Chain constraints in a AND like manner.
            This is used if you build a complex constraint chain initially for later use and you want to avoid searching all the constraint later on.
            @return reference on chained constraint */
        Constraint & AndRef(const Constraint & t)
        {
            if (next != 0) return next->And(t);
            else { next = new Constraint(t); type = (LinkType)((int)type | _And); return *next; }
        }

        /** Chain constraints in a AND like manner */
        inline Constraint & AndRef(const String & field, const Conditions::Condition & cond) { return AndRef(Constraint(field, cond)); }
        /** Chain constraints in a AND like manner */
        inline Constraint & AndRef(const int fieldIndex, const Conditions::Condition & cond) { return AndRef(Constraint(fieldIndex, cond)); }

        /** Chain constraints in a OR like manner
            @return reference on chained constraint */
        Constraint & OrRef(const Constraint & t)
        {
            if (next != 0) return next->Or(t);
            else { next = new Constraint(t); type = (LinkType)((int)type | _Or);    return *next; }
        }

        /** Chain constraints in a OR like manner */
        inline Constraint & OrRef(const String & field, const Conditions::Condition & cond) { return OrRef(Constraint(field, cond)); }
        /** Chain constraints in a OR like manner */
        inline Constraint & OrRef(const int fieldIndex, const Conditions::Condition & cond) { return OrRef(Constraint(fieldIndex, cond)); }

        /** The next constraint if any */
        Constraint *        next;
        /** The link type */
        enum LinkType       { None = 0, _And = 1, _Or = 2, _AndOpened = 5, _OrOpened = 6, _Opened = 4 };
        LinkType            type;
        uint32              parenthesisCount;

        inline const String & getEscapedTableName() const { return T::getEscapedTableName(); }
        inline bool getConstraintAsString(String & constraintOnSelection, String & constraints, String & constraintOnPresentation, const uint32 previousParenthesisCount = 0) const
        {
            // Start by our field
            if (!condition->getConditionAsString(constrainedField, constraintOnSelection, constraints, constraintOnPresentation)) return false;
            bool isConstraintEmpty = constraints.getLength() == 0;
            if (next == 0 && parenthesisCount)
            {
                constraints += String(')', parenthesisCount);
                constraints += " ";
            }

            if (previousParenthesisCount < parenthesisCount)
                constraints =  String('(', parenthesisCount - previousParenthesisCount) + constraints;

            // Should link the chained constraints here
            if (next != 0)
            {
                if (parenthesisCount > next->parenthesisCount)
                {
                    constraints += String(')', parenthesisCount - next->parenthesisCount);
                    constraints += " ";
                }

                String tmpConstraints;
                if (!next->getConstraintAsString(constraintOnSelection, tmpConstraints, constraintOnPresentation, parenthesisCount)) return false;

                // Prepare for next constraint link word
                if (tmpConstraints.getLength() != 0)
                {
                    if (!isConstraintEmpty && !next->condition->isNoAry()) constraints += type & _And ? String(" AND ") : (type & _Or ? String(" OR ") : String());
                    constraints += tmpConstraints;
                }
            }
            // Now return
            return true;
        }

        /** Modify the condition */
        inline void modifyCondition(const Conditions::Condition & cond)         { if (condition) delete condition; condition = cond.clone(); }
        /** Update the current condition parameter (this kind of defeat inheritance paradigm, but is too useful) */
        inline bool updateParameter(const Var& a)                               { if (condition) return condition->update(a); return false; }
        /** Update the current condition parameter (this kind of defeat inheritance paradigm, but is too useful) */
        inline bool updateParameter(const Var& a, const Var& b)                 { if (condition) return condition->update(a, b); return false; }
        /** Enclose the constraint in parenthesis */
        inline Constraint & encloseInParenthesis(const bool & enclosed = true)
        {
            Constraint * current = this;
            if (enclosed)
            {
                type = (LinkType)((int)type | _Opened);

                while (current)
                {
                    current->parenthesisCount ++;
                    current = current->next;
                }
            }
            else
            {
                type = (LinkType)((int)type & ~_Opened);
                while (current)
                {
                    current->parenthesisCount--;
                    current = current->next;
                }
            }
            return *this;
        }
        /** Create the constraint text */
        UnescapedString createConstraintText() const
        {
            String constraintOnSelection;
            String constraints;
            String constraintOnPresentation;
            if (!getConstraintAsString(constraintOnSelection, constraints, constraintOnPresentation))
                return String("");

            String sBase = "SELECT ";
            sBase += constraintOnSelection.getLength() == 0 ? String("*") : constraintOnSelection;
            sBase += " FROM ";
            sBase += getEscapedTableName();
            if (constraints.getLength())
            {
                sBase += " WHERE ";
                sBase += constraints;
            }
            if (constraintOnPresentation.getLength())
                sBase += " ";
            sBase += constraintOnPresentation;
            return sBase;
        }
        /** Create the count constraint text */
        UnescapedString createCountText() const
        {

            UnescapedString initialConstraintText = createConstraintText();
            if (!initialConstraintText) return String("");

            String sBase = ", (SELECT COUNT(*) FROM (";
            sBase += initialConstraintText;
            sBase += ")) AS xZ_X_Count_T823 FROM";
            return initialConstraintText.upToFirst("FROM") + sBase + initialConstraintText.fromFirst("FROM");
        }

        /** Create the subconstraint text */
        UnescapedString createSubConstraintText() const
        {
            return "(" + createConstraintText() + ")";
        }
        /** Create the constraint text */
        UnescapedString createDeleteConstraintText() const
        {
            String constraintOnSelection;
            String constraints;
            String constraintOnPresentation;
            if (!getConstraintAsString(constraintOnSelection, constraints, constraintOnPresentation))
                return String("");

            String sBase = "DELETE FROM ";
            sBase += getEscapedTableName();
            if (constraints.getLength())
            {
                sBase += " WHERE ";
                sBase += constraints;
            }
            if (constraintOnPresentation.getLength())
                sBase += " ";
            sBase += constraintOnPresentation;
            return sBase;
        }

        /** Value constructor */
        Constraint(const String & field, const Conditions::Condition & cond) : constrainedField(field), condition(cond.clone()), next(0), type(None), parenthesisCount(0) {}
        /** Value constructor */
        Constraint(const int fieldIndex, const Conditions::Condition & cond) : constrainedField(T::fromPositionIncomplete(fieldIndex)->columnName), condition(cond.clone()), next(0), type(None), parenthesisCount(0) {}
        /** Copy constructor transfer the data (and modify/destroy the initial copy) */
        Constraint(const Constraint & constraint) : constrainedField(constraint.constrainedField), condition(constraint.condition), next(constraint.next), type(constraint.type), parenthesisCount(constraint.parenthesisCount)
        { const_cast<Constraint&>(constraint).next = 0; const_cast<Constraint&>(constraint).type = None; const_cast<Constraint&>(constraint).condition = 0; const_cast<Constraint&>(constraint).parenthesisCount = 0; }
        /** Clone a constraint */
        Constraint * clone() const { return new Constraint(constrainedField, *condition); }
        /** Default destructor */
        ~Constraint() { if (next) delete next; next = 0; type = None; if (condition) delete condition; }
    };

#ifdef DOXYGEN
    namespace Macro
    {
        /** You can use constraint by writing the code as C++ code. If you work outside Database namespace, this can be troublesome.
            There are 2 macros that simplifies the code and provides a compile-time safety check instead of runtime (so invalid SQL request doesn't compile,
            instead of doing unwanted side-effects).

            If you need to combine constraints, check the BuildConstraint macro.
            Take care that Constraints are using move semantics (a copy of a constraint take tranfers ownership of the initial constraint to the copy).
            This is faster in 99% of the case, since you avoid allocations, but it can bite if you reuse your constraint in different context, and chain them.
            If you need to keep a Constraint intact for re-use, you might want to use the clone() method.
        */
        namespace UsingConstraints
        {
            /** Build a constraint object those name is Name.
                This provides compile-time check that the field exists in the table.<br>
                <b>This means that the C++ code won't compile for invalid SQL</b><br>
                Those are equivalent:
                @code
                // This:
                BuildConstraint(MyTable, theConstraint, ID, _C::Equal(34));
                // Expands to this:
                Database::Constraint<MyTable> theConstraint((int)MyTable::__ID__, Database::Condition::Equal(34)); // Safe: Won't compile if MyTable doesn't contain "ID" field

                // Run-time check is given by:
                Database::Constraint<MyTable> theConstraint("ID", Database::Condition::Equal(34)); // Beware, if MyTable doesn't contain a "ID" field, this will still compile
                @endcode
                You can combine multiple constraint by doing code like this:
                @code
                BuildConstraint(MyTable, firstConstraint, ID, _C::Greater(34));
                BuildConstraint(MyTable, secondConstraint, Name, _C::Like("John*"));
                Database::Pool<MyTable> pool = Database::find(firstConstraint.And(secondConstraint)); // SELECT * FROM MyTable WHERE ID > 34 AND Name LIKE "John*"
                // or
                Database::Pool<MyTable> pool = Database::find(firstConstraint.Or(secondConstraint)); // SELECT * FROM MyTable WHERE ID > 34 OR Name LIKE "John*"
                @endcode

                @param X        The table structure
                @param Name     The name of the instance created in the current scope
                @param Field    The field to constrain
                @param Cond     The constraint's condition. */
            MACRO BuildConstraint(Table X, Instance Name, ConstraintOn Field, Conditions Cond);
            /** Build a anonymous constraint object.
                Similarly to BuildConstraint, this one is useful if you don't need the constraint to persist (like if you are chaining constraints).
                This provides compile-time check that the field exists in the table.<br>
                <b>This means that the C++ code won't compile for invalid SQL</b><br>
                @sa BuildConstraint
                Those are equivalent:
                @code
                // This:
                AnonConstraint(MyTable, ID, _C::Equal(34));
                // Expands to this:
                Database::Constraint<MyTable>((int)MyTable::__ID__, Database::Condition::Equal(34)); // Safe: Won't compile if MyTable doesn't contain "ID" field
                @endcode
                You can combine multiple constraint (the main purpose of this anonymous constraint in fact) by doing code like this:
                @code
                BuildConstraint(MyTable, firstConstraint, ID, _C::Greater(34));
                Database::Pool<MyTable> pool = Database::find(firstConstraint.And(AnonConstraint(MyTable, Name, _C::Like("John*")))); // SELECT * FROM MyTable WHERE ID > 34 AND Name LIKE "John*"
                // or if you intend to use a lot of constraints;
                Database::Pool<MyTable> pool = Database::find(firstConstraint.Or(AnonConstraint(MyTable, Name, _C::Like("John*")))
                                                                             .And(AnonConstraint(MyTable, Age, _C::Less("45")))); // SELECT * FROM MyTable WHERE (ID > 34 OR Name LIKE "John*") AND Age < 45
                @endcode

                @param X        The table structure
                @param Field    The field to constrain
                @param Cond     The constraint's condition. */
            MACRO AnonConstraint(Table X, ConstraintOn Field, Conditions Cond);
            /** Build a constraint object, and fetch a pool from it.
                This provides compile-time check that the field exists in the table.<br>
                <b>This means that the C++ code won't compile for invalid SQL</b><br>
                Those are equivalent:
                @code
                // This:
                BuildConstraintAndPool(MyTable, pool, theConstraint, ID, _C::Equal(34));
                // Expands to this:
                Database::Constraint<MyTable> theConstraint((int)MyTable::__ID__, Database::Condition::Equal(34));
                Database::Pool<MyTable> pool = Database::find(theConstaint);

                @endcode
                @param X        The table structure
                @param Pool     The result pool
                @param Name     The name of the instance created in the current scope
                @param Field    The field to constrain
                @param Cond     The constraint's condition. */
            MACRO BuildConstraintAndPool(Table X, Result Pool, Instance Name, ConstraintOn Field, Conditions Cond);
            /** Build a anonymous constraint object, and fetch a pool from it.
                Use this if you don't intend to reuse the constraint.
                This provides compile-time check that the field exists in the table.<br>
                <b>This means that the C++ code won't compile for invalid SQL</b><br>
                Those are equivalent:
                @code
                // This:
                BuildPool(MyTable, pool, ID, _C::Equal(34));
                // Expands to this:
                Database::Pool<MyTable> pool = Database::find(Database::Constraint<MyTable>((int)MyTable::__ID__, Database::Condition::Equal(34)));

                @endcode
                @param X        The table structure
                @param Pool     The result pool
                @param Field    The field to constrain
                @param Cond     The constraint's condition. */
            MACRO BuildPool(Table X, Result Pool, ConstraintOn Field, Conditions Cond);

            /** A shortcut to avoid typing both namespace */
            typedef Database::Conditions _C;
        }
    }
#else
    // We don't use typedef here as we don't want to pollute the current namespace
#define _C      Database::Conditions
    // If compilation breaks here, it's because you're building a constraint on a non existing field! Better safe than sorry, you must check this
#define BuildConstraint(X, Name, Field, Cond)   Database::Constraint<X> Name((int)X::__##Field##__, Cond);
    // If compilation breaks here, it's because you're building a constraint on a non existing field! Better safe than sorry, you must check this
#define AnonConstraint(X, Field, Cond)     Database::Constraint<X>((int)X::__##Field##__, Cond)
    // If compilation breaks here, it's because you're building a constraint on a non existing field! Better safe than sorry, you must check this
#define BuildConstraintAndPool(X, pool, Name, Field, Cond)   Database::Constraint<X> Name((int)X::__##Field##__, Cond); Database::Pool<X> pool = Database::find(Name);
    // If compilation breaks here, it's because you're building a constraint on a non existing field! Better safe than sorry, you must check this
#define BuildPool(X, pool, Field, Cond)   Database::Pool<X> pool = Database::find(Database::Constraint<X>((int)X::__##Field##__, Cond));
#endif

    /** This is the most useful method in the Database namespace, to get a result pool from a constraint.
        Even with large result pool, it's safe to write code like this:
        @code
            Constraint<Color> onlyBlueColor(Name, Like("%blue%"));
            // Moveable interface means that the temporary is modified, but the result array itself isn't copied (so it's fast).
            Pool<Color> pool = find(onlyBlueColor);
            // Then do what you want with the pool:
            for (int i = 0; i < pool.count; i++) { printf("Color: %s\n", pool[i].Name); }
        @endcode
        @param t    The constraint to apply on the database data map
        @return A moveable pool of result */
    template <typename T>
    Pool<T> find(const Constraint<T> & t)
    {
        // Need to perform the select here
        String base = t.createCountText();
        if (!base.getLength()) return Pool<T>(0);
        base += ";";

        // The query retrieve results
        const SQLFormat::Results * res = SQLFormat::sendQuery(T::__DBIndex__, base);
        Var countT;
        // Extract the count of data for this query
        if (res == NULL) return Pool<T>(0);
        if (!SQLFormat::getResults(res, countT, 0, "xZ_X_Count_T823")) { SQLFormat::cleanResults(res); return Pool<T>(0); }

        // Now, create the array of results
        int count = countT.like(&count);
        Pool<T> results(count);
        // And store data in it
        for (unsigned int i = 0; (int)i < count; i++)
        {
            T & row = results[i];
            row.setRowFieldsUnsafe(res, i);
        }

        SQLFormat::cleanResults(res);
        return results;
    }

    /** Use this method with a constraint to delete the row matching the constraint in the database
        Even with large result pool, it's safe to write code like this:
        @code
            Constraint<Color> onlyBlueColor(Name, Like("%blue%"));
            if (deleteInDB(onlyBlueColor))
            {
                // Done
            }
        @endcode
        @param t    The constraint to apply on the database data map
        @return true on successful deletion, false otherwise */
    template <typename T>
    bool deleteInDB(const Constraint<T> & t)
    {
        // Need to perform the select here
        String base = t.createDeleteConstraintText();
        if (!base.getLength()) return false;
        base += ";";

        // The query retrieve all results at once
        const SQLFormat::Results * res = SQLFormat::sendQuery(T::__DBIndex__, base);
        if (res == NULL) return false;

        SQLFormat::cleanResults(res);
        return true;
    }
}

#endif
