/*
 * This source file is part of the bstring string library.  This code was
 * written by Paul Hsieh in 2002 - 2006, and is covered by the BSD open source 
 * license. Refer to the accompanying documentation for details on usage and 
 * license.
 */

/*
 * bstrwrap.h
 *
 * This file is the C++ wrapper for the bstring functions.
 */

#ifndef BSTRWRAP_INCLUDE
#define BSTRWRAP_INCLUDE


/////////////////// Configuration defines //////////////////////////////

// By default it is assumed that your compiler can deal with and has enabled
// exception handlling.  If this is not the case then you will need to 
// #define BSTRLIB_DOESNT_THROW_EXCEPTIONS
#if !defined(BSTRLIB_THROWS_EXCEPTIONS) && !defined(BSTRLIB_DOESNT_THROW_EXCEPTIONS)
#define BSTRLIB_THROWS_EXCEPTIONS
#endif

////////////////////////////////////////////////////////////////////////
/** @cond */
#include "BString/bstrlib.h"
#include "../Utils/StaticAssert.hpp"
#include "../Platform/Platform.hpp"
/** @endcond */

#ifdef __cplusplus


// This is required for disabling exception handling
#if (DEBUG==1)
#define bstringThrow(er) do{ fprintf(stderr, "%s", er); Platform::breakUnderDebugger(); } while(0)
#else
#define bstringThrow(er) do{ fprintf(stderr, "[FATAL ERROR] %s:%d => %s", __FILE__, __LINE__, er); } while(0)
#endif

// Assignment operator not obvious, conditional expression constant
#ifdef _MSC_VER
#pragma warning(disable:4512 4127)
#endif

#if WantFloatParsing != 1
extern "C" int ftoa(float v, char buf[15], int);
extern "C" int lftoa(double v, char buf[20], int);
extern "C" char* utoa(unsigned long value, char* result, int base);
#endif


/** The better string lib namespace. */
namespace Bstrlib 
{
	// Forward declaration 
	struct  String;
	
	/** Provide write protected char functionality */
	class  CharWriteProtected 
	{
		// Members
	private:
		/** Required to access the String class */
		friend struct String;
		/** The reference to the working string */
		const struct tagbstring& s;
		/** The index in itself */
		const unsigned int idx;

		/** Constructor is private and only used in the String class */
		CharWriteProtected(const struct tagbstring& c, int i) : s(c), idx((unsigned int)i) 
			{	if (idx >=(unsigned) s.slen) bstringThrow("character index out of bounds");	}
		
		// Operators
	public:
		/** Assignment operator */		
		inline char operator=(char c) 
		{
			if (s.mlen <= 0)
				bstringThrow("Write protection error");
			else 
			{
#ifndef BSTRLIB_THROWS_EXCEPTIONS
				if (idx >=(unsigned) s.slen)
					return '\0';
#endif
				s.data[idx] = (unsigned char) c;
			}
			return (char) s.data[idx];
		}
		
		/** Assignment operator */		
		inline unsigned char operator=(unsigned char c) 
		{
			if (s.mlen <= 0)
				bstringThrow("Write protection error");
			else 
			{
#ifndef BSTRLIB_THROWS_EXCEPTIONS
				if (idx >=(unsigned) s.slen)
					return '\0';
#endif
				s.data[idx] = c;
			}
			return s.data[idx];
		}
		/** Access operator */
		inline operator unsigned char() const 
		{
#ifndef BSTRLIB_THROWS_EXCEPTIONS
			if (idx >=(unsigned) s.slen)
				return (unsigned char) '\0';
#endif
			return s.data[idx];
		}
	};
	
	/** The string class.

        The functions in this string class are very straightforward to use. */
	struct String : public tagbstring 
	{
	// Construction and destruction
	//public:
		/** Default constructor - Allocate 8 bytes of memory */
		String();
		/** Char and unsigned char constructor */
		String(char c);
		String(unsigned char c);
		String(char c, int len);
		/** C-Style string constructor */
		String(const char *s);
		/** C-Style string with len limitation
            @warning len is the minimum size, s might be larger */
		String(int len, const char *s);

		/** Copy constructors */
		String(const String& b);
		String(const tagbstring& x);
		/** Bulk construction from a given block */
		String(const void * blk, int len);
	
		/** Destructor */
		~String();

	// Operators
	//public:		

		/** = operator */
		const String& operator=(char c);
		const String& operator=(unsigned char c);
		const String& operator=(const char *s);
		const String& operator=(const String& b);
		const String& operator=(const tagbstring& x);
		
		/** += operator */
		const String& operator +=(char c);
		const String& operator +=(unsigned char c);
		const String& operator +=(const char *s);
		const String& operator +=(const String& b);
		const String& operator +=(const tagbstring& x);
        const String& operator +=(const int val);
        const String& operator +=(const unsigned int val);
        const String& operator +=(const int64 val);
        const String& operator +=(const uint64 val);
        const String& operator +=(const float val);
        const String& operator +=(const double val);
		
		/** *= operator, this one simply repeats the string count times */
		inline const String& operator *=(int count) 
		{
			this->Repeat(count);
			return *this;
		}
		
		/** + operator */
		const String operator+(char c) const;
		const String operator+(unsigned char c) const;
		const String operator+(const unsigned char *s) const;
		const String operator+(const char *s) const;
		const String operator+(const String& b) const;
		const String operator+(const tagbstring& x) const;
		const String operator+(const int c) const;
		const String operator+(const unsigned int c) const;
		const String operator+(const int64 c) const;
		const String operator+(const uint64 c) const;
		const String operator+(const float c) const;
		const String operator+(const double c) const;
		
		
		/** * operator, repeat and copy */
		inline const String operator * (int count) const 
		{
			String retval(*this);
			retval.Repeat(count);
			return retval;
		}
		
		/** Comparison operators */
		bool operator ==(const String& b) const;
		bool operator ==(const char * s) const;
		bool operator ==(const unsigned char * s) const;
		bool operator ==(char s[]) const { return this->operator == ((const char*)s); }
		bool operator !=(const String& b) const;
		bool operator !=(const char * s) const;
		bool operator !=(const unsigned char * s) const;
		bool operator !=(char s[]) const { return this->operator != ((const char*)s); }
		bool operator <(const String& b) const;
		bool operator <(const char * s) const;
		bool operator <(const unsigned char * s) const;
		bool operator <=(const String& b) const;
		bool operator <=(const char * s) const;
		bool operator <=(const unsigned char * s) const;
		bool operator >(const String& b) const;
		bool operator >(const char * s) const;
		bool operator >(const unsigned char * s) const;
		bool operator >=(const String& b) const;
		bool operator >=(const char * s) const;
		bool operator >=(const unsigned char * s) const;

		inline bool operator !() const { return slen == 0; }
		inline operator bool() const { return slen > 0; }
		
	// Casting 
	//public:
		/** To char * and unsigned char * */
		inline operator const char* () const            { return (const char *)data; }
		inline operator const unsigned char* () const   { return (const unsigned char *)data; }
        inline const char * getData() const             { return (const char *)data; }

#if WantFloatParsing == 1
		/** To double, as long as the string contains a double value (0 on error) 
			@warning if you have disabled float parsing, you must provide a double atof (const char*) function */
		operator double() const;
		/** To float, as long as the string contains a float value (0 on error) 
			@warning if you have disabled float parsing, you must provide a double atof (const char*) function */
		operator float() const;
		
        /** Serialize a float to this string with the given precision after the decimal point
			@warning if you have disabled float parsing, you must provide a int ftoa(float, char out[15], int precision) function */
		void storeFloat(float v, int precision) { Format(String::Print("%%.%df", precision), v); } 
        /** Serialize a double to this string with the given precision after the decimal point
			@warning if you have disabled float parsing, you must provide a int ftoa(float, char out[15], int precision) function */
		void storeDouble(double v, int precision) { Format(String::Print("%%.%dlg", precision), v); } 
#define HasFloatParsing 1
#else
		/** To double, as long as the string contains a double value (0 on error) 
			@warning if you have disabled float parsing, you must provide a double atof (const char*) function */
		operator double() const { return atof((const char*)data); }
		/** To float, as long as the string contains a float value (0 on error) 
			@warning if you have disabled float parsing, you must provide a double atof (const char*) function */
		operator float() const { return (float)atof((const char*)data); }

        /** Serialize a float to this string with the given precision after the decimal point 
			@warning if you have disabled float parsing, you must provide a int ftoa(float, char out[15], int precision) function */
		void storeFloat(float v, int precision) { char Buf[15]; ftoa(v, Buf, precision); *this = Buf; } 
        /** Serialize a double to this string with the given precision after the decimal point
			@warning if you have disabled float parsing, you must provide a int dtoa(double, char out[20], int precision) function */
		void storeDouble(double v, int precision) { char Buf[20]; lftoa(v, Buf, precision); *this = Buf; } 
#endif
		/** To int, as long as the string contains a int value (0 on error) */
		operator signed int() const;
		/** To unsigned int, as long as the string contains a int value (0 on error) */
		operator unsigned int() const;
        /** To int64, as long as the string contains a int64 value (0 on error) */
        operator int64() const;		
        
        /** Handy function to get the hexadecimal representation of a number 
            @return a lowercase hexadecimal version, with "0x" prefix for the given number */
        static String getHexOf(const uint64 c);

        /** Get the hexadecimal encoded integer in string.
            Both leading 0x and no leading 0x are supported, and both uppercase and lowercase are supported.
            @warning This method is a convenience wrapper around Scan function. It's not optimized for speed.
                     If you intend to parse a integer in unknown format (decimal, hexadecimal, octal, binary) use parseInt() instead
            @sa parseInt */
        inline uint32 fromHex() const { uint32 ret = 0; dropUpTo("0x").asUppercase().Scan("%08X", &ret); return ret; }

        /** Get the integer out of this string. 
            This method support any usual encoding of the integer, and detect the integer format automatically.
            This method is optimized for speed, and does no memory allocation on heap
            Supported formats examples: "0x1234, 0700, -1234, 0b00010101"
            @param base   If provided, only the given base is supported (default to 0 for auto-detection).
            @return The largest possible integer that's parseable. */
        int64 parseInt(const int base = 0) const;

	// Accessors
	//public:
		/** Return the length of the string */
		inline int getLength() const {return slen;}
		/** Bound checking character retrieval */
		inline unsigned char Character(int i) const 
		{
			if (((unsigned) i) >=(unsigned) slen)
			{
#ifdef BSTRLIB_THROWS_EXCEPTIONS
				bstringThrow("character idx out of bounds");
#else
				return '\0';
#endif
			}
			return data[i];
		}
		/** Bound checking character retrieval */
		inline unsigned char operator[](int i) const { return Character(i); }
		/** Character retrieval when write protected */
		inline CharWriteProtected Character(int i) { return CharWriteProtected(*this, i); }
		inline CharWriteProtected operator[](int i) { return Character(i); }
		
		/** Space allocation.
                    This allocate the given length, and return the allocated buffer.
                    You must call releaseLock() once you know the final string's size in byte.
                    @param length   The buffer size in bytes
                    @return A pointer on a new allocated buffer of length bytes, or 0 on error. Use releaseLock() once you're done with the buffer. */
		char * Alloc(int length);
		// Unlocking mechanism for char * like access
		/** Release a lock acquired with Alloc method */
		inline void releaseLock(const int & len ) { slen = len; mlen = slen + 1; }
		
		// Search methods.
		/** Check if the argument is equal in a case insensitive compare */
		bool caselessEqual(const String& b) const;
		/** Case insensitive compare */
		int caselessCmp(const String& b) const;
		/** Find a given substring starting at position pos */
		int Find(const String& b, int pos = 0) const;
		/** Find a given substring starting at position pos */
		int Find(const char * b, int pos = 0) const;
		/** Find a given substring starting at position pos in a case insensitive search */
		int caselessFind(const String& b, int pos = 0) const;
		/** Find a given substring starting at position pos in a case insensitive search */
		int caselessFind(const char * b, int pos = 0) const;
		/** Find a given char starting at position pos */
		int Find(char c, int pos = 0) const;
		/** Reverse find a given substring starting at position pos */
		int reverseFind(const String& b, int pos) const;
		/** Reverse find a given substring starting at position pos */
		int reverseFind(const char * b, int pos) const;
		/** Reverse find a given substring starting at position pos in a case insensitive search */
		int caselessReverseFind(const String& b, int pos) const;
		/** Reverse find a given substring starting at position pos in a case insensitive search */
		int caselessReverseFind(const char * b, int pos) const;
		/** Reverse find a given char starting at position pos */
		int reverseFind(char c, int pos = -1) const;
		/** Find the first occurrence matching any char of the given substring starting at position pos */
		int findAnyChar(const String& b, int pos = 0) const;
		/** Find the first occurrence matching any char of the given substring starting at position pos */
		int findAnyChar(const char * s, int pos = 0) const;
		/** Reverse find the first occurrence matching any char of the given substring starting at position pos */
		int reverseFindAnyChar(const String& b, int pos) const;
		/** Reverse find the first occurrence matching any char of the given substring starting at position pos */
		int reverseFindAnyChar(const char * s, int pos) const;
		/** Find the first occurrence not matching any char of the given substring starting at position pos */
		int invFindAnyChar(const String& b, int pos = 0) const;
		/** Find the first occurrence not matching any char of the given substring starting at position pos */
		int invFindAnyChar(const char * b, int pos = 0) const;
		/** Reverse find the first occurrence not matching any char of the given substring starting at position pos */
		int invReverseFindAnyChar(const String& b, int pos) const;
		/** Reverse find the first occurrence not matching any char of the given substring starting at position pos */
		int invReverseFindAnyChar(const char * b, int pos) const;
		/** Count the number of times the given substring appears in the string */
		int Count(const String & needle) const;
        /** Extract tokens from by splitting the string with the given char, starting at position pos (first call should set pos == 0)
            While called in loop, it extracts the token one by one, until either the returned value is empty, or pos > getLength() */
        String extractToken(char c, int & pos) const;
		
		// Search and substitute methods.
		/** Find the given substring starting at position pos and replace it */
		String & findAndReplace(const String& find, const String& repl, int pos = 0);
		/** Find the given substring starting at position pos and replace it */
		String & findAndReplace(const String& find, const char * repl, int pos = 0);
		/** Find the given substring starting at position pos and replace it */
		String & findAndReplace(const char * find, const String& repl, int pos = 0);
		/** Find the given substring starting at position pos and replace it */
		String & findAndReplace(const char * find, const char * repl, int pos = 0);
		/** Find the given substring starting at position pos and replace it with caseless search */
		String & findAndReplaceCaseless(const String& find, const String& repl, int pos = 0);
		/** Find the given substring starting at position pos and replace it with caseless search */
		String & findAndReplaceCaseless(const String& find, const char * repl, int pos = 0);
		/** Find the given substring starting at position pos and replace it with caseless search */
		String & findAndReplaceCaseless(const char * find, const String& repl, int pos = 0);
		/** Find the given substring starting at position pos and replace it with caseless search */
		String & findAndReplaceCaseless(const char * find, const char * repl, int pos = 0);

#if (WantRegularExpressions == 1)
        // Regular expressions
        /** Match this string against a regular expression and extract any captured element.
            The common regular expression syntax's term are supported but there is no extension.
            The supported syntax is:
            @verbatim
               .        Match any character
               ^        Match beginning of a buffer
               $        Match end of a buffer
               ()       Grouping and substring capturing
               [...]    Match any character from set
               [^...]   Match any character but ones from set
               \s       Match whitespace
               \S       Match non-whitespace
               \d       Match decimal digit
               \r       Match carriage return
               \n       Match newline
               +        Match one or more times (greedy)
               +?       Match one or more times (non-greedy)
               *        Match zero or more times (greedy)
               *?       Match zero or more times (non-greedy)
               ?        Match zero or once
               \xDD     Match byte with hex value 0xDD
               \meta    Match one of the meta character: ^$().[*+\?
            @endverbatim

            @param regEx         The regular expression to match against. @sa RE macro to avoid double escape
                                 of the backlash in some regular expression.
            @param captures      If provided will be filled with the captures.
                                 The size of the array must match the regular expression capture's amount.
                                 No allocation is done in this method.
            @param caseSensitive If set, the search is case sensitive

            @return An empty string on success, or the error string on failure */
        String regExMatch(const String & regEx, String * captures, const bool caseSensitive = true) const;

        /** The opaque structure used to store the regular expression engine */
        struct RegExOpaque;
        /** Compile a regular expression to match later on.
            You don't need this method to use regExMatch. You need this method if you intend to match a lot of Strings
            with the same regular expression. In that case, you can save some time by first compiling the regular expression
            and then match using this compiled object.

            @param regEx        The regular expression to match. @sa RE macro to avoid double escape of backlashes.
            @param opaque       On output, will be filled with an opaque structure you must pass to the match function. You must delete the returned object
            @return The number of expected captures for this expression, or -1 on error (use regExMatch to figure out the error string)
            @sa regExMatch */
        int regExCompile(const String & regEx, RegExOpaque *& opaque) const;
        /** Match the expression against the compiled regular expression
            @param opaque       The opaque object that's created with regExCompile
            @param captures     If provided will be filled with the captures.
                                The size of the array must match the regular expression capture's amount.
                                No allocation is done in this method.
            @param caseSensitive If set, the search is case sensitive

            @return An empty string on success, or the error string on failure */
        String regExMatchEx(RegExOpaque & opaque, String * captures, const bool caseSensitive = true) const;
#endif

		// Extraction method.
		/** Extract the string starting at left of len char long.
		    You can use negative left value to start from right, or on the length too.
		    For example, the following code returns:
		    @code
                String ret = String("abcdefgh").midString(0, 3); // ret = "abc"
                String ret = String("abcdefgh").midString(0, 30); // ret = "abcdefgh"
                String ret = String("abcdefgh").midString(10, 1); // ret = ""
                String ret = String("abcdefgh").midString(-1, 0); // ret = ""
                String ret = String("abcdefgh").midString(-3, 3); // ret = "fgh"
                String ret = String("abcdefgh").midString(-3, 2); // ret = "fg"
                String ret = String("abcdefgh").midString([whatever], -3); // ret = "fgh"
                String ret = String("abcdefgh").midString([whatever], -30); // ret = "abcdefgh"
		    @endcode
		    @param left     The index to start with (0 based)
		    @param len      The number of bytes to return */
		const String midString(int left, int len) const;
		
		/** Get the string up to the first occurrence of the given string 
		    If not found, it returns the whole string.
		    For example, this code returns:
		    @code
		        String ret = String("abcdefdef").upToFirst("d"); // ret == "abc"
		        String ret = String("abcdefdef").upToFirst("g"); // ret == "abcdefdef"
		    @endcode 
		    @param find         The text to look for
		    @param includeFind  If set, the needle is included in the result */
		const String upToFirst(const String & find, const bool includeFind = false) const;
		/** Get the string up to the last occurrence of the given string 
		    If not found, it returns the whole string.
		    For example, this code returns:
		    @code
		        String ret = String("abcdefdef").upToLast("d"); // ret == "abcdef"
		        String ret = String("abcdefdef").upToLast("g"); // ret == "abcdefdef"
		    @endcode 
		    @param find         The text to look for
		    @param includeFind  If set, the needle is included in the result */
		const String upToLast(const String & find, const bool includeFind = false) const;
		/** Get the string from the last occurrence of the given string.
		    If not found, it returns an empty string if includeFind is false, or the whole string if true
		    For example, this code returns:
		    @code
		        String ret = String("abcdefdef").fromLast("d"); // ret == "ef"
		        String ret = String("abcdefdef").fromLast("d", true); // ret == "def"
		        String ret = String("abcdefdef").fromLast("g"); // ret == ""
		    @endcode 
		    @param find         The text to look for
		    @param includeFind  If set, the needle is included in the result */
		const String fromLast(const String & find, const bool includeFind = false) const;
		/** Get the string from the first occurrence of the given string 
		    If not found, it returns an empty string if includeFind is false, or the whole string if true
		    For example, this code returns:
		    @code
		        String ret = String("abcdefdef").fromFirst("d"); // ret == "efdef"
		        String ret = String("abcdefdef").fromFirst("d", true); // ret == "defdef"
		        String ret = String("abcdefdef").fromFirst("g"); // ret == ""
		    @endcode 
		    @param find         The text to look for
		    @param includeFind  If set, the needle is included in the result */
		const String fromFirst(const String & find, const bool includeFind = false) const;
		/** Split a string when the needle is found first, returning the part before the needle, and 
		    updating the string to start on or after the needle.
		    If the needle is not found, it returns an empty string if includeFind is false, or the whole string if true.
		    For example this code returns:
		    @code
		        String text = "abcdefdef";
		        String ret = text.splitFrom("d"); // ret = "abc", text = "efdef"
		        ret = text.splitFrom("f", true);  // ret = "e", text = "fdef"
		    @endcode 
		    @param find         The string to look for
		    @param includeFind  If true the string is updated to start on the find text. */
		const String splitFrom(const String & find, const bool includeFind = false);
		/** Get the substring from the given needle up to the given needle.
		    For example, this code returns:
		    @code
		        String text = "abcdefdef";
		        String ret = text.fromTo("d", "f"); // ret = "e"
		        ret = text.fromTo("d", "f", true);  // ret = "def"
		        ret = text.fromTo("d", "g"); // ret = ""
		        ret = text.fromTo("d", "g", true); // ret = "defdef"
		        ret = text.fromTo("g", "f", [true or false]); // ret = ""
		    @endcode 
		    
		    @param from The first needle to look for
		    @param to   The second needle to look for 
		    @return If "from" needle is not found, it returns an empty string, else if "to" needle is not found, 
		            it returns an empty string upon includeFind being false, or the string starting from "from" if true. */
		const String fromTo(const String & from, const String & to, const bool includeFind = false) const;
		/** Get the substring from the given needle if found, or the whole string if not. 
		    For example, this code returns:
		    @code
		        String text = "abcdefdef";
		        String ret = text.dropUpTo("d"); // ret = "efdef"
		        ret = text.dropUpTo("d", true); // ret = "defdef"
		        ret = text.dropUpTo("g", [true or false]); // ret = "abcdefdef"
		    @endcode
		    @param find         The string to look for
		    @param includeFind  If true the string is updated to start on the find text. */
        const String dropUpTo(const String & find, const bool includeFind = false) const;		
		/** Get the substring up to the given needle if found, or the whole string if not, and split from here. 
		    For example, this code returns:
		    @code
		        String text = "abcdefdef";
		        String ret = text.splitUpTo("d"); // ret = "abc", text = "efdef"
		        ret = text.splitUpTo("g", [true or false]); // ret = "efdef", text = ""
                text = "abcdefdef";
		        ret = text.splitUpTo("d", true); // ret = "abcd", text = "efdef"
		    @endcode
		    @param find         The string to look for
		    @param includeFind  If true the string is updated to start on the find text. */
        const String splitUpTo(const String & find, const bool includeFind = false);
        
        /** Split the string at the given position.
            The string is modified to start at the split position.
            @return the string data up to split point. */
        const String splitAt(const int pos);
        		
		// Standard manipulation methods.
		/** Set the substring at pos to the string b. If pos > len, (pos-len) 'fill' char are inserted */
		void setSubstring(int pos, const String& b, unsigned char fill = ' ');
		/** Set the substring at pos to the string b. If pos > len, (pos-len) 'fill' char are inserted */
		void setSubstring(int pos, const char * b, unsigned char fill = ' ');
		/** Insert at pos the string b. If pos > len, (pos-len) 'fill' char are inserted */
		void Insert(int pos, const String& b, unsigned char fill = ' ');
		/** Insert at pos the string b. If pos > len, (pos-len) 'fill' char are inserted */
		void Insert(int pos, const char * b, unsigned char fill = ' ');
		/** Insert at pos len times the 'fill' char */
		void insertChars(int pos, int len, unsigned char fill = ' ');
		/** Replace, at pos, len char to the string b. 
			If pos > string length or pos+len > string length, the missing 'fill' char are inserted  */
		void Replace(int pos, int len, const String& b, unsigned char fill = ' ');
		/** Replace, at pos, len char to the string b. 
			If pos > string length or pos+len > string length, the missing 'fill' char are inserted  */
		void Replace(int pos, int len, const char * s, unsigned char fill = ' ');
		/** Remove, at pos, len char from the string */
		void Remove(int pos, int len);
		/** Truncate the string at the given len */
		void Truncate(int len);
		
		// Miscellaneous methods.
		/** Scan the string and extract to the given data (fmt follows scanf's format spec) 
		    @warning Only one parameter can be extracted by this method.
		    @return the number of parameter extracted (1 or 0 on error) */
		int Scan(const char * fmt, void * data) const;
		/** Printf like format */
		String & Format(const char * fmt, ...)
#ifdef __GNUC__
        __attribute__ ((format (printf, 2, 3)))
#endif
        ;
		/** Printf like format */
		static String Print(const char * fmt, ...)
#ifdef __GNUC__
        __attribute__ ((format (printf, 1, 2)))
#endif
        ;
		/** Printf like format for ascii */
		void Formata(const char * fmt, ...)
#ifdef __GNUC__
        __attribute__ ((format (printf, 2, 3)))
#endif
        ;
		/** Fill the string with 'fill' char length times */
		void Fill(int length, unsigned char fill = ' ');
		/** Fill the string with 'fill' char length times */
		inline String Filled(int length, unsigned char fill = ' ') const 
		{ 
		    String a = *this; 
		    a.Fill(length, fill); 
		    return a; 
		}
		/** Repeat the same string count times */
		void Repeat(int count);
		/** Trim the left side of this string with the given chars */
		void leftTrim(const String& b = String(" \t\v\f\r\n", sizeof(" \t\v\f\r\n")));
		/** Trim the right side of this string with the given chars */
		void rightTrim(const String& b = String(" \t\v\f\r\n", sizeof(" \t\v\f\r\n")));
		/** Trim the both sides of this string with the given chars */
		inline void Trim(const String& b = String(" \t\v\f\r\n", sizeof(" \t\v\f\r\n"))) 
		{
			rightTrim(b);
			leftTrim(b);
		}
		/** Trim the both sides of this string with the given chars */
        inline String Trimmed(const String& b = String(" \t\v\f\r\n", sizeof(" \t\v\f\r\n"))) const
        {
            String a = *this;
            a.Trim(b);
            return a;
        }
		/** Change to uppercase */
		void toUppercase();
        /** Get a uppercased version */
        inline String asUppercase() const { String ret(*this); ret.toUppercase(); return ret; }
		/** Change to lowercase */
		void toLowercase();
        /** Get a lowercased version */
        inline String asLowercase() const { String ret(*this); ret.toLowercase(); return ret; }
        /** Normalized path, ready for concatenation */
        String normalizedPath(char sep = '/', const bool includeLastSep = true) const;
		/** Align the string so it fits in the given length.
		    @code
		        String text = "abc";
		        String alignedLeft = text.alignedTo(6, -1); // "abc   "
		        String alignedRight = text.alignedTo(6, 1); // "   abc"
		        String centered = text.alignedTo(6, 0); // " abc  "
		        String larger = text.alignedTo(2, -1 or 1 or 0); // "abc"
		    @endcode
		    @param length   The length of the final string. 
		                    If the current string is larger than this length, the current string is returned 
		    @param fill     The char to fill the empty space with
		    @param side     If 1, it's right aligned, 0 is centered (+- 1 char) and -1 is left aligned */
		String alignedTo(const int length, int side = -1, char fill = ' ') const;
		
        /** Replace a token by another token
            @return A reference on this string, modified */
        String & replaceAllTokens(char from, char to);

		// Write protection methods.
		/** Write protect this string */
		void writeProtect();
		/** Allow writing to this string */
		void writeAllow();
		/** Is the current string write protected ? */
		inline bool isWriteProtected() const { return mlen <= 0; }

		
		// Prevent unwanted conversion
	private:
	    /** Prevent unwanted conversion */
	    template <typename T> bool operator == (const T & t) const
	    {   // If compiler stops here, it's because you're trying to compare a string with a type it doesn't understand. 
	        // Try to cast this type to (const char*) or (String), but don't rely on default.
	        CompileTimeAssertFalse(T); return false;
	    }
	    /** Prevent unwanted conversion */
	    template <typename T> bool operator < (const T & t) const
	    {   // If compiler stops here, it's because you're trying to compare a string with a type it doesn't understand. 
	        // Try to cast this type to (const char*) or (String), but don't rely on default.
	        CompileTimeAssertFalse(T); return false;
	    }
	    /** Prevent unwanted conversion */
	    template <typename T> bool operator > (const T & t) const
	    {   // If compiler stops here, it's because you're trying to compare a string with a type it doesn't understand. 
	        // Try to cast this type to (const char*) or (String), but don't rely on default.
	        CompileTimeAssertFalse(T); return false;
	    }
	    /** Prevent unwanted conversion */
	    template <typename T> bool operator <= (const T & t) const
	    {   // If compiler stops here, it's because you're trying to compare a string with a type it doesn't understand. 
	        // Try to cast this type to (const char*) or (String), but don't rely on default.
	        CompileTimeAssertFalse(T); return false;
	    }
	    /** Prevent unwanted conversion */
	    template <typename T> bool operator >= (const T & t) const
	    {   // If compiler stops here, it's because you're trying to compare a string with a type it doesn't understand. 
	        // Try to cast this type to (const char*) or (String), but don't rely on default.
	        CompileTimeAssertFalse(T); return false;
	    }
	    /** Prevent unwanted conversion */
	    template <typename T> bool operator != (const T & t) const 
	    {   // If compiler stops here, it's because you're trying to compare a string with a type it doesn't understand. 
	        // Try to cast this type to (const char*) or (String), but don't rely on default.
	        CompileTimeAssertFalse(T); return false;
	    }
	};

	extern const String operator+(const char *a, const String& b);
	extern const String operator+(const unsigned char *a, const String& b);
	extern const String operator+(char c, const String& b);
	extern const String operator+(unsigned char c, const String& b);
	extern const String operator+(const tagbstring& x, const String& b);
	inline const String operator * (int count, const String& b) 
	{
		String retval(b);
		retval.Repeat(count);
		return retval;
	}

    #define _STRINGIFY_(a) #a
    /** This is used to create regular expression strings without double escaping the strings.
        Like this:
        @code
            RE(var, "(\w+\d)"); // Equivalent to const char * var = "(\\w+\\d)"; ie. that puts (\w+\d) in var
        @endcode
        @sa String::regExMatch method */
    #define RE(var, re)  static char var##_[] = _STRINGIFY_(re); const char * var = ( var##_[ sizeof(var##_) - 2] = '\0',  (var##_ + 1) )

} // namespace Bstrlib
#endif

#endif
