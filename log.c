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
#include "auxiliary_functions.h"
#include "structures.h"

#define BUFFER_SIZE 1024
#define TMP_SIZE 256
#define HASH_LEN 41
#define MAX_FILES 1024

typedef struct tree_entry
{
    char type[10];
    char name[PATH_MAX];
    char hash[HASH_LEN];
}tree_entry;


void print_all_files_in_tree(char* hash, char* root_path, char* nesting, int mode, int i) // mode 0 - all files are deleted, 1 - added
{

    char tree[PATH_MAX];
    sprintf(tree, "%s/.fuk/objects/%.2s/%.38s", root_path, hash, hash + 2);


    char tree_decompressed[PATH_MAX];

    sprintf(tree_decompressed, "%s/.fuk/objects/tree_decompressed%d", root_path, i);
    i++;

    FILE* decompressed_tree = fopen(tree_decompressed, "wb");

    decompress_file(tree, decompressed_tree);

    fclose(decompressed_tree);

    decompressed_tree = fopen(tree_decompressed, "r");

    char tmp[TMP_SIZE];
    fgets(tmp, sizeof(char) * TMP_SIZE, decompressed_tree); // skip headers

    tree_entry current;

    while (fscanf(decompressed_tree, "%s %s : %s", current.type, current.name, current.hash) != -1)
    {
        if (!strcmp(current.type, "tear"))
        {
            if (mode)
            {
                printf("Added file: %s%s/%s\n", root_path, nesting, current.name);
            }
            else
            {
                printf("Deleted file: %s%s/%s\n", root_path, nesting, current.name);
            }
        }
        else
        {
            char new_nesting[PATH_MAX];
            sprintf(new_nesting, "%s/%s", nesting, current.name);
            print_all_files_in_tree(current.hash, root_path, new_nesting, mode, i);
        }
    }

    fclose(decompressed_tree);
}


void compare_trees(char* current_tree_hash, char* comprasion_tree_hash, char* root_path, char* current_nesting, int i)
{
    // create path
    char current_commit_path_tree[PATH_MAX];
    sprintf(current_commit_path_tree, "%s/.fuk/objects/%.2s/%.38s", root_path, current_tree_hash, current_tree_hash + 2);
    char comprasion_commit_path_tree[PATH_MAX];
    sprintf(comprasion_commit_path_tree, "%s/.fuk/objects/%.2s/%.38s", root_path, comprasion_tree_hash, comprasion_tree_hash + 2);

    char current_tree_decompressed[PATH_MAX];
    char comprasion_tree_decompressed[PATH_MAX];

    sprintf(current_tree_decompressed, "%s/.fuk/objects/current_tree_decompressed%d", root_path, i);
    sprintf(comprasion_tree_decompressed, "%s/.fuk/objects/comprasion_tree_decompressed%d", root_path, i);
    i++;

    FILE* current_tree = fopen(current_tree_decompressed,"wb");
    FILE* comprasion_tree = fopen(comprasion_tree_decompressed, "wb");

    decompress_file(current_commit_path_tree, current_tree);
    decompress_file(comprasion_commit_path_tree, comprasion_tree);

    fclose(current_tree);
    fclose(comprasion_tree);

    current_tree = fopen(current_tree_decompressed,"r");
    comprasion_tree = fopen(comprasion_tree_decompressed, "r");

    char tmp[TMP_SIZE];
    fgets(tmp, sizeof(char) * TMP_SIZE, current_tree); // skip headers
    fgets(tmp, sizeof(char) * TMP_SIZE, comprasion_tree);

    tree_entry current;
    tree_entry comprasion;

    int used_rows[MAX_FILES] = {0}; // this array shows rows in comprasion file. id
    int curr_row = 0;
    
    while (fscanf(current_tree, "%s %s : %s", current.type, current.name, current.hash) != -1)
    {

        if (!strcmp(current.type, "tear")) // try to find tear in comparion tree
        {
            curr_row = 0;
            int f = 1;
            while (fscanf(comprasion_tree, "%s %s : %s", comprasion.type, comprasion.name, comprasion.hash) != -1)
            {
                curr_row++;
                if (!strcmp(current.name, comprasion.name)) // we found file with same name
                {
                    used_rows[curr_row] = 1; 
                    if (strcmp(current.hash, comprasion.hash))
                    {
                        printf("Modified file: %s%s/%s\n", root_path, current_nesting, current.name);
                        f = 0;
                        break;
                    }
                    else
                    {
                        printf("Unchanged file: %s%s/%s\n", root_path, current_nesting, current.name);
                        f = 0;
                        break;
                    }
                }
            }
            fseek(comprasion_tree, 0, SEEK_SET);
            fgets(tmp, sizeof(char) * TMP_SIZE, comprasion_tree);
            if (f)
            {
                printf("New file: %s%s/%s\n", root_path, current_nesting, current.name);
            }
        }
        else // this is another tree
        {
            curr_row = 0;
            int f = 1;
            while (fscanf(comprasion_tree, "%s %s : %s", comprasion.type, comprasion.name, comprasion.hash) != -1)
            {
                curr_row++;
                if (!strcmp(current.name, comprasion.name)) // we found file with same name
                {
                    used_rows[curr_row] = 1; 
                    char new_nesting[PATH_MAX];
                    sprintf(new_nesting, "%s/%s", current_nesting, current.name);
                    compare_trees(current.hash, comprasion.hash, root_path, new_nesting, i);
                    // printf("Changed directory: %s%s/%s\n", root_path, current_nesting, current.name);
                    f = 0;
                }
            }
            fseek(comprasion_tree, 0, SEEK_SET);
            fgets(tmp, sizeof(char) * TMP_SIZE, comprasion_tree);
            if (f)
            {
                printf("New directory: %s%s/%s\n", root_path, current_nesting, current.name);
                char new_nesting[PATH_MAX];
                sprintf(new_nesting, "%s/%s", current_nesting, current.name);
                print_all_files_in_tree(current.hash, root_path, new_nesting, 1, i);
            }
        }
    }
    
    
    
    fseek(current_tree, 0, SEEK_SET);
    fgets(tmp, sizeof(char) * TMP_SIZE, current_tree);
    curr_row = 0;
    
    // the same logic, but vice versa
    while (fscanf(comprasion_tree, "%s %s : %s", comprasion.type, comprasion.name, comprasion.hash) != -1)
    {
        curr_row++;
        if (used_rows[curr_row])
        {
            continue;
        }

        if (!strcmp(comprasion.type, "tear")) // try to find tear in comparion tree
        {
            int f = 1;
            while (fscanf(current_tree, "%s %s : %s", current.type, current.name, current.hash) != -1)
            {
                if (!strcmp(current.name, comprasion.name)) // we found file with same name
                {
                    if (strcmp(current.hash, comprasion.hash))
                    {
                        printf("Modified file: %s%s/%s\n", root_path, current_nesting, current.name);
                        f = 0;
                        break;
                    }
                    else
                    {
                        printf("Unchanged file: %s%s/%s\n", root_path, current_nesting, current.name);
                        f = 0;
                        break;
                    }
                }
            }
            fseek(current_tree, 0, SEEK_SET);
            fgets(tmp, sizeof(char) * TMP_SIZE, current_tree);
            if (f)
            {
                printf("New file: %s%s/%s\n", root_path, current_nesting, current.name);
            }
        }
        else // this is another tree
        {
            int f = 1;
            while (fscanf(current_tree, "%s %s : %s", current.type, current.name, current.hash) != -1)
            {
                if (!strcmp(current.name, comprasion.name)) // we found file with same name
                {
                    char new_nesting[PATH_MAX];
                    sprintf(new_nesting, "%s/%s", current_nesting, current.name);
                    compare_trees(current.hash, comprasion.hash, root_path, new_nesting, i);
                    // printf("Changed directory: %s%s/%s\n", root_path, current_nesting, current.name);
                    f = 0;
                }
            }
            fseek(current_tree, 0, SEEK_SET);
            fgets(tmp, sizeof(char) * TMP_SIZE, current_tree);
            if (f)
            {
                printf("Deleted directory: %s%s/%s\n", root_path, current_nesting, comprasion.name);
                char new_nesting[PATH_MAX];
                sprintf(new_nesting, "%s/%s", current_nesting, comprasion.name);
                print_all_files_in_tree(comprasion.hash, root_path, new_nesting, 0, i);
            }
        }
    }
}

void status_fuk() // function for compairing index and tree from last commit
{
    char cwd[PATH_MAX];
    getcwd(cwd, sizeof(cwd));

    char root_path[PATH_MAX];
    char* ce = check_repo_existing(cwd, root_path);

    if (ce == NULL) // F_OK checks for existence
    {
        printf("There is no repo!");
        return;
    }

    // try to find commit with this hash
    char head_path[PATH_MAX];
    sprintf(head_path, "%s/.fuk/HEAD", root_path);

    FILE* head = fopen(head_path, "r");

    char current_branch_path[PATH_MAX];
    fscanf(head, "branch: %s", current_branch_path);
    fclose(head);

    FILE* current_branch = fopen(current_branch_path, "r");
    char current_commit_hash[HASH_LEN];
    fscanf(current_branch, "%s", current_commit_hash);

    if (!strcmp(current_commit_hash, "NULL"))
    {
        printf("There have been no commits yet");
        return;
    }

    char objects_path[PATH_MAX];
    sprintf(objects_path, "%s/.fuk/objects", root_path);

    char current_commit_path[PATH_MAX];
    sprintf(current_commit_path, "%s/.fuk/objects/%.2s/%.38s", root_path, current_commit_hash, current_commit_hash + 2);
    char current_commit_path_tmp[PATH_MAX];
    strcpy(current_commit_path_tmp, objects_path);
    strcat(current_commit_path_tmp, "/current_commit");

    // decompress current commit
    FILE* current_decompressed_commit = fopen(current_commit_path_tmp, "wb");
    decompress_file(current_commit_path, current_decompressed_commit);
    fclose(current_decompressed_commit);

    // this we should  compare with index
    file_tree root;
    build_tree(root_path, &root); // build tree from index
    save_tree(&root, root_path);

    current_decompressed_commit = fopen(current_commit_path_tmp, "r");

    char current_tree_hash[HASH_LEN];

    char tmp[TMP_SIZE];

    // get tree hashes
    fgets(tmp, sizeof(char) * TMP_SIZE, current_decompressed_commit); // skip header
    fgets(tmp, sizeof(char) * TMP_SIZE, current_decompressed_commit);
    sprintf(current_tree_hash, "%s", tmp + 5); // +5, because tree: has len 5

    fclose(current_decompressed_commit);

    char current_nesting[PATH_MAX] = "";
    compare_trees(root.hash, current_tree_hash, root_path, current_nesting, 0);

    return;
}


void fuk_diff(char* hash) // hash of comparing commit
{
    char cwd[PATH_MAX];
    getcwd(cwd, sizeof(cwd));

    char root_path[PATH_MAX];
    char* ce = check_repo_existing(cwd, root_path);

    if (ce == NULL) // F_OK checks for existence
    {
        printf("There is no repo!");
        return;
    }

    // try to find commit with this hash
    char head_path[PATH_MAX];
    sprintf(head_path, "%s/.fuk/HEAD", root_path);

    FILE* head = fopen(head_path, "r");

    char current_branch_path[PATH_MAX];
    fscanf(head, "branch: %s", current_branch_path);
    fclose(head);

    FILE* current_branch = fopen(current_branch_path, "r");
    char current_commit_hash[HASH_LEN];
    fscanf(current_branch, "%s", current_commit_hash);

    if (!strcmp(current_commit_hash, "NULL"))
    {
        printf("There have been no commits yet");
        return;
    }

    char objects_path[PATH_MAX];
    sprintf(objects_path, "%s/.fuk/objects", root_path);

    char current_commit_path[PATH_MAX];
    sprintf(current_commit_path, "%s/.fuk/objects/%.2s/%.38s", root_path, current_commit_hash, current_commit_hash + 2);
    char current_commit_path_tmp[PATH_MAX];
    strcpy(current_commit_path_tmp, objects_path);
    strcat(current_commit_path_tmp, "/current_commit");

    char path_commit_for_comparison[PATH_MAX];
    sprintf(path_commit_for_comparison, "%s/.fuk/objects/%.2s/%.38s", root_path, hash, hash + 2);
    char path_commit_for_comparison_tmp[PATH_MAX];
    strcpy(path_commit_for_comparison_tmp, objects_path);
    strcat(path_commit_for_comparison_tmp, "/comprasion_commit");

    FILE* current_decompressed_commit = fopen(current_commit_path_tmp, "wb");
    decompress_file(current_commit_path, current_decompressed_commit);
    fclose(current_decompressed_commit);

    FILE* comprasion_decompressed_commit = fopen(path_commit_for_comparison_tmp, "wb");
    decompress_file( path_commit_for_comparison, comprasion_decompressed_commit);
    fclose(comprasion_decompressed_commit);

    current_decompressed_commit = fopen(current_commit_path_tmp, "r");
    comprasion_decompressed_commit = fopen(path_commit_for_comparison_tmp, "r");

    char current_tree_hash[HASH_LEN];
    char comprasion_tree_hash[HASH_LEN];

    char tmp[TMP_SIZE];

    // get tree hashes
    fgets(tmp, sizeof(char) * TMP_SIZE, current_decompressed_commit); // skip header
    fgets(tmp, sizeof(char) * TMP_SIZE, current_decompressed_commit);
    sprintf(current_tree_hash, "%s", tmp + 5); // +5, because tree: has len 5

    fgets(tmp, sizeof(char) * TMP_SIZE, comprasion_decompressed_commit); // skip header
    fgets(tmp, sizeof(char) * TMP_SIZE, comprasion_decompressed_commit);
    sprintf( comprasion_tree_hash, "%s", tmp + 5); // +5, because tree: has len 5

    fclose(current_decompressed_commit);
    fclose(comprasion_decompressed_commit);

    char current_nesting[PATH_MAX] = "";
    compare_trees(current_tree_hash, comprasion_tree_hash, root_path, current_nesting, 0);
}


void fuk_log(char* hash, int n)
{
    int f = 0; // flag shows, when current hash equals to target. if argument was missed, f = 1 default and we start from current commit

    if (hash == NULL)
    {
        f = 1;
    }


    char cwd[PATH_MAX];
    getcwd(cwd, sizeof(cwd));

    char root_path[PATH_MAX];
    char* ce = check_repo_existing(cwd, root_path);

    if (ce == NULL) // F_OK checks for existence
    {
        printf("There is no repo!");
        return;
    }


    // try to find commit with this hash
    char head_path[PATH_MAX];
    sprintf(head_path, "%s/.fuk/HEAD", root_path);

    FILE* head = fopen(head_path, "r");

    char current_branch_path[PATH_MAX];
    fscanf(head, "branch: %s", current_branch_path);
    fclose(head);

    FILE* current_branch = fopen(current_branch_path, "r");
    char current_commit_hash[HASH_LEN];
    fscanf(current_branch, "%s", current_commit_hash);

    if (!strcmp(current_commit_hash, "NULL"))
    {
        printf("There have been no commits yet");
        return;
    }

    else if (f || !strcmp(current_commit_hash, hash)) // it means that hash is from first commit
    {
        f = 1;
    }

    char current_commit_path[PATH_MAX];
    char path_decompressed_commit[PATH_MAX];

    char tmp[TMP_SIZE];
    char buffer[BUFFER_SIZE];
    char previous_commit_hash[HASH_LEN];

    do
    {
        sprintf(current_commit_path, "%s/.fuk/objects/%.2s/%.38s", root_path, current_commit_hash, current_commit_hash + 2);

        sprintf(path_decompressed_commit, "%s/.fuk/objects/decompressed_commit", root_path);
        FILE* current_commit = fopen(path_decompressed_commit, "wb");
        decompress_file(current_commit_path, current_commit);
        fclose(current_commit);
        current_commit = fopen(path_decompressed_commit, "r");

        int offset = 0;
        fgets(tmp, TMP_SIZE * sizeof(char), current_commit); // read header
        offset += sprintf(buffer + offset,"%s", tmp);
        fgets(tmp, TMP_SIZE * sizeof(char), current_commit); // read tree
        offset += sprintf(buffer + offset,"%s", tmp);
        fgets(tmp, TMP_SIZE * sizeof(char), current_commit); // read parent
        sscanf(tmp, "parent %s", previous_commit_hash);
        offset += sprintf(buffer + offset,"%s", tmp);
        fgets(tmp, TMP_SIZE * sizeof(char), current_commit); // read date and time
        offset += sprintf(buffer + offset,"%s", tmp);
        fgets(tmp, TMP_SIZE * sizeof(char), current_commit); // read empty string
        fgets(tmp, TMP_SIZE * sizeof(char), current_commit); // read message
        offset += sprintf(buffer + offset,"%s", tmp);

        if (!f)
        {
            if (!strcmp(current_commit_hash, hash))
            {
                printf("%s\n\n", buffer);
                f = 1;
            }
        }
        else
        {
            printf("%s\n\n", buffer);
        }
        n--;
        strcpy(current_commit_hash, previous_commit_hash);
    } while (strcmp(previous_commit_hash, "NULL") && n != 0);

    return;
}



