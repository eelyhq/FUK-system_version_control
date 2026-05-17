#ifndef STRUCTURES_H
#define STRUCTURES_H

typedef struct file_tree_s  {
    char name[NAME_MAX];
    int is_dir;
    char hash[41];
    struct file_tree_s* children[100]; // pointers to subdirectories
    int child_count;
} file_tree;

#endif
