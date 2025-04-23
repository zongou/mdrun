#include "cmd_node.h"
#include "lang_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

struct cmd_node *create_cmd_node(int level, const char *heading) {
    struct cmd_node *node = calloc(1, sizeof(struct cmd_node));
    if (!node) return NULL;

    node->level = level;
    if (heading) {
        node->heading_text = strdup(heading);
    }
    return node;
}

void add_code_block(struct cmd_node *node, const char *info, const char *content) {
    if (!node || !content) return;

    // Skip if language is not supported
    if (info && !is_language_supported(info)) return;

    struct code_block *block = calloc(1, sizeof(struct code_block));
    if (!block) return;

    if (info) block->info = strdup(info);
    if (content) {
        size_t len = strlen(content);
        // Remove trailing newline if present
        while (len > 0 && content[len - 1] == '\n') {
            len--;
        }
        block->content = malloc(len + 1);
        if (block->content) {
            memcpy(block->content, content, len);
            block->content[len] = '\0';
        }
    }

    // Add to end of list to preserve order
    if (!node->code_blocks) {
        node->code_blocks = block;
    } else {
        struct code_block *last = node->code_blocks;
        while (last->next) {
            last = last->next;
        }
        last->next = block;
    }
}

void add_env_var(struct cmd_node *node, const char *key, const char *value) {
    if (!node || !key || !value) return;

    struct env_entry *entry = calloc(1, sizeof(struct env_entry));
    if (!entry) return;

    entry->key = strdup(key);
    entry->value = strdup(value);

    // Add to front of list
    entry->next = node->env;
    node->env = entry;
}

void free_cmd_node(struct cmd_node *node) {
    if (!node) return;

    // Free children first (depth-first cleanup)
    struct cmd_node *child = node->children;
    struct cmd_node *next_child;
    while (child) {
        next_child = child->next;
        child->next = NULL; // Prevent circular references
        free_cmd_node(child);
        child = next_child;
    }

    // Free code blocks
    struct code_block *block = node->code_blocks;
    struct code_block *next_block;
    while (block) {
        next_block = block->next;
        if (block->info) free(block->info);
        if (block->content) free(block->content);
        free(block);
        block = next_block;
    }

    // Free environment variables
    struct env_entry *env = node->env;
    struct env_entry *next_env;
    while (env) {
        next_env = env->next;
        if (env->key) free(env->key);
        if (env->value) free(env->value);
        free(env);
        env = next_env;
    }

    // Free node's own resources
    if (node->heading_text) free(node->heading_text);
    if (node->description) free(node->description);
    free(node);
}

int execute_code_blocks(struct cmd_node *node, char **args, int num_args) {
    if (!node) return 0;

    struct code_block *block = node->code_blocks;
    while (block) {
        if (block->info && block->content) {
            const struct language_config *config = get_language_config(block->info);
            if (!config) {
                fprintf(stderr, "Unsupported language: %s\n", block->info);
                return 0;
            }

            // Fork and execute
            pid_t pid = fork();
            if (pid == -1) {
                perror("fork failed");
                return 0;
            }

            if (pid == 0) {
                // Child process
                int total_args = config->prefix_args_count + (num_args > 0 ? num_args : 0);
                char **exec_args = calloc(total_args + 1, sizeof(char *));
                if (!exec_args) _exit(1);

                // Fill argument array
                int arg_idx = 0;
                for (size_t i = 0; i < config->prefix_args_count; i++) {
                    if (strcmp(config->prefix_args[i], "$CODE") == 0) {
                        exec_args[arg_idx++] = block->content;
                    } else if (strcmp(config->prefix_args[i], "$NAME") == 0) {
                        exec_args[arg_idx++] = (char *)config->name;
                    } else {
                        exec_args[arg_idx++] = (char *)config->prefix_args[i];
                    }
                }

                // Add user arguments
                for (int i = 0; i < num_args; i++) {
                    exec_args[arg_idx++] = args[i];
                }
                exec_args[arg_idx] = NULL;

                execvp(exec_args[0], exec_args);
                perror("execvp failed");
                free(exec_args);
                _exit(1);
            } else {
                // Parent process
                int status;
                waitpid(pid, &status, 0);
                if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
                    fprintf(stderr, "Command failed with status %d\n", WEXITSTATUS(status));
                    return 0;
                }
            }
        }
        block = block->next;
    }
    return 1;
}

void print_cmd_node(struct cmd_node *node, int level) {
    if (!node) return;

    // Print indentation
    for (int i = 0; i < level; i++) printf("  ");

    // Print node info
    if (node->heading_text) {
        printf("Heading(%d): %s\n", node->level, node->heading_text);
    }

    // Print description if exists
    if (node->description) {
        for (int i = 0; i < level + 1; i++) printf("  ");
        printf("Description: %s\n", node->description);
    }

    // Print environment variables
    struct env_entry *env = node->env;
    while (env) {
        for (int i = 0; i < level + 1; i++) printf("  ");
        printf("%s=%s\n", env->key, env->value);
        env = env->next;
    }

    // Print code blocks
    struct code_block *block = node->code_blocks;
    while (block) {
        for (int i = 0; i < level + 1; i++) printf("  ");
        printf("Code(%s): %s\n", block->info ? block->info : "none", block->content);
        block = block->next;
    }

    // Print children
    struct cmd_node *child = node->children;
    while (child) {
        print_cmd_node(child, level + 1);
        child = child->next;
    }
}