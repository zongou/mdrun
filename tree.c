#include "tree.h"

// Constants for tree printing
const char *NEW_LINE      = "\n";
const char *EMPTY_SPACE   = "    ";
const char *MIDDLE_ITEM   = "├── ";
const char *CONTINUE_ITEM = "│   ";
const char *LAST_ITEM     = "└── ";

// Internal function prototypes
static char *print_items(Tree *items[], int count, int spaces[], int space_count);
static char *print_text(const char *text, int spaces[], int space_count, int last);
static char *create_string_buffer(size_t size);

// Create a new tree node
Tree *new_tree(const char *text) {
    Tree *t = (Tree *)malloc(sizeof(Tree));
    strncpy(t->text, text, MAX_TEXT_LENGTH - 1);
    t->text[MAX_TEXT_LENGTH - 1] = '\0';
    t->item_count                = 0;
    return t;
}

// Add a new node to the tree
Tree *add_node(Tree *t, const char *text) {
    if (t->item_count >= MAX_CHILDREN)
        return NULL;
    Tree *child               = new_tree(text);
    t->items[t->item_count++] = child;
    return child;
}

// Add an existing tree as a subtree
void add_subtree(Tree *t, Tree *subtree) {
    if (t->item_count >= MAX_CHILDREN)
        return;
    t->items[t->item_count++] = subtree;
}

// Helper function to create a string buffer
static char *create_string_buffer(size_t size) {
    return (char *)malloc(size);
}

// Print tree to string
char *print_tree(Tree *t) {
    char *result = create_string_buffer(MAX_TEXT_LENGTH * 100); // Arbitrary size
    char *temp   = create_string_buffer(MAX_TEXT_LENGTH * 100);

    strcpy(result, t->text);
    strcat(result, NEW_LINE);

    int   spaces[100] = {0}; // For tracking vertical lines
    char *items_str   = print_items(t->items, t->item_count, spaces, 0);
    strcat(result, items_str);

    free(items_str);
    free(temp);
    return result;
}

// Print items recursively
static char *print_items(Tree *items[], int count, int spaces[], int space_count) {
    char *result = create_string_buffer(MAX_TEXT_LENGTH * 100);
    result[0]    = '\0';

    for (int i = 0; i < count; i++) {
        int   last     = (i == count - 1);
        char *text_str = print_text(items[i]->text, spaces, space_count, last);
        strcat(result, text_str);
        free(text_str);

        if (items[i]->item_count > 0) {
            int new_spaces[100];
            memcpy(new_spaces, spaces, sizeof(int) * space_count);
            new_spaces[space_count] = last;
            char *items_str         = print_items(items[i]->items, items[i]->item_count,
                                                  new_spaces, space_count + 1);
            strcat(result, items_str);
            free(items_str);
        }
    }

    return result;
}

// Print text with proper indentation and symbols
static char *print_text(const char *text, int spaces[], int space_count, int last) {
    char *result = create_string_buffer(MAX_TEXT_LENGTH * 100);
    result[0]    = '\0';

    for (int i = 0; i < space_count; i++) {
        strcat(result, spaces[i] ? EMPTY_SPACE : CONTINUE_ITEM);
    }

    const char *indicator = last ? LAST_ITEM : MIDDLE_ITEM;
    strcat(result, indicator);
    strcat(result, text);
    strcat(result, NEW_LINE);

    return result;
}

// Free tree memory
void free_tree(Tree *t) {
    if (!t) return;
    for (int i = 0; i < t->item_count; i++) {
        free_tree(t->items[i]);
    }
    free(t);
}