#include <stdio.h>
#include <string.h>
#include <stdlib.h>
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

#define MAX_MESSAGE_LEN 100
#define  SIXTY_FOUR_KB 65536

int main(const int argc, char** argv) {
    if (argc == 2)
    {
        if (!strcmp(argv[1], "init")) // check, if arg is init
        {
            init_fuk();
        }
    }
    if (argc == 3) {
        if (!strcmp(argv[1], "add"))
        {
            char absolute_path[PATH_MAX];

            if (realpath(argv[2], absolute_path) == NULL)
            {
                printf("File doesn't exists or path is invalid\n");
                return 1;
            }

            add_fuk(absolute_path);
        }
        else if (!strcmp(argv[1], "remove"))
        {
            char absolute_path[PATH_MAX];

            realpath(argv[2], absolute_path);
            remove_fuk(absolute_path);
        }
    }
    if (argc == 4) {
        if (!strcmp(argv[1], "commit"))
        {
            if (!strcmp(argv[2], "-m"))
            {
                commit_fuk(argv[3]);
            }
            else
            {
                printf("Unknown flag");
            }

        }
    }


    return 0;
}

