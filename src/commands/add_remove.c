#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include <openssl/evp.h>
#include <fcntl.h>
#include <dirent.h>

/* Local project headers */
#include "../../include/auxiliary_functions.h"
#include "../../include/add_remove.h"
#include "../../include/z_compressor.h"

#define  SIXTY_FOUR_KB 65536

// Recursive function for adding files to index
int fuk_add(char* user_input_path, int mode) // mode 0 - silent, 1 - loud
{
    // 1. Create structure with file attributes
    struct stat path_stat;
    stat(user_input_path, &path_stat);

    // 2. Recursive case: if user_input_path - path to directory, enter to it and add all file
    if (S_ISDIR(path_stat.st_mode)) {
        DIR *dir;
        struct dirent *entry;

        dir = opendir(user_input_path);

        // 2.1. Read the directory file by file
        while ((entry = readdir(dir)) != NULL) {

            // 2.2. Every directory contains "." (current dir) and ".." (parent dir)
            // We want to ignore them
            if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..") || !strcmp(entry->d_name, user_input_path) || !strcmp(entry->d_name, ".fuk")) {
                continue;
            }

            char new_path[PATH_MAX];
            sprintf(new_path, "%s/%s", user_input_path, entry->d_name);
            // 2.3. Get new attributes, old forget
            stat(new_path, &path_stat);

            fuk_add(new_path, mode);
        }

        closedir(dir);
    }
    // 3. Base case: path to file
    else if (S_ISREG(path_stat.st_mode)) {
        // 3.1. Resolve environment and check for an existing repository

        char cwd[PATH_MAX];
        getcwd(cwd, sizeof(cwd));

        char root_path[PATH_MAX];
        if (!check_repo_existing(cwd, root_path))
        {
            printf("There is no repository here\n");
            return 1;
        }

        // 3.2 Get path to index
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

        // 3.3. If this file already has been added: check, if this file was modified,
        // replace old hash to new
        int row_number_to_skip = 0;
        while (fscanf(index, "%s : %s", file_in_index, previous_hash) != -1)
        {
            if (!strcmp(user_input_path, file_in_index))
            {
                if (!strcmp(previous_hash, hash_in_hex))
                {
                    printf("File %s has already been added\n", user_input_path);
                    fclose(index);
                    return 1;
                }
                // 3.3.1. Delete row with old file version
                else
                {
                    char path_index_lock[PATH_MAX];
                    sprintf(path_index_lock, "%s/.fuk/index.lock", root_path);

                    // 3.3.2. We can't delete one row, and we have to rewrite all rows without target
                    FILE* index_lock = fopen(path_index_lock, "a");
                    fseek(index, 0, SEEK_SET);

                    char temp_hash[HASH_LEN];
                    while (fscanf(index, "%s : %s", file_in_index, temp_hash) != -1)
                    {
                        if (row_number_to_skip != 0)
                        {
                            fprintf(index_lock, "%s : %s\n", file_in_index, temp_hash);
                        }
                        row_number_to_skip--;
                    }

                    fclose(index_lock);
                    rename(path_index_lock, path_index);
                    break;
                }
            }
            row_number_to_skip++;
        }

        // 3.4. If file doesn't exist yet
        fclose(index);

        char dir_name[3];
        sprintf(dir_name,"%02x", *hash);

        char path[PATH_MAX];
        sprintf(path, "%s/.fuk/objects/", root_path);
        strcat(path, dir_name);

        mkdir(path, 0777);

        char hex_file_name[41] = "/";

        for (int i = 0; i < 19; i++)
        {
            sprintf(hex_file_name + (i * 2) + 1, "%02x", hash[i+1]);
        }

        strcat(path, hex_file_name);

        // 3.4. Create tear
        FILE* tear = fopen(path, "wb");
        compress_file(user_input_path, tear, 0);
        fclose(tear);

        index = fopen(path_index, "a");

        fprintf(index, "%s : %s\n", user_input_path, hash_in_hex); // write file name and hash

        fclose(index);

        if (mode)
        {
            printf("File %s successfully addded\n", user_input_path);
            return 0;
        }

    }
}

// Recursive function for removing files from index
int fuk_remove(char* user_input_path)
{
    // 1. Create structure with file attributes
    struct stat path_stat;
    stat(user_input_path, &path_stat);

    // 2. Recursive case: if user_input_path - path to directory, enter to it and remove all file
    if (S_ISDIR(path_stat.st_mode)) {
        DIR *dir;
        struct dirent *entry;

        dir = opendir(user_input_path);

        // 2.2. Read the directory file by file
        while ((entry = readdir(dir)) != NULL) {

            // 2.3. every directory contains "." (current dir) and ".." (parent dir)
            // we want to ignore them
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0 || !strcmp(entry->d_name, user_input_path) || !strcmp(entry->d_name, ".fuk")) {
                continue;
            }

            char new_path[PATH_MAX];
            sprintf(new_path, "%s/%s", user_input_path, entry->d_name);
            stat(new_path, &path_stat);

            fuk_remove(new_path);
        }

        closedir(dir);
    }
    // 3. Base case: path to file
    else if (S_ISREG(path_stat.st_mode))
    {
        // 3.1. Resolve environment and check for an existing repository

        // This flag shows: would we add this file or this file already exists
        int f = 1;

        char cwd[PATH_MAX];
        getcwd(cwd, sizeof(cwd));

        char root_path[PATH_MAX];
        if (!check_repo_existing(cwd, root_path))
        {
            printf("There is no repo!\n");
            return 1;
        }

        // 3.2 Get path to index
        char path_index[PATH_MAX];
        sprintf(path_index, "%s/.fuk/index", root_path);

        FILE* index = fopen(path_index, "r");

        char file_in_index[NAME_MAX];
        char hash[41];

        // 3.3. Delete rows with removed files from index
        int row_number_to_skip = 0;
        while (fscanf(index, "%s : %s", file_in_index, hash) != -1)
        {
            if (!strcmp(user_input_path, file_in_index))
            {
                char path_index_lock[PATH_MAX];
                sprintf(path_index_lock, "%s/.fuk/index.lock", root_path);

                FILE* index_lock = fopen(path_index_lock, "a");
                fseek(index, 0, SEEK_SET);

                char temp_hash[41];
                while (fscanf(index, "%s : %s", file_in_index, temp_hash) != -1)
                {
                    if (row_number_to_skip != 0)
                    {
                        fprintf(index_lock, "%s : %s\n", file_in_index, temp_hash);
                    }
                    row_number_to_skip--;
                }

                printf("File %s successfully removed\n", user_input_path);

                fclose(index);
                fclose(index_lock);
                rename(path_index_lock, path_index);
                return 0;
            }
            row_number_to_skip++;
        }


        if (index != NULL)
        {
            fclose(index);
        }
        printf("This file does not exist\n");
        return 1;

    }
}