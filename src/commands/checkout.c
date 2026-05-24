#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <openssl/evp.h>
#include "../../include/z_compressor.h"
#include "../../include/auxiliary_functions.h"
#include "../../include/structures.h"

#define BUFFER_SIZE 1024
#define TMP_SIZE 256
#define MAX_FILES 1024


// Function for checkout all from tree
void recursive_checkout(char* root_path, char* tree_hash, char* nesting)
{
    // 1. Get decompressed tree
    char tree_path[PATH_MAX];
    sprintf(tree_path, "%s/.fuk/objects/%.2s/%.38s", root_path, tree_hash, tree_hash + 2);

    char decompressed_tree_path[PATH_MAX];
    sprintf(decompressed_tree_path, "%s/.fuk/objects/decompressed_tree", root_path);
    FILE* tree_file = fopen(decompressed_tree_path, "wb");

    decompress_file(tree_path, tree_file, 1);
    fclose(tree_file);

    tree_file = fopen(decompressed_tree_path, "r");

    char tmp[TMP_SIZE];
    fgets(tmp, sizeof(char) * TMP_SIZE, tree_file); // skip header

    tree_entry tree;

    // 2. Read row by row
    while (fscanf(tree_file, "%s %s : %s", tree.type, tree.name, tree.hash) != -1 )
    {
        // 2.1. Recursive case: it is subdir. Checkout all from it
        if (!strcmp(tree.type, "tree"))
        {
            sprintf(nesting, "%s/%s", nesting, tree.name); // convert path to subdir
            recursive_checkout(root_path, tree.hash, nesting);
        }
        // 2.2. Base case: it is file, checkout it
        else
        {
            char file_path[PATH_MAX];
            sprintf(file_path, "%s/%s/%s", root_path, nesting, tree.name);
            FILE* checkout_file = fopen(file_path, "wb");

            char path_to_compressed_file[PATH_MAX];
            sprintf(path_to_compressed_file, "%s/.fuk/objects/%.2s/%.38s", root_path, tree.hash, tree.hash + 2 );

            decompress_file(path_to_compressed_file, checkout_file, 0);

            fclose(checkout_file);
        }
    }
}

// Function for checkout all from branch or commit
int fuk_checkout_all(char* argument)
{
    // 1. Check, there are exists uncommited changes
    char hash[HASH_LEN];
    argument_to_hash(argument, hash);

    int f = prevent_data_lose();

    if (f)
    {
        return 1;
    }

    // 2. Resolve environment and check for an existing repository
    char cwd[PATH_MAX];
    getcwd(cwd, sizeof(cwd));

    char root_path[PATH_MAX];
    char* ce = check_repo_existing(cwd, root_path);

    int is_branch = 0;
    char branch_path[PATH_MAX] = "";

    // 3. If the argument is different from the resolved hash, it is a branch name
    if (strcmp(argument, hash))
    {
        is_branch = 1;
        sprintf(branch_path, "%s/.fuk/branches/%s", root_path, argument);
    }

    // 4. Get commit hash
    char head_path[PATH_MAX];
    sprintf(head_path, "%s/.fuk/HEAD", root_path);

    FILE* head = fopen(head_path, "r");

    char head_type[32];
    char head_val[PATH_MAX];
    fscanf(head, "%s %s", head_type, head_val);
    fclose(head);

    char current_commit_hash[HASH_LEN];
    if (strcmp(head_type, "branch:") == 0)
    {
        FILE* current_branch = fopen(head_val, "r");
        if (current_branch) {
            fscanf(current_branch, "%s", current_commit_hash);
            fclose(current_branch);
        }
    }
    else if (strcmp(head_type, "commit:") == 0)
    {
        strcpy(current_commit_hash, head_val);
    }


    if (!strcmp(current_commit_hash, "NULL"))
    {
        printf("There have been no commits yet\n");
        return 1;
    }

    if (f || !strcmp(current_commit_hash, hash)) // it means that hash is from first commit
    {
        f = 1;
    }

    char current_commit_path[PATH_MAX];
    char path_decompressed_commit[PATH_MAX];

    char tmp[TMP_SIZE];
    char buffer[BUFFER_SIZE];
    char previous_commit_hash[HASH_LEN];
    char tree_hash[HASH_LEN];

    do
    {
        sprintf(current_commit_path, "%s/.fuk/objects/%.2s/%.38s", root_path, current_commit_hash, current_commit_hash + 2);

        sprintf(path_decompressed_commit, "%s/.fuk/objects/decompressed_commit", root_path);
        FILE* current_commit = fopen(path_decompressed_commit, "wb");
        decompress_file(current_commit_path, current_commit, 1);
        fclose(current_commit);
        current_commit = fopen(path_decompressed_commit, "r");

        int offset = 0;
        fgets(tmp, TMP_SIZE * sizeof(char), current_commit); // read header
        offset += sprintf(buffer + offset,"%s", tmp);
        fgets(tmp, TMP_SIZE * sizeof(char), current_commit); // read tree
        offset += sprintf(buffer + offset,"%s", tmp);
        sscanf(tmp, "tree %s", tree_hash);
        fgets(tmp, TMP_SIZE * sizeof(char), current_commit); // read parent
        sscanf(tmp, "parent %s", previous_commit_hash);

        if (f || !strcmp(current_commit_hash, hash))
        {
            f = 1;
            break;
        }

        strcpy(current_commit_hash, previous_commit_hash);
    } while (strcmp(previous_commit_hash, "NULL"));

    if (!f)
    {
        printf("There is no commit with such a hash\n");
        return 1;
    }

    char nesting[PATH_MAX] = "";

    recursive_checkout(root_path, tree_hash, nesting);

    head = fopen(head_path, "w");
    if (is_branch)
    {
        fprintf(head, "branch: %s\n", branch_path);
        printf("Switched to branch '%s'\n", argument);
    }
    else
    {
        fprintf(head, "commit: %s\n", hash);
        printf("Note: switching to '%s'.\nYou are in 'detached HEAD' state.\n", hash);
    }
    fclose(head);
}

int fuk_checkout(char* argument,  char* file_path)
{
    char hash[HASH_LEN];
    argument_to_hash(argument, hash);

    char cwd[PATH_MAX];
    getcwd(cwd, sizeof(cwd));

    if (access(file_path, F_OK))
    {
        printf("There is no such file here\n");
        return 1;
    }

    char root_path[PATH_MAX];
    char* ce = check_repo_existing(cwd, root_path);

    char* file;

    if (file_path[0] == '/') // it means, that user write absolute path
    {
        if (strlen(root_path) > strlen(file_path))
        {
            printf("Invalid path/file name\n");
            return 1;
        }
        //skip absolute part
        file = file_path + strlen(root_path) + 1;
    }
    else
    {
        file = file_path;
    }

   int f = 0; // flag shows, when current hash equals to target. if argument was missed, f = 1 default and we start from current commit

    if (hash == NULL)
    {
        f = 1;
    }

    if (ce == NULL) // F_OK checks for existence
    {
        printf("There is no repository here\n");
        return 1;
    }

    // try to find commit with this hash
    char head_path[PATH_MAX];
    sprintf(head_path, "%s/.fuk/HEAD", root_path);

    FILE* head = fopen(head_path, "r");

    char head_type[32];
    char head_val[PATH_MAX];
    fscanf(head, "%s %s", head_type, head_val);
    fclose(head);

    char current_commit_hash[HASH_LEN];
    if (strcmp(head_type, "branch:") == 0)
    {
        FILE* current_branch = fopen(head_val, "r");
        if (current_branch) {
            fscanf(current_branch, "%s", current_commit_hash);
            fclose(current_branch);
        }
    }
    else if (strcmp(head_type, "commit:") == 0)
    {
        strcpy(current_commit_hash, head_val);
    }

    if (!strcmp(current_commit_hash, "NULL"))
    {
        printf("There have been no commits yet\n");
        return 1;
    }

    if (f || !strcmp(current_commit_hash, hash)) // it means that hash is from first commit
    {
        f = 1;
    }

    char current_commit_path[PATH_MAX];
    char path_decompressed_commit[PATH_MAX];

    char tmp[TMP_SIZE];
    char buffer[BUFFER_SIZE];
    char previous_commit_hash[HASH_LEN];
    char tree_hash[HASH_LEN];


    do
    {
        sprintf(current_commit_path, "%s/.fuk/objects/%.2s/%.38s", root_path, current_commit_hash, current_commit_hash + 2);

        sprintf(path_decompressed_commit, "%s/.fuk/objects/decompressed_commit", root_path);
        FILE* current_commit = fopen(path_decompressed_commit, "wb");
        decompress_file(current_commit_path, current_commit, 1);
        fclose(current_commit);
        current_commit = fopen(path_decompressed_commit, "r");

        int offset = 0;
        fgets(tmp, TMP_SIZE * sizeof(char), current_commit); // read header
        offset += sprintf(buffer + offset,"%s", tmp);
        fgets(tmp, TMP_SIZE * sizeof(char), current_commit); // read tree
        offset += sprintf(buffer + offset,"%s", tmp);
        sscanf(tmp, "tree %s", tree_hash);
        fgets(tmp, TMP_SIZE * sizeof(char), current_commit); // read parent
        sscanf(tmp, "parent %s", previous_commit_hash);

        if (f || !strcmp(current_commit_hash, hash))
        {
            f = 1;
            break;
        }

        strcpy(current_commit_hash, previous_commit_hash);
    } while (strcmp(previous_commit_hash, "NULL"));

    if (!f)
    {
        printf("There is no commit with such a hash\n");
        return 1;
    }
    else
    {
        char tree_path[PATH_MAX];
        sprintf(tree_path, "%s/.fuk/objects/%.2s/%.38s", root_path, tree_hash,tree_hash + 2);

        char decompressed_tree_path[PATH_MAX];
        sprintf(decompressed_tree_path, "%s/.fuk/objects/decompressed_tree", root_path);
        FILE* tree_file = fopen(decompressed_tree_path, "wb");

        decompress_file(tree_path, tree_file, 1);
        fclose(tree_file);

        tree_file = fopen(decompressed_tree_path, "r");

        char tmp[TMP_SIZE];
        fgets(tmp, sizeof(char) * TMP_SIZE, tree_file); // skip header


        tree_entry tree;

        // search in tree, try to enter into target dir
        while (cnt_slashes_in_path(file))
        {
            int offset = 0;
            char dir_name[PATH_MAX];
            while (file[offset] != '/')
            {
                dir_name[offset] = file[offset];
                offset++;
            }

            file += offset + 1;

            while (fscanf(tree_file, "%s %s : %s", tree.type, tree.name, tree.hash) != -1)
            {
                if (!strcmp(tree.type, "tree") && !strcmp(tree.name, dir_name))
                {
                    sprintf(tree_path, "%s/.fuk/objects/%.2s/%.38s", root_path, tree.hash, tree.hash + 2);

                    sprintf(decompressed_tree_path, "%s/.fuk/objects/decompressed_tree", root_path);
                    tree_file = fopen(decompressed_tree_path, "wb");

                    decompress_file(tree_path, tree_file, 1);
                    fclose(tree_file);

                    tree_file = fopen(decompressed_tree_path, "r");
                    fgets(tmp, sizeof(char) * TMP_SIZE, tree_file); // skip header
                    break;
                }
            }
        }

        while (fscanf(tree_file, "%s %s : %s", tree.type, tree.name, tree.hash) != -1)
        {
            // it means that we are found target file
            if (!strcmp(tree.name, file))
            {
                FILE* checkout_file = fopen(file_path, "wb");

                char path_to_compressed_file[PATH_MAX];
                sprintf(path_to_compressed_file, "%s/.fuk/objects/%.2s/%.38s", root_path, tree.hash, tree.hash + 2 );

                decompress_file(path_to_compressed_file, checkout_file, 0);

                fclose(checkout_file);
                return 0;
            }
        }
    }
}