/*
 * This source file is part of the bstring string library.  This code was
 * written by Paul Hsieh in 2002 - 2006, and is covered by the BSD open source 
 * license. Refer to the accompanying documentation for details on usage and 
 * license.
 */

/*
 * bstrwrap.c
 *
 * This file is the C++ wrapper for the bstring functions.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <limits.h>
#include <ctype.h>
#include "../../../include/Strings/bstring.hpp"

#if defined(BSTRLIB_MEMORY_DEBUG)
#include "memdbg.h"
#endif

// Forward declare the uint64 serializer
namespace Strings { char* ulltoa(uint64 value, char* result, int base); }


namespace Bstrlib 
{
	// Constructors.
	String::String()
	{
        slen = 0; mlen = 8;
        data = (unsigned char*)malloc(mlen);
		if (!data)
		{
			mlen = 0;
			bstringThrow("Failure in default constructor");
		}
		else data[0] = '\0';
	}
	
	String::String(const void * blk, int len)
	{
        slen = mlen = 0; data = 0;
		if (len >= 0)
		{
			mlen = len + 1;
			slen = len;
			data = (unsigned char *) malloc(mlen);
		}
		if (!data)
		{
			mlen = slen = 0;
			bstringThrow("Failure in block constructor");
		}
		else 
		{
			if (slen > 0)	memcpy(data, blk, slen);
			data[slen] = '\0';
		}
	}
	
	String::String(char c, int len)
	{
        slen = mlen = 0; data = 0;
		if (len >= 0)
		{
			mlen = len + 1;
			slen = len;
			data = (unsigned char *) malloc(mlen);
		}
		if (!data)
		{
			mlen = slen = 0;
			bstringThrow("Failure in repeat(char) constructor");
		}
		else 
		{
			if (slen > 0)	memset(data, c, slen);
			data[slen] = '\0';
		}
	}
	
	String::String(char c)
	{
        slen = mlen = 0; data = 0;
        data = (unsigned char*)malloc(2);
        if (data)
        {
            data[0] = (unsigned char)c;
            data[1] = '\0';
            mlen = 2; slen = 1;
        } else
			bstringThrow("Failure in (char) constructor");
	}
	
	String::String(unsigned char c) 
	{
        slen = mlen = 0; data = 0;
        data = (unsigned char*)malloc(2);
        if (data)
        {
            data[0] = c;
            data[1] = '\0';
            mlen = 2; slen = 1;
        } else
			bstringThrow("Failure in (char) constructor");
	}
	
	String::String(const char *s)
	{
        slen = mlen = 0; data = 0;
		if (s)
		{
            slen = (int)strlen(s);
            mlen = slen + 1;
            data = (unsigned char*)malloc(mlen);

            if (!data)
            {
                mlen = slen = 0;
                bstringThrow("Failure in (char *) constructor");
            }
			else memcpy(data, s, mlen);
		}
	}
	
	String::String(int len, const char *s)
	{
        slen = mlen = 0; data = 0;
		if (s)
		{
			slen = (int)strlen(s);
			mlen = slen + 1;
			if (mlen < len)
				mlen = len;
			
            data = (unsigned char *) malloc(mlen);
            if (!data)
			{
                mlen = slen = 0;
                bstringThrow("Failure in (int len, char *) constructor");
			}
            memcpy(data, s, slen + 1);
		}
	}
	
	String::String(const String& b)
	{
		slen = b.slen;
		mlen = slen + 1;
		data = 0;
		if (mlen > 0)
			data = (unsigned char *) malloc(mlen);
		if (!data)
		{
			bstringThrow("Failure in (String) constructor");
		}
		else 
		{
			memcpy(data, b.data, slen);
			data[slen] = '\0';
		}
	}
	
	String::String(const tagbstring& x) 
	{
		slen = x.slen;
		mlen = slen + 1;
		data = 0;
		if (slen >= 0 && x.data != NULL)
			data = (unsigned char *) malloc(mlen);
		if (!data)
		{
			bstringThrow("Failure in (tagbstring) constructor");
		}
		else 
		{
			memcpy(data, x.data, slen);
			data[slen] = '\0';
		}
	}
	
	// Destructor.
	
	String::~String() 
	{
		if (data != NULL)
		{
			free(data);
			data = NULL;
		}
		mlen = 0;
		slen = -__LINE__;
	}
	
	// = operator.
	
	const String& String::operator=(char c) 
	{
		if (mlen <= 0)	bstringThrow("Write protection error");
		if (2 >= mlen)	Alloc(2);
		if (!data)
		{
			mlen = slen = 0;
			bstringThrow("Failure in =(char) operator");
		}
		else 
		{
			slen = 1;
			data[0] = (unsigned char) c;
			data[1] = '\0';
		}
		return *this;
	}
	
	const String& String::operator=(unsigned char c) 
	{
		if (mlen <= 0)	bstringThrow("Write protection error");
		if (2 >= mlen)	Alloc(2);
		if (!data)
		{
			mlen = slen = 0;
			bstringThrow("Failure in =(char) operator");
		}
		else 
		{
			slen = 1;
			data[0] = c;
			data[1] = '\0';
		}
		return *this;
	}
	
	const String& String::operator=(const char *s) 
	{
		int tmpSlen;
		if (mlen <= 0)	bstringThrow("Write protection error");
		if (NULL == s)	s = "";
		if ((tmpSlen = (int)strlen(s)) >= mlen)	Alloc(tmpSlen);
		
		if (data)
		{
			slen = tmpSlen;
			memcpy(data, s, tmpSlen + 1);
		}
		else 
		{
			mlen = slen = 0;
			bstringThrow("Failure in =(const char *) operator");
		}
		return *this;
	}
	
	const String& String::operator=(const String& b) 
	{
        if (&b == this)     return *this;
		if (mlen <= 0)		bstringThrow("Write protection error");
		if (b.slen >= mlen)	Alloc(b.slen);
		
		slen = b.slen;
		if (!data)
		{
			mlen = slen = 0;
			bstringThrow("Failure in =(String) operator");
		}
		else 
		{
			memcpy(data, b.data, slen);
			data[slen] = '\0';
		}
		return *this;
	}
	
	const String& String::operator=(const tagbstring& x) 
	{
		if (mlen <= 0)	bstringThrow("Write protection error");
		if (x.slen < 0)	bstringThrow("Failure in =(tagbstring) operator, badly formed tagbstring");
		if (x.slen >= mlen)	Alloc(x.slen);
		
		slen = x.slen;
		if (!data)
		{
			mlen = slen = 0;
			bstringThrow("Failure in =(tagbstring) operator");
		}
		else 
		{
			memcpy(data, x.data, slen);
			data[slen] = '\0';
		}
		return *this;
	}
	
	const String& String::operator +=(const String& b) 
	{
		if (BSTR_ERR == bconcat(this, (bstring)&b))
			bstringThrow("Failure in concatenate");
		return *this;
	}
	
	const String& String::operator +=(const char *s) 
	{
		struct tagbstring x;
		
		char * d;
		int i, l;
		
		if (mlen <= 0)	bstringThrow("Write protection error");
		
		/* Optimistically concatenate directly */
		l = mlen - slen;
		d = (char *) &data[slen];
		for (i = 0; i < l; i++)
		{
			if ((*d++ = *s++) == '\0')
			{
				slen += i;
				return *this;
			}
		}
		slen += i;
		
		cstr2tbstr(x, s);
		if (BSTR_ERR == bconcat(this, &x))	bstringThrow("Failure in concatenate");

		return *this;
	}
	
	const String& String::operator +=(char c) 
	{
		if (BSTR_ERR == bconchar(this, c))	bstringThrow("Failure in concatenate");
		return *this;
	}
	
	const String& String::operator +=(unsigned char c) 
	{
		if (BSTR_ERR == bconchar(this, (char) c)) bstringThrow("Failure in concatenate");
		return *this;
	}
	
	const String& String::operator +=(const tagbstring& x) 
	{
		if (mlen <= 0)	bstringThrow("Write protection error");
		if (x.slen < 0)	bstringThrow("Failure in +=(tagbstring) operator, badly formed tagbstring");
		Alloc(x.slen + slen + 1);
		
		if (!data)
		{
			mlen = slen = 0;
			bstringThrow("Failure in +=(tagbstring) operator");
		}
		else 
		{
			memcpy(data + slen, x.data, x.slen);
			slen += x.slen;
			data[slen] = '\0';
		}
		return *this;
	}
	
	const String String::operator+(char c) const 
	{
		String retval(*this);
		retval += c;
		return retval;
	}
	
	const String String::operator+(unsigned char c) const 
	{
		String retval(*this);
		retval += c;
		return retval;
	}
	
	const String String::operator+(const String& b) const 
	{
		String retval(*this);
		retval += b;
		return retval;
	}
	
	const String String::operator+(const char *s) const 
	{
		String retval(*this);
		if (s == NULL)	return retval;
		retval += s;
		return retval;
	}
	
	const String String::operator+(const unsigned char *s) const 
	{
		String retval(*this);
		if (s == NULL)	return retval;
		retval +=(char *) s;
		return retval;
	}
	
	const String String::operator+(const int c) const
	{
	    String retval(*this);
	    retval += c;
	    return retval;
	}
	const String String::operator+(const unsigned int c) const
	{
	    String retval(*this);
	    retval += c;
	    return retval;
	}
	const String String::operator+(const float c) const
	{
	    String retval(*this);
	    retval += c;
	    return retval;
	}
	const String String::operator+(const double c) const
	{
	    String retval(*this);
	    retval += c;
	    return retval;
	}
	const String String::operator+(const int64 c) const
	{
	    String retval(*this);
	    retval += c;
	    return retval;
	}
	const String String::operator+(const uint64 c) const
	{
	    String retval(*this);
	    retval += c;
	    return retval;
	}
	
	const String& String::operator +=(const int c) 
	{
#ifndef HasFloatParsing
        char buffer[12] = { c < 0 ? '-': 0 };
        utoa((unsigned long)(c < 0 ? -c : c), &buffer[c < 0 ? 1 : 0], 10);
		return *this += buffer;
#else
        return *this += String::Print("%d", c);
#endif
	}
	const String& String::operator +=(const unsigned int c) 
	{
#ifndef HasFloatParsing
        char buffer[11] = { 0 };
        utoa((unsigned long)c, buffer, 10);
		return *this += buffer;
#else
        return *this += String::Print("%u", c);
#endif
	}
	const String& String::operator +=(const int64 c) 
	{
#ifndef HasFloatParsing
        char buffer[22] = { c < 0 ? '-': 0 };
        Strings::ulltoa((uint64)(c < 0 ? -c : c), &buffer[c < 0 ? 1 : 0], 10);
		return *this += buffer;
#else
        return *this += String::Print(PF_LLD, c);
#endif
	}
	const String& String::operator +=(const uint64 c) 
	{
#ifndef HasFloatParsing
        char buffer[21] = { 0 };
        Strings::ulltoa(c, buffer, 10);
		return *this += buffer;
#else
        return *this += String::Print(PF_LLU, c);
#endif
	}
	String String::getHexOf(const uint64 c) 
	{
        char buffer[23] = { '0', 'x' };
        Strings::ulltoa(c, buffer+2, 16);
		return String(buffer);
	}

    int64 String::parseInt(int base) const
    {
        const char * text = (const char*)data;
        bool negative = text[0] == '-';
        text += (int)negative;
        const char baseText[] = "0123456789abcdef", BASEText[] = "0123456789ABCDEF";

        // Check the current base, if auto detection is activated
        if (!base)
        {
            if (text[0] != '0') base = 10;
            else switch(text[1])
            {
            case 'X': case 'x': base = 16; text += 2; break;
            case 'B': case 'b': base = 2; text += 2; break;
            default: base = 8; text += 1; break;
            }
        }
        // Let's start conversion
        int64 ret = 0;
        while (text)
        {
            const char * charPos = strchr(baseText, *text);
            int digit = (int)(charPos - baseText);
            if (charPos == NULL && base > 10) { charPos = strchr(BASEText, *text); digit = (int)(charPos - BASEText); }
            if (charPos == NULL) break;
            if (digit >= base) break;
            ret = ret * base + digit;
            text++;
        }
        return negative ? -ret : ret;
    }

	const String& String::operator +=(const float c) 
	{
#ifndef HasFloatParsing
        char buffer[15] = { 0 };
        ftoa(c, buffer, 6);
		return *this += buffer;
#else
        return *this += String::Print("%lg", c);
#endif
	}
	const String& String::operator +=(const double c) 
	{
#ifndef HasFloatParsing
        char buffer[15] = { 0 };
        ftoa((float)c, buffer, 6);
		return *this += buffer;
#else
        return *this += String::Print("%lg", c);
#endif
	}
	
	const String String::operator+(const tagbstring& x) const 
	{
		if (x.slen < 0)	bstringThrow("Failure in + (tagbstring) operator, badly formed tagbstring");
		String retval(*this);
		retval += x;
		return retval;
	}
	
	bool String::operator ==(const String& b) const 
	{
		int retval;
		if (BSTR_ERR ==(retval = biseq((bstring)this, (bstring)&b))) bstringThrow("Failure in compare (==)");
		return retval != 0;
	}
	
	bool String::operator ==(const char * s) const 
	{
		int retval;
		if (NULL == s)	return slen == 0;
		if (BSTR_ERR ==(retval = biseqcstr((bstring) this, s)))	bstringThrow("Failure in compare (==)");
		return retval != 0;
	}
	
	bool String::operator ==(const unsigned char * s) const 
	{
		int retval;
		if (NULL == s) return slen == 0;
		if (BSTR_ERR ==(retval = biseqcstr((bstring) this, (char *) s))) bstringThrow("Failure in compare (==)");
		return retval != 0;
	}
	
	bool String::operator !=(const String& b) const 
	{
		return !((*this) == b);
	}
	
	bool String::operator !=(const char * s) const 
	{
		return !((*this) == s);
	}
	
	bool String::operator !=(const unsigned char * s) const 
	{
		return !((*this) == s);
	}
	
	bool String::operator <(const String& b) const 
	{
		int retval;
		if (SHRT_MIN ==(retval = bstrcmp((bstring) this, (bstring)&b)))
			bstringThrow("Failure in compare (<)");
		return retval < 0;
	}
	
	bool String::operator <(const char * s) const 
	{
		if (NULL == s) return false;
		return strcmp((const char *)this->data, s) < 0;
	}
	
	bool String::operator <(const unsigned char * s) const 
	{
		if (NULL == s) return false;
		return strcmp((const char *)this->data, (const char *)s) < 0;
	}
	
	bool String::operator <=(const String& b) const 
	{
		int retval;
		if (SHRT_MIN ==(retval = bstrcmp((bstring) this, (bstring)&b)))	bstringThrow("Failure in compare (<=)");
		return retval <= 0;
	}
	
	bool String::operator <=(const char * s) const 
	{
		if (NULL == s) return slen == 0;
		return strcmp((const char *)this->data, s) <= 0;
	}
	
	bool String::operator <=(const unsigned char * s) const 
	{
		if (NULL == s) return slen == 0;
		return strcmp((const char *)this->data, (const char *)s) <= 0;
	}
	
	bool String::operator >(const String& b) const 
	{
		return !((*this) <= b);
	}
	
	bool String::operator >(const char * s) const 
	{
		return !((*this) <= s);
	}
	
	bool String::operator >(const unsigned char * s) const 
	{
		return !((*this) <= s);
	}
	
	bool String::operator >=(const String& b) const 
	{
		return !((*this) < b);
	}
	
	bool String::operator >=(const char * s) const 
	{
		return !((*this) < s);
	}
	
	bool String::operator >=(const unsigned char * s) const 
	{
		return !((*this) < s);
	}
	
#ifdef HasFloatParsing
	String::operator double() const
	{
		char * ep = NULL;
		return data ? strtod((const char*)data, &ep) : 0;
	}

	String::operator float() const
	{
		char * ep = NULL;
		return data ? (float)strtod((const char*)data, &ep) : 0;
	}
#endif
	
	String::operator signed int() const 
	{
		return (signed int)parseInt(10);
	}
	
	String::operator unsigned int() const 
	{
		return (unsigned int)parseInt(10);
	}

	String::operator int64() const 
	{
		return (int64)parseInt(10);
	}

	int String::Scan(const char * fmt, void * data) const
	{
		return sscanf((const char *)this->data, fmt, data); 
	}

	
#ifdef __TURBOC__
# ifndef BSTRLIB_NOVSNP
#  define BSTRLIB_NOVSNP
# endif
#endif
	
	/* Give WATCOM C/C++, MSVC some latitude for their non - support of vsnprintf */
#if defined(__WATCOMC__) || defined(_MSC_VER)
#define exvsnprintf(r,b,n,f,a) {r = _vsnprintf (b,n,f,a);}
#else
#ifdef BSTRLIB_NOVSNP
	/* This is just a hack.  If you are using a system without a vsnprintf, it is 
	not recommended that bformat be used at all. */
#define exvsnprintf(r,b,n,f,a) {vsprintf (b,f,a); r = -1;}
#define START_VSNBUFF (256)
#else
	
#if defined(__GNUC__) && !defined(__PPC__)
	/* Something is making gcc complain about this prototype not being here, so 
	I've just gone ahead and put it in. */
	extern "C" 
	{
		extern int vsnprintf(char *buf, size_t count, const char *format, va_list arg);
	}
#endif
	
#define exvsnprintf(r,b,n,f,a) {r = vsnprintf (b,n,f,a);}
#endif
#endif
	
#ifndef START_VSNBUFF
#define START_VSNBUFF (16)
#endif
	
	/*
	* Yeah I'd like to just call a vformat function or something, but because of
	* the ANSI specified brokeness of the va_* macros, it is actually not 
	* possible to do this correctly.
	*/
	String & String::Format(const char * fmt, ...) 
	{
		bstring b;
		va_list arglist;
		int r, n;
		
		if (mlen <= 0)		bstringThrow("Write protection error");
		if (fmt == NULL)
			*this = "<NULL>";
		else
		{
			if ((b = bfromcstr("")) == NULL)
			{
				*this = "<NULL>";
			}
			else 
			{
				if ((n = 2*(int)strlen(fmt)) < START_VSNBUFF)
					n = START_VSNBUFF;
				for (;;)
				{
					if (BSTR_OK != balloc(b, n + 2))
					{
						b = bformat("<OUTM>");
						break;
					}
					
					va_start(arglist, fmt);
					exvsnprintf(r, (char *) b->data, n + 1, fmt, arglist);
					va_end(arglist);
					
					b->data[n] = '\0';
					b->slen = (int)strlen((char *) b->data);
					
					if (b->slen < n)	break;
					if (r > n)			n = r;
					else 				n += n;
				}
				*this = *b;
				bdestroy(b);
			}
		}
		return *this;
	}

	/*
	* Yeah I'd like to just call a vformat function or something, but because of
	* the ANSI specified brokeness of the va_* macros, it is actually not 
	* possible to do this correctly.
	*/
	String String::Print(const char * fmt, ...) 
	{
        bstring b;
		va_list arglist;
		int r, n;
		String ret;

		if (fmt == NULL) return ret;
		else 
		{
			if ((b = bfromcstr("")) == NULL)
			{
				ret = "<NULL>";
			}
			else 
			{
				if ((n = 2*(int)strlen(fmt)) < START_VSNBUFF)
					n = START_VSNBUFF;
				for (;;)
				{
					if (BSTR_OK != balloc(b, n + 2))
					{
						b = bformat("<OUTM>");
						break;
					}
					
					va_start(arglist, fmt);
					exvsnprintf(r, (char *) b->data, n + 1, fmt, arglist);
					va_end(arglist);
					
					b->data[n] = '\0';
					b->slen = (int)strlen((char *) b->data);
					
					if (b->slen < n)	break;
					if (r > n)			n = r;
					else 				n += n;
				}
				ret = *b;
                bdestroy(b);
			}
		}
		return ret;
	}
	

	void String::Formata(const char * fmt, ...) 
	{
		bstring b;
		va_list arglist;
		int r, n;
		
		if (mlen <= 0)	 bstringThrow("Write protection error");
		if (fmt == NULL)
		{
			*this += "<NULL>";
		}
		else 
		{
			if ((b = bfromcstr("")) == NULL)
			{
				*this += "<NULL>";
			}
			else 
			{
				if ((n = 2*(int)strlen(fmt)) < START_VSNBUFF)
					n = START_VSNBUFF;
				for (;;)
				{
					if (BSTR_OK != balloc(b, n + 2))
					{
						b = bformat("<OUTM>");
						break;
					}
					
					va_start(arglist, fmt);
					exvsnprintf(r, (char *) b->data, n + 1, fmt, arglist);
					va_end(arglist);
					
					b->data[n] = '\0';
					b->slen = (int)strlen((char *) b->data);
					
					if (b->slen < n)	break;
					if (r > n)			n = r;
					else 				n += n;
				}
				*this += *b;
				bdestroy(b);
			}
		}
	}
	
	bool String::caselessEqual(const String& b) const 
	{
		int ret;
		if (BSTR_ERR ==(ret = biseqcaseless((bstring) this, (bstring) &b)))	bstringThrow("String::caselessEqual Unable to compare");
		return ret == 1;
	}
	
	int String::caselessCmp(const String& b) const 
	{
		int ret;
		if (SHRT_MIN ==(ret = bstricmp((bstring) this, (bstring) &b)))		bstringThrow("String::caselessCmp Unable to compare");
		return ret;
	}

	int String::Count(const String & b) const
	{
		int i = 0, j = 0;
		int count = 0;
		
		for (; i + j < slen; )
		{
			if ((unsigned char) b[j] == data[i + j])
			{
				j++;
				if (j == b.slen)	{ i+= j; j = 0; count ++; continue; }
				continue;
			}
			i++;
			j = 0;
		}
		
		return count;
	}
	
	int String::Find(const String& b, int pos) const 
	{
		return binstr((bstring) this, pos, (bstring) &b);
	}
	
	int String::Find(const char * b, int pos) const 
	{
		int i, j;
		
		if (NULL == b) return BSTR_ERR;
		
		if ((unsigned int) pos > (unsigned int) slen)
			return BSTR_ERR;
		if ('\0' == b[0])			return pos;
		if (pos == slen)			return BSTR_ERR;
		
		i = pos;
		j = 0;
		
		for (; i + j < slen; )
		{
			if ((unsigned char) b[j] == data[i + j])
			{
				j++;
				if ('\0' == b[j])	return i;
				continue;
			}
			i++;
			j = 0;
		}
		
		return BSTR_ERR;
	}
	
	int String::caselessFind(const String& b, int pos) const 
	{
		return binstrcaseless((bstring) this, pos, (bstring) &b);
	}
	
	int String::caselessFind(const char * b, int pos) const 
	{
		struct tagbstring t;
		
		if (NULL == b) return BSTR_ERR;
		
		if ((unsigned int) pos > (unsigned int) slen)	return BSTR_ERR;
		if ('\0' == b[0])			return pos;
		if (pos == slen)			return BSTR_ERR;
		
		btfromcstr(t, b);
		return binstrcaseless((bstring) this, pos, (bstring) &t);
	}
	
	int String::Find(char c, int pos) const 
	{
		if (pos < 0)	return BSTR_ERR;
		for (; pos < slen; pos++)
		{
			if (data[pos] ==(unsigned char) c)
				return pos;
		}
		return BSTR_ERR;
	}
	
	int String::reverseFind(const String& b, int pos) const 
	{
		return binstrr((bstring) this, pos, (bstring) &b);
	}
	
	int String::reverseFind(const char * b, int pos) const 
	{
		struct tagbstring t;
		if (NULL == b) return BSTR_ERR;
		cstr2tbstr(t, b);
		return binstrr((bstring) this, pos, &t);
	}
	
	int String::caselessReverseFind(const String& b, int pos) const 
	{
		return binstrrcaseless((bstring) this, pos, (bstring) &b);
	}
	
	int String::caselessReverseFind(const char * b, int pos) const 
	{
		struct tagbstring t;
		
		if (NULL == b) return BSTR_ERR;
		
		if ((unsigned int) pos > (unsigned int) slen)		return BSTR_ERR;
		if ('\0' == b[0])									return pos;
		if (pos == slen)									return BSTR_ERR;
		
		btfromcstr(t, b);
		return binstrrcaseless((bstring) this, pos, (bstring) &t);
	}
	
	int String::reverseFind(char c, int pos) const 
	{
        if (pos < 0) pos = slen-1;
		if (pos > slen)			return BSTR_ERR;
		if (pos == slen)		pos--;
		for (; pos >= 0; pos--)
		{
			if (data[pos] ==(unsigned char) c)	return pos;
		}
		return BSTR_ERR;
	}
	
	int String::findAnyChar(const String& b, int pos) const 
	{
		return binchr((bstring) this, pos, (bstring) &b);
	}
	
	int String::findAnyChar(const char * s, int pos) const 
	{
		struct tagbstring t;
		if (NULL == s) return BSTR_ERR;
		cstr2tbstr(t, s);
		return binchr((bstring) this, pos, (bstring) &t);
	}
	
	int String::invFindAnyChar(const String& b, int pos) const 
	{
		return bninchr((bstring) this, pos, (bstring) &b);
	}
	
	int String::invFindAnyChar(const char * s, int pos) const 
	{
		struct tagbstring t;
		if (NULL == s) return BSTR_ERR;
		cstr2tbstr(t, s);
		return bninchr((bstring) this, pos, &t);
	}
	
	int String::reverseFindAnyChar(const String& b, int pos) const 
	{
		return binchrr((bstring) this, pos, (bstring) &b);
	}
	
	int String::reverseFindAnyChar(const char * s, int pos) const 
	{
		struct tagbstring t;
		if (NULL == s) return BSTR_ERR;
		cstr2tbstr(t, s);
		return binchrr((bstring) this, pos, &t);
	}
	
	int String::invReverseFindAnyChar(const String& b, int pos) const 
	{
		return bninchrr((bstring) this, pos, (bstring) &b);
	}
	
	int String::invReverseFindAnyChar(const char * s, int pos) const 
	{
		struct tagbstring t;
		if (NULL == s) return BSTR_ERR;
		cstr2tbstr(t, s);
		return bninchrr((bstring) this, pos, &t);
	}
	

    String String::extractToken(char c, int & pos) const
    {
        String ret;
        if (pos >= slen) return ret;

        int findNextPos = Find(c, pos);
        if (findNextPos == -1) findNextPos = slen;
        ret = midString(pos, findNextPos - pos);
        pos = findNextPos + 1;
        return ret;
    }

	const String String::midString(int left, int len) const 
	{
		struct tagbstring t;
        if (len < 0)
        {   // Want data from the right of the string, without specifying the start point
            // For example String("abcdefgh").midString(0, -3) => "fgh" (whatever the left start point)
            left = -len < slen ? slen + len : 0;
            len = -len;
        }
		if (left < 0)
		{   // Want data from the right of the string
			len = len > -left ? -left : len;
			left = slen + left;
		}
		if (len > slen - left)	len = slen - left;
		if (len <= 0 || left < 0)	return String("");
		blk2tbstr(t, data + left, len);
		return String(t);
	}
	
	char * String::Alloc(int length) 
	{
		if (BSTR_ERR == balloc((bstring)this, length))	bstringThrow("Failure in Alloc");
		return (char*)data;
	}
	
	void String::Fill(int length, unsigned char fill) 
	{
		slen = 0;
		if (BSTR_ERR == bsetstr(this, length, NULL, fill))	bstringThrow("Failure in fill");
	}
	
	void String::setSubstring(int pos, const String& b, unsigned char fill) 
	{
		if (BSTR_ERR == bsetstr(this, pos, (bstring) &b, fill))	bstringThrow("Failure in setstr");
	}
	
	void String::setSubstring(int pos, const char * s, unsigned char fill) 
	{
		struct tagbstring t;
		if (NULL == s) return;
		cstr2tbstr(t, s);
		if (BSTR_ERR == bsetstr(this, pos, &t, fill))
		{
			bstringThrow("Failure in setstr");
		}
	}
	
	void String::Insert(int pos, const String& b, unsigned char fill) 
	{
		if (BSTR_ERR == binsert(this, pos, (bstring) &b, fill))	bstringThrow("Failure in insert");
	}
	
	void String::Insert(int pos, const char * s, unsigned char fill) 
	{
		struct tagbstring t;
		if (NULL == s) return;
		cstr2tbstr(t, s);
		if (BSTR_ERR == binsert(this, pos, &t, fill))	bstringThrow("Failure in insert");
	}
	
	void String::insertChars(int pos, int len, unsigned char fill) 
	{
		if (BSTR_ERR == binsertch(this, pos, len, fill)) bstringThrow("Failure in insertchrs");
	}
	
	void String::Replace(int pos, int len, const String& b, unsigned char fill) 
	{
		if (BSTR_ERR == breplace(this, pos, len, (bstring) &b, fill)) bstringThrow("Failure in Replace");
	}
	
	void String::Replace(int pos, int len, const char * s, unsigned char fill) 
	{
		struct tagbstring t;
		int q;
		
		if (mlen <= 0)						bstringThrow("Write protection error");
		if (NULL == s || (pos | len) < 0)	return;
        
		
        if (pos + len >= slen)
        {
            cstr2tbstr(t, s);
            if (BSTR_ERR == bsetstr(this, pos, &t, fill))
                bstringThrow("Failure in Replace");
            else if (pos + t.slen < slen)
            {
                slen = pos + t.slen;
                data[slen] = '\0';
            }
        }
        else 
        {
            /* Aliasing case */
            if ((unsigned int)(data - (unsigned char *) s) < (unsigned int) slen)
            {
                Replace(pos, len, String(s), fill);
                return;
            }
            
            if ((q = (int)strlen(s)) > len)
            {
                Alloc(slen + q - len);
                if (NULL == data)	return;
            }
            if (q != len) memmove(data + pos + q, data + pos + len, slen - (pos + len));
            memcpy(data + pos, s, q);
            slen += q - len;
            data[slen] = '\0';
        }
	}
	
	String & String::findAndReplace(const String& find, const String& repl, int pos) 
	{
		if (BSTR_ERR == bfindreplace(this, (bstring) &find, (bstring) &repl, pos))	bstringThrow("Failure in findreplace");
		return *this;
	}
	

	String & String::findAndReplace(const String& find, const char * repl, int pos) 
	{
		struct tagbstring t;
		if (NULL == repl) return *this;
		cstr2tbstr(t, repl);
		if (BSTR_ERR == bfindreplace(this, (bstring) &find, (bstring) &t, pos))	bstringThrow("Failure in findreplace");
        return *this;
	}
	
	String & String::findAndReplace(const char * find, const String& repl, int pos) 
	{
		struct tagbstring t;
		if (NULL == find) return *this;

		cstr2tbstr(t, find);
		if (BSTR_ERR == bfindreplace(this, (bstring) &t, (bstring) &repl, pos))	bstringThrow("Failure in findreplace");
		return *this;
	}
	
	String & String::findAndReplace(const char * find, const char * repl, int pos) 
	{
		struct tagbstring t, u;
		if (NULL == repl || NULL == find) return *this;
		cstr2tbstr(t, find);
		cstr2tbstr(u, repl);
		if (BSTR_ERR == bfindreplace(this, (bstring) &t, (bstring) &u, pos)) bstringThrow("Failure in findreplace");
		return *this;
	}
	
	String & String::findAndReplaceCaseless(const String& find, const String& repl, int pos) 
	{
		if (BSTR_ERR == bfindreplacecaseless(this, (bstring) &find, (bstring) &repl, pos))	bstringThrow("Failure in findreplacecaseless");
        return *this;
	}

	
	String & String::findAndReplaceCaseless(const String& find, const char * repl, int pos) 
	{
		struct tagbstring t;
		if (NULL == repl) return *this;
		cstr2tbstr(t, repl);
		if (BSTR_ERR == bfindreplacecaseless(this, (bstring) &find, (bstring) &t, pos))	bstringThrow("Failure in findreplacecaseless");
        return *this;
	}
	
	String & String::findAndReplaceCaseless(const char * find, const String& repl, int pos) 
	{
		struct tagbstring t;
		if (NULL == find) return *this;
		cstr2tbstr(t, find);
		if (BSTR_ERR == bfindreplacecaseless(this, (bstring) &t, (bstring) &repl, pos))	bstringThrow("Failure in findreplacecaseless");
        return *this;
	}
	
	String & String::findAndReplaceCaseless(const char * find, const char * repl, int pos) 
	{
		struct tagbstring t, u;
		if (NULL == repl || NULL == find) return *this;
		cstr2tbstr(t, find);
		cstr2tbstr(u, repl);
		if (BSTR_ERR == bfindreplacecaseless(this, (bstring) &t, (bstring) &u, pos)) bstringThrow("Failure in findreplacecaseless");
        return *this;
	}
	
	void String::Remove(int pos, int len) 
	{
		if (BSTR_ERR == bdelete(this, pos, len)) bstringThrow("Failure in remove");
	}
	
	void String::Truncate(int len) 
	{
		if (len < 0) return;
		if (len < slen)
		{
			slen = len;
			data[len] = '\0';
		}
	}
	
	void String::leftTrim(const String& b) 
	{
		int l = invFindAnyChar(b, 0);
		if (l == BSTR_ERR)	l = slen;
		Remove(0, l);
	}
	
	void String::rightTrim(const String& b) 
	{
		int l = invReverseFindAnyChar(b, slen - 1);
		if (l == BSTR_ERR)	l = slen - 1;
		slen = l + 1;
		if (mlen > slen) data[slen] = '\0';
	}
	
	void String::toUppercase() 
	{
		if (BSTR_ERR == btoupper((bstring) this))	bstringThrow("Failure in toupper");
	}
	
	void String::toLowercase() 
	{
		if (BSTR_ERR == btolower((bstring) this))	bstringThrow("Failure in tolower");
	}
	
	void String::Repeat(int count) 
	{
		count *= slen;
		if (count == 0)
		{
			Truncate(0);
			return;
		}
		if (count < 0 || BSTR_ERR == bpattern(this, count))	bstringThrow("Failure in repeat");
	}
/*	
	int String::gets(bNgetc getcPtr, void * parm, char terminator) 
	{
		if (mlen <= 0)	bstringThrow("Write protection error");
		bstring b = bgets(getcPtr, parm, terminator);
		if (b == NULL)
		{
			slen = 0;
			return -1;
		}
		*this = *b;
		bdestroy(b);
		return 0;
	}
	
	int String::read(bNread readPtr, void * parm) 
	{
		if (mlen <= 0)	bstringThrow("Write protection error");
		bstring b = bread(readPtr, parm);
		if (b == NULL)
		{
			slen = 0;
			return -1;
		}
		*this = *b;
		bdestroy(b);
		return 0;
	}
*/	
	const String operator+(const char *a, const String& b) 
	{
		return String(a) + b;
	}
	
	const String operator+(const unsigned char *a, const String& b) 
	{
		return String((const char *)a) + b;
	}
	
	const String operator+(char c, const String& b) 
	{
		return String(c) + b;
	}
	
	const String operator+(unsigned char c, const String& b) 
	{
		return String(c) + b;
	}
	
	const String operator+(const tagbstring& x, const String& b) 
	{
		return String(x) + b;
	}
	
    String String::normalizedPath(char sep, const bool includeLastSep) const
    {
        int rightLimit = slen - 1;
        while (rightLimit >= 0 && data[rightLimit] == sep) rightLimit--;
        return includeLastSep || rightLimit < 0 ? midString(0, rightLimit+1) + sep : midString(0, rightLimit+1);
    }

    String & String::replaceAllTokens(char from, char to)
    {
        if (!from || !to) return *this;
        for (int i = 0; i < slen; i++)
            if (data[i] == from) data[i] = to;
        return *this;
    }


	// Get the string up to the first occurrence of the given string 
	const String String::upToFirst(const String & find, const bool includeFind) const
	{   
	    int pos = Find(find, 0); 
	    if (pos == -1) return includeFind ? "" : *this;
	    return midString(0, includeFind ? pos + find.getLength() : pos);
	}
	// Get the string up to the last occurrence of the given string 
	const String String::upToLast(const String & find, const bool includeFind) const
	{   
	    int pos = reverseFind(find, slen - 1); 
	    if (pos == -1) return includeFind ? "" : *this;
	    return midString(0, includeFind ? pos + find.getLength() : pos);
	}
	// Get the string from the first occurrence of the given string 
	const String String::fromFirst(const String & find, const bool includeFind) const
	{
	    int pos = Find(find, 0); 
	    if (pos == -1) return includeFind ? *this : String();
	    return midString(includeFind ? pos : pos + find.getLength(), slen);
	}
	// Get the string from the first occurrence of the given string 
	const String String::dropUpTo(const String & find, const bool includeFind) const
	{
	    const unsigned int pos = Find(find);
	    if (pos == -1) return *this;
	    return midString(includeFind ? pos : pos + find.getLength(), slen);
	}

	// Get the string from the last occurrence of the given string
	const String String::fromLast(const String & find, const bool includeFind) const
	{
	    int pos = reverseFind(find, slen - 1); 
	    if (pos == -1) return includeFind ? *this : String();
	    return midString(includeFind ? pos : pos + find.getLength(), slen);
	}
	// Split a string when the needle is found first, returning the part before the needle, and 
	// updating the string to start on or after the needle.
	const String String::splitFrom(const String & find, const bool includeFind)
	{
	    int pos = Find(find, 0);
	    if (pos == -1) 
	    {
	        if (includeFind)
	        {
	            String ret(*this);
	            *this = "";
	            return ret;
	        }
	        return String();
	    }
	    int size = pos + find.getLength();
	    String ret = midString(0, includeFind ? size : pos);
	    if (BSTR_ERR == bdelete(this, 0, size)) bstringThrow("Failure in remove");
	    return ret;
	}
	// Split a string when the needle is found first, returning the part before the needle, and 
	// updating the string to start on or after the needle.
	const String String::fromTo(const String & from, const String & to, const bool includeFind) const
	{
	    const int fromPos = Find(from);
	    if (fromPos == -1) return "";
	    const int toPos = Find(to, fromPos + from.slen);
	    return midString(includeFind ? fromPos : fromPos + from.slen, toPos != -1 ? (includeFind ? toPos + to.slen - fromPos : toPos - fromPos - from.slen)
	                                       // If the "to" needle was not found, either we return the whole string (includeFind) or an empty string
	                                       : (includeFind ? slen - fromPos : 0));
	}
	// Get the substring up to the given needle if found, or the whole string if not, and split from here. 
    const String String::splitUpTo(const String & find, const bool includeFind)
    {
	    int pos = Find(find, 0);
	    if (pos == -1) 
	    {
            String ret(*this);
            *this = "";
            return ret;
	    }
	    int size = pos + find.getLength();
	    String ret = midString(0, includeFind ? size : pos);
	    if (BSTR_ERR == bdelete(this, 0, size)) bstringThrow("Failure in remove");
	    return ret;
    }
    // Split the string at the given position.
    const String String::splitAt(const int pos)
    {
	    String ret = midString(0, pos);
	    if (BSTR_ERR == bdelete(this, 0, pos)) bstringThrow("Failure in remove");
	    return ret;
    }
	
	// Align the string so it fits in the given length.
	String String::alignedTo(const int length, int side, char fill) const
	{
	    if (slen > length) return *this;
	    int diffInSize = (length - slen), 
	        leftCount = side == 1 ? diffInSize : (side == 0 ? diffInSize / 2 : 0),
	        rightCount = side == -1 ? diffInSize : (side == 0 ? (diffInSize + 1) / 2 : 0);
	        
	    return String().Filled(leftCount, fill) + *this + Filled(rightCount, fill);
	}
	
	
	

	void String::writeProtect() 
	{
		if (mlen >= 0)	mlen = -1;
	}
	
	void String::writeAllow() 
	{
		if (mlen == -1)		mlen = slen + (slen == 0);
		else if (mlen < 0)	bstringThrow("Cannot unprotect a constant");
	}

#if (WantRegularExpressions == 1)
    // Compiled regular expression
    struct String::RegExOpaque 
    {
        unsigned char code[256];
        unsigned char data[256];
        int codeSize;
        int dataSize;
        int captureCount;   // Number of bracket pairs
        int anchored;   // Must match from string start
        bool caseInsensitive;
        const char *errorText;   // Error string
        
        RegExOpaque() : codeSize(0), dataSize(0), captureCount(0), anchored(0), caseInsensitive(false), errorText(0) { memset(code, 0, ArrSz(code)); memset(data, 0, ArrSz(data)); }
    };

   // Forward declare the functions we'll be using
    namespace RegExp
    {
        typedef String::RegExOpaque RegExOpaque;
        // Captured substring
        struct cap 
        {
            const char *ptr;  // Pointer to the substring
            int len;          // Substring length
        };

        // Instruction set for our regular expressions processor
        enum 
        {
            END, BRANCH, ANY, EXACT, ANYOF, ANYBUT, OPEN, CLOSE, BOL, EOL, STAR, PLUS,
            STARQ, PLUSQ, QUEST, SPACE, NONSPACE, DIGIT
        };

        static const char * meta_characters = "|.^$*+?()[\\";
        static const char * errorNoMatch = "No match";

        static void setJumpOffset(RegExOpaque * r, const int pc, const int offset) 
        {
            if (r->codeSize - offset > 0xff) 
                r->errorText = "Jump offset is too big";
            else r->code[pc] = (unsigned char) (r->codeSize - offset);
        }

        static void emit(RegExOpaque *r, int code) 
        {
            if (r->codeSize >= (int) (sizeof(r->code) / sizeof(r->code[0]))) 
                r->errorText = "RE is too long (code overflow)";
            else r->code[r->codeSize++] = (unsigned char) code;
        }

        static void storeCharInData(RegExOpaque *r, int ch) 
        {
            if (r->dataSize >= (int) sizeof(r->data)) 
                r->errorText = "RE is too long (data overflow)";
            else r->data[r->dataSize++] = ch;
        }

        static void exact(RegExOpaque *r, const char **re) 
        {
            int  oldDataSize = r->dataSize;

            while (**re != '\0' && (strchr(meta_characters, **re)) == NULL) 
                storeCharInData(r, *(*re)++);
           
            emit(r, EXACT); emit(r, oldDataSize); emit(r, r->dataSize - oldDataSize);
        }

        static int getEscapeChar(const char **re) 
        {
            switch (*(*re)++) 
            {
            case 'n':  return '\n';    
            case 'r':  return '\r'; 
            case 't':  return '\t';
            case '0':  return 0; 
            case 'S':  return NONSPACE << 8;
            case 's':  return SPACE << 8;  
            case 'd':  return DIGIT << 8;  
            default:  return (*re)[-1];  
            }
        }

        static void anyof(RegExOpaque *r, const char **re) 
        {
            int  esc, oldDataSize = r->dataSize, op = ANYOF;

            if (**re == '^') { op = ANYBUT; (*re)++; }

            while (**re != '\0')
                switch (*(*re)++) 
                {
                case ']':
                    emit(r, op);
                    emit(r, oldDataSize);
                    emit(r, r->dataSize - oldDataSize);
                return;
                case '\\':
                    esc = getEscapeChar(re);
                    if ((esc & 0xff) == 0) 
                    { storeCharInData(r, 0); storeCharInData(r, esc >> 8); } 
                    else storeCharInData(r, esc);
                break;
                default:
                    storeCharInData(r, (*re)[-1]);
                break;
                }

            r->errorText = "No closing ']' bracket";
        }

        static void relocate(RegExOpaque *r, int begin, int shift) 
        {
            emit(r, END);
            memmove(r->code + begin + shift, r->code + begin, r->codeSize - begin);
            r->codeSize += shift;
        }

        static void quantifier(RegExOpaque *r, int prev, int op) 
        {
            if (r->code[prev] == EXACT && r->code[prev + 2] > 1) 
            {
                r->code[prev + 2]--;
                emit(r, EXACT); emit(r, r->code[prev + 1] + r->code[prev + 2]); emit(r, 1);
                prev = r->codeSize - 3;
            }
            relocate(r, prev, 2); r->code[prev] = op; setJumpOffset(r, prev + 1, prev);
        }

        static void exactOneChar(RegExOpaque *r, int ch) 
        {
            emit(r, EXACT); emit(r, r->dataSize); emit(r, 1);
            storeCharInData(r, ch);
        }

        static void fixupBranch(RegExOpaque *r, int fixup) 
        {
            if (fixup > 0) 
            { 
                emit(r, END); setJumpOffset(r, fixup, fixup - 2); 
            }
        }

        static void compile(RegExOpaque *r, const char **re) 
        {
            int  op, esc, branchStart, lastOp, fixup, capIndex, level;

            fixup = 0;
            level = r->captureCount;
            branchStart = lastOp = r->codeSize;

            for (;;)
                switch (*(*re)++) 
                {
                case '\0': (*re)--; return;
                case '^': emit(r, BOL); break;
                case '$': emit(r, EOL); break;

                case '.': lastOp = r->codeSize; emit(r, ANY); break;
                case '[': lastOp = r->codeSize; anyof(r, re); break;
                case '?': quantifier(r, lastOp, QUEST); break;
                case '\\':
                    lastOp = r->codeSize;
                    esc = getEscapeChar(re);
                    if (esc & 0xff00) emit(r, esc >> 8);
                    else exactOneChar(r, esc);
                    break;

                case '(':
                    lastOp = r->codeSize; capIndex = ++r->captureCount;
                    emit(r, OPEN); emit(r, capIndex);
                    compile(r, re);
                    if (*(*re)++ != ')') { r->errorText = "No closing bracket"; return; }
                    emit(r, CLOSE); emit(r, capIndex);
                    break;
                case ')':
                    (*re)--;
                    fixupBranch(r, fixup);
                    if (level == 0) { r->errorText = "Unbalanced brackets"; return; }
                    return;

                case '+':
                case '*':
                    op = (*re)[-1] == '*' ? STAR: PLUS;
                    if (**re == '?') { (*re)++; op = op == STAR ? STARQ : PLUSQ; }
                    quantifier(r, lastOp, op);
                    break;

                case '|':
                    fixupBranch(r, fixup);
                    relocate(r, branchStart, 3);
                    r->code[branchStart] = BRANCH;
                    setJumpOffset(r, branchStart + 1, branchStart);
                    fixup = branchStart + 2;
                    r->code[fixup] = 0xff;
                    break;

                default: (*re)--; lastOp = r->codeSize; exact(r, re); break;
                }
        }

        // Compile regular expression. If success, 1 is returned.
        // If error, 0 is returned and RegExOpaque.errorText points to the error message.
        static const char *compile2(RegExOpaque *r, const char *re) 
        {
            r->errorText = NULL;
            r->codeSize = r->dataSize = r->captureCount = r->anchored = 0;

            if (*re == '^') r->anchored++;

            // This will capture what matches full RE
            emit(r, OPEN);  emit(r, 0);

            while (*re != '\0') compile(r, &re);

            if (r->code[2] == BRANCH) fixupBranch(r, 4);

            emit(r, CLOSE); emit(r, 0); emit(r, END);
            return r->errorText;
        }

        static const char *match(const RegExOpaque *, int, const char *, int, int *, struct cap *);

        static void loopGreedy(const RegExOpaque *r, int pc, const char *s, int len, int *ofs) 
        {
            int savedOffset = *ofs, matchedOffset = *ofs;
            while (!match(r, pc + 2, s, len, ofs, NULL)) 
            {
                savedOffset = *ofs;
                if (!match(r, pc + r->code[pc + 1], s, len, ofs, NULL)) 
                    matchedOffset = savedOffset;
                *ofs = savedOffset;
            }

            *ofs = matchedOffset;
        }

        static void loopNonGreedy(const RegExOpaque *r, int pc, const char *s, int len, int *ofs) 
        {
            int  savedOffset = *ofs;

            while (!match(r, pc + 2, s, len, ofs, NULL)) 
            {
                savedOffset = *ofs;
                if (!match(r, pc + r->code[pc + 1], s, len, ofs, NULL)) break;
            }

            *ofs = savedOffset;
        }

        static int isAnyOf(const unsigned char *p, int len, const char *s, int *ofs) 
        {
            int ch = s[*ofs];

            for (int i = 0; i < len; i++)
                if (p[i] == ch) { (*ofs)++; return 1; }

            return 0;
        }

        static int isAnyBut(const unsigned char *p, int len, const char *s, int *ofs) 
        {
            int ch = s[*ofs];

            for (int i = 0; i < len; i++)
                if (p[i] == ch) return 0;

            (*ofs)++;
            return 1;
        }

        static int lowercase(const char *s) 
        {
            return ::tolower(* (const unsigned char *) s);
        }

        static int casecmp(const void *p1, const void *p2, size_t len) 
        {
            const char *s1 = (const char*)p1, *s2 = (const char*)p2;
            int diff = 0;

            if (len > 0)
            do 
            {
                diff = lowercase(s1++) - lowercase(s2++);
            } while (diff == 0 && s1[-1] != '\0' && --len > 0);

            return diff;
        }

        static const char *match(const RegExOpaque *r, int pc, const char *s, int len,
                                 int *ofs, struct cap *caps) 
        {
            int n, savedOffset;
            const char *errorText = NULL;
            int (*cmp)(const void *string1, const void *string2, size_t len);

            while (errorText == NULL && r->code[pc] != END) 
            {
                switch (r->code[pc]) 
                {
                case BRANCH:
                    savedOffset = *ofs;
                    errorText = match(r, pc + 3, s, len, ofs, caps);
                    if (errorText != NULL) { *ofs = savedOffset; errorText = match(r, pc + r->code[pc + 1], s, len, ofs, caps); }
                    pc += r->code[pc + 2];
                    break;

                case EXACT:
                    errorText = errorNoMatch;
                    n = r->code[pc + 2];  // String length
                    cmp = r->caseInsensitive ? casecmp : memcmp;
                    if (n <= len - *ofs && !cmp(s + *ofs, r->data + r->code[pc + 1], n)) { (*ofs) += n; errorText = NULL; }
                    pc += 3;
                    break;

                case QUEST:
                    errorText = NULL;
                    savedOffset = *ofs;
                    if (match(r, pc + 2, s, len, ofs, caps) != NULL) *ofs = savedOffset;
                    pc += r->code[pc + 1];
                    break;

                case STAR:
                    errorText = NULL;
                    loopGreedy(r, pc, s, len, ofs);
                    pc += r->code[pc + 1];
                    break;

                case STARQ:
                    errorText = NULL;
                    loopNonGreedy(r, pc, s, len, ofs);
                    pc += r->code[pc + 1];
                    break;

                case PLUS:
                    if ((errorText = match(r, pc + 2, s, len, ofs, caps)) != NULL) break;
                    loopGreedy(r, pc, s, len, ofs); pc += r->code[pc + 1];
                    break;

                case PLUSQ:
                    if ((errorText = match(r, pc + 2, s, len, ofs, caps)) != NULL) break;
                    loopNonGreedy(r, pc, s, len, ofs); pc += r->code[pc + 1];
                    break;

                case SPACE:
                    errorText = errorNoMatch;
                    if (*ofs < len && isspace(((unsigned char *)s)[*ofs])) { (*ofs)++; errorText = NULL; }
                    pc++;
                    break;

                case NONSPACE:
                    errorText = errorNoMatch;
                    if (*ofs <len && !isspace(((unsigned char *)s)[*ofs])) { (*ofs)++; errorText = NULL; }
                    pc++;
                    break;

                case DIGIT:
                    errorText = errorNoMatch;
                    if (*ofs < len && isdigit(((unsigned char *)s)[*ofs])) { (*ofs)++; errorText = NULL; }
                    pc++;
                    break;

                case ANY:
                    errorText = errorNoMatch;
                    if (*ofs < len) { (*ofs)++; errorText = NULL; }
                    pc++;
                    break;

                case ANYOF:
                    errorText = errorNoMatch;
                    if (*ofs < len) errorText = isAnyOf(r->data + r->code[pc + 1], r->code[pc + 2], s, ofs) ? NULL : errorNoMatch;
                    pc += 3;
                    break;

                case ANYBUT:
                    errorText = errorNoMatch;
                    if (*ofs < len) errorText = isAnyBut(r->data + r->code[pc + 1], r->code[pc + 2], s, ofs) ? NULL : errorNoMatch;
                    pc += 3;
                    break;

                case BOL:
                    errorText = *ofs == 0 ? NULL : errorNoMatch;
                    pc++;
                    break;

                case EOL:
                    errorText = *ofs == len ? NULL : errorNoMatch;
                    pc++;
                    break;

                case OPEN:
                    if (caps != NULL) caps[r->code[pc + 1]].ptr = s + *ofs;
                    pc += 2;
                    break;

                case CLOSE:
                    if (caps != NULL) caps[r->code[pc + 1]].len = (s + *ofs) - caps[r->code[pc + 1]].ptr;
                    pc += 2;
                    break;

                case END: pc++; break;
                default: { static char buffer[128]; sprintf(buffer, "unknown cmd (%d) at %d", r->code[pc], pc); errorText = (const char*)buffer; } break;
                }
            }

            return errorText;
        }

        // Return 1 if match, 0 if no match.
        // If `captured_substrings' array is not NULL, then it is filled with the
        // values of captured substrings. captured_substrings[0] element is always
        // a full matched substring. The round bracket captures start from
        // captured_substrings[1].
        // It is assumed that the size of captured_substrings array is enough to
        // hold all captures. The caller function must make sure it is! So, the
        // array_size = number_of_round_bracket_pairs + 1
        static const char *match2(const RegExOpaque *r, const char *buf, int len, struct cap *caps) 
        {
            int  i, ofs = 0;
            const char *errorText = errorNoMatch;

            if (r->anchored) 
                errorText = match(r, 0, buf, len, &ofs, caps);
            else for (i = 0; i < len && errorText != NULL; i++) 
                {
                    ofs = i;
                    errorText = match(r, 0, buf, len, &ofs, caps);
                }
            return errorText;
        }

    }
    // Match the expression against the compiled regular expression
    String String::regExMatch(const String & regEx, String * captures, const bool caseSensitive) const
    {
        RegExOpaque obj;
        const char * error = RegExp::compile2(&obj, (const char*)regEx);
        if (error != NULL) return error;

        RegExp::cap * caps = new RegExp::cap[obj.captureCount + 1];
        if (!caps) return "Not enough memory for captures";
        obj.caseInsensitive = !caseSensitive;
        error = RegExp::match2(&obj, (const char*)data, slen, caps);
        for (int i = 0; i < obj.captureCount + 1; i++)
            captures[i] = String(caps[i].ptr, caps[i].len);
        delete[] caps;
        return error;
    }
    // Compile a regular expression to match later on.
    int String::regExCompile(const String & regEx, String::RegExOpaque *& opaque) 
    {
        delete opaque;
        opaque = new RegExOpaque;
        const char * error = RegExp::compile2(opaque, (const char*)regEx);
        if (error != NULL) return -1;
        return opaque->captureCount + 1;
    }
    // Match the expression against the compiled regular expression
    String String::regExMatchEx(String::RegExOpaque & opaque, String * captures, const bool caseSensitive) const
    {
        RegExp::cap * caps = new RegExp::cap[opaque.captureCount + 1];
        if (!caps) return "Not enough memory for captures";
        opaque.caseInsensitive = !caseSensitive;
        const char * error = RegExp::match2(&opaque, (const char*)data, slen, caps);
        if (captures)
        {
            for (int i = 0; i < opaque.captureCount + 1; i++)
                captures[i] = String(caps[i].ptr, caps[i].len);
        }
        delete[] caps;
        return error;
    }
    // Clean a precompiled regular expression opaque object
    void String::regExClean(RegExOpaque *& opaque)
    {
        delete0(opaque);
    }
#endif
} // namespace Bstrlib

