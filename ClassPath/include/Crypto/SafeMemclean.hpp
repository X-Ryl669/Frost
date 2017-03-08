#ifndef hpp_CPP_SafeMemclean_CPP_hpp
#define hpp_CPP_SafeMemclean_CPP_hpp

namespace Crypto
{
    /** This function is used to actually clear the given buffer (with 0).
        Unlike memset, it can't be optimized out (it's compiled with dead-code removal optimization off)
        So when you're cleaning your sensitive data, please use this function.

        @warning Only pass POD data to this method so use it in your destructor in case of complex object.
    
        @param buffer   The buffer to clean
        @param size     The buffer size in bytes  */
    extern void SafeMemclean(unsigned char * buffer, const unsigned int size);

    /** Similar cleaning method for object.
        @sa SafeMemclean */
    template <class T>
    static void SafeObjClean(T & obj) { SafeMemclean(&obj, sizeof(obj)); }
    /** Similar cleaning method for static array.
        @sa SafeMemclean */
    template <class T, size_t N>
    static void SafeClean(T (&obj)[N]) { SafeMemclean(obj, N * sizeof(T)); }
}



#endif
