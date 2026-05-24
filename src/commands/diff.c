#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <openssl/evp.h>

/* Local project headers */
#include "../../include/z_compressor.h"
#include "../../include/auxiliary_functions.h"
#include "../../include/structures.h"
#include "../../include/diff.h"

#define TMP_SIZE 256

#define COLOR_RESET   "\x1b[0m"
#define COLOR_RED     "\x1b[31m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_BOLD    "\x1b[1m"
#define COLOR_CYAN    "\x1b[36m"

// Function for comparing two commits
int fuk_diff(char* commit1_arg, char* commit2_arg)
{
    // 1. Define, user write two commits or one
    char hash1[HASH_LEN];
    argument_to_hash(commit1_arg, hash1);

    // 2. Resolve environment and check for an existing repository
    char cwd[PATH_MAX];
    getcwd(cwd, sizeof(cwd));

    char root_path[PATH_MAX];
    char* ce = check_repo_existing(cwd, root_path);

    if (ce == NULL)
    {
        printf("There is no repository here\n");
        return 0;
    }

    char hash2[HASH_LEN];

    // 3. If second commit missed, take it form HEAD
    if (commit2_arg == NULL || strlen(commit2_arg) == 0)
    {
        char head_path[PATH_MAX];
        sprintf(head_path, "%s/.fuk/HEAD", root_path);

        FILE* head = fopen(head_path, "r");

        char current_branch_path[PATH_MAX];
        fscanf(head, "branch: %s", current_branch_path);
        fclose(head);

        FILE* current_branch = fopen(current_branch_path, "r");


        fscanf(current_branch, "%s", hash2);
        fclose(current_branch);

        if (!strcmp(hash2, "NULL"))
        {
            printf("There have been no commits yet\n");
            return 0;
        }
    }
    else
    {
        argument_to_hash(commit2_arg, hash2);
    }

    // 4. Decompress both commits
    char objects_path[PATH_MAX];
    sprintf(objects_path, "%s/.fuk/objects", root_path);

    char current_commit_path[PATH_MAX];
    sprintf(current_commit_path, "%s/.fuk/objects/%.2s/%.38s", root_path, hash2, hash2 + 2);
    char current_commit_path_tmp[PATH_MAX];
    strcpy(current_commit_path_tmp, objects_path);
    strcat(current_commit_path_tmp, "/current_commit");

    char path_commit_for_comparison[PATH_MAX];
    sprintf(path_commit_for_comparison, "%s/.fuk/objects/%.2s/%.38s", root_path, hash1, hash1 + 2);
    char path_commit_for_comparison_tmp[PATH_MAX];
    strcpy(path_commit_for_comparison_tmp, objects_path);
    strcat(path_commit_for_comparison_tmp, "/comprasion_commit");

    FILE* current_decompressed_commit = fopen(current_commit_path_tmp, "wb");
    decompress_file(current_commit_path, current_decompressed_commit, 1);
    fclose(current_decompressed_commit);

    FILE* comprasion_decompressed_commit = fopen(path_commit_for_comparison_tmp, "wb");
    decompress_file(path_commit_for_comparison, comprasion_decompressed_commit, 1);
    fclose(comprasion_decompressed_commit);

    current_decompressed_commit = fopen(current_commit_path_tmp, "r");
    comprasion_decompressed_commit = fopen(path_commit_for_comparison_tmp, "r");

    char current_tree_hash[HASH_LEN];
    char comprasion_tree_hash[HASH_LEN];
    char tmp[TMP_SIZE];

    // 5. Get tree hashes
    fgets(tmp, sizeof(char) * TMP_SIZE, current_decompressed_commit); // skip header
    fgets(tmp, sizeof(char) * TMP_SIZE, current_decompressed_commit);
    sprintf(current_tree_hash, "%s", tmp + 5);

    fgets(tmp, sizeof(char) * TMP_SIZE, comprasion_decompressed_commit); // skip header
    fgets(tmp, sizeof(char) * TMP_SIZE, comprasion_decompressed_commit);
    sprintf(comprasion_tree_hash, "%s", tmp + 5);

    fclose(current_decompressed_commit);
    fclose(comprasion_decompressed_commit);
    remove(current_commit_path_tmp);
    remove(path_commit_for_comparison_tmp);

    char current_nesting[PATH_MAX] = "";
    int* flag = (int*)malloc(sizeof(int));
    *flag = 0;

    // 6. comprasion_tree_hash — old condition (commit1), current_tree_hash — new (commit2)
    compare_trees(current_tree_hash, comprasion_tree_hash, root_path, current_nesting, 0, COLOR_GREEN, 1, flag);

    free(flag);
}
