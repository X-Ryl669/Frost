#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>

#include "EBSC.hpp"

// We need platform code too for large malloc & calloc & free
#include "../../include/Platform/Platform.hpp"

namespace BSC
{
    // Error or size
    class EOS
    {
        int output;
        
    public:
        Error getError() const { return output <= 0 ? (Error)output : Success; }
        uint32 value() const { return output >= 0 ? (uint32)output : 0; }
        
        
        EOS(int value) : output(value) {}
        EOS(Error error) : output((int)error) {}
    };

    /**
    * Calculates Adler-32 checksum for input memory block.
    * @param T          - the input memory block of n bytes.
    * @param n          - the length of the input memory block.
    * @param features   - the set of additional features.
    * @return the value of cyclic redundancy check.
    */
//    unsigned int bsc_adler32(const uint8 * T, int n, int features);

    /**
    * Constructs the burrows wheeler transformed string of a given string.
    * @param T              - the input/output string of n chars.
    * @param n              - the length of the given string.
    * @param num_indexes    - the length of secondary indexes array, can be NULL.
    * @param indexes        - the secondary indexes array, can be NULL.
    * @param features       - the set of additional features.
    * @return the primary index if no error occurred, error code otherwise.
    */
    EOS BWTEncode(uint8 * T, int n, uint8 * num_indexes, int * indexes, int features);

    /**
    * Reconstructs the original string from burrows wheeler transformed string.
    * @param T              - the input/output string of n chars.
    * @param n              - the length of the given string.
    * @param index          - the primary index.
    * @param num_indexes    - the length of secondary indexes array, can be 0.
    * @param indexes        - the secondary indexes array, can be NULL.
    * @param features       - the set of additional features.
    * @return LIBBSC_NO_ERROR if no error occurred, error code otherwise.
    */
    EOS BWTDecode(uint8 * T, int n, int index, uint8 num_indexes, int * indexes, int features);

/** The log2 table for a byte */
extern const char log2Table[256];

/**
 * Constructs the suffix array of a given string.
 * @param T[0..n-1] The input string.
 * @param SA[0..n-1] The output array of suffixes.
 * @param n The length of the given string.
 * @param openMP enables OpenMP optimization.
 * @return 0 if no error occurred, -1 or -2 otherwise.
 */
int
divsufsort(const uint8 *T, int *SA, int n, int openMP);


/**
 * Constructs the burrows-wheeler transformed string of a given string.
 * @param T[0..n-1] The input string.
 * @param U[0..n-1] The output string. (can be T)
 * @param A[0..n-1] The temporary array. (can be NULL)
 * @param n The length of the given string.
 * @param num_indexes The length of secondary indexes array. (can be NULL)
 * @param indexes The secondary indexes array. (can be NULL)
 * @param openMP enables OpenMP optimization.
 * @return The primary index if no error occurred, -1 or -2 otherwise.
 */
int
divbwt(const uint8 *T, uint8 *U, int *A, int n, uint8 * num_indexes, int * indexes, int openMP);


    /**
    * Compress a memory block using Quantized Local Frequency Coding.
    * @param input      - the input memory block of n bytes.
    * @param output     - the output memory block of n bytes.
    * @param n          - the length of the input memory block.
    * @param coder      - the entropy coding algorithm.
    * @param features   - the set of additional features.
    * @return the length of compressed memory block if no error occurred, error code otherwise.
    */
    EOS coder_compress(const uint8 * input, uint8 * output, int n, Coder coder, int features);

    /**
    * Decompress a memory block using Quantized Local Frequency Coding.
    * @param input      - the input memory block.
    * @param output     - the output memory block.
    * @param coder      - the entropy coding algorithm.
    * @param features   - the set of additional features.
    * @return the length of decompressed memory block if no error occurred, error code otherwise.
    */
    EOS bsc_coder_decompress(const uint8 * input, uint8 * output, Coder coder, int features);










    /**
    * Compress a memory block using Quantized Local Frequency Coding algorithm.
    * @param input      - the input memory block of n bytes.
    * @param output     - the output memory block of n bytes.
    * @param inputSize  - the length of the input memory block.
    * @param outputSize - the length of the output memory block.
    * @return the length of compressed memory block if no error occurred, error code otherwise.
    */
    EOS qlfc_static_encode_block(const uint8 * input, uint8 * output, int inputSize, int outputSize);

    /**
    * Decompress a memory block using Quantized Local Frequency Coding algorithm.
    * @param input      - the input memory block of n bytes.
    * @param output     - the output memory block of n bytes.
    * @param inputSize  - the length of the input memory block.
    * @param outputSize - the length of the output memory block.
    * @return the length of decompressed memory block if no error occurred, error code otherwise.
    */
    EOS qlfc_adaptive_encode_block(const uint8 * input, uint8 * output, int inputSize, int outputSize);

    /**
    * Compress a memory block using Quantized Local Frequency Coding algorithm.
    * @param input      - the input memory block of n bytes.
    * @param output     - the output memory block of n bytes.
    * @return the length of compressed memory block if no error occurred, error code otherwise.
    */
    EOS qlfc_static_decode_block(const uint8 * input, uint8 * output);

    /**
    * Decompress a memory block using Quantized Local Frequency Coding algorithm.
    * @param input      - the input memory block of n bytes.
    * @param output     - the output memory block of n bytes.
    * @return the length of decompressed memory block if no error occurred, error code otherwise.
    */
    EOS qlfc_adaptive_decode_block(const uint8 * input, uint8 * output);






    /**
    * Autodetects segments for better compression of heterogeneous files.
    * @param input      - the input memory block of n bytes.
    * @param n          - the length of the input memory block.
    * @param segments   - the output array of segments of k elements size.
    * @param k          - the size of the output segments array.
    * @param features   - the set of additional features.
    * @return The number of segments if no error occurred, error code otherwise.
    */
    EOS detect_segments(const uint8 * input, int n, int * segments, int k, int features);

    /**
    * Autodetects order of contexts for better compression of binary files.
    * @param input      - the input memory block of n bytes.
    * @param n          - the length of the input memory block.
    * @param features   - the set of additional features.
    * @return The detected contexts order if no error occurred, error code otherwise.
    */
    EOS detect_contextsorder(const uint8 * input, int n, int features);

    /**
    * Reverses memory block to change order of contexts.
    * @param T          - the input/output memory block of n bytes.
    * @param n          - the length of the memory block.
    * @param features   - the set of additional features.
    * @return LIBBSC_NO_ERROR if no error occurred, error code otherwise.
    */
    EOS reverse_block(uint8 * T, int n, int features);

    /**
    * Autodetects record size for better compression of multimedia files.
    * @param input      - the input memory block of n bytes.
    * @param n          - the length of the input memory block.
    * @param features   - the set of additional features.
    * @return The size of record if no error occurred, error code otherwise.
    */
    EOS detect_recordsize(const uint8 * input, int n, int features);

    /**
    * Reorders memory block for specific size of record (Forward transform).
    * @param T          - the input/output memory block of n bytes.
    * @param n          - the length of the memory block.
    * @param recordSize - the size of record.
    * @param features   - the set of additional features.
    * @return LIBBSC_NO_ERROR if no error occurred, error code otherwise.
    */
    EOS reorder_forward(uint8 * T, int n, int recordSize, int features);

    /**
    * Reorders memory block for specific size of record (Reverse transform).
    * @param T          - the input/output memory block of n bytes.
    * @param n          - the length of the memory block.
    * @param recordSize - the size of record.
    * @param features   - the set of additional features.
    * @return LIBBSC_NO_ERROR if no error occurred, error code otherwise.
    */
    EOS reorder_reverse(uint8 * T, int n, int recordSize, int features);


    /**
    * Preprocess a memory block by LZP algorithm.
    * @param input      - the input memory block of n bytes.
    * @param output     - the output memory block of n bytes.
    * @param n          - the length of the input/output memory blocks.
    * @param hashSize   - the hash table size.
    * @param minLen     - the minimum match length.
    * @param features   - the set of additional features.
    * @return The length of preprocessed memory block if no error occurred, error code otherwise.
    */
    EOS lzp_compress(const uint8 * input, uint8 * output, int n, int hashSize, int minLen, int features);

    /**
    * Reconstructs the original memory block after LZP algorithm.
    * @param input      - the input memory block of n bytes.
    * @param output     - the output memory block.
    * @param n          - the length of the input memory block.
    * @param hashSize   - the hash table size.
    * @param minLen     - the minimum match length.
    * @param features   - the set of additional features.
    * @return The length of original memory block if no error occurred, error code otherwise.
    */
    EOS lzp_decompress(const uint8 * input, uint8 * output, int n, int hashSize, int minLen, int features);
}

