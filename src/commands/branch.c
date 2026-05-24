#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <openssl/evp.h>

/* Local project headers */
#include "../../include/branch.h"
#include "../../include/auxiliary_functions.h"

#define MAX_MESSAGE_LEN 100
#define  SIXTY_FOUR_KB 65536
#define HASH_LEN 41

// Function for creating new branch. If this function was called from detached condition - also go to this branch and
// stay in previous branch in another case
int branch_create_fuk(char* new_branch_name, int mode) // mode 1 - default, 0 - detached head
{
    if (strlen(new_branch_name) > 30)
    {
        printf("The branch name is too long, it should not exceed 30 letters\n");
        return 1;
    }

    // 1. Resolve environment and check for an existing repository
    char cwd[PATH_MAX];
    getcwd(cwd, sizeof(cwd));

    char root_path[PATH_MAX];
    char* ce = check_repo_existing(cwd, root_path);

    if (ce == NULL)
    {
        printf("There is no repository here\n");
        return 1;
    }

    char new_branch_path[PATH_MAX];
    sprintf(new_branch_path, "%s/.fuk/refs/heads/%s", root_path, new_branch_name);

    if (!access(new_branch_path, F_OK))
    {
        printf("This branch already exists\n");
        return 1;
    }

    // 2. Create new branch
    if (mode)
    {
        FILE* new_branch_file = fopen(new_branch_path, "w");

        // 2.1. Take current commit hash
        char head_path[PATH_MAX];
        sprintf(head_path, "%s/.fuk/HEAD", root_path);

        FILE* head = fopen(head_path, "r");

        char current_branch_path[PATH_MAX];
        fscanf(head, "branch: %s", current_branch_path);
        fclose(head);

        FILE* current_branch = fopen(current_branch_path, "r");
        char current_commit_hash[HASH_LEN];
        fscanf(current_branch, "%s", current_commit_hash);

        fprintf(new_branch_file, "%s", current_commit_hash);

        fclose(current_branch);
        fclose(new_branch_file);
    }
    // 3. Create new branch and go there
    else
    {
        FILE* new_branch_file = fopen(new_branch_path, "w");

        // 3.1. Take current commit hash
        char head_path[PATH_MAX];
        sprintf(head_path, "%s/.fuk/HEAD", root_path);

        FILE* head = fopen(head_path, "r");

        char commit_hash[HASH_LEN];
        fscanf(head, "commit: %s", commit_hash);
        fclose(head);

        head = fopen(head_path, "w");
        fprintf(head, "branch: %s", new_branch_path);
        fclose(head);

        fprintf(new_branch_file, "%s", commit_hash);

        fclose(new_branch_file);
    }

    return 0;
}

// Function for printing all branches and points current
void print_branches_fuk()
{
    // 1. Resolve environment and check for an existing repository
    char cwd[PATH_MAX];
    getcwd(cwd, sizeof(cwd));

    char root_path[PATH_MAX];
    char* ce = check_repo_existing(cwd, root_path);

    if (ce == NULL) // F_OK checks for existence
    {
        printf("There is no repo!");
        return;
    }

    // 2. Take current commit hash
    char head_path[PATH_MAX];
    sprintf(head_path, "%s/.fuk/HEAD", root_path);

    FILE* head = fopen(head_path, "r");

    char current_branch_path[PATH_MAX];
    fscanf(head, "branch: %s", current_branch_path);
    fclose(head);

    int offset = strlen(".fuk/refs/heads/") + 1;
    offset += strlen(root_path);

    char* pbranch = current_branch_path + offset;

    printf("* %s\n", pbranch);

    DIR *dir;
    struct dirent *entry;

    char path[PATH_MAX];
    sprintf(path, "%s/.fuk/refs/heads/", root_path);

    dir = opendir(path);

    // 3. Read the directory file by file
    while ((entry = readdir(dir)) != NULL) {

        // 3.1. Every directory contains "." (current dir) and ".." (parent dir)
        // we want to ignore them
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0 || !strcmp(entry->d_name, pbranch)) {
            continue;
        }

        // 3.2. entry->d_name contains the name of the file
        printf("%s\n", entry->d_name);
    }

    closedir(dir);
}