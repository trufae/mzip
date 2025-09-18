#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <zlib.h>

int main() {
    unsigned char hello[] = "hello\n";
    z_stream strm = {0};
    unsigned char compressed[100] = {0};
    
    strm.next_in = hello;
    strm.avail_in = 6;
    strm.next_out = compressed;
    strm.avail_out = 100;
    
    deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY);
    deflate(&strm, Z_FINISH);
    deflateEnd(&strm);
    
    unsigned int comp_size = strm.total_out;
    printf("Original: %s", hello);
    printf("Compressed size: %u\n", comp_size);
    
    unsigned char decompressed[10] = {0};
    z_stream dstrm = {0};
    dstrm.next_in = compressed;
    dstrm.avail_in = comp_size;
    dstrm.next_out = decompressed;
    dstrm.avail_out = 10;
    
    inflateInit2(&dstrm, -MAX_WBITS);
    int ret = inflate(&dstrm, Z_FINISH);
    inflateEnd(&dstrm);
    
    printf("Decompressed: %s", decompressed);
    printf("Return code: %d\n", ret);
    
    return 0;
}
