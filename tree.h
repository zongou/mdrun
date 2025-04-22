#ifndef TREE_H
#define TREE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_CHILDREN    100
#define MAX_TEXT_LENGTH 1024

// Tree structure
typedef struct Tree {
    char         text[MAX_TEXT_LENGTH];
    struct Tree *items[MAX_CHILDREN];
    int          item_count;
} Tree;

// Function prototypes
Tree *new_tree(const char *text);
Tree *add_node(Tree *t, const char *text);
void  add_subtree(Tree *t, Tree *subtree);
char *print_tree(Tree *t);
void  free_tree(Tree *t);

#endif /* TREE_H */