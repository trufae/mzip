#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Usage: %s <mode> <input_file> <output_file>\n", argv[0]);
        printf("  mode: 'c' to compress, 'd' to decompress\n");
        return 1;
    }

    char mode = argv[1][0];
    if (mode != 'c' && mode != 'd') {
        printf("Invalid mode. Use 'c' to compress or 'd' to decompress\n");
        return 1;
    }

    FILE *in_file = fopen(argv[2], "rb");
    if (!in_file) {
        perror("Failed to open input file");
        return 1;
    }

    /* Get input file size */
    fseek(in_file, 0, SEEK_END);
    long in_size = ftell(in_file);
    fseek(in_file, 0, SEEK_SET);

    /* Read input file */
    unsigned char *in_data = (unsigned char*)malloc(in_size);
    if (!in_data) {
        perror("Memory allocation failed");
        fclose(in_file);
        return 1;
    }

    if (fread(in_data, 1, in_size, in_file) != in_size) {
        perror("Failed to read input file");
        free(in_data);
        fclose(in_file);
        return 1;
    }
    fclose(in_file);

    /* Prepare for compression/decompression */
    z_stream strm = {0};
    int ret;
    
    if (mode == 'c') {
        /* Compress */
        unsigned char *out_data = (unsigned char*)malloc(in_size * 2); /* Allocate enough for worst case */
        if (!out_data) {
            perror("Memory allocation failed");
            free(in_data);
            return 1;
        }
        
        /* Initialize for raw deflate */
        if (deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
            printf("deflateInit2 failed\n");
            free(in_data);
            free(out_data);
            return 1;
        }
        
        strm.next_in = in_data;
        strm.avail_in = in_size;
        strm.next_out = out_data;
        strm.avail_out = in_size * 2;
        
        /* Compress data */
        ret = deflate(&strm, Z_FINISH);
        if (ret != Z_STREAM_END) {
            printf("deflate failed with result %d\n", ret);
            deflateEnd(&strm);
            free(in_data);
            free(out_data);
            return 1;
        }
        
        unsigned long out_size = strm.total_out;
        deflateEnd(&strm);
        
        /* Save compressed data */
        FILE *out_file = fopen(argv[3], "wb");
        if (!out_file) {
            perror("Failed to open output file");
            free(in_data);
            free(out_data);
            return 1;
        }
        
        if (fwrite(out_data, 1, out_size, out_file) != out_size) {
            perror("Failed to write output file");
            fclose(out_file);
            free(in_data);
            free(out_data);
            return 1;
        }
        
        fclose(out_file);
        free(out_data);
        printf("Compressed %ld bytes to %lu bytes\n", in_size, out_size);
        
    } else {
        /* Decompress */
        unsigned char *out_data = (unsigned char*)malloc(in_size * 10); /* Allocate enough for worst case */
        if (!out_data) {
            perror("Memory allocation failed");
            free(in_data);
            return 1;
        }
        
        /* Initialize for raw inflate */
        if (inflateInit2(&strm, -MAX_WBITS) != Z_OK) {
            printf("inflateInit2 failed\n");
            free(in_data);
            free(out_data);
            return 1;
        }
        
        strm.next_in = in_data;
        strm.avail_in = in_size;
        strm.next_out = out_data;
        strm.avail_out = in_size * 10;
        
        /* Decompress data */
        ret = inflate(&strm, Z_FINISH);
        if (ret != Z_STREAM_END) {
            printf("inflate failed with result %d (error: %s)\n", ret, strm.msg ? strm.msg : "unknown");
            inflateEnd(&strm);
            free(in_data);
            free(out_data);
            return 1;
        }
        
        unsigned long out_size = strm.total_out;
        inflateEnd(&strm);
        
        /* Save decompressed data */
        FILE *out_file = fopen(argv[3], "wb");
        if (!out_file) {
            perror("Failed to open output file");
            free(in_data);
            free(out_data);
            return 1;
        }
        
        if (fwrite(out_data, 1, out_size, out_file) != out_size) {
            perror("Failed to write output file");
            fclose(out_file);
            free(in_data);
            free(out_data);
            return 1;
        }
        
        fclose(out_file);
        free(out_data);
        printf("Decompressed %ld bytes to %lu bytes\n", in_size, out_size);
    }
    
    free(in_data);
    return 0;
}