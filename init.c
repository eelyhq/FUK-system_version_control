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
#include "init.h"
#include "auxiliary_functions.h"
#define  SIXTY_FOUR_KB 65536


void init_fuk()
{
    char cwd[PATH_MAX];
    getcwd(cwd, sizeof(cwd)); // get current directory, to go up and search .fuk in higher dirs

    int f = check_repo_existing(cwd);

    if (f) // F_OK checks for existence
    {
        printf("There is already exist repo");
    }
    else
    {
        mkdir(".fuk", 0777); // 0777 gives read, write and execute permissions
        mkdir("./.fuk/objects", 0777); // creates directory for tears trees and commits
        mkdir("./.fuk/refs", 0777); // creates directory for pointers on branches
        mkdir("./.fuk/refs/heads", 0777);
        FILE* main = fopen("./.fuk/refs/heads/main", "w");
        fprintf(main, "NULL"); // it means that parent commit doesn't exist
        FILE* file_head = fopen("./.fuk/HEAD", "w"); // create file with current branch name
        fprintf(file_head, "branch: ./.fuk/refs/heads/main");
        fclose(file_head);
        creat("./.fuk/index", 0777); // create file with files, added in commit
        printf("Repo successfully created");
    }
}