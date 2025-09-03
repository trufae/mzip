# Makefile for mzip

CC?=gcc
CFLAGS?=-O2 -Wall
DESTDIR?=
PREFIX?=/usr/local
BINDIR?=$(PREFIX)/bin

# Default compression algorithms to enable
#COMPRESSION_FLAGS = -DMZIP_IMPLEMENTATION \
#                     -DMZIP_ENABLE_STORE \
#                     -DMZIP_ENABLE_DEFLATE

# Uncomment to enable additional compression algorithms
#COMPRESSION_FLAGS += -DMZIP_ENABLE_ZSTD -DMZSTD_IMPLEMENTATION
#COMPRESSION_FLAGS += -DMZIP_ENABLE_LZMA
#COMPRESSION_FLAGS += -DMZIP_ENABLE_BROTLI
#COMPRESSION_FLAGS += -DMZIP_ENABLE_LZ4
#COMPRESSION_FLAGS += -DMZIP_ENABLE_LZFSE

# Libraries
LIBS = -lz

# Include paths for vendored Brotli sources
CFLAGS += -I./brotli/c -I./brotli/c/include

# Main targets
all: mzip

# Brotli sources (vendored); compiled into the binary (no external lib)
BROTLI_COMMON_SRCS= \
	brotli/c/common/constants.c \
	brotli/c/common/context.c \
	brotli/c/common/platform.c \
	brotli/c/common/shared_dictionary.c \
	brotli/c/common/transform.c \
	brotli/c/common/dictionary.c

BROTLI_ENC_SRCS= \
	brotli/c/enc/backward_references_hq.c \
	brotli/c/enc/backward_references.c \
	brotli/c/enc/bit_cost.c \
	brotli/c/enc/block_splitter.c \
	brotli/c/enc/brotli_bit_stream.c \
	brotli/c/enc/cluster.c \
	brotli/c/enc/command.c \
	brotli/c/enc/compound_dictionary.c \
	brotli/c/enc/compress_fragment_two_pass.c \
	brotli/c/enc/compress_fragment.c \
	brotli/c/enc/dictionary_hash.c \
	brotli/c/enc/encode.c \
	brotli/c/enc/encoder_dict.c \
	brotli/c/enc/entropy_encode.c \
	brotli/c/enc/fast_log.c \
	brotli/c/enc/histogram.c \
	brotli/c/enc/literal_cost.c \
	brotli/c/enc/memory.c \
	brotli/c/enc/metablock.c \
	brotli/c/enc/static_dict.c \
	brotli/c/enc/utf8_util.c

BROTLI_DEC_SRCS= \
	brotli/c/dec/bit_reader.c \
	brotli/c/dec/decode.c \
	brotli/c/dec/huffman.c \
	brotli/c/dec/state.c

mzip: mzip.c main.c config.h deflate.inc.c crc32.inc.c zstd.inc.c $(BROTLI_COMMON_SRCS) $(BROTLI_ENC_SRCS) $(BROTLI_DEC_SRCS)
	$(CC) $(CFLAGS) $(COMPRESSION_FLAGS) -o $@ main.c mzip.c $(BROTLI_COMMON_SRCS) $(BROTLI_ENC_SRCS) $(BROTLI_DEC_SRCS) $(LIBS)

install:
	mkdir -p $(DESTDIR)$(BINDIR)
	cp -f mzip $(DESTDIR)$(BINDIR)/mzip

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/mzip


# Enable all supported compression methods
all-compression: COMPRESSION_FLAGS += -DMZIP_ENABLE_ZSTD -DMZSTD_IMPLEMENTATION
all-compression: mzip

clean:
	rm -f mzip *.o

.PHONY: all clean all-compression
