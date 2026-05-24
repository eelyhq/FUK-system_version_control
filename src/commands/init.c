#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include <openssl/evp.h>
#include <fcntl.h>

/* Local project headers */
#include "../../include/init.h"
#include "../../include/auxiliary_functions.h"


// Function for initialize repository
int fuk_init()
{
    // 1. Resolve environment and check for an existing repository
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("Failed to get current working directory");
        return 1;
    }

    char root_path[PATH_MAX];
    char* is_existing_repo = check_repo_existing(cwd, root_path);

    // 2. Abort if a repository is already initialized
    if (is_existing_repo != NULL)
    {
        fprintf(stderr, "Error: A repository already exists in this project.\n");
        return 1;
    }

    // 3. Create root hidden directory and initial directory layout

    mkdir(".fuk", 0777);
    mkdir("./.fuk/objects", 0777); // Stores blobs, trees, and commits
    mkdir("./.fuk/refs", 0777);
    mkdir("./.fuk/refs/heads", 0777); // Stores branch tip references

    // Create the default main branch file and write the initial parent commit (NULL)
    FILE* main = fopen("./.fuk/refs/heads/main", "w");
    fprintf(main, "NULL");
    fclose(main);


    // Set HEAD to point to the newly created main branch
    FILE* file_head = fopen("./.fuk/HEAD", "w");
    fprintf(file_head, "branch: %s/.fuk/refs/heads/main", cwd);
    fclose(file_head);


    // Initialize an empty staging index (created with non-executable 0666 permissions)
    int index_fd = creat("./.fuk/index", 0666);

    printf("Repository successfully created.\n");
    return 0;
}