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
#include "log.h"

#define MAX_MESSAGE_LEN 100
#define  SIXTY_FOUR_KB 65536
#define HASH_LEN 41


int main(const int argc, char** argv) {
    if (argc == 2)
    {
        if (!strcmp(argv[1], "init")) // check, if arg is init
        {
            init_fuk();
        }
    }
    if (argc >= 3) {
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
        else if (!strcmp(argv[1], "diff"))
        {
            fuk_diff(argv[2]);
        }
    }
    if (argc >= 4) {
        if (!strcmp(argv[1], "commit"))
        {
            if (!strcmp(argv[2], "-m"))
            {
                commit_fuk(argv[3]);
            }
            else
            {
                printf("Unknown flag\n");
            }

        }
    }
    if (!strcmp(argv[1], "log"))
    {
        if (argc == 3) // in this case user type only hash of first commit
        {
            char hash[HASH_LEN];
            strcpy(hash, argv[2]);
            fuk_log(hash, -1);
        }
        else if (argc == 4) // in this case user type only number of commits
        {
            int n = (int)strtol(argv[3], NULL, 10);

            if (n == 0)
            {
                printf("Invalid number\n");
            }
            fuk_log(NULL, n);
        }
        else if (argc == 5)
        {
            int n = (int)strtol(argv[4], NULL, 10);

            if (n == 0)
            {
                printf("Invalid number\n");
            }
            char hash[HASH_LEN];
            strcpy(hash, argv[2]);
            fuk_log(hash, n);
        }
        else
        {
            fuk_log(NULL, -1);
        }
    }

    // FILE* test = fopen("./test", "wb");
    // decompress_file("/home/eely/Documents/FUK_project/.fuk/objects/a4/4a2c0c4afd325c932947f808a16295b6bc93a6", test);

    return 0;
}

