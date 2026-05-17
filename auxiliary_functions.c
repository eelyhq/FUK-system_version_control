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
#include "auxiliary_functions.h"
#define  SIXTY_FOUR_KB 65536

int cnt_slashes_in_path(char* cwd)
{
    int res = 0;

    while (*cwd != '\0')
    {
        if (*cwd == '/') // count slashes in path
        {
            res++;
        }
        cwd++;
    }
    return res;
}

int make_header(char* header, long file_size, int i) // function that does header and returns size of it
{
    if (i == 0)
    {
        return sprintf(header, "tear: %ld\n", file_size);
    }
    else if (i == 1)
    {
        return sprintf(header, "tree: %ld\n", file_size);
    }
    return sprintf(header, "commit: %ld\n", file_size);
}

int check_repo_existing(char* cwd)
{
    int size_of_path = cnt_slashes_in_path(cwd);

    char cur_dir[PATH_MAX] = ""; // path for searching .fuk

    int f = 0; // flag, 0 means, that there is no repo and 1 vice versa
    for (int i = 0; i <= size_of_path; i++)
    {
        char path[PATH_MAX];
        strcpy(path, cur_dir ); // go to directory higher
        strcat(path, ".fuk");

        if (access(path, F_OK) == 0)
        {
            f = 1;
            break;
        }
        strcat(cur_dir, "../");
    }
    return f;
}

long get_file_size(FILE* file)
{
    fseek(file, 0, SEEK_END); // move pointer to the end of file
    long file_size = ftell(file);  // this returns just number of byte from start to the pointer, pointer we had moved to the end
    fseek(file, 0, SEEK_SET); // return pointer to start

    return file_size;
}

void get_hash(char* file_name, unsigned char* hash, int i)
{
    FILE* file = fopen(file_name, "rb"); // open in binary mode

    // 1. get file size
    long file_size = get_file_size(file);

    // 2. initialize context SHA-1
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha1(), NULL);

    // 3. create and add header to context
    char header[64];
    int header_len = make_header(header, file_size,i);

    EVP_DigestUpdate(ctx, header, header_len);

    char null_byte = '\0';
    EVP_DigestUpdate(ctx, &null_byte, 1);

    unsigned char buffer[SIXTY_FOUR_KB];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0)
    {
        EVP_DigestUpdate(ctx, buffer, bytes_read);
    }

    // 6. Finalize hash
    unsigned int hash_len;
    EVP_DigestFinal_ex(ctx, hash, &hash_len);

    EVP_MD_CTX_free(ctx);
    fclose(file);

    return;
}