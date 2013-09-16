#ifndef hpp_CPP_RandomNumbers_CPP_hpp
#define hpp_CPP_RandomNumbers_CPP_hpp

// We need types
#include "../Types.hpp"

/** Contains the different random generators */
namespace Random
{
    /** The generator interface

        While in most case, it is not required to use this code directly,
        using this code is done the following way :
        @code
            Random::SomeGenerator generator;
            // You can give a true random pool if you want, but you can also use default pool
            generator.Init();
            // Get a random number
            uint32 randomNumber = generator.Random();
        @endcode

        Using a true random pool, is done this way :
        @code
            Random::SomeGenerator generator;
            uint8 trueRandomSource[32]; // The size here is an example
            // Collect some entropy from a known true random source (like sound card, user's mouse move, etc...)
            generator.CollectEntropy(trueRandomSource, 32);
            // Initialize the generator with this pool
            generator.Init(trueRandomSource,32);
            // Get a random number
            uint32 randomNumber = generator.Random();
        @endcode

        @sa NumberBetween()
        */
    struct Generator
    {
        /** Use this method to collect entropy from true random sources into the buffer */
        virtual bool collectEntropy(uint8 * arrayToStoreEntropyTo, uint32 size) = 0;
        /** Reinitialize the number with the given entropy pool */
        virtual void Init(uint8 * pool = 0, uint32 size = 0) = 0;
        /** Get a 32 bits random number */
        virtual uint32 Random() = 0;
        /** Destructor */
        virtual ~Generator(){}
    };

    /** Get a random number inside the given range inclusive */
    uint32 numberBetween(const uint32 lowest, const uint32 highest);
    /** Fill the given block of data with random bytes 
        @param buffer   The buffer to fill randomness into
        @param size     The number of bytes inside the given buffer
        @param reSeed   When true, the generator is reseeded with the local entropy data */
    void fillBlock(uint8 * buffer, size_t size, const bool reSeed = false);
    /** Get the current generator object */
    Generator & getDefaultGenerator();
}

#endif

