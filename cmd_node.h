#ifndef CMD_NODE_H
#define CMD_NODE_H

// Environment variable entry
struct env_entry {
    char             *key;
    char             *value;
    struct env_entry *next;
};

// Code block entry
struct code_block {
    char              *info;
    char              *content;
    struct code_block *next;
};

// Command node structure
struct cmd_node {
    int   level;
    char *heading_text;
    struct code_block *code_blocks;
    struct env_entry *env;
    struct cmd_node *parent;
    struct cmd_node *next;
    struct cmd_node *children;
    char *description;
};

struct cmd_node *create_cmd_node(int level, const char *heading);
void add_code_block(struct cmd_node *node, const char *info, const char *content);
void add_env_var(struct cmd_node *node, const char *key, const char *value);
void free_cmd_node(struct cmd_node *node);
int execute_code_blocks(struct cmd_node *node, char **args, int num_args);
void print_cmd_node(struct cmd_node *node, int level);

#endif /* CMD_NODE_H */