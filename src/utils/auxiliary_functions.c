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
#include "../../include/auxiliary_functions.h"
#include "../../include/z_compressor.h"
#include "../../include/structures.h"

#define  SIXTY_FOUR_KB 65536
#define HASH_LEN 41
#define MAX_LINES 10000

#define BUFFER_SIZE 1024
#define TMP_SIZE 256

#define MAX_FILES 1024
#define COLOR_RESET   "\x1b[0m"
#define COLOR_RED     "\x1b[31m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_BOLD    "\x1b[1m"
#define COLOR_CYAN    "\x1b[36m"

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

char* check_repo_existing(char* cwd, char* root_path) // this function returns absolute root path if it exists and null otherwise
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
            if (cur_dir[0] == '\0') {
                // we are already in the root
                realpath(".", root_path);
            } else {
                realpath(cur_dir, root_path);
            }
            return root_path;
        }
        strcat(cur_dir, "../");
    }
    return NULL;
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

void argument_to_hash(const char* argument, char* hash)
{
    if (argument == NULL)
    {
        hash = NULL;
    }
    else if (strlen(argument) == 40) // it means, that argument is hash
    {
        strcpy(hash, argument);
    }
    else
    {
        char cwd[PATH_MAX];
        getcwd(cwd, sizeof(cwd));

        char root_path[PATH_MAX];
        char* ce = check_repo_existing(cwd, root_path);

        if (ce == NULL) // F_OK checks for existence
        {
            printf("There is no repo!");
            return;
        }

        char branch_path[PATH_MAX];
        sprintf(branch_path, "%s/.fuk/refs/heads/%s", root_path, argument);

        FILE* current_branch = fopen(branch_path, "r");
        char current_commit_hash[HASH_LEN];
        fscanf(current_branch, "%s", current_commit_hash);

        strcpy(hash, current_commit_hash);
    }
    return;
}


// Очистка массива строк
void free_lines(char** lines, int count)
{
    if (!lines) return;
    for (int i = 0; i < count; i++) {
        free(lines[i]);
    }
    free(lines);
}

// Чтение строк из файла с пропуском заголовка (ищем первый '\n' вместо '\0')
void read_blob_lines(const char* filepath, char*** lines_out, int* line_count_out) {
    *line_count_out = 0;
    *lines_out = NULL;

    if (filepath == NULL || strlen(filepath) == 0) return;

    FILE* f = fopen(filepath, "rb");
    if (!f) return;

    // Считаем размер файла
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* buffer = malloc(size + 1);
    fread(buffer, 1, size, f);
    buffer[size] = '\0';
    fclose(f);

    // Пропускаем первую строчку-заголовок (ищем первый символ переноса строки '\n')
    char* content = NULL;
    for (long i = 0; i < size; i++) {
        if (buffer[i] == '\n') {
            content = buffer + i + 1;
            break;
        }
    }

    // Если переноса строки не нашлось (например, пустой файл), используем весь буфер
    if (!content) {
        content = buffer;
    }

    char** lines = malloc(sizeof(char*) * MAX_LINES);
    int count = 0;

    // Разбиваем оставшееся содержимое на строки
    char* line = strtok(content, "\n");
    while (line != NULL && count < MAX_LINES) {
        lines[count] = strdup(line);
        count++;
        line = strtok(NULL, "\n");
    }

    free(buffer);
    *lines_out = lines;
    *line_count_out = count;
}

// Рекурсивный вывод разницы (обратный ход динамического программирования)
void backtrack_diff(int** dp, char** lines1, int i, char** lines2, int j)
{
    if (i > 0 && j > 0 && strcmp(lines1[i - 1], lines2[j - 1]) == 0) {
        backtrack_diff(dp, lines1, i - 1, lines2, j - 1);
        printf("  %s\n", lines1[i - 1]); // Строка не изменилась
    } else if (j > 0 && (i == 0 || dp[i][j - 1] >= dp[i - 1][j])) {
        backtrack_diff(dp, lines1, i, lines2, j - 1);
        printf("\033[32m+%s\033[0m\n", lines2[j - 1]); // Добавленная строка (Зеленый)
    } else if (i > 0 && (j == 0 || dp[i][j - 1] < dp[i - 1][j])) {
        backtrack_diff(dp, lines1, i - 1, lines2, j);
        printf("\033[31m-%s\033[0m\n", lines1[i - 1]); // Удаленная строка (Красный)
    }
}

// Построение DP-таблицы LCS и запуск построчного вывода
void print_diff_lcs(char** lines1, int n, char** lines2, int m)
{
    // Выделяем память под DP таблицу размера (n+1) x (m+1)
    int** dp = malloc((n + 1) * sizeof(int*));
    for (int i = 0; i <= n; i++) {
        dp[i] = calloc((m + 1), sizeof(int));
    }

    for (int i = 1; i <= n; i++) {
        for (int j = 1; j <= m; j++) {
            if (strcmp(lines1[i - 1], lines2[j - 1]) == 0) {
                dp[i][j] = dp[i - 1][j - 1] + 1;
            } else {
                dp[i][j] = (dp[i - 1][j] > dp[i][j - 1]) ? dp[i - 1][j] : dp[i][j - 1];
            }
        }
    }

    backtrack_diff(dp, lines1, n, lines2, m);

    for (int i = 0; i <= n; i++) {
        free(dp[i]);
    }
    free(dp);
}

// Главный менеджер сравнения двух конкретных файлов-блобов
void print_file_diff(char* old_hash, char* new_hash, char* filename, char* root_path, char* nesting)
{
    char old_decompressed[PATH_MAX] = "";
    char new_decompressed[PATH_MAX] = "";

    // Decompress old tear
    if (old_hash && strcmp(old_hash, "NULL") != 0 && strlen(old_hash) > 0) {
        char old_tear_path[PATH_MAX];
        sprintf(old_tear_path, "%s/.fuk/objects/%.2s/%.38s", root_path, old_hash, old_hash + 2);
        sprintf(old_decompressed, "%s/.fuk/objects/old_blob_tmp", root_path);
        FILE* f_old = fopen(old_decompressed, "wb");
        if (f_old) {
            decompress_file(old_tear_path, f_old, 1);
            fclose(f_old);
        }
    }

    // decompress new tear, if it exists in new commit
    if (new_hash && strcmp(new_hash, "NULL") != 0 && strlen(new_hash) > 0) {
        char new_tear_path[PATH_MAX];
        sprintf(new_tear_path, "%s/.fuk/objects/%.2s/%.38s", root_path, new_hash, new_hash + 2);
        sprintf(new_decompressed, "%s/.fuk/objects/new_blob_tmp", root_path);
        FILE* f_new = fopen(new_decompressed, "wb");
        if (f_new) {
            decompress_file(new_tear_path, f_new, 1);
            fclose(f_new);
        }
    }

    printf("\n\033[1mdiff --git a%s/%s b%s/%s\033[0m\n", nesting, filename, nesting, filename);

    char** lines1 = NULL;
    int count1 = 0;
    char** lines2 = NULL;
    int count2 = 0;

    if (strlen(old_decompressed) > 0) {
        read_blob_lines(old_decompressed, &lines1, &count1);
    }
    if (strlen(new_decompressed) > 0) {
        read_blob_lines(new_decompressed, &lines2, &count2);
    }

    // Запускаем LCS-сравнение
    print_diff_lcs(lines1, count1, lines2, count2);

    free_lines(lines1, count1);
    free_lines(lines2, count2);

    if (strlen(old_decompressed) > 0) remove(old_decompressed);
    if (strlen(new_decompressed) > 0) remove(new_decompressed);
}


void save_tree(file_tree* node, char* root_path) // returns hash
{
    // base case, node - file
    if (node -> is_dir == 0)
    {
        return;
    }

    for (int i = 0; i < node -> child_count; i++)
    {
        save_tree(node -> children[i], root_path);
    }

    // build tree in files
    char* buffer = malloc(sizeof(file_tree) * 1000);
    int offset = 0;
    for (int i = 0; i < node -> child_count; i++)
    {
        if (node -> children[i] -> is_dir)
        {
            offset += sprintf(buffer + offset, "tree %s : %s\n", node -> children[i] -> name, node -> children[i] -> hash);
        }
        else
        {
            offset += sprintf(buffer + offset, "tear %s : %s\n", node -> children[i] -> name, node -> children[i] -> hash);
        }
    }

    char path_tmp[PATH_MAX];
    sprintf(path_tmp, "%s/.fuk/objects/tmp", root_path);
    FILE* tmp = fopen(path_tmp, "w");

    fprintf(tmp,"%s", buffer);
    free(buffer);

    fclose(tmp);

    unsigned char hash[EVP_MAX_MD_SIZE];

    get_hash(path_tmp, hash, 1);

    char hash_in_hex[41];

    for (int i = 0; i < 20; i++)
    {
        sprintf(hash_in_hex + (i * 2), "%02x", hash[i]);
    }

    strcpy(node -> hash, hash_in_hex);

    char dir_name[PATH_MAX];
    sprintf(dir_name, "%s/.fuk/objects/%.2s",root_path, hash_in_hex);
    mkdir(dir_name, 0777);

    char new_name[PATH_MAX];
    sprintf(new_name, "%s/%s", dir_name, hash_in_hex + 2);

    FILE* f_dest = fopen(new_name, "wb");
    compress_file(path_tmp, f_dest, 1);
    fclose(f_dest);

    return;
}

void init_file_tree(file_tree* f_t)
{
    strcpy(f_t -> name, "root");
    f_t -> is_dir = 1;
    f_t -> hash[0] = '\0'; // the root has no hash
    f_t -> child_count = 0;
}


int build_tree(char* root_path, file_tree* root)
{
    init_file_tree(root);

    char file_name[NAME_MAX];
    char hash_in_hex[41];

    char path_index[PATH_MAX];
    sprintf(path_index, "%s/.fuk/index", root_path);
    FILE* index = fopen(path_index, "r");

    int f = 1;
    int root_path_len = (int)strlen(root_path);
    // build a tree
    while (fscanf(index, "%s : %s", file_name, hash_in_hex) != -1)
    {
        char* relative_path = file_name + root_path_len + 1;
        f = 0;

        file_tree* curr_dir = root ;

        int slash_cnt = cnt_slashes_in_path(relative_path);

        char* token;
        token = strtok(relative_path, "/");

        while (slash_cnt > 0)
        {
            int f = 1;
            for (int i = 0; i < curr_dir -> child_count; i++)
            {
                if (!strcmp(curr_dir -> children[i] -> name, token))
                {
                    token = strtok(NULL, "/");
                    slash_cnt--;
                    curr_dir = curr_dir -> children[i];
                    f = 0;
                    break;
                }
            }
            if (f)
            {
                file_tree* directory = (file_tree*)malloc(sizeof(file_tree));
                strcpy(directory -> name, token);
                strcpy(directory -> hash, hash_in_hex);
                directory -> is_dir = 1;
                directory -> child_count = 0;
                curr_dir->children[curr_dir->child_count] = directory;
                curr_dir->child_count++;

                token = strtok(NULL, "/");
                slash_cnt--;
                curr_dir = directory;
            }

        }

        file_tree* file = (file_tree*)malloc(sizeof(file_tree));
        strcpy(file -> name, token);
        strcpy(file -> hash, hash_in_hex);
        file -> is_dir = 0;
        curr_dir->children[curr_dir->child_count] = file;
        curr_dir->child_count++;
    }

    if (index != NULL)
    {
        fclose(index);
    }

    return f;
}

void print_all_files_in_tree(char* hash, char* root_path, char* nesting, int mode, int i, const char* color, int status_mode) // mode 0 - all files are deleted, 1 - added, status_mode 1 - staged, 2 - unstaged, 3 - untracked
{

    char tree[PATH_MAX];
    sprintf(tree, "%s/.fuk/objects/%.2s/%.38s", root_path, hash, hash + 2);


    char tree_decompressed[PATH_MAX];

    sprintf(tree_decompressed, "%s/.fuk/objects/tree_decompressed%d", root_path, i);
    i++;

    FILE* decompressed_tree = fopen(tree_decompressed, "wb");

    decompress_file(tree, decompressed_tree, 1);

    fclose(decompressed_tree);

    decompressed_tree = fopen(tree_decompressed, "r");

    char tmp[TMP_SIZE];
    fgets(tmp, sizeof(char) * TMP_SIZE, decompressed_tree); // skip headers

    tree_entry current;

    while (fscanf(decompressed_tree, "%s %s : %s", current.type, current.name, current.hash) != -1)
    {
        if (!strcmp(current.type, "tear"))
        {
            if (mode)
            {
                if (status_mode == 1)
                    printf("%sadded: %s%s/%s%s\n", color, root_path, nesting, current.name, COLOR_RESET);
                else if (status_mode == 3)
                    printf("%suntracked: %s%s/%s%s\n", color, root_path, nesting, current.name, COLOR_RESET);
            }
            else
            {
                if (status_mode == 1 || status_mode == 2)
                    printf("%sdeleted: %s%s/%s%s\n", color, root_path, nesting, current.name, COLOR_RESET);
            }
        }
        else
        {
            char new_nesting[PATH_MAX];
            sprintf(new_nesting, "%s/%s", nesting, current.name);
            print_all_files_in_tree(current.hash, root_path, new_nesting, mode, i, color, status_mode);
        }
    }

    fclose(decompressed_tree);
    remove(tree_decompressed);
    return;
}


void compare_trees(char* current_tree_hash, char* comprasion_tree_hash, char* root_path, char* current_nesting, int i, const char* color, int status_mode, int* flag)
{
    // create path
    char current_commit_path_tree[PATH_MAX];
    sprintf(current_commit_path_tree, "%s/.fuk/objects/%.2s/%.38s", root_path, current_tree_hash, current_tree_hash + 2);
    char comprasion_commit_path_tree[PATH_MAX];
    sprintf(comprasion_commit_path_tree, "%s/.fuk/objects/%.2s/%.38s", root_path, comprasion_tree_hash, comprasion_tree_hash + 2);

    char current_tree_decompressed[PATH_MAX];
    char comprasion_tree_decompressed[PATH_MAX];

    sprintf(current_tree_decompressed, "%s/.fuk/objects/current_tree_decompressed%d", root_path, i);
    sprintf(comprasion_tree_decompressed, "%s/.fuk/objects/comprasion_tree_decompressed%d", root_path, i);
    i++;

    FILE* current_tree = fopen(current_tree_decompressed,"wb");
    FILE* comprasion_tree = fopen(comprasion_tree_decompressed, "wb");

    decompress_file(current_commit_path_tree, current_tree, 1);
    decompress_file(comprasion_commit_path_tree, comprasion_tree, 1);

    fclose(current_tree);
    fclose(comprasion_tree);

    current_tree = fopen(current_tree_decompressed,"r");
    comprasion_tree = fopen(comprasion_tree_decompressed, "r");

    char tmp[TMP_SIZE];
    fgets(tmp, sizeof(char) * TMP_SIZE, current_tree); // skip headers
    fgets(tmp, sizeof(char) * TMP_SIZE, comprasion_tree);

    tree_entry current;
    tree_entry comprasion;

    int used_rows[MAX_FILES] = {0}; // this array shows rows in comprasion file. id
    int curr_row = 0;

    while (fscanf(current_tree, "%s %s : %s", current.type, current.name, current.hash) != -1)
    {

        if (!strcmp(current.type, "tear")) // try to find tear in comparion tree
        {
            curr_row = 0;
            int f = 1;
            while (fscanf(comprasion_tree, "%s %s : %s", comprasion.type, comprasion.name, comprasion.hash) != -1)
            {
                curr_row++;
                if (!strcmp(current.name, comprasion.name)) // we found file with same name
                {
                    used_rows[curr_row] = 1;
                    if (strcmp(current.hash, comprasion.hash))
                    {
                        if (status_mode == 1 || status_mode == 2)
                        {
                            printf("%smodified: %s%s/%s%s\n", color, root_path, current_nesting, current.name, COLOR_RESET);
                            print_file_diff(comprasion.hash, current.hash, current.name, root_path, current_nesting);
                            *flag = 1;
                        }
                        f = 0;
                        break;
                    }
                    else
                    {
                        // printf("Unchanged file: %s%s/%s\n", root_path, current_nesting, current.name);
                        f = 0;
                        break;
                    }
                }
            }
            fseek(comprasion_tree, 0, SEEK_SET);
            fgets(tmp, sizeof(char) * TMP_SIZE, comprasion_tree);
            if (f)
            {
                if (status_mode == 1)
                {
                    printf("%sadded: %s%s/%s%s\n", color, root_path, current_nesting, current.name, COLOR_RESET);
                    print_file_diff(NULL, current.hash, current.name, root_path, current_nesting);
                }
                else if (status_mode == 3)
                    printf("%suntracked: %s%s/%s%s\n", color, root_path, current_nesting, current.name, COLOR_RESET);
            }
        }
        else // this is another tree
        {
            curr_row = 0;
            int f = 1;
            while (fscanf(comprasion_tree, "%s %s : %s", comprasion.type, comprasion.name, comprasion.hash) != -1)
            {
                curr_row++;
                if (!strcmp(current.name, comprasion.name)) // we found file with same name
                {
                    used_rows[curr_row] = 1;
                    char new_nesting[PATH_MAX];
                    sprintf(new_nesting, "%s/%s", current_nesting, current.name);
                    compare_trees(current.hash, comprasion.hash, root_path, new_nesting, i, color, status_mode, flag);
                    // printf("Changed directory: %s%s/%s\n", root_path, current_nesting, current.name);
                    f = 0;
                    break;
                }
            }
            fseek(comprasion_tree, 0, SEEK_SET);
            fgets(tmp, sizeof(char) * TMP_SIZE, comprasion_tree);
            if (f)
            {
                if (status_mode == 1)
                    printf("%sadded dir: %s%s/%s%s\n", color, root_path, current_nesting, current.name, COLOR_RESET);
                else if (status_mode == 3)
                    printf("%suntracked dir: %s%s/%s%s\n", color, root_path, current_nesting, current.name, COLOR_RESET);

                char new_nesting[PATH_MAX];
                sprintf(new_nesting, "%s/%s", current_nesting, current.name);
                print_all_files_in_tree(current.hash, root_path, new_nesting, 1, i, color, status_mode);
            }
        }
    }



    fseek(current_tree, 0, SEEK_SET);
    fgets(tmp, sizeof(char) * TMP_SIZE, current_tree);
    curr_row = 0;

    // the same logic, but vice versa
    while (fscanf(comprasion_tree, "%s %s : %s", comprasion.type, comprasion.name, comprasion.hash) != -1)
    {
        curr_row++;
        if (used_rows[curr_row])
        {
            continue;
        }

        if (!strcmp(comprasion.type, "tear")) // try to find tear in comparion tree
        {
            int f = 1;
            while (fscanf(current_tree, "%s %s : %s", current.type, current.name, current.hash) != -1)
            {
                if (!strcmp(current.name, comprasion.name)) // we found file with same name
                {
                    if (strcmp(current.hash, comprasion.hash))
                    {
                        // printf("Modified file: %s%s/%s\n", root_path, current_nesting, current.name);
                        f = 0;
                        break;
                    }
                    else
                    {
                        // printf("Unchanged file: %s%s/%s\n", root_path, current_nesting, current.name);
                        f = 0;
                        break;
                    }
                }
            }
            fseek(current_tree, 0, SEEK_SET);
            fgets(tmp, sizeof(char) * TMP_SIZE, current_tree);
            if (f)
            {
                if (status_mode == 1 || status_mode == 2)
                {
                    printf("%sdeleted: %s%s/%s%s\n", color, root_path, current_nesting, comprasion.name, COLOR_RESET);
                    print_file_diff(comprasion.hash, NULL, comprasion.name, root_path, current_nesting);
                }

            }
        }
        else // this is another tree
        {
            int f = 1;
            while (fscanf(current_tree, "%s %s : %s", current.type, current.name, current.hash) != -1)
            {
                if (!strcmp(current.name, comprasion.name)) // we found file with same name
                {
                    char new_nesting[PATH_MAX];
                    sprintf(new_nesting, "%s/%s", current_nesting, current.name);
                    compare_trees(current.hash, comprasion.hash, root_path, new_nesting, i, color,status_mode, flag);
                    // printf("Changed directory: %s%s/%s\n", root_path, current_nesting, current.name);
                    f = 0;
                }
            }
            fseek(current_tree, 0, SEEK_SET);
            fgets(tmp, sizeof(char) * TMP_SIZE, current_tree);
            if (f)
            {
                if (status_mode == 1 || status_mode == 2)
                {
                    printf("%sdeleted dir: %s%s/%s%s\n", color, root_path, current_nesting, comprasion.name, COLOR_RESET);
                    char new_nesting[PATH_MAX];
                    sprintf(new_nesting, "%s/%s", current_nesting, comprasion.name);
                    print_all_files_in_tree(comprasion.hash, root_path, new_nesting, 0, i, color, status_mode);
                }
            }
        }
    }
    fclose(current_tree);
    fclose(comprasion_tree);
    remove(current_tree_decompressed);
    remove(comprasion_tree_decompressed);
}


int prevent_data_lose() // this function works same to status. It checks, that there is no uncommited changes
{
    char cwd[PATH_MAX];
    getcwd(cwd, sizeof(cwd));

    char root_path[PATH_MAX];
    char* ce = check_repo_existing(cwd, root_path);

    if (ce == NULL) // F_OK checks for existence
    {
        printf("There is no repo!");
        return 1 ;
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
    // --- CHANGED END ---

    if (!strcmp(current_commit_hash, "NULL"))
    {
        printf("There have been no commits yet");
        return 1;
    }

    char objects_path[PATH_MAX];
    sprintf(objects_path, "%s/.fuk/objects", root_path);

    char current_commit_path[PATH_MAX];
    sprintf(current_commit_path, "%s/.fuk/objects/%.2s/%.38s", root_path, current_commit_hash, current_commit_hash + 2);
    char current_commit_path_tmp[PATH_MAX];
    strcpy(current_commit_path_tmp, objects_path);
    strcat(current_commit_path_tmp, "/current_commit");

    // decompress current commit
    FILE* current_decompressed_commit = fopen(current_commit_path_tmp, "wb");
    decompress_file(current_commit_path, current_decompressed_commit, 1);
    fclose(current_decompressed_commit);

    // this comparing with index
    file_tree root;
    build_tree(root_path, &root); // build tree from index
    save_tree(&root, root_path);

    current_decompressed_commit = fopen(current_commit_path_tmp, "r");

    char current_tree_hash[HASH_LEN];

    char tmp[TMP_SIZE];

    // get tree hashes
    fgets(tmp, sizeof(char) * TMP_SIZE, current_decompressed_commit); // skip header
    fgets(tmp, sizeof(char) * TMP_SIZE, current_decompressed_commit);
    sprintf(current_tree_hash, "%s", tmp + 5); // +5, because tree: has len 5

    fclose(current_decompressed_commit);

    char current_nesting[PATH_MAX] = "";
    printf("\n%sChanges to be committed:%s\n", COLOR_GREEN, COLOR_RESET);
    int* flag = (int*)malloc(sizeof(int));
    *flag = 0;
    compare_trees(root.hash, current_tree_hash, root_path, current_nesting, 0, COLOR_GREEN , 1, flag);

    if (*flag)
    {
        printf("You have uncommited changes\n");
        return 1;
    }
    else
    {
        return 0;
    }
    free(flag);
}
