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
#include "init.h"
#include "add_remove.h"
#include "commit.h"
#include "z_compressor.h"

#define  SIXTY_FOUR_KB 65536

int main(const int argc, char** argv) {
    // if (argc == 2)
    // {
    //     if (!strcmp(argv[1], "init")) // check, if arg is init
    //     {
    //         init_fuk();
    //     }
    // }
    // if (!strcmp(argv[1], "add"))
    // {
    //     char file_name[NAME_MAX];
    //     memcpy(file_name, argv[2], sizeof(char) * strlen(argv[2]));
    //     add_fuk(file_name);
    // }
    // else if (!strcmp(argv[1], "remove"))
    // {
    //     char file_name[NAME_MAX];
    //     memcpy(file_name, argv[2], sizeof(char) * strlen(argv[2]));
    //     remove_fuk(file_name);
    // }
    // else if (!strcmp(argv[1], "commit"))
    // {
    //     commit_fuk();
    // }


    FILE* dest = fopen("./test", "wb");

    decompress_file("/home/eely/Documents/FUK_project/.fuk/objects/a5/b568cd86e36a50786ddcda435f444f2f4b26be", dest);

    fclose(dest);


    return 0;
}

