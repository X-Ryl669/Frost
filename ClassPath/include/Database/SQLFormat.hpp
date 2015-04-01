#ifndef hpp_CPP_SQLFormat_CPP_hpp
#define hpp_CPP_SQLFormat_CPP_hpp

// We need string declaration
#include "../Strings/Strings.hpp"
// We need variant too
#include "../Variant/Variant.hpp"

namespace Database
{
    typedef Strings::FastString String;
    struct DatabaseDeclaration;
    struct Blob;
    /** A database connection.
        The type used inside this connection is opaque and depends on the database itself */
    struct DatabaseConnection
    {
        // Type definition and enumeration
    public:
        /** The callback structure */
        struct ClassErrorCallback
        {
        public:
            /** Error source */
            enum ErrorType
            {
                BadQuery        = 1,    //!< Ill formed query
                ConnectionLost  = 2,    //!< Connection lost
            };
            /** Call back to implement */
            virtual void databaseErrorCallback(DatabaseConnection * connection, const uint32 index, const ErrorType & error, const String & message) = 0;
        };

        /** Callback to use when error occurs*/
        static ClassErrorCallback * errorCallback;

        // Interface
    public:
        /** Get the low level object used for the index-th connection */
        virtual void * getLowLevelConnection(const uint32 index = 0) = 0;
        /** Set the low level object used for the index-th connection */
        virtual bool setLowLevelConnection(const uint32 index, void * connection) = 0;
        /** Get the connection parameters
            @param index    The index of the connection to retrieve
            @param dbName   On output, set to the database name to connect to
            @param dbURL    On output, set to the database URL to connect to (can contain username:password part)
            @return true on successful connection */
        virtual bool getDatabaseConnectionParameter(const uint32 index, String & dbName, String & dbURL) const = 0;
        /** Create models on the database connections
            @param forceReinstall   Force reinstalling the database model
            @return true on successful model creation */
        virtual bool createModels(const bool forceReinstall = false) = 0;

        /** Specify a callback to call on error.
            @param callback Callback to use on error  */
        static void setErrorCallback(ClassErrorCallback & callback) { errorCallback = &callback; }
        /** Notify an error on this connection */
        inline void notifyError(const uint32 index, const ClassErrorCallback::ErrorType & error, const String & message)
        {
            if(errorCallback) errorCallback->databaseErrorCallback(this, index, error, message);
        }
        /** Notify an error on any connection */
        static void notifyError(const String & message)
        {
            if(errorCallback) errorCallback->databaseErrorCallback(0, (uint32)-1, ClassErrorCallback::ConnectionLost, message);
        }
        /** Constructor */
        DatabaseConnection() {}

        /** Get the error callback pointer */
        virtual ~DatabaseConnection() {}
    };

    /** A DatabaseConnection builder from TLS */
    struct BuildDatabaseConnection
    {
        /** Build a database connection */
        virtual DatabaseConnection * buildDatabaseConnection() const = 0;
    };

    /** This structure is provided so that each SQL based implementation
        implements its own method */
    struct SQLFormat
    {
        /** The escape quote used by the driver */
        static const char escapeQuote;
        /** The Database connection builder     */
        static const BuildDatabaseConnection * builder;

        /** This structure stores the results of a SQL query.
            Any library back end must deal with that structure */
        struct Results
        {
            /** Don't allow modifications here */
            const void *    privateData;
            /** index of the result row to get. Don't allow modifications here */
            const int       privateIndex;
            /** Don't allow modifications here */
            const void *    privateData2;

            Results() : privateData(0), privateIndex(-1), privateData2(0) {}
        };

        /** Here should escape the given String for the Database
            This is SQL back end dependent. */
        static String escapeString(const String & sStr, const char = escapeQuote, const uint32 dbConnIndex = 0);

        /** Initialize the SQL library and connect to the server.
            Usually, dataBase and URL are used to build a connection URL to connect to the database.
            With some file based drivers, like SQLite, the algorithm to figure out which file is used is like this:
            @verbatim
               If (URL) is a file and it exists, then use it.
               If (URL) is a folder, then try to use a file called dataBase.db in this folder
               If (URL) is empty, try to use a file called dataBase
            @endverbatim
            With classical server based approach, then URL is the URL to connect to, and dataBase is the database to
            select (issuing "USE dataBase;" SQL command if asked to)
            @param dataBase         The database to use
            @param URL              The URL to try to connect to (please read the comment for the algorithm used in case of SQLite)
            @param User             The user to connect with
            @param Password         The password to emit when asked for credentials
            @param Port             The connection port (the URL is passed directly to the database driver, and
                                    some of them do not support port in the URL)
            @param selectDatabase   Select the database upon connection (if you don't do that, you have to do it manually before first use)
            @param dbIndex          The database index if using multiple databases
            @return true if the connection succeed, false on bad parameter or no connection */
        static bool initialize(const String & dataBase, const String & URL, const String & User, const String & Password, const unsigned short Port, const bool selectDatabase = true, const uint32 dbIndex = 0);
        /** Finalize access to this library.
            You must call this on the very end of your program, ideally when the program is exiting.
            @param dbIndex The index to the database connection to close, or -1 to close all in once
            @warning When called with dbIndex == -1, the thread's local variable is removed, so all the threads will loose their connections too.
            @warning When NOT called with dbIndex == -1, only the connection to the given database is deleted, and not the main database connection object
            @sa destructCreatedDatabaseConnection */
        static void finalize(const uint32 dbIndex);
        /** Create the given user. Must be called by a connection root. If the user already exist, return true*/
        static bool createDBUser(const String & databaseName, const String & User, const String & Password);
        /** Delete the given user. Must be called by a connection root. */
        static bool deleteDBUser(const String & User);
        /** Send a query to the SQL server
            @return a pointer to the result object (NULL on error) */
        static const Results * sendQuery(const uint32 dbIndex, const String & sStr, const void * DBConnection = NULL);
        /** Returns the last inserted ID */
        static unsigned int getLastInsertedID(const uint32 dbIndex, const void * DBConnection = NULL);
        /** Get the results of a previous query.
            rowIndex must never decrease between calls.
            If trying to test whether a row exists or not, you can pass an empty fieldName, and a default fieldIndex */
        static bool getResults(const Results *, Type::Var & res, const unsigned int rowIndex, const String & fieldName, const unsigned int fieldIndex = -1);
        /** Count the rows.
            Depending on the database driver chose, this can either return a cached data, or actually count them.
            After calling this, the results row pointer is on the bottom of the results set, so you must clean the results before you can actually get results */
        static int getResultsCount(const Results *);
        /** Clean the result object */
        static void cleanResults(const Results *);

        /** Check if the database already exists. This does not check if it fits the model. */
        static bool checkDatabaseExists(const uint32 dbIndex);
        /** Create the similar database as the model.
            @param dbIndex              The database index in the multiple database connection array (usually 0 if you only have a single database)
            @param model                A pointer to the table model used for the database
            @param databaseName         If the driver requires to specify the database name before creation/testing then this is required
            @param forceReinstall       By default, this function checks if the database exists, and if the number of tables is the same as the model (so it's fast).
                                        If you need to recreate the table (for example if the model was updated) set this to true.
            @return true on success, false if the creation of the table failed, or if the connection to the database was not working. */
        static bool createDatabaseLikeModel(const uint32 dbIndex, Database::DatabaseDeclaration * model, const String & databaseName = "", const bool forceReinstall = false);
        /** If you've declared your model with Macro::DeclaringTables::EndTableDeclarationRegister, then
            you can call this method to automatically create all the database schema at once. */
        static bool createModelsForAllConnections(const bool forceReinstall = false);
        /** Clean the current data from the given model */
        static bool deleteDataFromModel(const uint32 dbIndex, Database::DatabaseDeclaration * model, const String & databaseName = "");
        /** Erase the given model */
        static bool deleteTablesFromModel(const uint32 dbIndex, Database::DatabaseDeclaration * model);
        /** Optimize the table so they use minimal disk space. Not all database support this however */
        static bool optimizeTables(const uint32 dbIndex);
        /** Return the last error, if any */
        static String getLastError(const uint32 dbIndex);

        /** Create a new database connection */
        static void * createDatabaseConnection(const String & dataBaseName, const String & URL);
        /** Destruct a created database connection.
            Do not use this to delete automatically generated connections. */
        static void destructCreatedDatabaseConnection(void *);
        /** Reset the current database connection by the one given */
        static bool resetDatabaseConnection(const uint32 dbIndex, void * newConnection);
        /** Set the error callback to be called upon database connection errors */
        static void setErrorCallback(DatabaseConnection::ClassErrorCallback & callback);

        /** Start a transaction */
        static void startTransaction(const uint32 dbIndex);
        /** Commit current transaction */
        static void commitTransaction(const uint32 dbIndex);
        /** Rollback current transaction */
        static void rollbackTransaction(const uint32 dbIndex);


        /** Serialize a blob */
        static void serializeBlob(const Database::Blob * blob, String & output);
        /** Unserialize a blob */
        static void unserializeBlob(Database::Blob * blob, const String & input);

        /** Change the default connection builder */
        static void useDatabaseConnectionBuilder(BuildDatabaseConnection * builder);
    };

}



#endif
