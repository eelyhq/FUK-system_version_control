#include <stdio.h>
#include <string.h>
#include <unistd.h> // required for getcwd
#include <limits.h> // required for PATH_MAX
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <openssl/evp.h> // required for sha-1 hashing
#include <zlib.h> // required for compressing
#include <fcntl.h>
#include "../../include/init.h"
#include "../../include/z_compressor.h"
#include "../../include/auxiliary_functions.h"
#define  SIXTY_FOUR_KB 65536


int compress_file(const char* file_path, FILE* f_dest, int i)
{
    int ret, flush; // declare variables
    unsigned have;
    z_stream strm;
    unsigned char in[SIXTY_FOUR_KB];
    unsigned char out[SIXTY_FOUR_KB];

    // 1. initialize zlib stream
    strm.zalloc = Z_NULL; // Z_NULL to zlib uses standard C funcs
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    ret = deflateInit(&strm, Z_DEFAULT_COMPRESSION); // say to use default compression argorithm

    if (ret != Z_OK)
    {
        return ret;
    }

    FILE* f_src = fopen(file_path, "rb");

    // 2. compression loop
    long file_size = get_file_size(f_src);

    char header[64];
    int header_len = make_header(header, file_size, i);

    strm.next_in = (unsigned char*)header;
    strm.avail_in = header_len;

    do {
        strm.avail_out = SIXTY_FOUR_KB;
        strm.next_out = out;
        deflate(&strm, Z_NO_FLUSH);
        unsigned have = SIXTY_FOUR_KB - strm.avail_out;
        fwrite(out, 1, have, f_dest);
    } while (strm.avail_out == 0);

    do
    {
        strm.avail_in = fread(in, 1, SIXTY_FOUR_KB, f_src); // read 64 kb of file

        flush = feof(f_src) ? Z_FINISH : Z_NO_FLUSH; // check, have reached the end of file?
        strm.next_in = in; // points, from where take data

        do
        {
            strm.avail_out = SIXTY_FOUR_KB; // write, that have 64 kb buffer
            strm.next_out = out; // points on this buffer
            ret = deflate(&strm, flush); // compress data

            have = SIXTY_FOUR_KB - strm.avail_out;
            if (fwrite(out, 1, have, f_dest) != have || ferror(f_dest)) // write compressed data from out to f_dest
            {
                (void)deflateEnd(&strm);
                return Z_ERRNO;
            }
        } while (strm.avail_out == 0);

    } while (flush != Z_FINISH);

    (void)deflateEnd(&strm);
    fclose(f_src);

    return Z_OK;
}

int decompress_file(const char* file_path, FILE* f_dest, int mode) //mode 1 - with header and 0 - without
{
    int ret;
    unsigned have;
    z_stream strm;
    unsigned char in[SIXTY_FOUR_KB];
    unsigned char out[SIXTY_FOUR_KB];

    // initialize zlib stream
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.next_in = Z_NULL;

    ret = inflateInit(&strm);
    if (ret != Z_OK) {
        return ret;
    }

    FILE* f_src = fopen(file_path, "rb");
    if (!f_src) {
        (void)inflateEnd(&strm);
        return Z_ERRNO;
    }

    // If mode == 0, we need to skip the first line.
    // If mode == 1, skip_header is 0, so we write the decompressed data exactly as is.
    int skip_header = (mode == 0) ? 1 : 0;

    do {
        strm.avail_in = fread(in, 1, SIXTY_FOUR_KB, f_src);

        if (strm.avail_in == 0)
            break;

        strm.next_in = in;

        do {
            strm.avail_out = SIXTY_FOUR_KB;
            strm.next_out = out;

            ret = inflate(&strm, Z_NO_FLUSH);

            // Optional but good practice: catch corruption early
            if (ret == Z_STREAM_ERROR || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
                (void)inflateEnd(&strm);
                fclose(f_src);
                return ret;
            }

            have = SIXTY_FOUR_KB - strm.avail_out;

            if (skip_header) {
                // We are looking for the '\n' in the DECOMPRESSED data
                int found = 0;
                for (unsigned int i = 0; i < have; ++i) {
                    if (out[i] == '\n') {
                        skip_header = 0; // Header skipped, stop skipping for future chunks
                        found = 1;
                        unsigned int write_len = have - (i + 1);

                        // Write everything in this chunk that comes AFTER the newline
                        if (write_len > 0) {
                            if (fwrite(out + i + 1, 1, write_len, f_dest) != write_len || ferror(f_dest)) {
                                (void)inflateEnd(&strm);
                                fclose(f_src);
                                return Z_ERRNO;
                            }
                        }
                        break;
                    }
                }

                if (!found) {
                    // Newline wasn't found in this decompressed chunk.
                    // Skip writing entirely and get the next chunk.
                    continue;
                }
            } else {
                // Normal writing: either we are in mode 1, or the header has already been bypassed
                if (fwrite(out, 1, have, f_dest) != have || ferror(f_dest)) {
                    (void)inflateEnd(&strm);
                    fclose(f_src);
                    return Z_ERRNO;
                }
            }

        } while (strm.avail_out == 0);

    } while (ret != Z_STREAM_END);

    (void)inflateEnd(&strm);
    fclose(f_src);

    return ret == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;
}