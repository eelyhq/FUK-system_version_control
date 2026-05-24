#ifndef STRUCTURES_H
#define STRUCTURES_H
#define HASH_LEN 41
#include <limits.h>

typedef struct file_tree_s  {
    char name[NAME_MAX];
    int is_dir;
    char hash[41];
    struct file_tree_s* children[100]; // pointers to subdirectories
    int child_count;
} file_tree;

typedef struct tree_entry
{
    char type[10];
    char name[PATH_MAX];
    char hash[HASH_LEN];
}tree_entry;

#endif
