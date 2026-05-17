#ifndef AUXILIARY_FUNCS
#define AUXILIARY_FUNCS
#include <stdio.h>

int cnt_slashes_in_path(char*);
int make_header(char*, long, int);
int check_repo_existing(char*);
long get_file_size(FILE*);
void get_hash(char*, unsigned char*, int);

#endif