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
#include "commit.h"
#include "structures.h"
#include "z_compressor.h"
#define  SIXTY_FOUR_KB 65536


void init_file_tree(file_tree* f_t)
{
    strcpy(f_t -> name, "root");
    f_t -> is_dir = 1;
    f_t -> hash[0] = '\0'; // the root has no hash
    f_t -> child_count = 0;
}

void save_tree(file_tree* node) // returns hash
{
    // base case, node - file
    if (node -> is_dir == 0)
    {
        return;
    }

    for (int i = 0; i < node -> child_count; i++)
    {
        save_tree(node -> children[i]);
    }

    // build tree in files
    char* buffer = malloc(sizeof(file_tree) * 1000);
    int offset = 0;
    for (int i = 0; i < node -> child_count; i++)
    {
        if (node -> children[i] -> is_dir)
        {
            offset += sprintf(buffer + offset, "tree %s : %s\n", node -> children[i] -> name, node -> children[i] -> hash);
        }
        else
        {
            offset += sprintf(buffer + offset, "tear %s : %s\n", node -> children[i] -> name, node -> children[i] -> hash);
        }
    }

    FILE* tmp = fopen("./.fuk/objects/tmp", "w");

    fprintf(tmp,"%s", buffer);
    free(buffer);

    fclose(tmp);

    unsigned char hash[EVP_MAX_MD_SIZE];

    get_hash("./.fuk/objects/tmp", hash, 1);

    char hash_in_hex[41];

    for (int i = 0; i < 20; i++)
    {
        sprintf(hash_in_hex + (i * 2), "%02x", hash[i]);
    }

    strcpy(node -> hash, hash_in_hex);

    char dir_name[18];
    sprintf(dir_name, "./.fuk/objects/%.2s", hash_in_hex);
    mkdir(dir_name, 0777);

    char new_name[50] = "./.fuk/objects/";
    offset = 15;
    offset += sprintf(new_name + offset, "%.2s/", hash_in_hex);
    sprintf(new_name + offset,"%s", hash_in_hex + 2);

    FILE* f_dest = fopen(new_name, "wb");
    compress_file("./.fuk/objects/tmp", f_dest, 1);

    return;
}

void commit_fuk()
{
    file_tree root;
    init_file_tree(&root);

    char file_name[NAME_MAX];
    char hash_in_hex[41];

    FILE* index = fopen("./.fuk/index", "r");

    int f = 1;
    // build a tree
    while (fscanf(index, "%s : %s", file_name, hash_in_hex) != -1)
    {
        f = 0;
        file_tree* curr_dir = &root ;

        int slash_cnt = cnt_slashes_in_path(file_name);

        char* token;
        token = strtok(file_name, "/");

        while (slash_cnt > 0)
        {
            int f = 1;
            for (int i = 0; i < curr_dir -> child_count; i++)
            {
                if (!strcmp(curr_dir -> children[i] -> name, token))
                {
                    token = strtok(NULL, "/");
                    slash_cnt--;
                    curr_dir = curr_dir -> children[i];
                    f = 0;
                    break;
                }
            }
            if (f)
            {
                file_tree* directory = (file_tree*)malloc(sizeof(file_tree));
                strcpy(directory -> name, token);
                strcpy(directory -> hash, hash_in_hex);
                directory -> is_dir = 1;
                directory -> child_count = 0;
                curr_dir->children[curr_dir->child_count] = directory;
                curr_dir->child_count++;

                token = strtok(NULL, "/");
                slash_cnt--;
                curr_dir = directory;
            }

        }

        file_tree* file = (file_tree*)malloc(sizeof(file_tree));
        strcpy(file -> name, token);
        strcpy(file -> hash, hash_in_hex);
        file -> is_dir = 0;
        curr_dir->children[curr_dir->child_count] = file;
        curr_dir->child_count++;
    }

    if (f)
    {
        printf("No files to commit");
        return;
    }

    save_tree(&root);

    FILE* head = fopen("./.fuk/HEAD", "r");
    char branch[PATH_MAX];
    fscanf(head, "branch: %s", branch);
    fclose(head);

    FILE* parent_commit = fopen(branch, "r");
    char p_c[41];
    fscanf(parent_commit, "%s", p_c);

    char* buffer = malloc(sizeof(char) * 1000);
    if (p_c == NULL)
    {
        sprintf(buffer, "tree %s\nparent", root.hash);
    }
    else
    {
        sprintf(buffer, "tree %s\nparent %s\n", root.hash, p_c);
    }

    FILE* tmp = fopen("./.fuk/objects/tmp", "w");

    fprintf(tmp,"%s", buffer);
    free(buffer);

    fclose(tmp);

    unsigned char hash[EVP_MAX_MD_SIZE];

    get_hash("./.fuk/objects/tmp", hash, 1);

    char hash_in_hex2[41];

    for (int i = 0; i < 20; i++)
    {
        sprintf(hash_in_hex2 + (i * 2), "%02x", hash[i]);
    }


    char dir_name[18];
    sprintf(dir_name, "./.fuk/objects/%.2s", hash_in_hex);
    mkdir(dir_name, 0777);

    char new_name[50] = "./.fuk/objects/";
    int offset = 15;
    offset += sprintf(new_name + offset, "%.2s/", hash_in_hex);
    sprintf(new_name + offset,"%s", hash_in_hex + 2);

    FILE* f_dest = fopen(new_name, "wb");
    compress_file("./.fuk/objects/tmp", f_dest, 2);
}