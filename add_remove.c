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
#include "auxiliary_functions.h"
#include "add_remove.h"
#include "z_compressor.h"
#define  SIXTY_FOUR_KB 65536


void add_fuk(char* user_input_path)
{
    if (access(user_input_path, F_OK))
    {
        printf("There is no such file here");
        return;
    }

    int f = 1;

    char cwd[PATH_MAX];
    getcwd(cwd, sizeof(cwd));

    char root_path[PATH_MAX];
    if (!check_repo_existing(cwd, root_path))
    {
        printf("There is no repo!");
        return;
    }

    char path_index[PATH_MAX];
    sprintf(path_index, "%s/.fuk/index", root_path);
    FILE* index = fopen(path_index, "r");

    char file_in_index[NAME_MAX];
    char previous_hash[41];

    unsigned char hash[EVP_MAX_MD_SIZE];
    get_hash(user_input_path, hash, 0);

    char hash_in_hex[41];

    for (int i = 0; i < 20; i++)
    {
        sprintf(hash_in_hex + (i * 2), "%02x", hash[i]);
    }


    while (fscanf(index, "%s : %s", file_in_index, previous_hash) != -1)
    {
        if (!strcmp(user_input_path, file_in_index))
        {
            if (!strcmp(previous_hash, hash_in_hex))
            {
                printf("File %s has already been added", user_input_path);
                fclose(index);
                f = 0;
                break;
            }
        }
    }

    if (f)
    {
        fclose(index);


        unsigned char dir_name[2]; // size 2 for first byte and null terminator
        sprintf(dir_name,"%02X", *hash); // make directory with name of first byte of hash
        unsigned char path[PATH_MAX];
        sprintf(path, "%s/.fuk/objects/", root_path);
        strcat(path, dir_name);

        mkdir(path, 0777);

        char hex_file_name[40] = "/"; // 1 byte for slash 19 bytes on name and 1 for terminate null

        for (int i = 0; i < 19; i++)
        {
            sprintf(hex_file_name + (i * 2) + 1, "%02x", hash[i+1]);
        }

        strcat(path, hex_file_name);

        FILE* tear = fopen(path, "wb");
        compress_file(user_input_path, tear, 0);
        fclose(tear);

        FILE* index = fopen(path_index, "a");

        fprintf(index, "%s : %s\n", user_input_path, hash_in_hex); // write file name and hash

        fclose(index);

        printf("File %s successfully addded", user_input_path);
    }
}


void remove_fuk(char* user_input_path)
{

    int f = 1;

    char cwd[PATH_MAX];
    getcwd(cwd, sizeof(cwd));

    char root_path[PATH_MAX];
    if (!check_repo_existing(cwd, root_path))
    {
        printf("There is no repo!");
        return;
    }

    char path_index[PATH_MAX];
    sprintf(path_index, "%s/.fuk/index", root_path);

    FILE* index = fopen(path_index, "r");

    char file_in_index[NAME_MAX];
    char hash[41];
    int row_number_to_skip = 0;
    while (fscanf(index, "%s : %s", file_in_index, hash) != -1)
    {
        if (!strcmp(user_input_path, file_in_index)) // if file really was added
        {
            char path_index_lock[PATH_MAX];
            sprintf(path_index_lock, "%s/.fuk/index.lock", root_path);

            FILE* index_lock = fopen(path_index_lock, "a"); // we can't delete one row, and we had to rewrite all rows without target
            fseek(index, 0, SEEK_SET);

            while (fscanf(index, "%s : %s", file_in_index, hash) != -1)
            {
                if (row_number_to_skip != 0)
                {
                    fprintf(index_lock, "%s : %s\n", file_in_index, hash);
                }
                row_number_to_skip--;
            }

            printf("File %s successfully removed", user_input_path);

            f = 0;
            fclose(index);
            fclose(index_lock);
            rename(path_index_lock, path_index);
            break;
        }
        row_number_to_skip++;
    }

    if (f)
    {
        printf("This file does not exist");
    }
}