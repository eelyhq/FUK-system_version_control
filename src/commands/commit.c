#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include <openssl/evp.h>
#include <fcntl.h>
#include <time.h>

/* Local project headers */
#include "../../include/auxiliary_functions.h"
#include "../../include/commit.h"
#include "../../include/structures.h"
#include "../../include/z_compressor.h"

#define  SIXTY_FOUR_KB 65536
#define TMP_SIZE 256

// Function for creating commit from files in
int fuk_commit(char* message)
{
    // 1. Resolve environment and check for an existing repository
    char cwd[PATH_MAX];
    getcwd(cwd, sizeof(cwd));

    char root_path[PATH_MAX];
    char* re = check_repo_existing(cwd, root_path);

    if (re == NULL)
    {
        printf("There is no repository here\n");
        return 1;
    }

    // 2. Creating and saving tree from index
    file_tree root;

    int f  = build_tree(root_path, &root);

    if (f)
    {
        printf("No files to commit\n");
        return 0;
    }

    save_tree(&root, root_path);

    // 3. Read previous commit to write it in as parent for current
    char head_path[PATH_MAX];
    sprintf(head_path, "%s/.fuk/HEAD", root_path);

    FILE* head = fopen(head_path, "r");
    char branch[PATH_MAX];
    fscanf(head, "branch: %s", branch);
    fclose(head);

    FILE* parent_commit = fopen(branch, "r");

    char p_c[41];
    fscanf(parent_commit, "%s", p_c);
    fclose(parent_commit);

    // 4. the p_c variable contains hash of previous commit, and we have to compare current tree with
    // previous and if they are equals, we have to decline commit and message, that there is nothing to do
    if (strcmp(p_c, "NULL"))
    {
        char path_to_commit[PATH_MAX];
        sprintf(path_to_commit, "%s/.fuk/objects/%.2s/%.38s", root_path, p_c, p_c+2);

        char path_to_decompressed_previous_commit[PATH_MAX];
        sprintf(path_to_decompressed_previous_commit, "%s/.fuk/objects/decompressed_commit", root_path);
        FILE* decompressed_previous_commit = fopen(path_to_decompressed_previous_commit, "wb");
        decompress_file(path_to_commit, decompressed_previous_commit, 1);
        fclose(decompressed_previous_commit);
        decompressed_previous_commit = fopen(path_to_decompressed_previous_commit, "r");

        char previous_tree_hash[41];

        char temp[TMP_SIZE];
        fscanf(decompressed_previous_commit, "commit: %s\ntree %s\n", temp, previous_tree_hash);
        fclose(decompressed_previous_commit);
        remove(path_to_decompressed_previous_commit);

        if (!strcmp(previous_tree_hash, root.hash))
        {
            printf("Nothing to commit, working tree clean\n");
            return 0;
        }
    }

    // 5. Format the commit metadata (tree hash, parent hash, author date/time, and message)
    // into a buffer, write it to an uncompressed temporary file, and clean up the buffer.
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

    // 7. Store the commit
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

    // 8. Update the active branch file so that its contents point to the
    // newly generated commit hash, moving the branch pointer forward
    FILE* parent_commit2 = fopen(branch, "w");
    if (parent_commit2 != NULL)
    {
        fprintf(parent_commit2, "%s", hash_in_hex2);
        fclose(parent_commit2);
    }

    printf("Successful commit\n");
    return 0;
}