#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>     // Required for getcwd
#include <limits.h>     // Required for PATH_MAX
#include <sys/types.h>
#include <openssl/evp.h> // Required for SHA-1 hashing elsewhere in the project

/* Local project headers */
#include "../include/init.h"
#include "../include/add_remove.h"
#include "../include/auxiliary_functions.h"
#include "../include/commit.h"
#include "../include/log.h"
#include "../include/branch.h"
#include "../include/status.h"
#include "../include/diff.h"
#include "../include/checkout.h"


#define MAX_MESSAGE_LEN 100
#define SIXTY_FOUR_KB   65536
#define HASH_LEN        41

void print_usage(void) {
    printf("Usage: fuk <command> [<args>]\n\n");
    printf("Available commands:\n");
    printf("  init                               Create an empty fuk repository\n");
    printf("  status                             Show the working tree status\n");
    printf("  add <file_path>                    Add file contents to the index\n");
    printf("  remove <file_path>                 Remove files from the working tree\n");
    printf("  commit -m \"<message>\"              Record changes to the repository\n");
    printf("  log                                Show commit history\n");
    printf("  diff <file/commit> [commit]        Show changes between commits/files\n");
    printf("  branch [<name>]                    List or create branches\n");
    printf("  checkout <branch> [<file>]         Switch branches or restore files\n");
}

int main(const int argc, char** argv) {
    // 1. Basic command-line validation
    if (argc < 2) {
        print_usage();
        return 1;
    }

    char* command = argv[1];

    // 2. Resolve environment and check repository existence
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("Failed to retrieve current working directory");
        return 1;
    }

    char root_path[PATH_MAX] = {0};
    char* repo_exists = check_repo_existing(cwd, root_path);

    // 3. Determine HEAD state (Attached vs. Detached HEAD)
    // By default, assume state attached if no repo exists yet
    int is_detached = 0;

    if (repo_exists != NULL) {
        char head_path[PATH_MAX];
        snprintf(head_path, sizeof(head_path), "%s/.fuk/HEAD", root_path);

        FILE* head = fopen(head_path, "r");
        if (head != NULL) {
            char type[32];
            // Read first token safely, preventing buffer overflow (max 31 chars)
            if (fscanf(head, "%31s", type) == 1) {
                // If HEAD points directly to a commit hash instead of a branch ref,
                // we are in a detached HEAD state.
                if (strcmp(type, "commit:") == 0) {
                    is_detached = 1;
                }
            }
            fclose(head);
        }
    }

    /* =========================================================================
     * Command Router
     * Each block validates its specific arguments and environment requirements
     * before running the operation.
     * ========================================================================= */

    // --- INIT ---
    if (strcmp(command, "init") == 0) {
        if (fuk_init())
        {
            return 1;
        }
        return 0;
    }

    // --- STATUS ---
    if (strcmp(command, "status") == 0) {
        if (is_detached) {
            fprintf(stderr, "Error: Cannot run status in a detached HEAD state.\n");
            return 1;
        }

        if (fuk_status())
        {
            return 1;
        }

        return 0;
    }

    // --- ADD ---
    if (strcmp(command, "add") == 0) {
        if (is_detached) {
            fprintf(stderr, "Error: Cannot stage changes in a detached HEAD state.\n");
            return 1;
        }
        if (argc < 3) {
            fprintf(stderr, "Error: Missing file path.\nUsage: fuk add <file_path>\n");
            return 1;
        }

        char absolute_path[PATH_MAX];
        if (realpath(argv[2], absolute_path) == NULL) {
            fprintf(stderr, "Error: File '%s' does not exist or path is invalid.\n", argv[2]);
            return 1;
        }

        if (fuk_add(absolute_path, 1))
        {
            return 1;
        }

        return 0;
    }

    // --- REMOVE ---
    if (strcmp(command, "remove") == 0) {
        if (is_detached) {
            fprintf(stderr, "Error: Cannot remove files in a detached HEAD state.\n");
            return 1;
        }
        if (argc < 3) {
            fprintf(stderr, "Error: Missing file path.\nUsage: fuk remove <file_path>\n");
            return 1;
        }

        char absolute_path[PATH_MAX];
        if (realpath(argv[2], absolute_path) == NULL) {
            fprintf(stderr, "Error: File '%s' does not exist or path is invalid.\n", argv[2]);
            return 1;
        }

        fuk_remove(absolute_path);
        return 0;
    }

    // --- COMMIT ---
    if (strcmp(command, "commit") == 0) {
        if (is_detached) {
            fprintf(stderr, "Error: Cannot commit in a detached HEAD state.\n");
            return 1;
        }
        if (argc < 4 || strcmp(argv[2], "-m") != 0) {
            fprintf(stderr, "Usage: fuk commit -m \"<commit message>\"\n");
            return 1;
        }

        if (fuk_commit(argv[3]))
        {
            return 1;
        }
        return 0;
    }

    // --- DIFF ---
    if (strcmp(command, "diff") == 0) {
        if (is_detached) {
            fprintf(stderr, "Error: Cannot run diff in a detached HEAD state.\n");
            return 1;
        }
        if (argc < 3) {
            fprintf(stderr, "Usage: fuk diff <file_or_commit> [another_commit]\n");
            return 1;
        }

        if (argc == 3) {
            if (fuk_diff(argv[2], NULL))
            {
                return 1;
            }
        } else {
            if (fuk_diff(argv[2], argv[3]))
            {
                return 1;
            }
        }
        return 0;
    }

    // --- LOG ---
    if (strcmp(command, "log") == 0) {
        if (is_detached) {
            fprintf(stderr, "Error: Cannot run log in a detached HEAD state.\n");
            return 1;
        }

        if (argc == 3) {
            // Case A: Range log (e.g., hash1..hash2)
            // Format check: Expecting exactly 82 characters (40 char SHA1 + '..' + 40 char SHA1)
            if (strlen(argv[2]) == 82 && argv[2][40] == '.' && argv[2][41] == '.') {
                char hash1[HASH_LEN] = {0};
                char hash2[HASH_LEN] = {0};

                // Matches 40 characters, matches ".." literally, matches next 40 characters
                if (sscanf(argv[2], "%40s..%40s", hash1, hash2) == 2) {
                    if (fuk_log(hash2, -1, hash1))
                    {
                        return 1;
                    }
                } else {
                    fprintf(stderr, "Error: Invalid range format. Expected <hash1>..<hash2>\n");
                    return 1;
                }
            }
            // Case B: Standard log starting from a specific commit hash
            else {
                if (strlen(argv[2]) != 40) {
                    fprintf(stderr, "Error: Invalid commit hash size (must be 40 hex characters).\n");
                    return 1;
                }
                if (fuk_log(argv[2], -1, NULL))
                {
                    return 1;
                }
            }
        }
        // Case C: Limiting the log output size (e.g., fuk log --n 5)
        else if (argc == 4) {
            if (strcmp(argv[2], "--n") != 0) {
                fprintf(stderr, "Usage: fuk log --n <count>\n");
                return 1;
            }

            char* endptr;
            long n = strtol(argv[3], &endptr, 10);
            if (*endptr != '\0' || n <= 0) {
                fprintf(stderr, "Error: Invalid commit count '%s'. Must be a positive integer.\n", argv[3]);
                return 1;
            }
            if (fuk_log(NULL, (int)n, NULL))
            {
                return 1;
            }
        }
        // Case D: Starting from a commit with a limited count (e.g., fuk log <hash> --n 5)
        else if (argc == 5) {
            if (strcmp(argv[3], "--n") != 0) {
                fprintf(stderr, "Usage: fuk log <hash> --n <count>\n");
                return 1;
            }
            if (strlen(argv[2]) != 40) {
                fprintf(stderr, "Error: Invalid start commit hash.\n");
                return 1;
            }

            char* endptr;
            long n = strtol(argv[4], &endptr, 10);
            if (*endptr != '\0' || n <= 0) {
                fprintf(stderr, "Error: Invalid commit count. Must be a positive integer.\n", argv[4]);
                return 1;
            }
            if (fuk_log(argv[2], (int)n, NULL))
            {
                return 1;
            }
        }
        // Case E: Default log showing all history
        else if (argc == 2) {
            if (fuk_log(NULL, -1, NULL))
            {
                return 1;
            }
        }
        else {
            fprintf(stderr, "Error: Invalid log command formatting.\n");
            return 1;
        }
        return 0;
    }

    // --- BRANCH ---
    if (strcmp(command, "branch") == 0) {
        if (argc == 2)
        {
            print_branches_fuk();
        }
        else if (argc == 3)
        {
            // Note: Second parameter indicates attached (1) or detached (0) status
            branch_create_fuk(argv[2], !is_detached);
            print_branches_fuk();
        }
        else
            {
            fprintf(stderr, "Usage:\n  fuk branch          (List branches)\n  fuk branch <name>   (Create branch)\n");
            return 1;
        }
        return 0;
    }

    // --- CHECKOUT ---
    if (strcmp(command, "checkout") == 0) {
        if (argc == 3)
        {
            if (fuk_checkout_all(argv[2]))
            {
                return 1;
            }
        }
        else if (argc >= 4)
        {
            if ( fuk_checkout(argv[2], argv[3]))
            {
                return 1;
            }
        }
        else
        {
            fprintf(stderr, "Usage:\n  fuk checkout <commit_or_branch>\n  fuk checkout <commit_or_branch> <file>\n");
            return 1;
        }
        return 0;
    }

    // --- UNKNOWN COMMAND FALLBACK ---
    fprintf(stderr, "Error: Unknown command '%s'\n\n", command);
    print_usage();
    return 1;
}