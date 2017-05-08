/**
 * Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */


/*-************************************
*  Compiler specific
**************************************/
#ifdef _MSC_VER    /* Visual Studio */
#  define _CRT_SECURE_NO_WARNINGS     /* fgets */
#  pragma warning(disable : 4127)     /* disable: C4127: conditional expression is constant */
#  pragma warning(disable : 4146)     /* disable: C4146: minus unsigned expression */
#endif


/*-************************************
*  Includes
**************************************/
#include <stdlib.h>       /* free */
#include <stdio.h>        /* fgets, sscanf */
#include <time.h>         /* clock_t, clock() */
#include <string.h>       /* strcmp */
#include "mem.h"
#define ZSTD_STATIC_LINKING_ONLY   /* ZSTD_maxCLevel, ZSTD_customMem, ZSTD_getDictID_fromFrame */
#include "zstd.h"         /* ZSTD_compressBound */
#include "zstd_errors.h"  /* ZSTD_error_srcSize_wrong */
#include "zstdmt_compress.h"
#include "zdict.h"        /* ZDICT_trainFromBuffer */
#include "datagen.h"      /* RDG_genBuffer */
#define XXH_STATIC_LINKING_ONLY   /* XXH64_state_t */
#include "xxhash.h"       /* XXH64_* */


/*-************************************
*  Constants
**************************************/
#define KB *(1U<<10)
#define MB *(1U<<20)
#define GB *(1U<<30)

static const U32 nbTestsDefault = 10000;
#define COMPRESSIBLE_NOISE_LENGTH (10 MB)
#define FUZ_COMPRESSIBILITY_DEFAULT 50
static const U32 prime32 = 2654435761U;


/*-************************************
*  Display Macros
**************************************/
#define DISPLAY(...)          fprintf(stderr, __VA_ARGS__)
#define DISPLAYLEVEL(l, ...)  if (g_displayLevel>=l) { DISPLAY(__VA_ARGS__); }
static U32 g_displayLevel = 2;

#define DISPLAYUPDATE(l, ...) if (g_displayLevel>=l) { \
            if ((FUZ_GetClockSpan(g_displayClock) > g_refreshRate) || (g_displayLevel>=4)) \
            { g_displayClock = clock(); DISPLAY(__VA_ARGS__); \
            if (g_displayLevel>=4) fflush(stderr); } }
static const clock_t g_refreshRate = CLOCKS_PER_SEC / 6;
static clock_t g_displayClock = 0;

static clock_t g_clockTime = 0;


/*-*******************************************************
*  Fuzzer functions
*********************************************************/
#define MAX(a,b) ((a)>(b)?(a):(b))

static clock_t FUZ_GetClockSpan(clock_t clockStart)
{
    return clock() - clockStart;  /* works even when overflow. Max span ~ 30 mn */
}

/*! FUZ_rand() :
    @return : a 27 bits random value, from a 32-bits `seed`.
    `seed` is also modified */
#define FUZ_rotl32(x,r) ((x << r) | (x >> (32 - r)))
unsigned int FUZ_rand(unsigned int* seedPtr)
{
    static const U32 prime2 = 2246822519U;
    U32 rand32 = *seedPtr;
    rand32 *= prime32;
    rand32 += prime2;
    rand32  = FUZ_rotl32(rand32, 13);
    *seedPtr = rand32;
    return rand32 >> 5;
}

static void* allocFunction(void* opaque, size_t size)
{
    void* address = malloc(size);
    (void)opaque;
    return address;
}

static void freeFunction(void* opaque, void* address)
{
    (void)opaque;
    free(address);
}


/*======================================================
*   Basic Unit tests
======================================================*/

typedef struct {
    void* start;
    size_t size;
    size_t filled;
} buffer_t;

static const buffer_t g_nullBuffer = { NULL, 0 , 0 };

static buffer_t FUZ_createDictionary(const void* src, size_t srcSize, size_t blockSize, size_t requestedDictSize)
{
    buffer_t dict = { NULL, 0, 0 };
    size_t const nbBlocks = (srcSize + (blockSize-1)) / blockSize;
    size_t* const blockSizes = (size_t*) malloc(nbBlocks * sizeof(size_t));
    if (!blockSizes) return dict;
    dict.start = malloc(requestedDictSize);
    if (!dict.start) { free(blockSizes); return dict; }
    {   size_t nb;
        for (nb=0; nb<nbBlocks-1; nb++) blockSizes[nb] = blockSize;
        blockSizes[nbBlocks-1] = srcSize - (blockSize * (nbBlocks-1));
    }
    {   size_t const dictSize = ZDICT_trainFromBuffer(dict.start, requestedDictSize, src, blockSizes, (unsigned)nbBlocks);
        free(blockSizes);
        if (ZDICT_isError(dictSize)) { free(dict.start); return g_nullBuffer; }
        dict.size = requestedDictSize;
        dict.filled = dictSize;
        return dict;   /* how to return dictSize ? */
    }
}

static void FUZ_freeDictionary(buffer_t dict)
{
    free(dict.start);
}


static int basicUnitTests(U32 seed, double compressibility, ZSTD_customMem customMem)
{
    size_t const CNBufferSize = COMPRESSIBLE_NOISE_LENGTH;
    void* CNBuffer = malloc(CNBufferSize);
    size_t const skippableFrameSize = 11;
    size_t const compressedBufferSize = (8 + skippableFrameSize) + ZSTD_compressBound(COMPRESSIBLE_NOISE_LENGTH);
    void* compressedBuffer = malloc(compressedBufferSize);
    size_t const decodedBufferSize = CNBufferSize;
    void* decodedBuffer = malloc(decodedBufferSize);
    size_t cSize;
    int testResult = 0;
    U32 testNb=0;
    ZSTD_CStream* zc = ZSTD_createCStream_advanced(customMem);
    ZSTD_DStream* zd = ZSTD_createDStream_advanced(customMem);
    ZSTD_inBuffer  inBuff, inBuff2;
    ZSTD_outBuffer outBuff;
    buffer_t dictionary = g_nullBuffer;
    unsigned dictID = 0;

    /* Create compressible test buffer */
    if (!CNBuffer || !compressedBuffer || !decodedBuffer || !zc || !zd) {
        DISPLAY("Not enough memory, aborting \n");
        goto _output_error;
    }
    RDG_genBuffer(CNBuffer, CNBufferSize, compressibility, 0., seed);

    /* Create dictionary */
    MEM_STATIC_ASSERT(COMPRESSIBLE_NOISE_LENGTH >= 4 MB);
    dictionary = FUZ_createDictionary(CNBuffer, 4 MB, 4 KB, 40 KB);
    if (!dictionary.start) {
        DISPLAY("Error creating dictionary, aborting \n");
        goto _output_error;
    }
    dictID = ZDICT_getDictID(dictionary.start, dictionary.filled);

    /* generate skippable frame */
    MEM_writeLE32(compressedBuffer, ZSTD_MAGIC_SKIPPABLE_START);
    MEM_writeLE32(((char*)compressedBuffer)+4, (U32)skippableFrameSize);
    cSize = skippableFrameSize + 8;

    /* Basic compression test */
    DISPLAYLEVEL(3, "test%3i : compress %u bytes : ", testNb++, COMPRESSIBLE_NOISE_LENGTH);
    ZSTD_initCStream_usingDict(zc, CNBuffer, 128 KB, 1);
    outBuff.dst = (char*)(compressedBuffer)+cSize;
    outBuff.size = compressedBufferSize;
    outBuff.pos = 0;
    inBuff.src = CNBuffer;
    inBuff.size = CNBufferSize;
    inBuff.pos = 0;
    { size_t const r = ZSTD_compressStream(zc, &outBuff, &inBuff);
      if (ZSTD_isError(r)) goto _output_error; }
    if (inBuff.pos != inBuff.size) goto _output_error;   /* entire input should be consumed */
    { size_t const r = ZSTD_endStream(zc, &outBuff);
      if (r != 0) goto _output_error; }  /* error, or some data not flushed */
    cSize += outBuff.pos;
    DISPLAYLEVEL(3, "OK (%u bytes : %.2f%%)\n", (U32)cSize, (double)cSize/COMPRESSIBLE_NOISE_LENGTH*100);

    DISPLAYLEVEL(3, "test%3i : check CStream size : ", testNb++);
    { size_t const s = ZSTD_sizeof_CStream(zc);
      if (ZSTD_isError(s)) goto _output_error;
      DISPLAYLEVEL(3, "OK (%u bytes) \n", (U32)s);
    }

    /* Attempt bad compression parameters */
    DISPLAYLEVEL(3, "test%3i : use bad compression parameters : ", testNb++);
    {   size_t r;
        ZSTD_parameters params = ZSTD_getParams(1, 0, 0);
        params.cParams.searchLength = 2;
        r = ZSTD_initCStream_advanced(zc, NULL, 0, params, 0);
        if (!ZSTD_isError(r)) goto _output_error;
        DISPLAYLEVEL(3, "init error : %s \n", ZSTD_getErrorName(r));
    }

    /* skippable frame test */
    DISPLAYLEVEL(3, "test%3i : decompress skippable frame : ", testNb++);
    ZSTD_initDStream_usingDict(zd, CNBuffer, 128 KB);
    inBuff.src = compressedBuffer;
    inBuff.size = cSize;
    inBuff.pos = 0;
    outBuff.dst = decodedBuffer;
    outBuff.size = CNBufferSize;
    outBuff.pos = 0;
    { size_t const r = ZSTD_decompressStream(zd, &outBuff, &inBuff);
      if (r != 0) goto _output_error; }
    if (outBuff.pos != 0) goto _output_error;   /* skippable frame output len is 0 */
    DISPLAYLEVEL(3, "OK \n");

    /* Basic decompression test */
    inBuff2 = inBuff;
    DISPLAYLEVEL(3, "test%3i : decompress %u bytes : ", testNb++, COMPRESSIBLE_NOISE_LENGTH);
    ZSTD_initDStream_usingDict(zd, CNBuffer, 128 KB);
    { size_t const r = ZSTD_setDStreamParameter(zd, DStream_p_maxWindowSize, 1000000000);  /* large limit */
      if (ZSTD_isError(r)) goto _output_error; }
    { size_t const remaining = ZSTD_decompressStream(zd, &outBuff, &inBuff);
      if (remaining != 0) goto _output_error; }  /* should reach end of frame == 0; otherwise, some data left, or an error */
    if (outBuff.pos != CNBufferSize) goto _output_error;   /* should regenerate the same amount */
    if (inBuff.pos != inBuff.size) goto _output_error;   /* should have read the entire frame */
    DISPLAYLEVEL(3, "OK \n");

    /* Re-use without init */
    DISPLAYLEVEL(3, "test%3i : decompress again without init (re-use previous settings): ", testNb++);
    outBuff.pos = 0;
    { size_t const remaining = ZSTD_decompressStream(zd, &outBuff, &inBuff2);
      if (remaining != 0) goto _output_error; }  /* should reach end of frame == 0; otherwise, some data left, or an error */
    if (outBuff.pos != CNBufferSize) goto _output_error;   /* should regenerate the same amount */
    if (inBuff.pos != inBuff.size) goto _output_error;   /* should have read the entire frame */
    DISPLAYLEVEL(3, "OK \n");

    /* check regenerated data is byte exact */
    DISPLAYLEVEL(3, "test%3i : check decompressed result : ", testNb++);
    {   size_t i;
        for (i=0; i<CNBufferSize; i++) {
            if (((BYTE*)decodedBuffer)[i] != ((BYTE*)CNBuffer)[i]) goto _output_error;
    }   }
    DISPLAYLEVEL(3, "OK \n");

    DISPLAYLEVEL(3, "test%3i : check DStream size : ", testNb++);
    { size_t const s = ZSTD_sizeof_DStream(zd);
      if (ZSTD_isError(s)) goto _output_error;
      DISPLAYLEVEL(3, "OK (%u bytes) \n", (U32)s);
    }

    /* Byte-by-byte decompression test */
    DISPLAYLEVEL(3, "test%3i : decompress byte-by-byte : ", testNb++);
    {   /* skippable frame */
        size_t r = 1;
        ZSTD_initDStream_usingDict(zd, CNBuffer, 128 KB);
        inBuff.src = compressedBuffer;
        outBuff.dst = decodedBuffer;
        inBuff.pos = 0;
        outBuff.pos = 0;
        while (r) {   /* skippable frame */
            inBuff.size = inBuff.pos + 1;
            outBuff.size = outBuff.pos + 1;
            r = ZSTD_decompressStream(zd, &outBuff, &inBuff);
            if (ZSTD_isError(r)) goto _output_error;
        }
        /* normal frame */
        ZSTD_initDStream_usingDict(zd, CNBuffer, 128 KB);
        r=1;
        while (r) {
            inBuff.size = inBuff.pos + 1;
            outBuff.size = outBuff.pos + 1;
            r = ZSTD_decompressStream(zd, &outBuff, &inBuff);
            if (ZSTD_isError(r)) goto _output_error;
        }
    }
    if (outBuff.pos != CNBufferSize) goto _output_error;   /* should regenerate the same amount */
    if (inBuff.pos != cSize) goto _output_error;   /* should have read the entire frame */
    DISPLAYLEVEL(3, "OK \n");

    /* check regenerated data is byte exact */
    DISPLAYLEVEL(3, "test%3i : check decompressed result : ", testNb++);
    {   size_t i;
        for (i=0; i<CNBufferSize; i++) {
            if (((BYTE*)decodedBuffer)[i] != ((BYTE*)CNBuffer)[i]) goto _output_error;;
    }   }
    DISPLAYLEVEL(3, "OK \n");

    /* _srcSize compression test */
    DISPLAYLEVEL(3, "test%3i : compress_srcSize %u bytes : ", testNb++, COMPRESSIBLE_NOISE_LENGTH);
    ZSTD_initCStream_srcSize(zc, 1, CNBufferSize);
    outBuff.dst = (char*)(compressedBuffer);
    outBuff.size = compressedBufferSize;
    outBuff.pos = 0;
    inBuff.src = CNBuffer;
    inBuff.size = CNBufferSize;
    inBuff.pos = 0;
    { size_t const r = ZSTD_compressStream(zc, &outBuff, &inBuff);
      if (ZSTD_isError(r)) goto _output_error; }
    if (inBuff.pos != inBuff.size) goto _output_error;   /* entire input should be consumed */
    { size_t const r = ZSTD_endStream(zc, &outBuff);
      if (r != 0) goto _output_error; }  /* error, or some data not flushed */
    { unsigned long long origSize = ZSTD_findDecompressedSize(outBuff.dst, outBuff.pos);
      if ((size_t)origSize != CNBufferSize) goto _output_error; }  /* exact original size must be present */
    DISPLAYLEVEL(3, "OK (%u bytes : %.2f%%)\n", (U32)cSize, (double)cSize/COMPRESSIBLE_NOISE_LENGTH*100);

    /* wrong _srcSize compression test */
    DISPLAYLEVEL(3, "test%3i : wrong srcSize : %u bytes : ", testNb++, COMPRESSIBLE_NOISE_LENGTH-1);
    ZSTD_initCStream_srcSize(zc, 1, CNBufferSize-1);
    outBuff.dst = (char*)(compressedBuffer);
    outBuff.size = compressedBufferSize;
    outBuff.pos = 0;
    inBuff.src = CNBuffer;
    inBuff.size = CNBufferSize;
    inBuff.pos = 0;
    { size_t const r = ZSTD_compressStream(zc, &outBuff, &inBuff);
      if (ZSTD_isError(r)) goto _output_error; }
    if (inBuff.pos != inBuff.size) goto _output_error;   /* entire input should be consumed */
    { size_t const r = ZSTD_endStream(zc, &outBuff);
      if (ZSTD_getErrorCode(r) != ZSTD_error_srcSize_wrong) goto _output_error;    /* must fail : wrong srcSize */
      DISPLAYLEVEL(3, "OK (error detected : %s) \n", ZSTD_getErrorName(r)); }

    /* Complex context re-use scenario */
    DISPLAYLEVEL(3, "test%3i : context re-use : ", testNb++);
    ZSTD_freeCStream(zc);
    zc = ZSTD_createCStream_advanced(customMem);
    if (zc==NULL) goto _output_error;   /* memory allocation issue */
    /* use 1 */
    {   size_t const inSize = 513;
        ZSTD_initCStream_advanced(zc, NULL, 0, ZSTD_getParams(19, inSize, 0), inSize);   /* needs btopt + search3 to trigger hashLog3 */
        inBuff.src = CNBuffer;
        inBuff.size = inSize;
        inBuff.pos = 0;
        outBuff.dst = (char*)(compressedBuffer)+cSize;
        outBuff.size = ZSTD_compressBound(inSize);
        outBuff.pos = 0;
        { size_t const r = ZSTD_compressStream(zc, &outBuff, &inBuff);
            if (ZSTD_isError(r)) goto _output_error; }
        if (inBuff.pos != inBuff.size) goto _output_error;   /* entire input should be consumed */
        { size_t const r = ZSTD_endStream(zc, &outBuff);
            if (r != 0) goto _output_error; }  /* error, or some data not flushed */
    }
    /* use 2 */
    {   size_t const inSize = 1025;   /* will not continue, because tables auto-adjust and are therefore different size */
        ZSTD_initCStream_advanced(zc, NULL, 0, ZSTD_getParams(19, inSize, 0), inSize);   /* needs btopt + search3 to trigger hashLog3 */
        inBuff.src = CNBuffer;
        inBuff.size = inSize;
        inBuff.pos = 0;
        outBuff.dst = (char*)(compressedBuffer)+cSize;
        outBuff.size = ZSTD_compressBound(inSize);
        outBuff.pos = 0;
        { size_t const r = ZSTD_compressStream(zc, &outBuff, &inBuff);
            if (ZSTD_isError(r)) goto _output_error; }
        if (inBuff.pos != inBuff.size) goto _output_error;   /* entire input should be consumed */
        { size_t const r = ZSTD_endStream(zc, &outBuff);
            if (r != 0) goto _output_error; }  /* error, or some data not flushed */
    }
    DISPLAYLEVEL(3, "OK \n");

    /* CDict scenario */
    DISPLAYLEVEL(3, "test%3i : digested dictionary : ", testNb++);
    {   ZSTD_CDict* const cdict = ZSTD_createCDict(dictionary.start, dictionary.filled, 1);
        size_t const initError = ZSTD_initCStream_usingCDict(zc, cdict);
        if (ZSTD_isError(initError)) goto _output_error;
        cSize = 0;
        outBuff.dst = compressedBuffer;
        outBuff.size = compressedBufferSize;
        outBuff.pos = 0;
        inBuff.src = CNBuffer;
        inBuff.size = CNBufferSize;
        inBuff.pos = 0;
        { size_t const r = ZSTD_compressStream(zc, &outBuff, &inBuff);
          if (ZSTD_isError(r)) goto _output_error; }
        if (inBuff.pos != inBuff.size) goto _output_error;   /* entire input should be consumed */
        { size_t const r = ZSTD_endStream(zc, &outBuff);
          if (r != 0) goto _output_error; }  /* error, or some data not flushed */
        cSize = outBuff.pos;
        ZSTD_freeCDict(cdict);
        DISPLAYLEVEL(3, "OK (%u bytes : %.2f%%)\n", (U32)cSize, (double)cSize/CNBufferSize*100);
    }

    DISPLAYLEVEL(3, "test%3i : check CStream size : ", testNb++);
    { size_t const s = ZSTD_sizeof_CStream(zc);
      if (ZSTD_isError(s)) goto _output_error;
      DISPLAYLEVEL(3, "OK (%u bytes) \n", (U32)s);
    }

    DISPLAYLEVEL(4, "test%3i : check Dictionary ID : ", testNb++);
    { unsigned const dID = ZSTD_getDictID_fromFrame(compressedBuffer, cSize);
      if (dID != dictID) goto _output_error;
      DISPLAYLEVEL(4, "OK (%u) \n", dID);
    }

    /* DDict scenario */
    DISPLAYLEVEL(3, "test%3i : decompress %u bytes with digested dictionary : ", testNb++, (U32)CNBufferSize);
    {   ZSTD_DDict* const ddict = ZSTD_createDDict(dictionary.start, dictionary.filled);
        size_t const initError = ZSTD_initDStream_usingDDict(zd, ddict);
        if (ZSTD_isError(initError)) goto _output_error;
        inBuff.src = compressedBuffer;
        inBuff.size = cSize;
        inBuff.pos = 0;
        outBuff.dst = decodedBuffer;
        outBuff.size = CNBufferSize;
        outBuff.pos = 0;
        { size_t const r = ZSTD_decompressStream(zd, &outBuff, &inBuff);
          if (r != 0) goto _output_error; }  /* should reach end of frame == 0; otherwise, some data left, or an error */
        if (outBuff.pos != CNBufferSize) goto _output_error;   /* should regenerate the same amount */
        if (inBuff.pos != inBuff.size) goto _output_error;   /* should have read the entire frame */
        ZSTD_freeDDict(ddict);
        DISPLAYLEVEL(3, "OK \n");
    }

    /* test ZSTD_setDStreamParameter() resilience */
    DISPLAYLEVEL(3, "test%3i : wrong parameter for ZSTD_setDStreamParameter(): ", testNb++);
    { size_t const r = ZSTD_setDStreamParameter(zd, (ZSTD_DStreamParameter_e)999, 1);  /* large limit */
      if (!ZSTD_isError(r)) goto _output_error; }
    DISPLAYLEVEL(3, "OK \n");

    /* Memory restriction */
    DISPLAYLEVEL(3, "test%3i : maxWindowSize < frame requirement : ", testNb++);
    ZSTD_initDStream_usingDict(zd, CNBuffer, 128 KB);
    { size_t const r = ZSTD_setDStreamParameter(zd, DStream_p_maxWindowSize, 1000);  /* too small limit */
      if (ZSTD_isError(r)) goto _output_error; }
    inBuff.src = compressedBuffer;
    inBuff.size = cSize;
    inBuff.pos = 0;
    outBuff.dst = decodedBuffer;
    outBuff.size = CNBufferSize;
    outBuff.pos = 0;
    { size_t const r = ZSTD_decompressStream(zd, &outBuff, &inBuff);
      if (!ZSTD_isError(r)) goto _output_error;  /* must fail : frame requires > 100 bytes */
      DISPLAYLEVEL(3, "OK (%s)\n", ZSTD_getErrorName(r)); }

    DISPLAYLEVEL(3, "test%3i : ZSTD_initCStream_usingCDict_advanced with masked dictID : ", testNb++);
    {   ZSTD_compressionParameters const cParams = ZSTD_getCParams(1, CNBufferSize, dictionary.filled);
        ZSTD_frameParameters const fParams = { 1 /* contentSize */, 1 /* checksum */, 1 /* noDictID */};
        ZSTD_CDict* const cdict = ZSTD_createCDict_advanced(dictionary.start, dictionary.filled, 1 /* byReference */, cParams, customMem);
        size_t const initError = ZSTD_initCStream_usingCDict_advanced(zc, cdict, CNBufferSize, fParams);
        if (ZSTD_isError(initError)) goto _output_error;
        cSize = 0;
        outBuff.dst = compressedBuffer;
        outBuff.size = compressedBufferSize;
        outBuff.pos = 0;
        inBuff.src = CNBuffer;
        inBuff.size = CNBufferSize;
        inBuff.pos = 0;
        { size_t const r = ZSTD_compressStream(zc, &outBuff, &inBuff);
          if (ZSTD_isError(r)) goto _output_error; }
        if (inBuff.pos != inBuff.size) goto _output_error;   /* entire input should be consumed */
        { size_t const r = ZSTD_endStream(zc, &outBuff);
          if (r != 0) goto _output_error; }  /* error, or some data not flushed */
        cSize = outBuff.pos;
        ZSTD_freeCDict(cdict);
        DISPLAYLEVEL(3, "OK (%u bytes : %.2f%%)\n", (U32)cSize, (double)cSize/CNBufferSize*100);
    }

    DISPLAYLEVEL(3, "test%3i : try retrieving dictID from frame : ", testNb++);
    {   U32 const did = ZSTD_getDictID_fromFrame(compressedBuffer, cSize);
        if (did != 0) goto _output_error;
    }
    DISPLAYLEVEL(3, "OK (not detected) \n");

    DISPLAYLEVEL(3, "test%3i : decompress without dictionary : ", testNb++);
    {   size_t const r = ZSTD_decompress(decodedBuffer, CNBufferSize, compressedBuffer, cSize);
        if (!ZSTD_isError(r)) goto _output_error;  /* must fail : dictionary not used */
        DISPLAYLEVEL(3, "OK (%s)\n", ZSTD_getErrorName(r));
    }

    /* Empty srcSize */
    DISPLAYLEVEL(3, "test%3i : ZSTD_initCStream_advanced with pledgedSrcSize=0 and dict : ", testNb++);
    {   ZSTD_parameters params = ZSTD_getParams(5, 0, 0);
        params.fParams.contentSizeFlag = 1;
        ZSTD_initCStream_advanced(zc, dictionary.start, dictionary.filled, params, 0);
    } /* cstream advanced shall write content size = 0 */
    inBuff.src = CNBuffer;
    inBuff.size = 0;
    inBuff.pos = 0;
    outBuff.dst = compressedBuffer;
    outBuff.size = compressedBufferSize;
    outBuff.pos = 0;
    if (ZSTD_isError(ZSTD_compressStream(zc, &outBuff, &inBuff))) goto _output_error;
    if (ZSTD_endStream(zc, &outBuff) != 0) goto _output_error;
    cSize = outBuff.pos;
    if (ZSTD_findDecompressedSize(compressedBuffer, cSize) != 0) goto _output_error;
    DISPLAYLEVEL(3, "OK \n");

    DISPLAYLEVEL(3, "test%3i : pledgedSrcSize == 0 behaves properly : ", testNb++);
    {   ZSTD_parameters params = ZSTD_getParams(5, 0, 0);
        params.fParams.contentSizeFlag = 1;
        ZSTD_initCStream_advanced(zc, NULL, 0, params, 0);
    } /* cstream advanced shall write content size = 0 */
    inBuff.src = CNBuffer;
    inBuff.size = 0;
    inBuff.pos = 0;
    outBuff.dst = compressedBuffer;
    outBuff.size = compressedBufferSize;
    outBuff.pos = 0;
    if (ZSTD_isError(ZSTD_compressStream(zc, &outBuff, &inBuff))) goto _output_error;
    if (ZSTD_endStream(zc, &outBuff) != 0) goto _output_error;
    cSize = outBuff.pos;
    if (ZSTD_findDecompressedSize(compressedBuffer, cSize) != 0) goto _output_error;

    ZSTD_resetCStream(zc, 0); /* resetCStream should treat 0 as unknown */
    inBuff.src = CNBuffer;
    inBuff.size = 0;
    inBuff.pos = 0;
    outBuff.dst = compressedBuffer;
    outBuff.size = compressedBufferSize;
    outBuff.pos = 0;
    if (ZSTD_isError(ZSTD_compressStream(zc, &outBuff, &inBuff))) goto _output_error;
    if (ZSTD_endStream(zc, &outBuff) != 0) goto _output_error;
    cSize = outBuff.pos;
    if (ZSTD_findDecompressedSize(compressedBuffer, cSize) != ZSTD_CONTENTSIZE_UNKNOWN) goto _output_error;
    DISPLAYLEVEL(3, "OK \n");

    /* Overlen overwriting window data bug */
    DISPLAYLEVEL(3, "test%3i : wildcopy doesn't overwrite potential match data : ", testNb++);
    {   /* This test has a window size of 1024 bytes and consists of 3 blocks:
            1. 'a' repeated 517 times
            2. 'b' repeated 516 times
            3. a compressed block with no literals and 3 sequence commands:
                litlength = 0, offset = 24, match length = 24
                litlength = 0, offset = 24, match length = 3 (this one creates an overlength write of length 2*WILDCOPY_OVERLENGTH - 3)
                litlength = 0, offset = 1021, match length = 3 (this one will try to read from overwritten data if the buffer is too small) */

        const char* testCase =
            "\x28\xB5\x2F\xFD\x04\x00\x4C\x00\x00\x10\x61\x61\x01\x00\x00\x2A"
            "\x80\x05\x44\x00\x00\x08\x62\x01\x00\x00\x2A\x20\x04\x5D\x00\x00"
            "\x00\x03\x40\x00\x00\x64\x60\x27\xB0\xE0\x0C\x67\x62\xCE\xE0";
        ZSTD_DStream* zds = ZSTD_createDStream();

        ZSTD_initDStream(zds);
        inBuff.src = testCase;
        inBuff.size = 47;
        inBuff.pos = 0;
        outBuff.dst = decodedBuffer;
        outBuff.size = CNBufferSize;
        outBuff.pos = 0;

        while (inBuff.pos < inBuff.size) {
            size_t const r = ZSTD_decompressStream(zds, &outBuff, &inBuff);
            /* Bug will cause checksum to fail */
            if (ZSTD_isError(r)) goto _output_error;
        }

        ZSTD_freeDStream(zds);
    }
    DISPLAYLEVEL(3, "OK \n");

_end:
    FUZ_freeDictionary(dictionary);
    ZSTD_freeCStream(zc);
    ZSTD_freeDStream(zd);
    free(CNBuffer);
    free(compressedBuffer);
    free(decodedBuffer);
    return testResult;

_output_error:
    testResult = 1;
    DISPLAY("Error detected in Unit tests ! \n");
    goto _end;
}


/* ======   Fuzzer tests   ====== */

static size_t findDiff(const void* buf1, const void* buf2, size_t max)
{
    const BYTE* b1 = (const BYTE*)buf1;
    const BYTE* b2 = (const BYTE*)buf2;
    size_t u;
    for (u=0; u<max; u++) {
        if (b1[u] != b2[u]) break;
    }
    DISPLAY("Error at position %u / %u \n", (U32)u, (U32)max);
    DISPLAY(" %02X %02X %02X  :%02X:  %02X %02X %02X %02X %02X \n",
            b1[u-3], b1[u-2], b1[u-1], b1[u-0], b1[u+1], b1[u+2], b1[u+3], b1[u+4], b1[u+5]);
    DISPLAY(" %02X %02X %02X  :%02X:  %02X %02X %02X %02X %02X \n",
            b2[u-3], b2[u-2], b2[u-1], b2[u-0], b2[u+1], b2[u+2], b2[u+3], b2[u+4], b2[u+5]);
    return u;
}

static size_t FUZ_rLogLength(U32* seed, U32 logLength)
{
    size_t const lengthMask = ((size_t)1 << logLength) - 1;
    return (lengthMask+1) + (FUZ_rand(seed) & lengthMask);
}

static size_t FUZ_randomLength(U32* seed, U32 maxLog)
{
    U32 const logLength = FUZ_rand(seed) % maxLog;
    return FUZ_rLogLength(seed, logLength);
}

#define MIN(a,b)   ( (a) < (b) ? (a) : (b) )

#define CHECK(cond, ...) if (cond) { DISPLAY("Error => "); DISPLAY(__VA_ARGS__); \
                         DISPLAY(" (seed %u, test nb %u)  \n", seed, testNb); goto _output_error; }

static int fuzzerTests(U32 seed, U32 nbTests, unsigned startTest, double compressibility, int bigTests)
{
    static const U32 maxSrcLog = 24;
    static const U32 maxSampleLog = 19;
    size_t const srcBufferSize = (size_t)1<<maxSrcLog;
    BYTE* cNoiseBuffer[5];
    size_t const copyBufferSize= srcBufferSize + (1<<maxSampleLog);
    BYTE*  const copyBuffer = (BYTE*)malloc (copyBufferSize);
    size_t const cBufferSize   = ZSTD_compressBound(srcBufferSize);
    BYTE*  const cBuffer = (BYTE*)malloc (cBufferSize);
    size_t const dstBufferSize = srcBufferSize;
    BYTE*  const dstBuffer = (BYTE*)malloc (dstBufferSize);
    U32 result = 0;
    U32 testNb = 0;
    U32 coreSeed = seed;
    ZSTD_CStream* zc = ZSTD_createCStream();   /* will be reset sometimes */
    ZSTD_DStream* zd = ZSTD_createDStream();   /* will be reset sometimes */
    ZSTD_DStream* const zd_noise = ZSTD_createDStream();
    clock_t const startClock = clock();
    const BYTE* dict=NULL;   /* can keep same dict on 2 consecutive tests */
    size_t dictSize = 0;
    U32 oldTestLog = 0;
    int const cLevelLimiter = bigTests ? 3 : 2;

    /* allocations */
    cNoiseBuffer[0] = (BYTE*)malloc (srcBufferSize);
    cNoiseBuffer[1] = (BYTE*)malloc (srcBufferSize);
    cNoiseBuffer[2] = (BYTE*)malloc (srcBufferSize);
    cNoiseBuffer[3] = (BYTE*)malloc (srcBufferSize);
    cNoiseBuffer[4] = (BYTE*)malloc (srcBufferSize);
    CHECK (!cNoiseBuffer[0] || !cNoiseBuffer[1] || !cNoiseBuffer[2] || !cNoiseBuffer[3] || !cNoiseBuffer[4] ||
           !copyBuffer || !dstBuffer || !cBuffer || !zc || !zd || !zd_noise ,
           "Not enough memory, fuzzer tests cancelled");

    /* Create initial samples */
    RDG_genBuffer(cNoiseBuffer[0], srcBufferSize, 0.00, 0., coreSeed);    /* pure noise */
    RDG_genBuffer(cNoiseBuffer[1], srcBufferSize, 0.05, 0., coreSeed);    /* barely compressible */
    RDG_genBuffer(cNoiseBuffer[2], srcBufferSize, compressibility, 0., coreSeed);
    RDG_genBuffer(cNoiseBuffer[3], srcBufferSize, 0.95, 0., coreSeed);    /* highly compressible */
    RDG_genBuffer(cNoiseBuffer[4], srcBufferSize, 1.00, 0., coreSeed);    /* sparse content */
    memset(copyBuffer, 0x65, copyBufferSize);                             /* make copyBuffer considered initialized */
    ZSTD_initDStream_usingDict(zd, NULL, 0);  /* ensure at least one init */

    /* catch up testNb */
    for (testNb=1; testNb < startTest; testNb++)
        FUZ_rand(&coreSeed);

    /* test loop */
    for ( ; (testNb <= nbTests) || (FUZ_GetClockSpan(startClock) < g_clockTime) ; testNb++ ) {
        U32 lseed;
        const BYTE* srcBuffer;
        size_t totalTestSize, totalGenSize, cSize;
        XXH64_state_t xxhState;
        U64 crcOrig;
        U32 resetAllowed = 1;
        size_t maxTestSize;

        /* init */
        if (nbTests >= testNb) { DISPLAYUPDATE(2, "\r%6u/%6u    ", testNb, nbTests); }
        else { DISPLAYUPDATE(2, "\r%6u          ", testNb); }
        FUZ_rand(&coreSeed);
        lseed = coreSeed ^ prime32;

        /* states full reset (deliberately not synchronized) */
        /* some issues can only happen when reusing states */
        if ((FUZ_rand(&lseed) & 0xFF) == 131) { ZSTD_freeCStream(zc); zc = ZSTD_createCStream(); resetAllowed=0; }
        if ((FUZ_rand(&lseed) & 0xFF) == 132) { ZSTD_freeDStream(zd); zd = ZSTD_createDStream(); ZSTD_initDStream_usingDict(zd, NULL, 0);  /* ensure at least one init */ }

        /* srcBuffer selection [0-4] */
        {   U32 buffNb = FUZ_rand(&lseed) & 0x7F;
            if (buffNb & 7) buffNb=2;   /* most common : compressible (P) */
            else {
                buffNb >>= 3;
                if (buffNb & 7) {
                    const U32 tnb[2] = { 1, 3 };   /* barely/highly compressible */
                    buffNb = tnb[buffNb >> 3];
                } else {
                    const U32 tnb[2] = { 0, 4 };   /* not compressible / sparse */
                    buffNb = tnb[buffNb >> 3];
            }   }
            srcBuffer = cNoiseBuffer[buffNb];
        }

        /* compression init */
        if ((FUZ_rand(&lseed)&1) /* at beginning, to keep same nb of rand */
            && oldTestLog /* at least one test happened */ && resetAllowed) {
            maxTestSize = FUZ_randomLength(&lseed, oldTestLog+2);
            if (maxTestSize >= srcBufferSize)
                maxTestSize = srcBufferSize-1;
            {   U64 const pledgedSrcSize = (FUZ_rand(&lseed) & 3) ? 0 : maxTestSize;
                size_t const resetError = ZSTD_resetCStream(zc, pledgedSrcSize);
                CHECK(ZSTD_isError(resetError), "ZSTD_resetCStream error : %s", ZSTD_getErrorName(resetError));
            }
        } else {
            U32 const testLog = FUZ_rand(&lseed) % maxSrcLog;
            U32 const dictLog = FUZ_rand(&lseed) % maxSrcLog;
            U32 const cLevel = ( FUZ_rand(&lseed) %
                                (ZSTD_maxCLevel() -
                                (MAX(testLog, dictLog) / cLevelLimiter)))
                                 + 1;
            maxTestSize = FUZ_rLogLength(&lseed, testLog);
            oldTestLog = testLog;
            /* random dictionary selection */
            dictSize  = ((FUZ_rand(&lseed)&1)==1) ? FUZ_rLogLength(&lseed, dictLog) : 0;
            {   size_t const dictStart = FUZ_rand(&lseed) % (srcBufferSize - dictSize);
                dict = srcBuffer + dictStart;
            }
            {   U64 const pledgedSrcSize = (FUZ_rand(&lseed) & 3) ? 0 : maxTestSize;
                ZSTD_parameters params = ZSTD_getParams(cLevel, pledgedSrcSize, dictSize);
                params.fParams.checksumFlag = FUZ_rand(&lseed) & 1;
                params.fParams.noDictIDFlag = FUZ_rand(&lseed) & 1;
                {   size_t const initError = ZSTD_initCStream_advanced(zc, dict, dictSize, params, pledgedSrcSize);
                    CHECK (ZSTD_isError(initError),"ZSTD_initCStream_advanced error : %s", ZSTD_getErrorName(initError));
        }   }   }

        /* multi-segments compression test */
        XXH64_reset(&xxhState, 0);
        {   ZSTD_outBuffer outBuff = { cBuffer, cBufferSize, 0 } ;
            U32 n;
            for (n=0, cSize=0, totalTestSize=0 ; totalTestSize < maxTestSize ; n++) {
                /* compress random chunks into randomly sized dst buffers */
                {   size_t const randomSrcSize = FUZ_randomLength(&lseed, maxSampleLog);
                    size_t const srcSize = MIN (maxTestSize-totalTestSize, randomSrcSize);
                    size_t const srcStart = FUZ_rand(&lseed) % (srcBufferSize - srcSize);
                    size_t const randomDstSize = FUZ_randomLength(&lseed, maxSampleLog);
                    size_t const dstBuffSize = MIN(cBufferSize - cSize, randomDstSize);
                    ZSTD_inBuffer inBuff = { srcBuffer+srcStart, srcSize, 0 };
                    outBuff.size = outBuff.pos + dstBuffSize;

                    { size_t const compressionError = ZSTD_compressStream(zc, &outBuff, &inBuff);
                      CHECK (ZSTD_isError(compressionError), "compression error : %s", ZSTD_getErrorName(compressionError)); }

                    XXH64_update(&xxhState, srcBuffer+srcStart, inBuff.pos);
                    memcpy(copyBuffer+totalTestSize, srcBuffer+srcStart, inBuff.pos);
                    totalTestSize += inBuff.pos;
                }

                /* random flush operation, to mess around */
                if ((FUZ_rand(&lseed) & 15) == 0) {
                    size_t const randomDstSize = FUZ_randomLength(&lseed, maxSampleLog);
                    size_t const adjustedDstSize = MIN(cBufferSize - cSize, randomDstSize);
                    outBuff.size = outBuff.pos + adjustedDstSize;
                    {   size_t const flushError = ZSTD_flushStream(zc, &outBuff);
                        CHECK (ZSTD_isError(flushError), "flush error : %s", ZSTD_getErrorName(flushError));
            }   }   }

            /* final frame epilogue */
            {   size_t remainingToFlush = (size_t)(-1);
                while (remainingToFlush) {
                    size_t const randomDstSize = FUZ_randomLength(&lseed, maxSampleLog);
                    size_t const adjustedDstSize = MIN(cBufferSize - cSize, randomDstSize);
                    U32 const enoughDstSize = (adjustedDstSize >= remainingToFlush);
                    outBuff.size = outBuff.pos + adjustedDstSize;
                    remainingToFlush = ZSTD_endStream(zc, &outBuff);
                    CHECK (ZSTD_isError(remainingToFlush), "flush error : %s", ZSTD_getErrorName(remainingToFlush));
                    CHECK (enoughDstSize && remainingToFlush, "ZSTD_endStream() not fully flushed (%u remaining), but enough space available", (U32)remainingToFlush);
            }   }
            crcOrig = XXH64_digest(&xxhState);
            cSize = outBuff.pos;
        }

        /* multi - fragments decompression test */
        if (!dictSize /* don't reset if dictionary : could be different */ && (FUZ_rand(&lseed) & 1)) {
            CHECK (ZSTD_isError(ZSTD_resetDStream(zd)), "ZSTD_resetDStream failed");
        } else {
            ZSTD_initDStream_usingDict(zd, dict, dictSize);
        }
        {   size_t decompressionResult = 1;
            ZSTD_inBuffer  inBuff = { cBuffer, cSize, 0 };
            ZSTD_outBuffer outBuff= { dstBuffer, dstBufferSize, 0 };
            for (totalGenSize = 0 ; decompressionResult ; ) {
                size_t const readCSrcSize = FUZ_randomLength(&lseed, maxSampleLog);
                size_t const randomDstSize = FUZ_randomLength(&lseed, maxSampleLog);
                size_t const dstBuffSize = MIN(dstBufferSize - totalGenSize, randomDstSize);
                inBuff.size = inBuff.pos + readCSrcSize;
                outBuff.size = inBuff.pos + dstBuffSize;
                decompressionResult = ZSTD_decompressStream(zd, &outBuff, &inBuff);
                CHECK (ZSTD_isError(decompressionResult), "decompression error : %s", ZSTD_getErrorName(decompressionResult));
            }
            CHECK (decompressionResult != 0, "frame not fully decoded");
            CHECK (outBuff.pos != totalTestSize, "decompressed data : wrong size")
            CHECK (inBuff.pos != cSize, "compressed data should be fully read")
            {   U64 const crcDest = XXH64(dstBuffer, totalTestSize, 0);
                if (crcDest!=crcOrig) findDiff(copyBuffer, dstBuffer, totalTestSize);
                CHECK (crcDest!=crcOrig, "decompressed data corrupted");
        }   }

        /*=====   noisy/erroneous src decompression test   =====*/

        /* add some noise */
        {   U32 const nbNoiseChunks = (FUZ_rand(&lseed) & 7) + 2;
            U32 nn; for (nn=0; nn<nbNoiseChunks; nn++) {
                size_t const randomNoiseSize = FUZ_randomLength(&lseed, maxSampleLog);
                size_t const noiseSize  = MIN((cSize/3) , randomNoiseSize);
                size_t const noiseStart = FUZ_rand(&lseed) % (srcBufferSize - noiseSize);
                size_t const cStart = FUZ_rand(&lseed) % (cSize - noiseSize);
                memcpy(cBuffer+cStart, srcBuffer+noiseStart, noiseSize);
        }   }

        /* try decompression on noisy data */
        ZSTD_initDStream(zd_noise);   /* note : no dictionary */
        {   ZSTD_inBuffer  inBuff = { cBuffer, cSize, 0 };
            ZSTD_outBuffer outBuff= { dstBuffer, dstBufferSize, 0 };
            while (outBuff.pos < dstBufferSize) {
                size_t const randomCSrcSize = FUZ_randomLength(&lseed, maxSampleLog);
                size_t const randomDstSize = FUZ_randomLength(&lseed, maxSampleLog);
                size_t const adjustedDstSize = MIN(dstBufferSize - outBuff.pos, randomDstSize);
                size_t const adjustedCSrcSize = MIN(cSize - inBuff.pos, randomCSrcSize);
                outBuff.size = outBuff.pos + adjustedDstSize;
                inBuff.size  = inBuff.pos + adjustedCSrcSize;
                {   size_t const decompressError = ZSTD_decompressStream(zd, &outBuff, &inBuff);
                    if (ZSTD_isError(decompressError)) break;   /* error correctly detected */
                    /* No forward progress possible */
                    if (outBuff.pos < outBuff.size && inBuff.pos == cSize) break;
    }   }   }   }
    DISPLAY("\r%u fuzzer tests completed   \n", testNb);

_cleanup:
    ZSTD_freeCStream(zc);
    ZSTD_freeDStream(zd);
    ZSTD_freeDStream(zd_noise);
    free(cNoiseBuffer[0]);
    free(cNoiseBuffer[1]);
    free(cNoiseBuffer[2]);
    free(cNoiseBuffer[3]);
    free(cNoiseBuffer[4]);
    free(copyBuffer);
    free(cBuffer);
    free(dstBuffer);
    return result;

_output_error:
    result = 1;
    goto _cleanup;
}


/* Multi-threading version of fuzzer Tests */
static int fuzzerTests_MT(U32 seed, U32 nbTests, unsigned startTest, double compressibility, int bigTests)
{
    static const U32 maxSrcLog = 24;
    static const U32 maxSampleLog = 19;
    size_t const srcBufferSize = (size_t)1<<maxSrcLog;
    BYTE* cNoiseBuffer[5];
    size_t const copyBufferSize= srcBufferSize + (1<<maxSampleLog);
    BYTE*  const copyBuffer = (BYTE*)malloc (copyBufferSize);
    size_t const cBufferSize   = ZSTD_compressBound(srcBufferSize);
    BYTE*  const cBuffer = (BYTE*)malloc (cBufferSize);
    size_t const dstBufferSize = srcBufferSize;
    BYTE*  const dstBuffer = (BYTE*)malloc (dstBufferSize);
    U32 result = 0;
    U32 testNb = 0;
    U32 coreSeed = seed;
    ZSTDMT_CCtx* zc = ZSTDMT_createCCtx(2);   /* will be reset sometimes */
    ZSTD_DStream* zd = ZSTD_createDStream();   /* will be reset sometimes */
    ZSTD_DStream* const zd_noise = ZSTD_createDStream();
    clock_t const startClock = clock();
    const BYTE* dict=NULL;   /* can keep same dict on 2 consecutive tests */
    size_t dictSize = 0;
    U32 oldTestLog = 0;
    int const cLevelLimiter = bigTests ? 3 : 2;

    /* allocations */
    cNoiseBuffer[0] = (BYTE*)malloc (srcBufferSize);
    cNoiseBuffer[1] = (BYTE*)malloc (srcBufferSize);
    cNoiseBuffer[2] = (BYTE*)malloc (srcBufferSize);
    cNoiseBuffer[3] = (BYTE*)malloc (srcBufferSize);
    cNoiseBuffer[4] = (BYTE*)malloc (srcBufferSize);
    CHECK (!cNoiseBuffer[0] || !cNoiseBuffer[1] || !cNoiseBuffer[2] || !cNoiseBuffer[3] || !cNoiseBuffer[4] ||
           !copyBuffer || !dstBuffer || !cBuffer || !zc || !zd || !zd_noise ,
           "Not enough memory, fuzzer tests cancelled");

    /* Create initial samples */
    RDG_genBuffer(cNoiseBuffer[0], srcBufferSize, 0.00, 0., coreSeed);    /* pure noise */
    RDG_genBuffer(cNoiseBuffer[1], srcBufferSize, 0.05, 0., coreSeed);    /* barely compressible */
    RDG_genBuffer(cNoiseBuffer[2], srcBufferSize, compressibility, 0., coreSeed);
    RDG_genBuffer(cNoiseBuffer[3], srcBufferSize, 0.95, 0., coreSeed);    /* highly compressible */
    RDG_genBuffer(cNoiseBuffer[4], srcBufferSize, 1.00, 0., coreSeed);    /* sparse content */
    memset(copyBuffer, 0x65, copyBufferSize);                             /* make copyBuffer considered initialized */
    ZSTD_initDStream_usingDict(zd, NULL, 0);  /* ensure at least one init */

    /* catch up testNb */
    for (testNb=1; testNb < startTest; testNb++)
        FUZ_rand(&coreSeed);

    /* test loop */
    for ( ; (testNb <= nbTests) || (FUZ_GetClockSpan(startClock) < g_clockTime) ; testNb++ ) {
        U32 lseed;
        const BYTE* srcBuffer;
        size_t totalTestSize, totalGenSize, cSize;
        XXH64_state_t xxhState;
        U64 crcOrig;
        U32 resetAllowed = 1;
        size_t maxTestSize;

        /* init */
        if (nbTests >= testNb) { DISPLAYUPDATE(2, "\r%6u/%6u    ", testNb, nbTests); }
        else { DISPLAYUPDATE(2, "\r%6u          ", testNb); }
        FUZ_rand(&coreSeed);
        lseed = coreSeed ^ prime32;

        /* states full reset (deliberately not synchronized) */
        /* some issues can only happen when reusing states */
        if ((FUZ_rand(&lseed) & 0xFF) == 131) {
            U32 const nbThreads = (FUZ_rand(&lseed) % 6) + 1;
            DISPLAYLEVEL(5, "Creating new context with %u threads \n", nbThreads);
            ZSTDMT_freeCCtx(zc);
            zc = ZSTDMT_createCCtx(nbThreads);
            resetAllowed=0;
        }
        if ((FUZ_rand(&lseed) & 0xFF) == 132) {
            ZSTD_freeDStream(zd);
            zd = ZSTD_createDStream();
            ZSTD_initDStream_usingDict(zd, NULL, 0);  /* ensure at least one init */
        }

        /* srcBuffer selection [0-4] */
        {   U32 buffNb = FUZ_rand(&lseed) & 0x7F;
            if (buffNb & 7) buffNb=2;   /* most common : compressible (P) */
            else {
                buffNb >>= 3;
                if (buffNb & 7) {
                    const U32 tnb[2] = { 1, 3 };   /* barely/highly compressible */
                    buffNb = tnb[buffNb >> 3];
                } else {
                    const U32 tnb[2] = { 0, 4 };   /* not compressible / sparse */
                    buffNb = tnb[buffNb >> 3];
            }   }
            srcBuffer = cNoiseBuffer[buffNb];
        }

        /* compression init */
        if ((FUZ_rand(&lseed)&1) /* at beginning, to keep same nb of rand */
            && oldTestLog /* at least one test happened */ && resetAllowed) {
            maxTestSize = FUZ_randomLength(&lseed, oldTestLog+2);
            if (maxTestSize >= srcBufferSize) maxTestSize = srcBufferSize-1;
            {   int const compressionLevel = (FUZ_rand(&lseed) % 5) + 1;
                size_t const resetError = ZSTDMT_initCStream(zc, compressionLevel);
                CHECK(ZSTD_isError(resetError), "ZSTDMT_initCStream error : %s", ZSTD_getErrorName(resetError));
            }
        } else {
            U32 const testLog = FUZ_rand(&lseed) % maxSrcLog;
            U32 const dictLog = FUZ_rand(&lseed) % maxSrcLog;
            U32 const cLevel = (FUZ_rand(&lseed) %
                                (ZSTD_maxCLevel() -
                                 (MAX(testLog, dictLog) / cLevelLimiter))) +
                               1;
            maxTestSize = FUZ_rLogLength(&lseed, testLog);
            oldTestLog = testLog;
            /* random dictionary selection */
            dictSize  = ((FUZ_rand(&lseed)&63)==1) ? FUZ_rLogLength(&lseed, dictLog) : 0;
            {   size_t const dictStart = FUZ_rand(&lseed) % (srcBufferSize - dictSize);
                dict = srcBuffer + dictStart;
            }
            {   U64 const pledgedSrcSize = (FUZ_rand(&lseed) & 3) ? 0 : maxTestSize;
                ZSTD_parameters params = ZSTD_getParams(cLevel, pledgedSrcSize, dictSize);
                DISPLAYLEVEL(5, "Init with windowLog = %u and pledgedSrcSize = %u \n",
                    params.cParams.windowLog, (U32)pledgedSrcSize);
                params.fParams.checksumFlag = FUZ_rand(&lseed) & 1;
                params.fParams.noDictIDFlag = FUZ_rand(&lseed) & 1;
                params.fParams.contentSizeFlag = pledgedSrcSize>0;
                DISPLAYLEVEL(5, "checksumFlag : %u \n", params.fParams.checksumFlag);
                { size_t const initError = ZSTDMT_initCStream_advanced(zc, dict, dictSize, params, pledgedSrcSize);
                  CHECK (ZSTD_isError(initError),"ZSTDMT_initCStream_advanced error : %s", ZSTD_getErrorName(initError)); }
                ZSTDMT_setMTCtxParameter(zc, ZSTDMT_p_overlapSectionLog, FUZ_rand(&lseed) % 12);
                ZSTDMT_setMTCtxParameter(zc, ZSTDMT_p_sectionSize, FUZ_rand(&lseed) % (2*maxTestSize+1));
        }   }

        /* multi-segments compression test */
        XXH64_reset(&xxhState, 0);
        {   ZSTD_outBuffer outBuff = { cBuffer, cBufferSize, 0 } ;
            U32 n;
            for (n=0, cSize=0, totalTestSize=0 ; totalTestSize < maxTestSize ; n++) {
                /* compress random chunks into randomly sized dst buffers */
                {   size_t const randomSrcSize = FUZ_randomLength(&lseed, maxSampleLog);
                    size_t const srcSize = MIN (maxTestSize-totalTestSize, randomSrcSize);
                    size_t const srcStart = FUZ_rand(&lseed) % (srcBufferSize - srcSize);
                    size_t const randomDstSize = FUZ_randomLength(&lseed, maxSampleLog);
                    size_t const dstBuffSize = MIN(cBufferSize - cSize, randomDstSize);
                    ZSTD_inBuffer inBuff = { srcBuffer+srcStart, srcSize, 0 };
                    outBuff.size = outBuff.pos + dstBuffSize;

                    DISPLAYLEVEL(5, "Sending %u bytes to compress \n", (U32)srcSize);
                    { size_t const compressionError = ZSTDMT_compressStream(zc, &outBuff, &inBuff);
                      CHECK (ZSTD_isError(compressionError), "compression error : %s", ZSTD_getErrorName(compressionError)); }
                    DISPLAYLEVEL(5, "%u bytes read by ZSTDMT_compressStream \n", (U32)inBuff.pos);

                    XXH64_update(&xxhState, srcBuffer+srcStart, inBuff.pos);
                    memcpy(copyBuffer+totalTestSize, srcBuffer+srcStart, inBuff.pos);
                    totalTestSize += inBuff.pos;
                }

                /* random flush operation, to mess around */
                if ((FUZ_rand(&lseed) & 15) == 0) {
                    size_t const randomDstSize = FUZ_randomLength(&lseed, maxSampleLog);
                    size_t const adjustedDstSize = MIN(cBufferSize - cSize, randomDstSize);
                    outBuff.size = outBuff.pos + adjustedDstSize;
                    DISPLAYLEVEL(5, "Flushing into dst buffer of size %u \n", (U32)adjustedDstSize);
                    {   size_t const flushError = ZSTDMT_flushStream(zc, &outBuff);
                        CHECK (ZSTD_isError(flushError), "ZSTDMT_flushStream error : %s", ZSTD_getErrorName(flushError));
            }   }   }

            /* final frame epilogue */
            {   size_t remainingToFlush = (size_t)(-1);
                while (remainingToFlush) {
                    size_t const randomDstSize = FUZ_randomLength(&lseed, maxSampleLog);
                    size_t const adjustedDstSize = MIN(cBufferSize - cSize, randomDstSize);
                    outBuff.size = outBuff.pos + adjustedDstSize;
                    DISPLAYLEVEL(5, "Ending into dst buffer of size %u \n", (U32)adjustedDstSize);
                    remainingToFlush = ZSTDMT_endStream(zc, &outBuff);
                    CHECK (ZSTD_isError(remainingToFlush), "ZSTDMT_endStream error : %s", ZSTD_getErrorName(remainingToFlush));
                    DISPLAYLEVEL(5, "endStream : remainingToFlush : %u \n", (U32)remainingToFlush);
            }   }
            crcOrig = XXH64_digest(&xxhState);
            cSize = outBuff.pos;
            DISPLAYLEVEL(5, "Frame completed : %u bytes \n", (U32)cSize);
        }

        /* multi - fragments decompression test */
        if (!dictSize /* don't reset if dictionary : could be different */ && (FUZ_rand(&lseed) & 1)) {
            CHECK (ZSTD_isError(ZSTD_resetDStream(zd)), "ZSTD_resetDStream failed");
        } else {
            ZSTD_initDStream_usingDict(zd, dict, dictSize);
        }
        {   size_t decompressionResult = 1;
            ZSTD_inBuffer  inBuff = { cBuffer, cSize, 0 };
            ZSTD_outBuffer outBuff= { dstBuffer, dstBufferSize, 0 };
            for (totalGenSize = 0 ; decompressionResult ; ) {
                size_t const readCSrcSize = FUZ_randomLength(&lseed, maxSampleLog);
                size_t const randomDstSize = FUZ_randomLength(&lseed, maxSampleLog);
                size_t const dstBuffSize = MIN(dstBufferSize - totalGenSize, randomDstSize);
                inBuff.size = inBuff.pos + readCSrcSize;
                outBuff.size = inBuff.pos + dstBuffSize;
                DISPLAYLEVEL(5, "ZSTD_decompressStream input %u bytes \n", (U32)readCSrcSize);
                decompressionResult = ZSTD_decompressStream(zd, &outBuff, &inBuff);
                CHECK (ZSTD_isError(decompressionResult), "decompression error : %s", ZSTD_getErrorName(decompressionResult));
                DISPLAYLEVEL(5, "inBuff.pos = %u \n", (U32)readCSrcSize);
            }
            CHECK (outBuff.pos != totalTestSize, "decompressed data : wrong size (%u != %u)", (U32)outBuff.pos, (U32)totalTestSize);
            CHECK (inBuff.pos != cSize, "compressed data should be fully read (%u != %u)", (U32)inBuff.pos, (U32)cSize);
            {   U64 const crcDest = XXH64(dstBuffer, totalTestSize, 0);
                if (crcDest!=crcOrig) findDiff(copyBuffer, dstBuffer, totalTestSize);
                CHECK (crcDest!=crcOrig, "decompressed data corrupted");
        }   }

        /*=====   noisy/erroneous src decompression test   =====*/

        /* add some noise */
        {   U32 const nbNoiseChunks = (FUZ_rand(&lseed) & 7) + 2;
            U32 nn; for (nn=0; nn<nbNoiseChunks; nn++) {
                size_t const randomNoiseSize = FUZ_randomLength(&lseed, maxSampleLog);
                size_t const noiseSize  = MIN((cSize/3) , randomNoiseSize);
                size_t const noiseStart = FUZ_rand(&lseed) % (srcBufferSize - noiseSize);
                size_t const cStart = FUZ_rand(&lseed) % (cSize - noiseSize);
                memcpy(cBuffer+cStart, srcBuffer+noiseStart, noiseSize);
        }   }

        /* try decompression on noisy data */
        ZSTD_initDStream(zd_noise);   /* note : no dictionary */
        {   ZSTD_inBuffer  inBuff = { cBuffer, cSize, 0 };
            ZSTD_outBuffer outBuff= { dstBuffer, dstBufferSize, 0 };
            while (outBuff.pos < dstBufferSize) {
                size_t const randomCSrcSize = FUZ_randomLength(&lseed, maxSampleLog);
                size_t const randomDstSize = FUZ_randomLength(&lseed, maxSampleLog);
                size_t const adjustedDstSize = MIN(dstBufferSize - outBuff.pos, randomDstSize);
                size_t const adjustedCSrcSize = MIN(cSize - inBuff.pos, randomCSrcSize);
                outBuff.size = outBuff.pos + adjustedDstSize;
                inBuff.size  = inBuff.pos + adjustedCSrcSize;
                {   size_t const decompressError = ZSTD_decompressStream(zd, &outBuff, &inBuff);
                    if (ZSTD_isError(decompressError)) break;   /* error correctly detected */
                    /* No forward progress possible */
                    if (outBuff.pos < outBuff.size && inBuff.pos == cSize) break;
    }   }   }   }
    DISPLAY("\r%u fuzzer tests completed   \n", testNb);

_cleanup:
    ZSTDMT_freeCCtx(zc);
    ZSTD_freeDStream(zd);
    ZSTD_freeDStream(zd_noise);
    free(cNoiseBuffer[0]);
    free(cNoiseBuffer[1]);
    free(cNoiseBuffer[2]);
    free(cNoiseBuffer[3]);
    free(cNoiseBuffer[4]);
    free(copyBuffer);
    free(cBuffer);
    free(dstBuffer);
    return result;

_output_error:
    result = 1;
    goto _cleanup;
}


/*-*******************************************************
*  Command line
*********************************************************/
int FUZ_usage(const char* programName)
{
    DISPLAY( "Usage :\n");
    DISPLAY( "      %s [args]\n", programName);
    DISPLAY( "\n");
    DISPLAY( "Arguments :\n");
    DISPLAY( " -i#    : Nb of tests (default:%u) \n", nbTestsDefault);
    DISPLAY( " -s#    : Select seed (default:prompt user)\n");
    DISPLAY( " -t#    : Select starting test number (default:0)\n");
    DISPLAY( " -P#    : Select compressibility in %% (default:%i%%)\n", FUZ_COMPRESSIBILITY_DEFAULT);
    DISPLAY( " -v     : verbose\n");
    DISPLAY( " -p     : pause at the end\n");
    DISPLAY( " -h     : display help and exit\n");
    return 0;
}


int main(int argc, const char** argv)
{
    U32 seed=0;
    int seedset=0;
    int argNb;
    int nbTests = nbTestsDefault;
    int testNb = 0;
    int proba = FUZ_COMPRESSIBILITY_DEFAULT;
    int result=0;
    int mainPause = 0;
    int mtOnly = 0;
    int bigTests = 1;
    const char* const programName = argv[0];
    ZSTD_customMem const customMem = { allocFunction, freeFunction, NULL };
    ZSTD_customMem const customNULL = { NULL, NULL, NULL };

    /* Check command line */
    for(argNb=1; argNb<argc; argNb++) {
        const char* argument = argv[argNb];
        if(!argument) continue;   /* Protection if argument empty */

        /* Parsing commands. Aggregated commands are allowed */
        if (argument[0]=='-') {

            if (!strcmp(argument, "--mt")) { mtOnly=1; continue; }
            if (!strcmp(argument, "--no-big-tests")) { bigTests=0; continue; }

            argument++;
            while (*argument!=0) {
                switch(*argument)
                {
                case 'h':
                    return FUZ_usage(programName);

                case 'v':
                    argument++;
                    g_displayLevel++;
                    break;

                case 'q':
                    argument++;
                    g_displayLevel--;
                    break;

                case 'p': /* pause at the end */
                    argument++;
                    mainPause = 1;
                    break;

                case 'i':   /* limit tests by nb of iterations (default) */
                    argument++;
                    nbTests=0; g_clockTime=0;
                    while ((*argument>='0') && (*argument<='9')) {
                        nbTests *= 10;
                        nbTests += *argument - '0';
                        argument++;
                    }
                    break;

                case 'T':   /* limit tests by time */
                    argument++;
                    nbTests=0; g_clockTime=0;
                    while ((*argument>='0') && (*argument<='9')) {
                        g_clockTime *= 10;
                        g_clockTime += *argument - '0';
                        argument++;
                    }
                    if (*argument=='m') g_clockTime *=60, argument++;
                    if (*argument=='n') argument++;
                    g_clockTime *= CLOCKS_PER_SEC;
                    break;

                case 's':   /* manually select seed */
                    argument++;
                    seed=0;
                    seedset=1;
                    while ((*argument>='0') && (*argument<='9')) {
                        seed *= 10;
                        seed += *argument - '0';
                        argument++;
                    }
                    break;

                case 't':   /* select starting test number */
                    argument++;
                    testNb=0;
                    while ((*argument>='0') && (*argument<='9')) {
                        testNb *= 10;
                        testNb += *argument - '0';
                        argument++;
                    }
                    break;

                case 'P':   /* compressibility % */
                    argument++;
                    proba=0;
                    while ((*argument>='0') && (*argument<='9')) {
                        proba *= 10;
                        proba += *argument - '0';
                        argument++;
                    }
                    if (proba<0) proba=0;
                    if (proba>100) proba=100;
                    break;

                default:
                    return FUZ_usage(programName);
                }
    }   }   }   /* for(argNb=1; argNb<argc; argNb++) */

    /* Get Seed */
    DISPLAY("Starting zstream tester (%i-bits, %s)\n", (int)(sizeof(size_t)*8), ZSTD_VERSION_STRING);

    if (!seedset) {
        time_t const t = time(NULL);
        U32 const h = XXH32(&t, sizeof(t), 1);
        seed = h % 10000;
    }

    DISPLAY("Seed = %u\n", seed);
    if (proba!=FUZ_COMPRESSIBILITY_DEFAULT) DISPLAY("Compressibility : %i%%\n", proba);

    if (nbTests<=0) nbTests=1;

    if (testNb==0) {
        result = basicUnitTests(0, ((double)proba) / 100, customNULL);  /* constant seed for predictability */
        if (!result) {
            DISPLAYLEVEL(3, "Unit tests using customMem :\n")
            result = basicUnitTests(0, ((double)proba) / 100, customMem);  /* use custom memory allocation functions */
    }   }

    if (!result && !mtOnly) result = fuzzerTests(seed, nbTests, testNb, ((double)proba) / 100, bigTests);
    if (!result) result = fuzzerTests_MT(seed, nbTests, testNb, ((double)proba) / 100, bigTests);

    if (mainPause) {
        int unused;
        DISPLAY("Press Enter \n");
        unused = getchar();
        (void)unused;
    }
    return result;
}
