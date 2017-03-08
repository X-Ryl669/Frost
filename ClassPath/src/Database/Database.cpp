#include "../../include/Database/Database.hpp"

#if WantDatabase == 1

const unsigned int  Database::Index::WantNewIndex = (unsigned int)-1;
const unsigned int  Database::Index::DelayAction = 0;
const uint64        Database::LongIndex::WantNewIndex = (uint64)-1;
const uint64        Database::LongIndex::DelayAction = 0;

namespace Database
{
    /** Provides a faster access to SQL command INSERT INTO */
    bool TableDescription::insertInto(const String & fields, const String & values)
    {
        const SQLFormat::Results * res =
#ifdef SQLDEBUG
            SQLFormat::sendQuery(databaseIndex, "-- Should create a new ID from the database here ");
            SQLFormat::cleanResults(res);
#else
            NULL;
#endif

        String commandIs;
        commandIs.Format("INSERT INTO %s (%s) VALUES (%s);",
            (const char*)tableName,
            (const char*)fields,
            (const char*)values);

        res = SQLFormat::sendQuery(databaseIndex, commandIs);
        if (res == NULL) return false;
        SQLFormat::cleanResults(res);
        return true;
    }

    /** Provides a faster access to SQL command UPDATE
        @warning fieldName and fieldValue are escaped inside this method, while whereName and whereValue are not */
    bool TableDescription::updateWhere(const String & fieldName, const String & fieldValue, const String & whereName, const String & whereValue)
    {
        const SQLFormat::Results * res =
#ifdef SQLDEBUG
            SQLFormat::sendQuery(databaseIndex, "-- Should update the database here ");
            SQLFormat::cleanResults(res);
#else
            NULL;
#endif
        String commandIs;
        if (whereValue.getLength())
            commandIs.Format("UPDATE %s SET %s = %s WHERE %s = %s;",
                (const char*)tableName,
                (const char*)SQLFormat::escapeString(fieldName),
                (const char*)SQLFormat::escapeString(fieldValue, '\''),
                (const char*)whereName,
                (const char*)whereValue);
        else
            commandIs.Format("UPDATE %s SET %s = %s WHERE %s;",
                (const char*)tableName,
                (const char*)SQLFormat::escapeString(fieldName),
                (const char*)SQLFormat::escapeString(fieldValue, '\''),
                (const char*)whereName);

        // Send the query
        res = SQLFormat::sendQuery(databaseIndex, commandIs);
        if (res == NULL) return false;
        SQLFormat::cleanResults(res);
        return true;
    }
    /** Provides a faster access to SQL command DELETE */
    bool TableDescription::deleteWhere(const String & name, const String & value)
    {
        const SQLFormat::Results * res =
#ifdef SQLDEBUG
            SQLFormat::sendQuery(databaseIndex, "-- Should delete from the database here ");
            SQLFormat::cleanResults(res);
#else
            NULL;
#endif
        String commandIs;
        if (value.getLength())
            commandIs.Format("DELETE FROM %s WHERE %s = %s;",
                (const char*)tableName,
                (const char*)name,
                (const char*)value);
        else
            commandIs.Format("DELETE FROM %s WHERE %s;",
                (const char*)tableName,
                (const char*)name);

        // Send the query
        res = SQLFormat::sendQuery(databaseIndex, commandIs);
        if (res == NULL) return false;
        SQLFormat::cleanResults(res);
        return true;
    }

    /** Build Where clause from existing field, ignoring the specified field
        @return the number of filled fields including the ignored field
    */
    int TableDescription::buildWhereClause(String & whereName, const String & fieldToIgnore)
    {
        String * names = 0, * values = 0;
        int count = getNotEmptyFieldsNameAndValueAsArray(names, values);
        if (count)
        {
            // Create the where sentence
            for (int j = 0; j < count; j++)
            {
                if (names[j] == fieldToIgnore) { continue; }
                if (whereName.getLength()) { whereName += " AND "; }
                whereName += SQLFormat::escapeString(names[j]) + " = " + SQLFormat::escapeString(values[j], '\'');
            }
            delete[] names; delete[] values;
            return count;
        }
        return 0;
    }
    void TableDescription::appendDefaultValue(String & whereClause)
    {
        for (unsigned int i = 0; i < (unsigned int)getFieldCount(); i++)
        {
            if (i == hasIndex()) continue;
            const FieldDescription * desc = fromPosition(i);
            if (!desc) return; // Bad table, don't act it's unsafe
            ModifiedCallback * mc = desc->getCB(this);
            if (mc && !mc->isInit() && desc->defaultValue.getLength())
            {
                if (whereClause.getLength()) whereClause += " AND ";
                whereClause += SQLFormat::escapeString(desc->columnName) +  " = " + SQLFormat::escapeString(desc->defaultValue, '\'');
            }
        }
    }

    bool TableDescription::updateIfAnyFieldModified(unsigned int indexOfIndex, unsigned int indexValue, bool & otherFieldModified)
    {
        otherFieldModified = false;
        String setClause;
        for (unsigned int i = 0; i < (unsigned int)getFieldCount(); i++)
        {
            if (i == indexOfIndex) continue;
            const FieldDescription * desc = fromPosition(i);
            if (!desc) return false; // Bad table, don't act it's unsafe
            ModifiedCallback * mc = desc->getCB(this);
            if (mc && mc->isInit())
            {
                otherFieldModified = true;
                if (setClause.getLength()) setClause += ", ";
                setClause += SQLFormat::escapeString(desc->columnName) + " = " + SQLFormat::escapeString(mc->asVariant().like(&setClause), '\'');
            }
        }

        if (setClause.getLength())
        {
            const SQLFormat::Results * res =
#ifdef SQLDEBUG
                SQLFormat::sendQuery(databaseIndex, "-- Should update the database here ");
                SQLFormat::cleanResults(res);
#else
                NULL;
#endif
            const FieldDescription * desc = fromPosition(indexOfIndex);
            if (!desc) return false;
            String commandIs;
            commandIs.Format("UPDATE %s SET %s WHERE %s = %u;",
                (const char*)tableName,
                (const char*)setClause,
                (const char*)SQLFormat::escapeString(desc->columnName),
                indexValue);

            // Send the query
            res = SQLFormat::sendQuery(databaseIndex, commandIs);
            if (res == NULL) return false;
            SQLFormat::cleanResults(res);
            return true;
        }
        return false;
    }
    bool TableDescription::updateIfAnyFieldModified(unsigned int indexOfIndex, uint64 indexValue, bool & otherFieldModified)
    {
        otherFieldModified = false;
        String setClause;
        for (unsigned int i = 0; i < (unsigned int)getFieldCount(); i++)
        {
            if (i == indexOfIndex) continue;
            const FieldDescription * desc = fromPosition(i);
            if (!desc) return false; // Bad table, don't act it's unsafe
            ModifiedCallback * mc = desc->getCB(this);
            if (mc && mc->isInit())
            {
                otherFieldModified = true;
                if (setClause.getLength()) setClause += ", ";
                setClause += SQLFormat::escapeString(desc->columnName) + " = " + SQLFormat::escapeString(mc->asVariant().like(&setClause), '\'');
            }
        }

        if (setClause.getLength())
        {
            const SQLFormat::Results * res =
#ifdef SQLDEBUG
                SQLFormat::sendQuery(databaseIndex, "-- Should update the database here ");
                SQLFormat::cleanResults(res);
#else
                NULL;
#endif
            const FieldDescription * desc = fromPosition(indexOfIndex);
            if (!desc) return false;
            String commandIs;
            commandIs.Format("UPDATE %s SET %s WHERE %s = " PF_LLU ";",
                (const char*)tableName,
                (const char*)setClause,
                (const char*)SQLFormat::escapeString(desc->columnName),
                indexValue);

            // Send the query
            res = SQLFormat::sendQuery(databaseIndex, commandIs);
            if (res == NULL) return false;
            SQLFormat::cleanResults(res);
            return true;
        }
        return false;
    }
    void TableDescription::updateReferenceIfRequired(const String & name, const String & value)
    {
        // Not implemented in this version, proved useless in reality
    }

    bool TableDescription::selectWhereImpl(const uint32 indexOfField, const Var & value)
    {
        if ((value.isEmpty() && indexOfField >= (uint32)getFieldCount())) return false;

        // Create the select command
        String commandIs;
        const FieldDescription * desc = fromPosition(indexOfField);
        if (!desc) return false;
        String field = desc->columnName;

        ModifiedCallback * mc = desc->getCB(this);
        if (!mc) return false;

        commandIs.Format("SELECT * FROM %s WHERE %s = %s LIMIT 1;",
                    (const char*)tableName,
                    (const char*)SQLFormat::escapeString(field),
                    (const char*)SQLFormat::escapeString(value.like(&commandIs), '\''));

        const SQLFormat::Results * res  = SQLFormat::sendQuery(databaseIndex, commandIs);
        if (res == NULL) { SQLFormat::cleanResults(res); return false; }

        bool isOkay = true;
        // Now get all the fields
        for (unsigned int i = 0; i < (unsigned int)getFieldCount(); i++)
        {
            desc = fromPosition(i);
            if (!desc) { isOkay = false; break; }
            // Plain field
            mc = desc->getCB(this);
            if (mc)
            {
                Var ret = mc->asVariant();
                if (!SQLFormat::getResults(res, ret, 0, desc->columnName, i))
                    { isOkay = false; break; }
                if (!ret.isEmpty())
                    mc->setValueDirect(ret);
            }
        }

        SQLFormat::cleanResults(res);
        return isOkay;
    }


    void TableDescription::hasBeenModifiedImpl(const uint32 indexOfField, const Var & value)
    {
        // Check when table are bounded
        if (holdData)
        {
            // We shouldn't synchronize to the database while it's being hold
            // However, we must track if the object was modified, as, on destruction, the object might be auto synchronized to the database
            wasModified = true;
            return;
        }
        if ((value.isEmpty() && indexOfField >= (uint32)getFieldCount())) return;

        // Check if the modified field is an index
        int indexOfIndex = hasIndex();
        String name = getFieldName((int)indexOfField), escapedName = SQLFormat::escapeString(name);
        if (indexOfField == (uint32)indexOfIndex)
        {
            // Check if it's a long index or a short index
            if (!hasLongIndex())
            {
                // We are updating the index, so either we have to query the field,
                // either we have to create a new one
                const unsigned int extractedValue = value.like(&extractedValue);
                if (extractedValue == Index::WantNewIndex)
                {
                    // We need to begin a transaction
                    // Get a list of set field
                    String sSetFieldName, sSetFieldValue;
                    getNotEmptyFieldsNameAndValue(sSetFieldName, sSetFieldValue);

                    // Need to get a new index from DB
                    String fields = escapedName + (sSetFieldName.getLength() ? ", " + sSetFieldName : "");
                    String values = sSetFieldName.getLength() ? "NULL, " + sSetFieldValue : "NULL";


                    if (insertInto(fields, values))
                    {
                        ModifiedCallback * mc = getFieldInstance(indexOfIndex);
                        if (mc) mc->setValueDirect(SQLFormat::getLastInsertedID(databaseIndex));
                    }
                }

                if (extractedValue != Index::DelayAction)
                {
                    // Check if at least a field has been modified, and in that case, we might need to update first
                    // (even if the row doesn't exist yet, else some information might be lost)
                    bool otherFieldModified = false;
                    if (extractedValue != Index::WantNewIndex)
                        updateIfAnyFieldModified(indexOfIndex, extractedValue, otherFieldModified);

                    // We need to retrieve all the field contents from the database here given the ID
                    if (!retrieveAllFields(indexOfIndex) && extractedValue != Index::WantNewIndex)
                    {
                        // Selection returned no fields, so let's create a new row with the specified ID
                        if (otherFieldModified)
                        {
                            String sSetFieldName, sSetFieldValue;
                            getNotEmptyFieldsNameAndValue(sSetFieldName, sSetFieldValue);

                            // Need to get a new index from DB
                            String fields = escapedName + (sSetFieldName.getLength() ? ", " + sSetFieldName : "");
                            String values = sSetFieldName.getLength() ? String().Format("%u, ", extractedValue) + sSetFieldValue : String().Format("%u", extractedValue);

                            if (insertInto(fields, values))
                                // And then retrieve All Fields again
                                retrieveAllFields(indexOfIndex);
                        }
                        else
                        {
                            // Need to get a new index from DB
                            if (insertInto(escapedName, String().Format("%u", extractedValue)))
                                // And then retrieve All Fields again
                                retrieveAllFields(indexOfIndex);
                        }
                    }
                }
            } else
            {
                // We are updating the index, so either we have to query the field,
                // either we have to create a new one
                const uint64 extractedValue = value.like(&extractedValue);
                if (extractedValue == LongIndex::WantNewIndex)
                {
                    // We need to begin a transaction
                    // Get a list of set field
                    String sSetFieldName, sSetFieldValue;
                    getNotEmptyFieldsNameAndValue(sSetFieldName, sSetFieldValue);

                    // Need to get a new index from DB
                    String fields = escapedName + (sSetFieldName.getLength() ? ", " + sSetFieldName : "");
                    String values = sSetFieldName.getLength() ? "NULL, " + sSetFieldValue : "NULL";


                    if (insertInto(fields, values))
                    {
                        ModifiedCallback * mc = getFieldInstance(indexOfIndex);
                        if (mc) mc->setValueDirect(SQLFormat::getLastInsertedID(databaseIndex));
                    }
                }

                if (extractedValue != LongIndex::DelayAction)
                {
                    // Check if at least a field has been modified, and in that case, we might need to update first
                    // (even if the row doesn't exist yet, else some information might be lost)
                    bool otherFieldModified = false;
                    if (extractedValue != LongIndex::WantNewIndex)
                        updateIfAnyFieldModified(indexOfIndex, extractedValue, otherFieldModified);

                    // We need to retrieve all the field contents from the database here given the ID
                    if (!retrieveAllFields(indexOfIndex) && extractedValue != Index::WantNewIndex)
                    {
                        // Selection returned no fields, so let's create a new row with the specified ID
                        if (otherFieldModified)
                        {
                            String sSetFieldName, sSetFieldValue;
                            getNotEmptyFieldsNameAndValue(sSetFieldName, sSetFieldValue);

                            // Need to get a new index from DB
                            String fields = escapedName + (sSetFieldName.getLength() ? ", " + sSetFieldName : "");
                            String values = sSetFieldName.getLength() ? String().Format("%llu, ", extractedValue) + sSetFieldValue : String().Format("%llu", extractedValue);

                            if (insertInto(fields, values))
                                // And then retrieve All Fields again
                                retrieveAllFields(indexOfIndex);
                        }
                        else
                        {
                            // Need to get a new index from DB
                            if (insertInto(escapedName, String().Format("%llu", extractedValue)))
                                // And then retrieve All Fields again
                                retrieveAllFields(indexOfIndex);
                        }
                    }
                }

            }
        } else
        {
            String valueAsString = value.like(&valueAsString);
            if (indexOfIndex < 0)
            {
                // This table doesn't have an index, so we need to find all fields that are already set
                // And update the record if the set's cardinal is not zero or insert a new item else
                // Some fields already exists, so update the values
                String whereClause;
                int count = buildWhereClause(whereClause, name);
                if (count)
                {
                    if (count == 1)
                    {
                        // Need to get a new index from DB
                        if (insertInto(escapedName, SQLFormat::escapeString(valueAsString, '\'')))
                        {
                            // And then retrieve All Fields again (to get default values)
                            retrieveAllFields(indexOfField);
                        }
                    }
                    else
                    {
                        // Need to specify the default value here, in the whereClause
                        appendDefaultValue(whereClause);
                        // Update now
                        if (updateWhere(name, valueAsString, whereClause))
                            updateReferenceIfRequired(name, valueAsString);
                    }
                }

            } else
            {
                // Check if we need to create a new entry (ID == WantNewIndex)
                // Or update an existing entry
                ModifiedCallback * mc = getFieldInstance(indexOfIndex);
                if (!hasLongIndex())
                {
                    const unsigned int extractedValue = mc ? mc->asVariant().like(&extractedValue) : 0;
                    if (extractedValue == Index::WantNewIndex)
                    {
                        // Need to get a new index from DB
                        if (insertInto(escapedName, SQLFormat::escapeString(valueAsString, '\'')))
                        {
                            // Set the index now
                            if (mc) mc->setValueDirect(SQLFormat::getLastInsertedID(databaseIndex));
                            retrieveAllFields(indexOfIndex);
                        }

                    } else if (extractedValue != Index::DelayAction)
                    {
                        if (updateWhere(name, valueAsString, SQLFormat::escapeString(getFieldName(indexOfIndex)), String().Format("%u", extractedValue)))
                            updateReferenceIfRequired(name, valueAsString);
                    }
                } else
                {
                    const uint64 extractedValue = mc ? mc->asVariant().like(&extractedValue) : 0;
                    if (extractedValue == LongIndex::WantNewIndex)
                    {
                        // Need to get a new index from DB
                        if (insertInto(escapedName, SQLFormat::escapeString(valueAsString, '\'')))
                        {
                            // Set the index now
                            if (mc) mc->setValueDirect(SQLFormat::getLastInsertedID(databaseIndex));
                            retrieveAllFields(indexOfIndex);
                        }

                    } else if (extractedValue != LongIndex::DelayAction)
                    {
                        if (updateWhere(name, valueAsString, SQLFormat::escapeString(getFieldName(indexOfIndex)), String().Format(PF_LLU, extractedValue)))
                            updateReferenceIfRequired(name, valueAsString);
                    }
                }
            }
        }
    }

    void TableDescription::synchronizeAllFields(const String & referenceColumn)
    {
        String commandIs;
        if (referenceColumn.getLength())
        {
            String setClause, value;

            for (unsigned int i = 0; i < (unsigned int)getFieldCount(); i++)
            {
                const FieldDescription * desc = fromPosition(i);
                if (!desc) return; // Bad table, don't act it's unsafe
                ModifiedCallback * mc = desc->getCB(this);
                if (mc && mc->isInit())
                {
                    String fieldName = desc->columnName;
                    String fieldValue = mc->asVariant().like(&fieldValue);
                    if (fieldName == referenceColumn) { value = fieldValue; continue; }

                    if (setClause.getLength()) setClause += ", ";
                    setClause += SQLFormat::escapeString(fieldName) + " = " + SQLFormat::escapeString(fieldValue, '\'');
                }
            }
            if (!setClause.getLength()) return;

            // Due to some database implementation, we have to check if we already have an existing row with this parameter
            {
                commandIs.Format("SELECT * FROM %s WHERE %s = %s LIMIT 1;",
                    (const char*)tableName,
                    (const char*)SQLFormat::escapeString(referenceColumn),
                    (const char*)SQLFormat::escapeString(value, '\''));

                const SQLFormat::Results * res  = SQLFormat::sendQuery(databaseIndex, commandIs);
                Var ret;
                if (res != NULL && SQLFormat::getResults(res, ret, 0, ""))
                {
                    commandIs.Format("UPDATE %s SET %s WHERE %s = %s;",
                        (const char*)tableName,
                        (const char*)setClause,
                        (const char*)SQLFormat::escapeString(referenceColumn),
                        (const char*)SQLFormat::escapeString(value, '\''));
                } else commandIs = "";
                SQLFormat::cleanResults(res);
            }
        }

        // Use insert into if not found yet (this will insert the object)
        if (!commandIs.getLength())
        {
            String name, value;
            for (unsigned int i = 0; i < (unsigned int)getFieldCount(); i++)
            {
                const FieldDescription * desc = fromPosition(i);
                if (!desc) return; // Bad table, don't act it's unsafe
                ModifiedCallback * mc = desc->getCB(this);
                if (mc && mc->isInit())
                {
                    String fieldName = desc->columnName;
                    String fieldValue = mc->asVariant().like(&fieldValue);

                    if (name.getLength()) { name += ", "; value += ", "; }
                    name += SQLFormat::escapeString(fieldName);
                    value += SQLFormat::escapeString(fieldValue, '\'');
                }
            }
            if (!value.getLength()) return;

            commandIs.Format("INSERT INTO %s (%s) VALUES (%s);",
                (const char*)tableName,
                (const char*)name,
                (const char*)value);
        }

        // Then build the query
        const SQLFormat::Results * res =
#ifdef SQLDEBUG
            SQLFormat::sendQuery(databaseIndex, "-- Should replace from the database here ");
            SQLFormat::cleanResults(res);
#else
            NULL;
#endif
        // Send the query
        res = SQLFormat::sendQuery(databaseIndex, commandIs);
        if (res)
        {
            wasModified = false;
            SQLFormat::cleanResults(res);
        }
    }

    int TableDescription::getNotEmptyFieldsNameAndValueAsArray(String * & name, String * & value)
    {
        delete[] name; delete[] value;
        unsigned int count = 0;
        unsigned int i = 0;
        for (; i < (unsigned int)getFieldCount(); i++)
        {
            if (i == hasIndex()) continue;
            ModifiedCallback * mc = getFieldInstance((int)i);
            if (mc && mc->isInit())
            {
                /*
                const FieldDescription * field = getTableDeclaration().findField(i + 1);
                if (field == NULL) return 0;
                if (field->value->isEqual(UniversalTypeIdentifier::getTypeID((Database::Index*)0))) continue;
                if (field->value->isEqual(UniversalTypeIdentifier::getTypeID((ReferenceBase*)0)))
                {
                    // Check if the reference is empty too
                    ReferenceBase * tmp = fieldArray[i].toPointer((ReferenceBase*)0);
                    if (tmp == 0) { continue; }
                    if (!tmp->isBound() && tmp->isEmpty())  continue;
                }
                */
                count++;
            }
        }

        if (count)
        {
            name = new String[count];
            if (name == 0) return 0;
            value = new String[count];
            if (value == 0) { delete[] name; return 0; }
            unsigned int j = 0;
            for (i = 0; i < (unsigned int)getFieldCount(); i++)
            {
                if (i == hasIndex()) continue;
                ModifiedCallback * mc = getFieldInstance((int)i);
                if (mc && mc->isInit())
                {
                    /*
                const FieldDescription * field = getTableDeclaration().findField(i + 1);
                    if (field == NULL) { delete[] name; delete[] value; return 0; }
                    if (field->value->isEqual(UniversalTypeIdentifier::getTypeID((Database::Index*)0))) continue;
                    if (field->value->isEqual(UniversalTypeIdentifier::getTypeID((ReferenceBase*)0)))
                    {
                        // Check if the reference is used and pointing to data
                        ReferenceBase * tmp = fieldArray[i].toPointer((ReferenceBase*)0);
                        if (tmp == 0) { continue; }
                        if (!tmp->isBound() && tmp->isEmpty())  continue;

                        name[j] = field->columnName;
                        // In all cases (bound or not) tmp->ref is set to the good value
                        value[j] = tmp->ref.like(value[j]);
                        j++;
                    } else
                    */
                    name[j] = getFieldName(i);
                    value[j] = mc->asVariant().like(&value[j]);
                    j++;
                }
            }
        }
        // Okay return
        return count;
    }

    void TableDescription::getNotEmptyFieldsNameAndValue(String & name, String & value)
    {
        String * arrayName = 0, * arrayValue = 0;
        int count = getNotEmptyFieldsNameAndValueAsArray(arrayName, arrayValue);

        for (unsigned int i = 0; i < (unsigned int)count; i++)
        {
            // This field is not empty, so update the name
            if (name.getLength()) { name += ", "; value += ", "; }
            name += SQLFormat::escapeString(arrayName[i]);
            value += SQLFormat::escapeString(arrayValue[i], '\'');
        }
        if (count) { delete[] arrayName; delete[] arrayValue; }
    }

    bool TableDescription::retrieveAllFields(unsigned int indexOfIndex)
    {
        if (indexOfIndex == -1) indexOfIndex = hasIndex();
        if ((int)indexOfIndex < 0) return false;
/*
        if (beingReferred)
        {
            // We are looping through circular references
            RunTimeError("Circular references detected - check database model for possible circular references");
            return false;
        }

        beingReferred = true;
        */
        // Create the select command
        String commandIs;
        const FieldDescription * desc = fromPosition(indexOfIndex);
        if (!desc) { /* beingReferred = false; */ return false; }
        String field = desc->columnName;

        ModifiedCallback * mc = desc->getCB(this);
        if (!mc) return false;

        commandIs.Format("SELECT * FROM %s WHERE %s = %s LIMIT 1;",
                    (const char*)tableName,
                    (const char*)SQLFormat::escapeString(field),
                    (const char*)SQLFormat::escapeString(mc->asVariant().like(&commandIs), '\''));

        const SQLFormat::Results * res  = SQLFormat::sendQuery(databaseIndex, commandIs);
        Var ret;
        if (res == NULL || !SQLFormat::getResults(res, ret, 0, "")) { SQLFormat::cleanResults(res); /* beingReferred = false; */ return false; }


//            UniversalTypeIdentifier::TypeID referenceType = UniversalTypeIdentifier::getTypeID((Database::ReferenceBase*)0);
//            unsigned int br = boundReferences;
        bool isOkay = true;
        // Now get all the fields
        for (unsigned int i = 0; i < (unsigned int)getFieldCount(); i++)
        {
            desc = fromPosition(i);
            if (!desc) { isOkay = false; break; }
/*
            if (field->value->isEqual(referenceType))
            {
                // Reference field, should update the referenced table too if bound
                ReferenceBase * tmp = fieldArray[i].toPointer((ReferenceBase*)0);
                if (tmp == 0) { isOkay = false; break; }
                // Get the result
                String ret;
                if (!SQLFormat::getResults(res, ret, 0, field->columnName))
                    { isOkay = false; break; }
                if (ret.getLength()) tmp->ref = ret;

                if (br && tmp->isBound())
                {
                    // And update the referenced table definition
                    TableDefinition * ref = tmp->tableRef;
                    // If the referenced table is not valid, clear it
                    if ((unsigned int)ret == Index::DelayAction)
                    {
                        ref->Reset();
                    } else
                    {
                        // Setting the index field automatically retrieve all the field in the referenced table
                        ref->getField(ref->getIndexName()) = tmp->ref.like(indexOfIndex);
                    }
                    br --;
                }
            }
            else
            {
*/
                // Plain field
                mc = desc->getCB(this);
                if (mc)
                {
                    Var ret = mc->asVariant();
                    if (!SQLFormat::getResults(res, ret, 0, desc->columnName, i))
                        { isOkay = false; break; }
                    if (!ret.isEmpty())
                        mc->setValueDirect(ret);
                }
//                }
        }

        SQLFormat::cleanResults(res);
//            beingReferred = false;
        return isOkay;
    }

    void TableDescription::Delete()
    {
        // Find the index field
        const FieldDescription * desc = fromPosition(hasIndex());
        if (desc)
        {
            // We are lucky, and index does exist
            ModifiedCallback * mc = desc->getCB(this);
            if (!hasLongIndex())
            {
                const unsigned int index = mc ? mc->asVariant().like(&index) : 0;
                // Is it set ?
                if (index != Index::WantNewIndex && index != Index::DelayAction)
                {
                    // It's set, so let's delete that specific record here
                    deleteWhere(SQLFormat::escapeString(desc->columnName), String().Format("%u", index));
                } else  // No set, so delete where other fields are set
                    desc = NULL;
            } else
            {
                const uint64 index = mc ? mc->asVariant().like(&index) : 0;
                // Is it set ?
                if (index != LongIndex::WantNewIndex && index != LongIndex::DelayAction)
                {
                    // It's set, so let's delete that specific record here
                    deleteWhere(SQLFormat::escapeString(desc->columnName), String().Format(PF_LLU, index));
                } else  // No set, so delete where other fields are set
                    desc = NULL;
            }
        }

        if (desc == NULL)
        {
            // There is no index here, so build the delete statement
            String whereClause;
            int count = buildWhereClause(whereClause);
            if (count)
            {
                deleteWhere(whereClause);
            }
        }

        Reset();
    }

    void TableDescription::Reset()
    {
//            UniversalTypeIdentifier::TypeID referenceType = UniversalTypeIdentifier::getTypeID((Database::ReferenceBase*)0);
        // Reset all the fields
        for (unsigned int i = 0; i < (unsigned int)getFieldCount(); i++)
        {
            const FieldDescription * desc = fromPosition(i);
            if (desc)
            {
                ModifiedCallback * mc = desc->getCB(this);
                if (mc) mc->setValueDirect(Var::Empty());
            }
/*
            fieldArray[i].Reset();
            // Check if the field array is a reference
            if (getTableDeclaration().findField((uint32)i+1)->value->isEqual(referenceType))
            {
                // It is, so initialize it with default reference
                fieldArray[i] = ReferenceBase();
            }
            */
        }

        // And set the index field to "New index required"
        if (hasIndex() >= 0)
        {
            ModifiedCallback * mc = getFieldInstance(hasIndex());
            if (mc) mc->setValueDirect(!hasLongIndex() ? Database::Index::DelayAction : Database::LongIndex::DelayAction);
        }
//            boundReferences = 0;
    }
    unsigned int TableDescription::getIndex() const
    {
        if (hasLongIndex()) return -1;
        ModifiedCallback * mc = const_cast<TableDescription*>(this)->getFieldInstance(hasIndex());
        if (mc) return mc->asVariant().like((unsigned int*)0);
        return -1;
    }
    uint64 TableDescription::getLongIndex() const
    {
        if (!hasLongIndex()) return -1;
        ModifiedCallback * mc = const_cast<TableDescription*>(this)->getFieldInstance(hasIndex());
        if (mc) return mc->asVariant().like((uint64*)0);
        return -1;
    }
    void TableDescription::setRowFieldsUnsafe(const SQLFormat::Results * res, const unsigned int index)
    {
        for (unsigned int fields = 0; fields < (unsigned int)getFieldCount(); fields++)
        {
            // The returned string
            const FieldDescription * desc = fromPosition(fields);
            ModifiedCallback * mc = desc ? desc->getCB(this) : 0;
            if (desc && mc)
            {
                Var ret = mc->asVariant();
                SQLFormat::getResults(res, ret, index, desc->columnName, fields);
                if (!ret.isEmpty())
                    mc->setValueDirect(ret);
            }
        }
    }

    DatabaseDeclarationRegistry & getDatabaseRegistry()
    {
        static DatabaseDeclarationRegistry reg; return reg;
    }


}

#endif

