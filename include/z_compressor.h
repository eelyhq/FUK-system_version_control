#ifndef Z_COMPRESSOR
#define Z_COMPRESSOR
#include <stdio.h>

int compress_file(const char*, FILE*, int );
int decompress_file(const char*, FILE*, int);

#endif