#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <openssl/evp.h>

/* Local project headers */
#include "../../include/add_remove.h"
#include "../../include/z_compressor.h"
#include "../../include/auxiliary_functions.h"

#define HASH_LEN 41
#define TMP_SIZE 256

#define COLOR_RESET   "\x1b[0m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_RED     "\x1b[31m"
#define COLOR_CYAN    "\x1b[36m"

// Function for comparing index and tree from last commit
int fuk_status()
{
    // 1. Resolve environment and check for an existing repository
    char cwd[PATH_MAX];
    getcwd(cwd, sizeof(cwd));

    char root_path[PATH_MAX];
    char* ce = check_repo_existing(cwd, root_path);


    // 2. Abort if the repository is missing
    if (ce == NULL)
    {
        printf("There is no repository here\n");
        return 1;
    }


    // 3. Create path to HEAD file and read current branch
    char head_path[PATH_MAX];
    sprintf(head_path, "%s/.fuk/HEAD", root_path);

    FILE* head = fopen(head_path, "r");

    char current_branch_path[PATH_MAX];
    fscanf(head, "branch: %s", current_branch_path);
    fclose(head);

    // 4. Open current branch and read current commit hash
    FILE* current_branch = fopen(current_branch_path, "r");
    char current_commit_hash[HASH_LEN];
    fscanf(current_branch, "%s", current_commit_hash);
    fclose(current_branch);

    // 5. Abort, if there is no commit, because index should compare with current commit
    if (!strcmp(current_commit_hash, "NULL"))
    {
        printf("There have been no commits yet\n");
        return 1;
    }

    // 6. Make path to compressed commit and decompress it
    char objects_path[PATH_MAX];
    sprintf(objects_path, "%s/.fuk/objects", root_path);

    char current_commit_path[PATH_MAX];
    sprintf(current_commit_path, "%s/.fuk/objects/%.2s/%.38s", root_path, current_commit_hash, current_commit_hash + 2);
    char current_commit_path_tmp[PATH_MAX];
    strcpy(current_commit_path_tmp, objects_path);
    strcat(current_commit_path_tmp, "/current_commit");

    FILE* current_decompressed_commit = fopen(current_commit_path_tmp, "wb");
    decompress_file(current_commit_path, current_decompressed_commit, 1);
    fclose(current_decompressed_commit);

    // 7. Create tree from index
    file_tree root;
    build_tree(root_path, &root);
    save_tree(&root, root_path);

    current_decompressed_commit = fopen(current_commit_path_tmp, "r");

    char current_tree_hash[HASH_LEN];
    char tmp[TMP_SIZE];

    // 8. Get tree hash from current commit
    fgets(tmp, sizeof(char) * TMP_SIZE, current_decompressed_commit); // skip header
    fgets(tmp, sizeof(char) * TMP_SIZE, current_decompressed_commit);
    sprintf(current_tree_hash, "%s", tmp + 5); // +5, because tree: has len 5
    fclose(current_decompressed_commit);

    // 9. Compare index tree with last commit
    char current_nesting[PATH_MAX] = "";
    printf("\n%sChanges to be committed:%s\n", COLOR_GREEN, COLOR_RESET);
    int* flag = (int*)malloc(sizeof(int));
    compare_trees(root.hash, current_tree_hash, root_path, current_nesting, 0, COLOR_GREEN , 1, flag);

    // 10. Ready to compare all files from directory of reposiory
    char index_path[PATH_MAX];
    char index_path_tmp[PATH_MAX];
    sprintf(index_path, "%s/.fuk/index", root_path);
    sprintf(index_path_tmp, "%s/.fuk/index_tmp", root_path);

    // 11. Rename index to add all file in index and build tree to compare index tree and tree from all files
    rename(index_path, index_path_tmp);

    FILE* new_index = fopen(index_path, "w");
    fclose(new_index);

    fuk_add(root_path, 0); // add all files in root repo directory

    file_tree all_files;

    build_tree(root_path, &all_files);
    save_tree(&all_files, root_path);

    // 12. Compare index tree and tree from all files. Use different status_mode
    printf("\n%sChanges not staged for commit:%s\n", COLOR_RED, COLOR_RESET);
    compare_trees(all_files.hash, root.hash, root_path, current_nesting, 0, COLOR_RED, 2, flag);

    printf("\n%sUntracked files:%s\n", COLOR_CYAN, COLOR_RESET);
    compare_trees(all_files.hash, root.hash, root_path, current_nesting, 0, COLOR_CYAN, 3, flag);

    // 13. Return index state
    rename(index_path_tmp, index_path);

    free(flag);

    return 0;
}