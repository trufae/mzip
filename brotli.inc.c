/* brotli.inc.c - Full Brotli integration (enc/dec) with zlib-like wrappers
 *
 * This unit integrates the upstream Google Brotli C implementation (encoder
 * and decoder) directly into this repository and exposes a small zlib-like
 * API used by mzip. No external libraries are required.
 *
 * Exposed wrappers (zlib-like):
 *   - brotliInit / brotliCompress / brotliEnd
 *   - brotliDecompressInit / brotliDecompress / brotliDecompressEnd
 */

#ifndef MBROTLI_H
#define MBROTLI_H

#ifdef MZIP_ENABLE_BROTLI
/* Thin zlib-like wrappers around vendored Brotli implementation */
#include "brotli-enc.inc.c"
#include "brotli-dec.inc.c"

#endif /* MZIP_ENABLE_BROTLI */

#endif /* MBROTLI_H */
