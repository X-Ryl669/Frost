// We need random number declaration
#include "../../include/Crypto/Random.hpp"

#ifdef _WIN32
  // We need the hardware scanner tool to figure out some real entropy source
  #include "../../include/Hardware/Scanner.hpp"
#endif
// We need to figure out the temporary folder path to get its size
#include "../../include/File/File.hpp"
// We need time and memory base definition
#include <memory.h>
#include <time.h>


namespace Random
{
    /** The generator interface 

        While in most case, it is not required to use this code directly, using this code is done the following way :
        @code
        Random::MersenneTwister generator;
        // You can give a true random pool if you want, but you can also use default pool (entropy is collected from CPU time)
        generator.Init(); 
        // Get a random number
        uint32 randomNumber = generator.Random();
        @endcode

        @sa Random::Generator
    */
    struct MersenneTwister : public Generator
    {
    private:
        /** Some constant for this generator */
        enum 
        {
            Size        = 624,
            Offset      = 397,
            Constant1   = 0x9D2C5680,
            Constant2   = 0xEFC60000,
            Constant3   = 0x9908B0DF,
            Constant4   = 0x80000000,
            Constant5   = 0x7FFFFFFF,
            Generator   = 0x6C078965,
            DefaultSeed = 0x12BD6AA,
            GenSeed1    = 0x19660D,
            GenSeed2    = 0x5D588B65,
        };
        /** Is the generator seeded */
        bool    isSeeded;
        /** The generator index */
        uint32  index;
        /** The generator state */
        uint32  state[Size];

        // Helper function
    private:
        /** Twiddle the given values */
        inline uint32 twiddle(uint32 u, uint32 v)
        {
            return (((u & Constant4) | (v & Constant5)) >> 1) ^ ((v & 1) ? Constant3 : 0);
        }
        
        // Interface
    public:
        /** Initialize with a seed */
        void initSeed(uint32 seed)
        {
            state[0] = seed;
            for (int j = 1; j < Size; j++)
                state[j] = (Generator * (state[j-1] ^ (state[j - 1] >> 30)) + j);
            index = Size;
            isSeeded = true;
        }

        /** Initialize the generator */
        virtual void Init(uint8 * pool = 0, uint32 size = 0)
        {
            initSeed(DefaultSeed);
            uint32 * array = (uint32*)pool;
            uint32 arraySize = size / sizeof(array[0]);
            uint8 entropyBucket[16];
            if (!pool || size < 16)
            {
                collectEntropy(entropyBucket, ArrSz(entropyBucket));
                array = (uint32*)entropyBucket;
                arraySize = ArrSz(entropyBucket) / sizeof(array[0]);
            }
            
            int i = 1, j = 0;
            for (int k = max(arraySize, (uint32)Size); k; --k)
            {
                state[i] = (state[i] ^ ((state[i - 1] ^ (state[i - 1] >> 30)) * GenSeed1)) + array[j] + j++;
                j %= arraySize;
                if (++i == Size) { state[0] = state[Size - 1]; i = 1; }
            }
            for (int k = Size - 1; k; --k)
            {
                state[i] = (state[i] ^ ((state[i - 1] ^ (state[i - 1] >> 30)) * GenSeed2)) - i;
                if ((++i) == Size) { state[0] = state[Size - 1]; i = 1; }
            }
            state[0] = Constant4;
            index = Size;
        }

        virtual bool collectEntropy(uint8 * arrayToStoreEntropyTo, uint32 size)
        {
            uint32 InitialSize = size;
            uint8 EntropyBucket[16] = {0};
            *(clock_t*)&EntropyBucket[0] = clock();
            *(time_t*)&EntropyBucket[4] = time(NULL) - (time_t)clock();
#ifdef _WIN32
            ::Sleep(0);
#else
            sched_yield();
#endif
            *(time_t*)&EntropyBucket[8] = time(NULL);
            *(clock_t*)&EntropyBucket[12] = clock();
            
            // Try to gather entropy from some real source
#if defined(_WIN32)
            uint64 driveSpace = Hardware::Scanner::getFreeDriveSpace(File::General::getSpecialPath(File::General::Temporary));
            uint64 freeRam = Hardware::Scanner::getFreeRAMSizePrecise();
            uint64 uptime = Hardware::Scanner::getUptime();
            uint64 * arr = (uint64*)EntropyBucket;
            arr[0] ^= driveSpace;
            arr[1] ^= freeRam;
            arr[0] ^= uptime << 8;
            arr[1] ^= (uptime & 0xFFFF00) >> 8;
#elif defined(_POSIX)
            FILE * rnd = fopen("/dev/random", "r");
            if (rnd)
            {
                fread(EntropyBucket, 1, 16, rnd);
                fclose(rnd);
            }
#endif
                        

            // This is lame code here
            for (; size > 16; size -= 16)
                memcpy(&arrayToStoreEntropyTo[size - 16], EntropyBucket, 16);

            memcpy(arrayToStoreEntropyTo, EntropyBucket, size);
            // Now try to hash the state so it is better presented
            for (size = 1; size < InitialSize; size ++)
            {
                arrayToStoreEntropyTo[size] += (uint8)(((uint32)arrayToStoreEntropyTo[size - 1] * 31) % 256);
            }

            return true; 
        }

        /** Get a 32 bits random number */
        virtual uint32 Random()
        {
            // If it's not seeded, seed it
            if (!isSeeded) Init();

            if ( index >= Size )
            {
                for (int i = 0; i < Size - Offset; ++i)
                    state[i] = state[i + Offset] ^ twiddle(state[i], state[i + 1]);
                
                for (int i = Size - Offset; i < (Size - 1); ++i)
                    state[i] = state[i + Offset - Size] ^ twiddle(state[i], state[i + 1]);
                state[Size - 1] = state[Offset - 1] ^ twiddle(state[Size - 1], state[0]);
                index = 0; // reset position
            }

            uint32 tmp = state[index++];
            tmp ^= (tmp >> 11);
            tmp ^= (tmp << 7) & Constant1;
            tmp ^= (tmp << 15) & Constant2;
            return tmp ^ (tmp >> 18);            
        }

    public:
        MersenneTwister(uint8 * pool = 0, uint32 size = 0) : isSeeded(false)
        {
            Init(pool, size);
        }
    };

    Generator & getDefaultGenerator()
    {
        static MersenneTwister xMT;
        return xMT;
    }

    uint32 numberBetween(const uint32 lowest, const uint32 highest)
    {
        Generator & gp = getDefaultGenerator();
        uint32 d, range;
        unsigned char *bp;
        int i, nbits;
        uint32 mask;

        if ( highest <= lowest ) 
            return lowest;

        range = highest - lowest;

        do
        {
            bp = (unsigned char *)&d;
            for (i = 0; i < sizeof(uint32); i++)
            {
                bp[i] = uint8(gp.Random() & 0xFF);
            }


            mask = 0x80000000;
            for (nbits = sizeof(uint32)*8; nbits > 0; nbits--, mask >>= 1)
            {
                if (range & mask)
                    break;
            }
            if (nbits < sizeof(uint32)*8)
            {
                mask <<= 1;
                mask--;
            }
            else
                mask = 0xFFFFFFFF;

            d &= mask;

        } while (d > range); 

        return (lowest + d);        
    }
    
    // Fill the given block of data with random bytes 
    void fillBlock(uint8 * buffer, size_t size, const bool reSeed)
    {
        if (!buffer || !size) return;
        if (reSeed) getDefaultGenerator().Init();
        
        size_t units = size / sizeof(uint32);
        for (size_t i = 0; i < units; i++)
            ((uint32*)buffer)[i] = getDefaultGenerator().Random();
        
        uint32 lastValue = getDefaultGenerator().Random();
        for (size_t i = units * sizeof(uint32); i < size; i++)
            buffer[i] = ((uint8*)&lastValue)[i - units];
    }
}
