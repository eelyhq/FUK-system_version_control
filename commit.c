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
#include <time.h>
#include "z_compressor.h"
#define  SIXTY_FOUR_KB 65536


void init_file_tree(file_tree* f_t)
{
    strcpy(f_t -> name, "root");
    f_t -> is_dir = 1;
    f_t -> hash[0] = '\0'; // the root has no hash
    f_t -> child_count = 0;
}

void save_tree(file_tree* node, char* root_path) // returns hash
{
    // base case, node - file
    if (node -> is_dir == 0)
    {
        return;
    }

    for (int i = 0; i < node -> child_count; i++)
    {
        save_tree(node -> children[i], root_path);
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

    char path_tmp[PATH_MAX];
    sprintf(path_tmp, "%s/.fuk/objects/tmp", root_path);
    FILE* tmp = fopen(path_tmp, "w");

    fprintf(tmp,"%s", buffer);
    free(buffer);

    fclose(tmp);

    unsigned char hash[EVP_MAX_MD_SIZE];

    get_hash(path_tmp, hash, 1);

    char hash_in_hex[41];

    for (int i = 0; i < 20; i++)
    {
        sprintf(hash_in_hex + (i * 2), "%02x", hash[i]);
    }

    strcpy(node -> hash, hash_in_hex);

    char dir_name[PATH_MAX];
    sprintf(dir_name, "%s/.fuk/objects/%.2s",root_path, hash_in_hex);
    mkdir(dir_name, 0777);

    char new_name[PATH_MAX];
    sprintf(new_name, "%s/%s", dir_name, hash_in_hex + 2);

    FILE* f_dest = fopen(new_name, "wb");
    compress_file(path_tmp, f_dest, 1);

    return;
}

void commit_fuk(char* message)
{
    char cwd[PATH_MAX];
    getcwd(cwd, sizeof(cwd)); // get current directory, to go up and search .fuk in higher dirs

    char root_path[PATH_MAX];
    char* re = check_repo_existing(cwd, root_path);

    if (re == NULL) // F_OK checks for existence
    {
        printf("There is no repo!");
        return;
    }

    file_tree root;
    init_file_tree(&root);

    char file_name[NAME_MAX];
    char hash_in_hex[41];

    char path_index[PATH_MAX];
    sprintf(path_index, "%s/.fuk/index", root_path);
    FILE* index = fopen(path_index, "r");

    int f = 1;
    int root_path_len = (int)strlen(root_path);
    // build a tree
    while (fscanf(index, "%s : %s", file_name, hash_in_hex) != -1)
    {
        char* relative_path = file_name + root_path_len + 1;
        f = 0;

        file_tree* curr_dir = &root ;

        int slash_cnt = cnt_slashes_in_path(relative_path);

        char* token;
        token = strtok(relative_path, "/");

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

    save_tree(&root, root_path);

    char head_path[PATH_MAX];
    sprintf(head_path, "%s/.fuk/HEAD", root_path);

    FILE* head = fopen(head_path, "r");
    char branch[PATH_MAX];
    fscanf(head, "branch: %s", branch);
    fclose(head);

    FILE* parent_commit = fopen(branch, "rw");
    char p_c[41];
    fscanf(parent_commit, "%s", p_c);
    fclose(parent_commit);

    // the p_c variable contains hash of previous commit, and we have to compare current tree with
    // previous and if they are equals, we have to decline commit and message, that there is nothing to do
    if (strcmp(p_c, "NULL"))
    {
        char path_to_commit[PATH_MAX];
        sprintf(path_to_commit, "%s/.fuk/objects/%.2s/%.38s", root_path, p_c, p_c+2);

        char path_to_decompressed_previous_commit[PATH_MAX];
        sprintf(path_to_decompressed_previous_commit, "%s/.fuk/objects/decompressed_commit", root_path);
        FILE* decompressed_previous_commit = fopen(path_to_decompressed_previous_commit, "wb");
        decompress_file(path_to_commit, decompressed_previous_commit);
        fclose(decompressed_previous_commit);
        decompressed_previous_commit = fopen(path_to_decompressed_previous_commit, "r");

        char previous_tree_hash[41];

        char temp[1001];
        fscanf(decompressed_previous_commit, "commit: %s\ntree %s\n", temp, previous_tree_hash);
        fclose(decompressed_previous_commit);

        if (!strcmp(previous_tree_hash, root.hash))
        {
            printf("Nothing to commit, working tree clean");
            return;
        }
    }

    char* buffer = malloc(sizeof(char) * 1000);

    time_t now = time(NULL);         // get current time
    struct tm *t = localtime(&now);  // convert to local time structure

    sprintf(buffer, "tree %s\nparent %s\ndate and time: %d.%d.%d %d:%d\n\nmessage: %s", root.hash, p_c, t->tm_mday, t->tm_mon + 1, t->tm_year + 1900, t->tm_hour, t->tm_min, message);

    char path_tmp[PATH_MAX];
    sprintf(path_tmp, "%s/.fuk/objects/tmp", root_path);

    FILE* tmp = fopen(path_tmp, "w");

    fprintf(tmp,"%s", buffer);
    free(buffer);
    fclose(tmp);

    unsigned char hash[EVP_MAX_MD_SIZE];

    get_hash(path_tmp, hash, 1);

    char hash_in_hex2[41];

    for (int i = 0; i < 20; i++)
    {
        sprintf(hash_in_hex2 + (i * 2), "%02x", hash[i]);
    }


    char dir_name[PATH_MAX];
    sprintf(dir_name, "%s/.fuk/objects/%.2s",root_path, hash_in_hex2);
    mkdir(dir_name, 0777);

    char new_name[PATH_MAX];
    int offset = sprintf(new_name, "%s/.fuk/objects/", root_path);
    offset += sprintf(new_name + offset, "%.2s/", hash_in_hex2);
    sprintf(new_name + offset,"%s", hash_in_hex2 + 2);

    FILE* f_dest = fopen(new_name, "wb");
    compress_file(path_tmp, f_dest, 2);
    fclose(f_dest);
    remove(path_tmp);

    FILE* parent_commit2 = fopen(branch, "w");
    fprintf(parent_commit, "%s", hash_in_hex2);

    fclose(parent_commit2);

    return;
}