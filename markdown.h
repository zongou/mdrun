#ifndef MARKDOWN_H
#define MARKDOWN_H

#include "cmd_node.h"

// Parse markdown content into command nodes
struct cmd_node *parse_markdown_content(const char *content);

// Find markdown file in current and parent directories
char *find_markdown_file(const char *program_name);

// Parse a table row into key-value pair
void parse_table_row(struct cmd_node *node, const char *row);

// Check if a line starts with a heading marker
int get_heading_level(const char *line);

// Check if a line starts a code block
int is_code_block_start(const char *line, char *info);

// Find and execute command under specified heading path
int find_and_execute_command(struct cmd_node *root, char **heading_path, int num_headings, char **args, int num_args);

#endif /* MARKDOWN_H */