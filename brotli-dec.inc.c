/* brotli-dec.inc.c - Thin zlib-like wrappers over upstream Brotli decoder */
#ifndef MBROTLI_DEC_H
#define MBROTLI_DEC_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef Z_OK
#define Z_OK            0
#define Z_STREAM_END    1
#define Z_ERRNO        (-1)
#define Z_STREAM_ERROR (-2)
#define Z_DATA_ERROR   (-3)
#define Z_MEM_ERROR    (-4)
#define Z_BUF_ERROR    (-5)
#define Z_VERSION_ERROR (-6)
#define Z_FINISH        4
#endif

#ifndef ZLIB_H
typedef struct z_stream_s z_stream;
#endif

#include "brotli/c/include/brotli/decode.h"

typedef struct {
    BrotliDecoderState* st;
} mzip_brotli_dec;

int brotliDecompressInit(z_stream *strm) {
    if (!strm) return Z_STREAM_ERROR;
    mzip_brotli_dec* ctx = (mzip_brotli_dec*)calloc(1, sizeof(mzip_brotli_dec));
    if (!ctx) return Z_MEM_ERROR;
    ctx->st = BrotliDecoderCreateInstance(NULL, NULL, NULL);
    if (!ctx->st) { free(ctx); return Z_MEM_ERROR; }
    strm->state = (void*)ctx;
    strm->total_in = 0;
    strm->total_out = 0;
    return Z_OK;
}

int brotliDecompress(z_stream *strm, int flush) {
    (void)flush; /* not used */
    if (!strm || !strm->state) return Z_STREAM_ERROR;
    mzip_brotli_dec* ctx = (mzip_brotli_dec*)strm->state;

    size_t avail_in = (size_t)strm->avail_in;
    const uint8_t* next_in = (const uint8_t*)strm->next_in;
    size_t avail_out = (size_t)strm->avail_out;
    uint8_t* next_out = (uint8_t*)strm->next_out;

    BrotliDecoderResult res = BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT;
    for (;;) {
        size_t in_before = avail_in;
        size_t out_before = avail_out;
        res = BrotliDecoderDecompressStream(ctx->st, &avail_in, &next_in, &avail_out, &next_out, NULL);
        strm->total_in  += (uLong)(in_before - avail_in);
        strm->total_out += (uLong)(out_before - avail_out);

        if (res == BROTLI_DECODER_RESULT_SUCCESS) break; /* finished */
        if (res == BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT) {
            if (avail_out == 0) break; /* need more space */
        } else if (res == BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT) {
            if (avail_in == 0) break; /* need more input */
        } else { /* error */
            return Z_DATA_ERROR;
        }
        if ((in_before == avail_in) && (out_before == avail_out)) break; /* no progress */
    }

    strm->next_in  = (Bytef*)next_in;
    strm->avail_in = (uInt)avail_in;
    strm->next_out = (Bytef*)next_out;
    strm->avail_out= (uInt)avail_out;

    if (res == BROTLI_DECODER_RESULT_SUCCESS) return Z_STREAM_END;
    return Z_OK;
}

int brotliDecompressEnd(z_stream *strm) {
    if (!strm || !strm->state) return Z_STREAM_ERROR;
    mzip_brotli_dec* ctx = (mzip_brotli_dec*)strm->state;
    BrotliDecoderDestroyInstance(ctx->st);
    free(ctx);
    strm->state = NULL;
    return Z_OK;
}

int brotliDecompressInit2(z_stream *strm, int windowBits) { (void)windowBits; return brotliDecompressInit(strm); }
int brotliDecompressInit2_(z_stream *strm, int windowBits, const char *version, int stream_size) {
    (void)windowBits; (void)version; (void)stream_size; return brotliDecompressInit(strm);
}

#endif /* MBROTLI_DEC_H */
