#include "../../Externals/sqlite3/sqlite3.h"
#include "../../include/Variant/UTIImpl.hpp"
#include "../../include/Database/Database.hpp"
#include "../../include/Logger/Logger.hpp"
#include "../../include/File/File.hpp"
#include "../../include/Threading/Threads.hpp"
#include "../../include/Utils/ScopePtr.hpp"

#if (HasDatabase == 1)

  #if (HasThreadLocalStorage != 1)
    #warning SQLite driver requires enabling ThreadLocalStorage flag to be built
  #else

#if (DEBUG==1)
    #define SQLDEBUG
#endif

// Under linux system, the compiler defines _REENTRANT if you build with pthread (or _MT on Windows)
#ifdef _MSC_VER
    #ifndef _MT
        #define MONOTHREAD_APPLICATION
    #endif
#else
    #ifndef _REENTRANT
        #define MONOTHREAD_APPLICATION
    #endif
#endif


namespace Database
{

    struct LoggerErrorCallback : public DatabaseConnection::ClassErrorCallback
    {
        virtual void databaseErrorCallback(DatabaseConnection * connection, const uint32 index, const ErrorType & error, const String & message)
        {
            const char * errorType[] = { "UNK", "RQT", "CON" };
            Logger::log(Logger::Error | Logger::Database, "DB ERROR(%d,%s): %s", index, errorType[(int)error], (const char*)message);
        }
    } defaultErrorCallback;

    DatabaseConnection::ClassErrorCallback * DatabaseConnection::errorCallback = &defaultErrorCallback;
    const char SQLFormat::escapeQuote = 0;
    const BuildDatabaseConnection * SQLFormat::builder = 0;
    static bool creatingDatabase = false; // Set to true only when creating database as the given URL == file doesn't exists for SQLite (not required in MySQL)

    /** The basic system for handling database connections is made this way:
           A static Builder instance (child of BuildDatabaseConnection) (likely SimpleBuilder for SQLite), will create a DatabaseConnection instance.
           The DatabaseConnection (likely SingleDatabaseConnection or MultipleDatabaseConnection) contains an array of underlying low-level connection objects
           (typically sqlite3 *, mysqlclient *, etc...), one for each database specified in the model.

        Unless disabled at compile time by macro, each thread owns a unique DatabaseConnection instance (stored in TLS). */


    /** A single database connection that doesn't check the index */
    struct SingleDatabaseConnection : public DatabaseConnection
    {
        sqlite3 * instance;
        String    databaseName;
        String    URL;

        /** Get the low level object used for the index-th connection */
        virtual void * getLowLevelConnection(const uint32 index = 0)
        {
            if (index) return 0;
            return instance;
        }
        /** Set the low level object used for the index-th connection */
        virtual bool setLowLevelConnection(const uint32 index, void * connection)
        {
            if (index) return false;
            if (instance != NULL) sqlite3_close(instance);
            instance = (sqlite3*)connection;
            return true;
        }

        /** Get the connection parameters
            @param index    The index of the connection to retrieve
            @param dbName   On output, set to the database name to connect to
            @param dbURL    On output, set to the database URL to connect to (can contain username:password part)
            @return true on successful connection */
        virtual bool getDatabaseConnectionParameter(const uint32 index, String & dbName, String & dbURL) const
        {
            if (index) return false;
            dbName = databaseName;
            dbURL = URL;
            return true;
        }
        /** Create models on the database connections
            @return true on successful model creation */
        virtual bool createModels(const bool forceReinstall = false)
        {
            return SQLFormat::createDatabaseLikeModel(0, getDatabaseRegistry().getDeclaration(databaseName), databaseName);
        }

        SingleDatabaseConnection(const String & databaseName, const String & URL) : instance(NULL), databaseName(databaseName), URL(URL) {}
        ~SingleDatabaseConnection() { if (instance != NULL) sqlite3_close(instance); instance = NULL; }
    };

    struct SimpleBuilder : public BuildDatabaseConnection
    {
        String    databaseName;
        String    URL;

        Database::DatabaseConnection * buildDatabaseConnection() const { return new SingleDatabaseConnection(databaseName, URL); }
    };

    SimpleBuilder & getSimpleBuilder() { static SimpleBuilder simpleBuilder; return simpleBuilder; }

    // This is used for TLS, each new thread will call this when first used
    DatabaseConnection * constructConnection(Threading::Thread::LocalVariable::Key, DatabaseConnection *) { return SQLFormat::builder ? SQLFormat::builder->buildDatabaseConnection() : 0; }
    void destructConnection(DatabaseConnection * conn) { delete conn; }
    static Threading::Thread::LocalVariable::Key & getTLSKey() { static Threading::Thread::LocalVariable::Key key = 0; return key; }
    static Threading::Thread::LocalVariable * getTLSVariable() { return Threading::Thread::getLocalVariable(getTLSKey()); }


    // The thread local storage
    void * getSQLiteConnection(uint32 dbIndex)
    {
        // If the builder doesn't exist, construct the default one
        if (!SQLFormat::builder)
        {
            SQLFormat::builder = &getSimpleBuilder();
        }

        // The first time we call this, it'll return a null connection, so we can act accordingly
        Threading::Thread::LocalVariable * databaseConnection = getTLSVariable();
        if (!databaseConnection)
        {
            // Ok, build a local variable for all threads, with a new connection for the first one.
            DatabaseConnection * connection = SQLFormat::builder->buildDatabaseConnection();
            getTLSKey() = Threading::Thread::appendLocalVariable(connection, constructConnection, destructConnection);
            // Check it worked, by querying the variable again
            databaseConnection = getTLSVariable();
            if (!databaseConnection) return 0;
        }
        // Specific signal to unregister the local variable key for all thread,
        // but you should now use deleteSQLiteConnection instead.
        Assert(dbIndex != (uint32)-1);

        // Ok, get the connection object.
        GetLocalVariable(DatabaseConnection, connection, getTLSKey());
        void * conn = connection ? connection->getLowLevelConnection(dbIndex) : 0;
        if (conn == 0)
        {
            // If the low level object is not valid, let's create one.
            String dbName, dbURL;
            connection->getDatabaseConnectionParameter(dbIndex, dbName, dbURL);
            conn = SQLFormat::createDatabaseConnection(dbName, dbURL);
            connection->setLowLevelConnection(dbIndex, conn);
        }
        return conn;
    }
    // Delete the database connection
    bool deleteSQLiteConnection(uint32 index)
    {
        // You are supposed to delete already existing connections.
        if (!SQLFormat::builder) return false;

        // You are supposed to delete already existing connections.
        Threading::Thread::LocalVariable * databaseConnection = getTLSVariable();
        if (!databaseConnection) return false;

        // Specific signal to unregister the local variable key for all thread.
        // It doesn't destruct the connection, you have to do it by your own before calling this.
        if (index == (uint32)-1) { Threading::Thread::removeLocalVariable(getTLSKey()); return true; }

        // Ok, get the connection object.
        GetLocalVariable(DatabaseConnection, connection, getTLSKey());
        if (!connection) return false;
        return connection->setLowLevelConnection(index, 0);
    }

    // Change the default connection builder
    void SQLFormat::useDatabaseConnectionBuilder(BuildDatabaseConnection * _builder)
    {
        SQLFormat::builder = _builder;
    }


    String constructFilePath(String fullPath, const String & URL)
    {
        // Check if the URL points to a file
        File::Info info(URL);
        if (info.doesExist())
        {
            if (info.isFile()) return URL;
            // Build a full path if the URL is a directory
            if (info.isDir())
            {
                if(fullPath.Find(".db") == -1) fullPath += String(".db");
                fullPath = URL.normalizedPath(Platform::Separator, true) + fullPath;
            }
        } else if (URL.getLength())
        {
            // Check if the URL points to a existing directory but non existing file we need to create.
            File::Info folder(info.path);
            if (folder.doesExist() && folder.isDir())  return URL;
        }
        // Else only use the given fullPath
        fullPath = File::General::normalizePath(fullPath).normalizedPath(Platform::Separator, false);
        return creatingDatabase ? URL : fullPath;
    }



    /** Return the last error */
    String getLastError(sqlite3 * DBConnection)
    {
        if (!DBConnection) return "";
        return sqlite3_errmsg(DBConnection);
    }



    void checkBusyDatabase(sqlite3 * connection)
    {
        int errorCode = sqlite3_errcode(connection);
        if( (errorCode == SQLITE_BUSY) || (errorCode == SQLITE_LOCKED) )
        {
#if (DEBUG==1)
            Logger::log(Logger::Dump | Logger::Error, ">>>>>>>>>>>>>>>>>>>>> DEADLOCK DETECTED IN SQLITE USAGE: %s", (const char *)getLastError(connection));
            // the timeout is 60 s (Cf. createDatabaseConnection)
            // If we are in busy mode, it's a deadlock
            Platform::breakUnderDebugger();
#endif
        }
    }

    /** The default implementation  */
    String SQLFormat::escapeString(const String & sStr, const char embrace, const uint32 dbIndex)
    {
        char * escapedStr = sqlite3_mprintf("%q", (const char *)sStr);
        if (escapedStr == NULL) return "";
        String ret = escapedStr;
        sqlite3_free(escapedStr);

        if (embrace)
            ret = String(embrace) + ret + String(embrace);

        return ret;
    }




    /** Return the last error */
    String SQLFormat::getLastError(const uint32 dbIndex)
    {
        return ::Database::getLastError((sqlite3*)getSQLiteConnection(dbIndex));
    }

    void signalError(const uint32 index, const void * DBConnection = NULL, const char * sql = NULL)
    {
        GetLocalVariable(DatabaseConnection, connection, getTLSKey());
        if (!connection)
        {
            if (DBConnection)
                Logger::log(Logger::Error | Logger::Database, (const char *)String::Print("Error while processing: %s => %s", sql ? sql : "", (const char *)::Database::getLastError((sqlite3*)DBConnection)));
            else
                DatabaseConnection::notifyError("Error with invalid database connection");
            return;
        }

        sqlite3 * conn = (sqlite3*)(DBConnection ? DBConnection : connection->getLowLevelConnection(index));
        connection->notifyError(index, DatabaseConnection::ClassErrorCallback::BadQuery, String::Print("%s : %s", sql ? sql : "", sqlite3_errmsg(conn)));
    }
    // This one does not handle errors, it can be used internally
    const SQLFormat::Results * sendQueryInternal(const void * DBConnection, const String & sStr, String * error = 0)
    {
        sqlite3_stmt *pStmt = NULL;
        if(!DBConnection) return 0;

        SQLFormat::Results * pRet = new SQLFormat::Results;
        const char *         szError = 0;
        int                  result = 0;

        result = sqlite3_prepare_v2((sqlite3 *)DBConnection,(const char *)sStr,	-1, &pStmt, &szError );
        if (result != SQLITE_OK || !szError)
        {
            if (error) *error = szError;
            delete pRet;
            return 0;
        }

        // Store the request handle and the count, if provided
        pRet->privateData = pStmt;

        return pRet;
    }

    const SQLFormat::Results * SQLFormat::sendQuery(const uint32 dbConnection, const String & sStr, const void * DBConnection)
    {
        Logger::log(Logger::Database, "%s [" PF_LLD "]", (const char *)(sStr), (int64)Threading::Thread::getCurrentThreadID());

        if (!DBConnection) DBConnection = getSQLiteConnection(dbConnection);

        const SQLFormat::Results * pRet = sendQueryInternal(DBConnection, sStr);
        if (!pRet)
        {
            checkBusyDatabase((sqlite3*)DBConnection);
            signalError(dbConnection, DBConnection, (const char *)sStr);
        }

        return pRet;
    }

    unsigned int SQLFormat::getLastInsertedID(const uint32 dbIndex, const void * DBConnection)
    {
        if (!DBConnection) DBConnection = getSQLiteConnection(dbIndex);
        return (unsigned int)(sqlite3_last_insert_rowid((sqlite3*)DBConnection));
    }







    /** Initialize the SQL library and connect to the server (return true on success) */
    bool SQLFormat::initialize(const String & dataBase, const String & URL, const String & User, const String & Password, const unsigned short Port, const bool selectDatabase, const uint32 dbIndex)
    {
        getSimpleBuilder().databaseName = dataBase;
        getSimpleBuilder().URL = URL;
        return true;
    }

    /** Create the given user. Must be called by a connection root */
    bool SQLFormat::createDBUser(const String & databaseName, const String & User, const String & Password)
    {
        return true;
    }
    /** Delete the given user. Must be called by a connection root. */
    bool SQLFormat::deleteDBUser(const String & User)
    {
        return true;
    }


    // Serialize a blob
    void SQLFormat::serializeBlob(const Database::Blob * inner, String & output)
    {
        unsigned int length = inner->innerData.getLength();
        unsigned int finalLength = output.getLength() + length * 2 + 4;
        char * _out = output.Alloc(finalLength);
        if (!_out) return;
        char * out = &_out[output.getLength()];
        const unsigned char * in = (const unsigned char*)inner->innerData;
        out[0] = 'X'; out[1] = '\'';
        static const uint16 * const hex = (const uint16 * const)(
        "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"
        "202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f"
        "404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f"
        "606162636465666768696a6b6c6d6e6f707172737475767778797a7b7c7d7e7f"
        "808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9f"
        "a0a1a2a3a4a5a6a7a8a9aaabacadaeafb0b1b2b3b4b5b6b7b8b9babbbcbdbebf"
        "c0c1c2c3c4c5c6c7c8c9cacbcccdcecfd0d1d2d3d4d5d6d7d8d9dadbdcdddedf"
        "e0e1e2e3e4e5e6e7e8e9eaebecedeeeff0f1f2f3f4f5f6f7f8f9fafbfcfdfeff");
        uint16 * ret = (uint16*)&out[2];
        for(unsigned int i = 0; i < length; ++i) ret[i] = hex[in[i]];
        ret[length]=0;
        out[length * 2 + 2] = '\'';
        output.releaseLock(finalLength);
    }
    // Unserialize a blob
    void SQLFormat::unserializeBlob(Database::Blob * blob, const String & input)
    {
        // Useless in our implementation, but set anyway
        if (!blob) return;
        if (input.midString(0, 2) == "X'")
        {
            // It's an encoded binary blob, let's decode it
            unsigned int length = (input.getLength() - 3) / 2;
            char * data = new char[length];
            const unsigned char * in = &((const unsigned char*)input)[2];
            for (unsigned int i = 0; i < length; i++)
            {
                uint8 hi = in[i * 2 + 0];
                uint8 lo = in[i * 2 + 1];
                data[i] = hi >= 'a' ? (hi + 10 - 'a') : (hi >= 'A' ? (hi + 10 - 'A') : (hi - '0'));
                data[i] <<= 4;
                data[i] |= lo >= 'a' ? (lo + 10 - 'a') : (lo >= 'A' ? (lo + 10 - 'A') : (lo - '0'));
            }
            blob->setData(data, length);
            delete[] data;
        }
        else blob->setData((const unsigned char*)input, input.getLength());
    }


    bool extractStatement(sqlite3_stmt * pStmt, Var & ret, unsigned int index)
    {
        if (ret.isExactly((Database::Index*)0))
        {
            ret = (Database::Index)(unsigned int)sqlite3_column_int(pStmt, index);
            return true;
        } else if (ret.isExactly((Database::LongIndex*)0))
        {
            ret = (Database::LongIndex)(int64)sqlite3_column_int64(pStmt, index);
            return true;
        } else if (ret.isExactly((unsigned int *)0))
        {
            ret = (unsigned int)sqlite3_column_int(pStmt, index);
            return true;
        } else if (ret.isExactly((int *)0))
        {
            ret = sqlite3_column_int(pStmt, index);
            return true;
        } else if (ret.isExactly((int64 *)0))
        {
            ret = sqlite3_column_int64(pStmt, index);
            return true;
        } else if (ret.isExactly((uint64 *)0))
        {
            ret = (uint64)sqlite3_column_int64(pStmt, index);
            return true;
        } else if (ret.isExactly((double *)0))
        {
            ret = sqlite3_column_double(pStmt, index);
            return true;
        } else if (ret.isExactly((Blob*)0))
        {
            // Blob are handled differently
            const void * blob = sqlite3_column_blob(pStmt, index);
            const int len = sqlite3_column_bytes(pStmt, index);
            if (!blob || !len) { ret.Reset(); return true; }
            Blob * inner = ret.toPointer((Blob*)0);
            inner->setData(blob, len);
            return true;
        }
        // Reset the value here
        ret.Reset();
        const char * text = (const char*)sqlite3_column_text(pStmt, index);
        if (text && text[0] != 0) ret = (String)text;
        return true;
    }

    /** Get the results of a previous query
        rowIndex must never decrease between calls */
    bool SQLFormat::getResults(const SQLFormat::Results * res, Type::Var & ret, const unsigned int rowIndex, const String & fieldName, const unsigned int fieldIndex)
    {
        // Check arguments
        if (res == NULL) return false;
        // the request handle
        sqlite3_stmt * pStmt = (sqlite3_stmt *)res->privateData;
        // Don't allow index decrease between calls
        if (res->privateIndex > (int)rowIndex)
            return false;

        // Check if we need to increase the row index to be on the right row
        if ((int)rowIndex > res->privateIndex)
        {
            for (;(unsigned int)res->privateIndex != rowIndex; (*const_cast<int*>(&res->privateIndex))++)
            {
                // If there is not enough row in the result we go out
                if(sqlite3_step(pStmt) != SQLITE_ROW)
                    return false;
            }
        }

        // Check for empty iteration testing
        if (fieldIndex == (unsigned int)-1 && !fieldName)
            return true;

        // Get an array of all current field in the result
        unsigned int fieldCount = sqlite3_column_count(pStmt);
        if (fieldCount == 0)
            return false;

        // Now find the requested field name
        unsigned int index = 0;
        if (fieldIndex < fieldCount)
        {   // Shortcut when the field index is known beforehand
            String columnName = sqlite3_column_name(pStmt, fieldIndex);
            if (fieldName == columnName)
                return extractStatement(pStmt, ret, fieldIndex);
        }

        for (index = 0; index < fieldCount; ++index)
        {
            String columnName = sqlite3_column_name(pStmt, index);
            if (fieldName == columnName)
                return extractStatement(pStmt, ret, index);
        }

        return false;
    }



    /** Clean the result object */
    void SQLFormat::cleanResults(const SQLFormat::Results * res)
    {
        if (res == NULL)
            return;

        int currentCount = 0;
        // the request handle

        sqlite3_stmt * pStmt = (sqlite3_stmt *)res->privateData;
        // Need to flush the result set (depending on queries, for those having side actions)
        while(sqlite3_step(pStmt) == SQLITE_ROW)
        {
            currentCount++;
        }

        sqlite3_finalize(pStmt);
        delete const_cast<Results*>(res);
        //checkBusyDatabase();
    }

    void SQLFormat::finalize(const uint32 dbIndex)
    {
        if (dbIndex == -1)
        {
            // Destroy all connections and the connection object
            uint32 index = 0;
            bool conn = deleteSQLiteConnection(index++);
            while (conn) { conn = deleteSQLiteConnection(index++); }
            // Then destroy the database connection object
            deleteSQLiteConnection(-1);
        }
        else deleteSQLiteConnection(dbIndex);
    }

    bool SQLFormat::createDatabaseLikeModel(const uint32 dbIndex, Database::DatabaseDeclaration * model, const String & databaseName, const bool forceReinstall)
    {
        // Check argument
        if (model == NULL) return false;

        void * DBConnection = getSQLiteConnection(dbIndex);
        if (!DBConnection) return false;

        // Check if the database already exists
        const Results * res = NULL;
        if (!forceReinstall)
        {
            res = sendQuery(dbIndex, "SELECT COUNT(*) AS count FROM sqlite_master");
            if (res != NULL)
            {
                Var ret = 0;
                bool hadTables = getResults(res, ret, 0, "count", 0);
                cleanResults(res);
                if (hadTables && ret.like((int*)0)) // If we have something in the master table, and if the tables count exist
                    return true;
            }
        }

        String error;
        // Create the tables now
        uint32 tableCount = model->getTableCount();

        if(tableCount)
        {
            String setCharset = "PRAGMA encoding = \"UTF-8\";";
            res = sendQuery(dbIndex, setCharset);
            if (res == NULL) error = getLastError(dbIndex);
            cleanResults(res);
        }
        unsigned int i = 0;
        for (; i < tableCount; i++)
        {
            String create = "CREATE TABLE ";
            String drop = "DROP TABLE IF EXISTS ";
            String indexes;
            const AbstractTableDescription * td = model->findTable(i);
            if (!td) return false;
            String tableName = escapeString(td->tableName);

            // First drop the table if it already exists
            drop += tableName;
            drop += " ;";
            res = sendQuery(dbIndex, drop);
            if (res == NULL) error = getLastError(dbIndex);
            cleanResults(res);


            // Then create the table
            create += tableName;
            create += " (\n";
            unsigned int j = 0;

            for (; j < td->fieldCount; j++)
            {
                const FieldDescription * field = td->getAbstractFieldDescription(j);
                if (!field) return false;

                create += escapeString(field->columnName);
                create += " ";
                String defaultText = field->defaultValue.getLength() ? (" DEFAULT " + field->defaultValue + " ") : " ";
                // Now, the big type switch
                if (field->value->isEqual(getTypeID((Database::Index*)0)))
                    create += "INTEGER PRIMARY KEY AUTOINCREMENT";
                if (field->value->isEqual(getTypeID((Database::LongIndex*)0)))
                    create += "INTEGER PRIMARY KEY AUTOINCREMENT"; // Under SQLite integer are 64 bits anyway
                else if (field->value->isEqual(getTypeID((String*)0)))
                    create += "TEXT" + (field->defaultValue.getLength() ? String(" DEFAULT " + escapeString(field->defaultValue) + " ") : String(" "));
                else if (field->value->isEqual(getTypeID((int*)0)))
                    create += "INTEGER" + defaultText;
                else if (field->value->isEqual(getTypeID((double*)0)))
                    create += "REAL" + defaultText;
                else if (field->value->isEqual(getTypeID((unsigned int*)0)))
                    create += "INTEGER UNSIGNED" + defaultText;
                else if (field->value->isEqual(getTypeID((int64*)0)))
                    create += "INTEGER" + defaultText;
                else if (field->value->isEqual(getTypeID((uint64*)0)))
                    create += "INTEGER UNSIGNED" + defaultText;
                else if (field->value->isEqual(getTypeID((Blob*)0)))
                    create += "BLOB DEFAULT NULL ";
                else if (field->value->isEqual(getTypeID((NotNullString*)0)))
                    create += "TEXT NOT NULL ";
                else if (field->value->isEqual(getTypeID((NotNullUniqueString*)0)))
                    create += "TEXT NOT NULL UNIQUE ";
                else if (field->value->isEqual(getTypeID((NotNullInt*)0)))
                    create += "INTEGER NOT NULL ";
                else if (field->value->isEqual(getTypeID((NotNullUnsigned*)0)))
                    create += "INTEGER UNSIGNED NOT NULL ";
                else if (field->value->isEqual(getTypeID((NotNullLongInt*)0)))
                    create += "INTEGER UNSIGNED NOT NULL ";
                else if (field->value->isEqual(getTypeID((NotNullUnsignedLongInt*)0)))
                    create += "INTEGER UNSIGNED NOT NULL ";
                else if (field->value->isEqual(getTypeID((NotNullDouble*)0)))
                    create += "REAL NOT NULL ";
//                 else if (field->value->isEqual(getTypeID((ReferenceBase*)0)))
//                     create += "INTEGER UNSIGNED NOT NULL DEFAULT 0 ";

                if (field->isIndex)
                    indexes += String::Print("CREATE %sINDEX %s ON %s (%s);", field->isUnique ? "UNIQUE " : "", (const char*)("I_" + escapeString(field->columnName)), (const char*)tableName, (const char*)escapeString(field->columnName));

                create += ",\n";
            }

            create.rightTrim("\n,");

            create += ") ";
            create += ";";

            res = sendQueryInternal(DBConnection, create, &error);
            cleanResults(res);
            if (error.getLength()) return false;


            // Create indexes if any required.
            if (indexes)
            {
                res = sendQueryInternal(DBConnection, indexes, &error);
                cleanResults(res);
                if (error.getLength()) return false;
            }
        }
        return true;
    }
    bool SQLFormat::createModelsForAllConnections(const bool forceReinstall)
    {
        // Create new connection here, as you aren't supposed to do this while the database is running
        DatabaseConnection * conn = SQLFormat::builder->buildDatabaseConnection();
        bool ret = false;
        if (conn)
        {
            // Allow creating files here
            creatingDatabase = true;
            ret = conn->createModels(forceReinstall);
            creatingDatabase = false;
        }
        delete conn;
        return ret;
    }
    bool SQLFormat::deleteDataFromModel(const uint32 dbIndex, Database::DatabaseDeclaration * model, const String & databaseName)
    {
        // Check argument
        if (model == NULL) return false;

        // Create the databases now
        String error;
        const Results * res = NULL;
        // Delete the tables now
        uint32 tableCount = model->getTableCount();

        for (uint32 i = 0; i < tableCount; i++)
        {
            String create = "DELETE FROM ";
            const AbstractTableDescription * td = model->findTable(i);
            if (td == NULL) return false;
            String tableName = escapeString(td->tableName);

            // First drop the table if it already exists
            create += tableName;
            create += " ;";
            res = sendQuery(dbIndex, create);
            if (res == NULL) error = getLastError(dbIndex);
            cleanResults(res);
        }

        return true;
    }

    bool SQLFormat::deleteTablesFromModel(const uint32 dbIndex, Database::DatabaseDeclaration * pModel)
    {
        bool ret = false;
        // set the file's size to 0 in order to not delete the file and keep the file's rights
        GetLocalVariable(DatabaseConnection, connection, getTLSKey());
        if (connection)
        {
            // If the low level object is not valid, let's create one.
            String dbName, fullPath;
            connection->getDatabaseConnectionParameter(dbIndex, dbName, fullPath);

            if (getTLSVariable()) getTLSVariable()->destruct();

            // Access to file will be freed at the end of the scope
            File::Info info(fullPath);
            if (!info.doesExist()) return true;

            File::BaseStream * stream(info.getStream());
            ret = stream->setSize(0);
            delete stream;
        }
        return ret;
    }

    bool SQLFormat::checkDatabaseExists(const uint32 dbIndex)
    {
        if (!SQLFormat::builder)
            SQLFormat::builder = &getSimpleBuilder();

        // On MySQL it would have been SELECT SCHEMA_NAME FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME = "dbName"

        // The first time we call this, it'll return a null connection, so we can act accordingly
        Utils::ScopePtr<DatabaseConnection> connection = SQLFormat::builder->buildDatabaseConnection();
        String dbName, dbURL;
        connection->getDatabaseConnectionParameter(dbIndex, dbName, dbURL);
        String fullPath = constructFilePath(dbName, dbURL);
        if (!File::Info(fullPath).doesExist()) return false;

        // Then check if all the tables in the models exists in the given file
        DatabaseDeclaration * model = getDatabaseRegistry().getDeclaration(dbName);
        if (!model) return false; // Likely the model is empty or non existant, it does not match this file

        void * DBConnection = getSQLiteConnection(dbIndex);
        if (DBConnection == NULL) return false; // Not a SQLite3 connection

        // Create the tables now
        uint32 tableCount = model->getTableCount();
        if(tableCount)
        {
            cleanResults(sendQueryInternal(DBConnection, "PRAGMA encoding = \"UTF-8\";"));
            // Then select all the tables in the database
            const Results * res = sendQueryInternal(DBConnection, "SELECT tbl_name FROM sqlite_master WHERE type = 'table';");
            if (!res) return false; // Not table found or error on the database

            Strings::StringArray tables;
            Type::Var ret;
            unsigned int i = 0;
            while (getResults(res, ret, i, "tbl_name", 0))
            {
                tables.Append(ret.like(&dbName));
                i++;
            }
            cleanResults(res);

            // It does not match the model
            if (i < tableCount) return false;

            for (unsigned int i = 0; i < tableCount; i++)
            {
                const AbstractTableDescription * td = model->findTable(i);
                if (!td) return false;

                if (tables.indexOf(td->tableName) == tables.getSize())
                    return false;
            }
        }
        return true;
    }

    bool SQLFormat::optimizeTables(const uint32 dbIndex)
    {
        const Results * res = sendQuery(dbIndex, "VACUUM;");
        if (res == 0) return false;
        cleanResults(res);
        return true;
    }

    void * SQLFormat::createDatabaseConnection(const String & dataBaseName, const String & URL)
    {
        sqlite3 * newConnection = NULL;
        if(!dataBaseName.getLength() && !URL.getLength()) return 0;

        // the good path for the file of the database
        String fullPath = constructFilePath(dataBaseName, URL);
        if(sqlite3_open((const char *)fullPath, &newConnection) != SQLITE_OK)
        {
            DatabaseConnection::notifyError("Error in createDatabaseConnection: " + fullPath);
            return 0;
        }
        sqlite3_busy_timeout(newConnection, 60000);
        return newConnection;
    }

    void SQLFormat::destructCreatedDatabaseConnection(void * DBConnection)
    {
        if (DBConnection != 0)
            sqlite3_close((sqlite3 *)(DBConnection));
    }


    bool SQLFormat::resetDatabaseConnection(const uint32 dbIndex, void * newConnection)
    {
        GetLocalVariable(DatabaseConnection, connection, getTLSKey());
        if (connection)
        {
            connection->setLowLevelConnection(dbIndex, newConnection);
            return true;
        }
        return false;
    }

    void SQLFormat::setErrorCallback(DatabaseConnection::ClassErrorCallback & callback)
    {
        DatabaseConnection::setErrorCallback(callback);
    }


    void SQLFormat::startTransaction(const uint32 dbIndex)
    {
        cleanResults(sendQuery(dbIndex, "BEGIN IMMEDIATE;"));
    }
    void SQLFormat::commitTransaction(const uint32 dbIndex)
    {
        cleanResults(sendQuery(dbIndex, "COMMIT;"));
    }
    void SQLFormat::rollbackTransaction(const uint32 dbIndex)
    {
        cleanResults(sendQuery(dbIndex, "ROLLBACK;"));
    }
}

#endif
#endif
