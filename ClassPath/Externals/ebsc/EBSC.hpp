/*--

This file is a part of bsc and/or libbsc, a program and a library for
lossless, block-sorting data compression.

   Copyright (c) 2009-2012 Ilya Grebnov <ilya.grebnov@gmail.com>

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

Please see the file LICENSE for full copyright information and file AUTHORS
for full list of contributors.

See also the bsc and libbsc web site:
  http://libbsc.com/ for more information.

The code was modified by Cyril RUSSO to embed in ClassPath

--*/

#ifndef __cplusplus
#error This can only be compiled in C++ project
#endif


#ifndef _ELIBBSC_ELIBBSC_H
#define _ELIBBSC_ELIBBSC_H

// We need types
#include "../../include/Types.hpp"


/** Interface to the embedded BSC compression and decompression routines */
namespace BSC
{
    /** Standard errors */
    enum Error
    {
        Success         =   0, //<! No error happened
        BadParameter    =  -1, //<! The function was called with a bad parameter
        NotEnoughMemory =  -2, //<! Not enough memory for operation
        NotCompressible =  -3, //<! The data is not compressible
        NotSupported    =  -4, //<! Not supported
        UnexpectedEOB   =  -5, //<! Unexpected end of block
        DataCorrupt     =  -6, //<! The data is corrupt
    };

    /** The block sorter used */
    enum BlockSorter
    {
        NoBlockSorter   = 0, //!< No block sorter used
        BWT             = 1, //!< Burrows Wheeler transformed block
    };

    /** The coded used */
    enum Coder
    {
        NoCoder        = 0, //!< No coder used
        QLFCStatic     = 1, //!< The QLFC static coder
        QLFCAdaptive   = 2, //!< The QLFC adaptive mode
    };

    /** The features used (these are flags) */
    enum Feature
    {
        NoFeature      = 0, //!< No feature used
        FastMode       = 1, //!< Fast mode used
        Multithreading = 2, //!< Multithreading was used
        LargePages     = 4, //!< Large pages were used
    };

    /** The default Lempel-Ziv hash size */
    const int defaultLZPHashSize = 16;
    /** The default Lempel-Ziv minimum length */
    const int defaultLZPMinLen = 128;
    /** The default block sorter */
    const BlockSorter defaultBlockSorter = BWT;
    /** The default coder used */
    const Coder defaultCoder = QLFCStatic;
    /** The default features used */
    const int defaultFeatures = (int)FastMode | (int)Multithreading;

    /** The BSC header size */
    const size_t HeaderSize = 28;

    /** The compression function for BSC */
    class EBSC
    {
        // Members
    private:
        /** The features to use */
        int features;
    
        // Helpers
    private:
         int compressInPlace(uint8 * input, const size_t n,
                      const int lzpHashSize = defaultLZPHashSize, const int lzpMinLen = defaultLZPMinLen, const BlockSorter blockSorter = defaultBlockSorter,
                      const Coder coder = defaultCoder);
         Error decompressInPlace(uint8 * input, const size_t inputSize, const size_t outputSize);
    
    
        // Interface
    public:
        /** Compress a memory block.
            @param input                              The input memory block of n bytes.
            @param output                             The output memory block of n + HeaderSize bytes.
            @param n                                  The length of the input memory block in bytes.
            @param lzpHashSize[0, 10..28]             The hash table size if LZP enabled, 0 otherwise.
            @param lzpMinLen[0, 4..255]               The minimum match length if LZP enabled, 0 otherwise.
            @param blockSorter[ST3..ST8, BWT]         The block sorting algorithm.
            @param coder[MTF or QLFC]                 The entropy coding algorithm.
            @return the length of compressed memory block if no error occurred, error code otherwise. */
        int compress(const uint8 * input, uint8 * output, const size_t n,
                     const int lzpHashSize = defaultLZPHashSize, const int lzpMinLen = defaultLZPMinLen, const BlockSorter blockSorter = defaultBlockSorter,
                     const Coder coder = defaultCoder);

        /** Store a memory block.
            @param input                              The input memory block of n bytes.
            @param output                             The output memory block of n + ELIBBSC_HEADER_SIZE bytes.
            @param n                                  The length of the input memory block.
            @return the length of stored memory block if no error occurred, error code otherwise. */
        int store(const uint8 * input, uint8 * output, const size_t n);

        /** Determinate the sizes of input and output memory blocks for decompress function.
            @param blockHeader                        The header of input(compressed) memory block of headerSize bytes.
            @param headerSize                         The length of header, should be at least HeaderSize bytes.
            @param blockSize[out]                     The length of the input memory block for decompress function.
            @param dataSize[out]                      The length of the output memory block for decompress function. */
        Error getBlockInfo(const uint8 * blockHeader, const size_t headerSize, size_t & blockSize, size_t & dataSize);

        /** Decompress a memory block.
            @note You should call getBlockInfo function to determinate the sizes of input and output memory blocks.
            @param input                              The input memory block of inputSize bytes.
            @param inputSize                          The length of the input memory block.
            @param output                             The output memory block of outputSize bytes.
            @param outputSize                         The length of the output memory block. */
       Error decompress(const uint8 * input, const size_t inputSize, uint8 * output, const size_t outputSize);

        /** Post process a decompressed block 
            @param input                              The input/output memory block of inputSize bytes.
            @param inputSize                          The length of the input/output memory block.
            @param sortingContext                     The sorting context used
            @param recordSize                         The record size used */
        Error postProcess(uint8 * input, const size_t inputSize, const int8 sortingContext, const int8 recordSize);

        // Construction and destruction
    public:
        /** Set the features to use for compression and decompression
            @param features     The set of additional features.
            If the specified features make unlogical/impossible combination, later call to the methods will return NotSupported error */
        EBSC(int features = defaultFeatures) : features(features) {}
    };

}

#endif
