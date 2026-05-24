#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <openssl/evp.h>

/* Local project headers */
#include "../../include/z_compressor.h"
#include "../../include/log.h"
#include "../../include/auxiliary_functions.h"
#include "../../include/structures.h"

#define BUFFER_SIZE 1024
#define TMP_SIZE 256

#define MAX_FILES 1024
#define COLOR_RESET   "\x1b[0m"
#define COLOR_RED     "\x1b[31m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_BOLD    "\x1b[1m"
#define COLOR_CYAN    "\x1b[36m"

// Function for printing commit logs
int fuk_log(char* argument1, int n, char* argument2) // all arguments are optional
{
    // 1. Define, what arguments are given. Probably branches
    char hash[HASH_LEN];
    argument_to_hash(argument1, hash);

    char hash2[HASH_LEN];
    argument_to_hash(argument2, hash2);

    // flag shows, when current hash equals to target. if argument was missed, f = 1 default and we start from current commit
    int f = 0;

    // 2. Resolve environment and check for an existing repository
    if (strlen(hash) == 0)
    {
        f = 1;
    }

    char cwd[PATH_MAX];
    getcwd(cwd, sizeof(cwd));

    char root_path[PATH_MAX];
    char* ce = check_repo_existing(cwd, root_path);

    if (ce == NULL) // F_OK checks for existence
    {
        printf("There is no repository here\n");
        return 1;
    }

    // 3. Try to find commit with this hash
    char head_path[PATH_MAX];
    sprintf(head_path, "%s/.fuk/HEAD", root_path);

    FILE* head = fopen(head_path, "r");

    char current_branch_path[PATH_MAX];
    fscanf(head, "branch: %s", current_branch_path);
    fclose(head);

    FILE* current_branch = fopen(current_branch_path, "r");
    char current_commit_hash[HASH_LEN];
    fscanf(current_branch, "%s", current_commit_hash);
    fclose(current_branch);

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

    // 4. Go from first commit to or end or on n commits back. or go from hash2 commit to hash1
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
        fgets(tmp, TMP_SIZE * sizeof(char), current_commit); // read parent
        sscanf(tmp, "parent %s", previous_commit_hash);
        offset += sprintf(buffer + offset,"%s", tmp);
        fgets(tmp, TMP_SIZE * sizeof(char), current_commit); // read date and time
        offset += sprintf(buffer + offset,"%s", tmp);
        fgets(tmp, TMP_SIZE * sizeof(char), current_commit); // read empty string
        fgets(tmp, TMP_SIZE * sizeof(char), current_commit); // read message
        offset += sprintf(buffer + offset,"%s", tmp);
        
        fclose(current_commit);

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
    } while (strcmp(previous_commit_hash, "NULL") && n != 0 && strcmp(previous_commit_hash, hash2));

    return 0;
}




