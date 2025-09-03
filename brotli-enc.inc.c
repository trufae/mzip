/* brotli-enc.inc.c - Thin zlib-like wrappers over upstream Brotli encoder */
#ifndef MBROTLI_ENC_H
#define MBROTLI_ENC_H

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

#include "brotli/c/include/brotli/encode.h"

typedef struct {
    BrotliEncoderState* st;
} mzip_brotli_enc;

static int clamp_quality(int lvl) {
    if (lvl < 0) return 5; /* default */
    if (lvl == 0) return 0; /* passthrough compression */
    if (lvl > 11) return 11;
    return lvl;
}

int brotliInit(z_stream *strm, int level) {
    if (!strm) return Z_STREAM_ERROR;
    mzip_brotli_enc* ctx = (mzip_brotli_enc*)calloc(1, sizeof(mzip_brotli_enc));
    if (!ctx) return Z_MEM_ERROR;
    ctx->st = BrotliEncoderCreateInstance(NULL, NULL, NULL);
    if (!ctx->st) { free(ctx); return Z_MEM_ERROR; }
    int q = clamp_quality(level);
    BrotliEncoderSetParameter(ctx->st, BROTLI_PARAM_QUALITY, (uint32_t)q);
    /* Window: default 22 is fine; keep as upstream default. */
    strm->state = (void*)ctx;
    strm->total_in = 0;
    strm->total_out = 0;
    return Z_OK;
}

int brotliCompress(z_stream *strm, int flush) {
    if (!strm || !strm->state) return Z_STREAM_ERROR;
    mzip_brotli_enc* ctx = (mzip_brotli_enc*)strm->state;
    BrotliEncoderOperation op = (flush == Z_FINISH) ? BROTLI_OPERATION_FINISH : BROTLI_OPERATION_PROCESS;

    size_t avail_in = (size_t)strm->avail_in;
    const uint8_t* next_in = (const uint8_t*)strm->next_in;
    size_t avail_out = (size_t)strm->avail_out;
    uint8_t* next_out = (uint8_t*)strm->next_out;

    for (;;) {
        size_t in_before = avail_in;
        size_t out_before = avail_out;
        BROTLI_BOOL ok = BrotliEncoderCompressStream(
            ctx->st, op, &avail_in, &next_in, &avail_out, &next_out, NULL);
        if (!ok) return Z_STREAM_ERROR;
        /* Update z_stream counters */
        strm->total_in  += (uLong)(in_before - avail_in);
        strm->total_out += (uLong)(out_before - avail_out);
        /* Break when we can't make more progress without more space/input. */
        if (BrotliEncoderHasMoreOutput(ctx->st)) {
            if (avail_out == 0) break; /* need more output space */
        } else {
            if (op != BROTLI_OPERATION_FINISH) {
                if (avail_in == 0) break; /* consumed all input */
            } else if (BrotliEncoderIsFinished(ctx->st)) {
                break; /* stream finished */
            }
        }
        if ((in_before == avail_in) && (out_before == avail_out)) break;
    }

    /* Write back pointers */
    strm->next_in  = (Bytef*)next_in;
    strm->avail_in = (uInt)avail_in;
    strm->next_out = (Bytef*)next_out;
    strm->avail_out= (uInt)avail_out;

    if (flush == Z_FINISH && BrotliEncoderIsFinished(ctx->st)) return Z_STREAM_END;
    return Z_OK;
}

int brotliEnd(z_stream *strm) {
    if (!strm || !strm->state) return Z_STREAM_ERROR;
    mzip_brotli_enc* ctx = (mzip_brotli_enc*)strm->state;
    BrotliEncoderDestroyInstance(ctx->st);
    free(ctx);
    strm->state = NULL;
    return Z_OK;
}

/* Compatibility initializers (ignored params) */
int brotliCompressInit2(z_stream *strm, int level, int windowBits, int memLevel, int strategy) {
    (void)windowBits; (void)memLevel; (void)strategy; return brotliInit(strm, level);
}
int brotliCompressInit2_(z_stream *strm, int level, int windowBits, int memLevel, int strategy, const char *version, int stream_size) {
    (void)version; (void)stream_size; return brotliCompressInit2(strm, level, windowBits, memLevel, strategy);
}

#endif /* MBROTLI_ENC_H */
