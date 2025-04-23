#include "cmd_node.h"
#include "lang_config.h"
#include "markdown.h"
#include "tree.h"
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>

// Global verbose flag
static int VERBOSE_PRINT = 0;

// Verbose message function
void verbosePrintf(const char *format, ...) {
    if (!VERBOSE_PRINT) return;
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
}

// Convert cmd_node to Tree structure
static Tree *convert_to_tree(struct cmd_node *node) {
    if (!node) return NULL;

    // Skip nodes without code blocks unless it's a root node (level 1)
    if (node->level > 1 && !node->code_blocks) {
        // Still process children even if we skip this node
        Tree *result = NULL;
        struct cmd_node *child = node->children;
        while (child) {
            Tree *child_tree = convert_to_tree(child);
            if (child_tree) {
                if (!result) {
                    result = child_tree;
                } else {
                    add_subtree(result, child_tree);
                }
            }
            child = child->next;
        }
        return result;
    }

    Tree *tree = new_tree(node->heading_text ? node->heading_text : "(root)");
    struct cmd_node *child = node->children;
    while (child) {
        Tree *child_tree = convert_to_tree(child);
        if (child_tree) {
            add_subtree(tree, child_tree);
        }
        child = child->next;
    }
    return tree;
}

static void print_help(const char *program_name) {
    fprintf(stderr, "Usage: %s [--file <markdown_file>] [--verbose] <heading...> [-- <args...>]\n", program_name);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -f, --file     Specify markdown file to use\n");
    fprintf(stderr, "  -v, --verbose  Enable verbose output\n");
    fprintf(stderr, "  -h, --help     Show this help message\n");
}

static void print_command_tree(struct cmd_node *root) {
    struct cmd_node *child = root->children;
    while (child) {
        if (child->level == 1) {
            Tree *tree = convert_to_tree(child);
            if (tree) {
                char *tree_str = print_tree(tree);
                if (tree_str) {
                    printf("%s\n", tree_str);
                    free(tree_str);
                }
                free_tree(tree);
            }
        }
        child = child->next;
    }
}

int main(int argc, char *argv[]) {
    char *markdown_file = NULL;
    char *found_file = NULL;
    char *buffer = NULL;
    FILE *file = NULL;
    struct cmd_node *root = NULL;
    int result = 1;

    // Find '--' separator in args
    int index = 0;
    while (index < argc) {
        if (strcmp(argv[index++], "--") == 0) break;
    }

    // Parse command line options
    struct option long_options[] = {
        {"file", required_argument, NULL, 'f'},
        {"help", no_argument, NULL, 'h'},
        {"verbose", no_argument, NULL, 'v'},
        {NULL, 0, NULL, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "f:hv", long_options, NULL)) != -1) {
        switch (opt) {
            case 'f': markdown_file = optarg; break;
            case 'h': print_help(argv[0]); return 0;
            case 'v': VERBOSE_PRINT = 1; break;
            default: print_help(argv[0]); return 1;
        }
    }

    // Find and read markdown file
    if (!markdown_file) {
        found_file = find_markdown_file(argv[0]);
        if (!found_file) {
            fprintf(stderr, "%s: No markdown file found\n", argv[0]);
            return 1;
        }
        markdown_file = found_file;
    }

    verbosePrintf("Using markdown file: %s\n", markdown_file);

    // Read file content
    if (!(file = fopen(markdown_file, "r"))) {
        perror("Error opening file");
        goto cleanup;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (!(buffer = malloc(size + 1))) {
        perror("Memory allocation failed");
        goto cleanup;
    }

    size_t read_size = fread(buffer, 1, size, file);
    buffer[read_size] = '\0';
    fclose(file);
    file = NULL;

    // Set environment variables
    setenv("MD_EXE", argv[0], 1);
    setenv("MD_FILE", markdown_file, 1);

    // Parse markdown and process commands
    if (!(root = parse_markdown_content(buffer))) {
        fprintf(stderr, "Failed to parse markdown content\n");
        goto cleanup;
    }

    if (optind < argc) {
        // Execute specific command
        char **heading_path = argv + optind;
        int num_headings = index - optind;
        char **cmd_args = argv + index;
        int num_args = argc - index;

        verbosePrintf("Executing command with %d heading(s) and %d argument(s)\n", 
                     num_headings, num_args);

        result = find_and_execute_command(root, heading_path, num_headings, 
                                        cmd_args, num_args) ? 0 : 1;
    } else {
        // Print command tree
        verbosePrintf("No command specified, printing trees\n");
        print_command_tree(root);
    }

cleanup:
    if (root) free_cmd_node(root);
    if (buffer) free(buffer);
    if (file) fclose(file);
    if (found_file) free(found_file);

    return result;
}