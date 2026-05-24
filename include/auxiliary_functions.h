#ifndef AUXILIARY_FUNCS
#define AUXILIARY_FUNCS
#include <stdio.h>
#include "structures.h"

int cnt_slashes_in_path(char*);
int make_header(char*, long, int);
char* check_repo_existing(char*, char*);
long get_file_size(FILE*);
void get_hash(char*, unsigned char*, int);
void argument_to_hash(const char* argument, char* hash);
void free_lines(char**, int);
void read_blob_lines(const char* , char*** , int* );
void backtrack_diff(int**, char** , int , char**, int);
void print_diff_lcs(char**, int, char**, int);
void print_file_diff(char*, char*, char*, char*, char*);
void save_tree(file_tree* node, char* root_path);
void init_file_tree(file_tree* f_t);
int build_tree(char* root_path, file_tree* root);
void print_all_files_in_tree(char* hash, char* root_path, char* nesting, int mode, int i, const char* color, int status_mode); // mode 0 - all files are deleted, 1 - added, status_mode 1 - staged, 2 - unstaged, 3 - untracked
void compare_trees(char* current_tree_hash, char* comprasion_tree_hash, char* root_path, char* current_nesting, int i, const char* color, int status_mode, int* flag);
int prevent_data_lose();


#endif