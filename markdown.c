#include "markdown.h"
#include <ctype.h>
#include <dirent.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

// Case insensitive string comparison
static int strcicmp(const char *a, const char *b) {
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
            return tolower((unsigned char)*a) - tolower((unsigned char)*b);
        }
        a++;
        b++;
    }
    return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}

char *find_markdown_file(const char *program_name) {
    char  current_dir[PATH_MAX];
    char *result = NULL;

    if (getcwd(current_dir, sizeof(current_dir)) == NULL) {
        return NULL;
    }

    char *working_dir = strdup(current_dir);
    if (!working_dir) return NULL;

    char *base_prog = strdup(program_name);
    if (!base_prog) {
        free(working_dir);
        return NULL;
    }

    // Get program name without path and extension
    char *base_name = basename(base_prog);
    char *dot       = strrchr(base_name, '.');
    if (dot) *dot = '\0';

    while (1) {
        DIR *d = opendir(working_dir);
        if (!d) break;

        struct dirent *entry;
        while ((entry = readdir(d)) != NULL) {
            char test_path[PATH_MAX];
            snprintf(test_path, sizeof(test_path), "%s/%s", working_dir, entry->d_name);

            // Check for program.md or .program.md (case insensitive)
            char expected_name[PATH_MAX];
            snprintf(expected_name, sizeof(expected_name), "%s.md", base_name);
            char hidden_name[PATH_MAX];
            snprintf(hidden_name, sizeof(hidden_name), ".%s.md", base_name);

            if (strcicmp(entry->d_name, expected_name) == 0 ||
                strcicmp(entry->d_name, hidden_name) == 0) {
                result = strdup(test_path);
                closedir(d);
                goto cleanup;
            }

            // Check for README.md if we haven't found a file yet
            if (!result && strcicmp(entry->d_name, "README.md") == 0) {
                result = strdup(test_path);
            }
        }
        closedir(d);

        // Move to parent directory
        char *parent = dirname(working_dir);
        if (strcmp(parent, working_dir) == 0) break; // Reached root
        strncpy(working_dir, parent, PATH_MAX);
    }

cleanup:
    free(working_dir);
    free(base_prog);
    return result;
}

int get_heading_level(const char *line) {
    int level = 0;
    while (*line == '#') {
        level++;
        line++;
    }
    return (level > 0 && level <= 6 && isspace(*line)) ? level : 0;
}

int is_code_block_start(const char *line, char *info) {
    while (isspace(*line)) line++;
    if (strncmp(line, "```", 3) != 0) return 0;

    // Extract language info if present
    line += 3;
    char *end = strchr(line, '\0');
    if (end) {
        size_t len = end - line;
        while (len > 0 && isspace(line[len - 1])) len--;
        if (len > 0 && info) {
            memcpy(info, line, len);
            info[len] = '\0';
        }
    }
    return 1;
}

void parse_table_row(struct cmd_node *node, const char *row) {
    if (!node || !row) return;

    const char *ptr = row;
    while (*ptr && isspace(*ptr)) ptr++;
    if (*ptr != '|') return;
    ptr++;

    // Parse key
    while (*ptr && isspace(*ptr)) ptr++;
    const char *key_start = ptr;
    while (*ptr && *ptr != '|') ptr++;
    if (!*ptr) return;
    const char *key_end = ptr - 1;
    while (key_end > key_start && isspace(*key_end)) key_end--;
    key_end++;

    // Skip pipe and whitespace
    ptr++;
    while (*ptr && isspace(*ptr)) ptr++;

    // Parse value
    const char *value_start = ptr;
    while (*ptr && *ptr != '|') ptr++;
    const char *value_end = ptr - 1;
    while (value_end > value_start && isspace(*value_end)) value_end--;
    value_end++;

    // Skip empty or separator rows
    if (key_end <= key_start || value_end <= value_start) return;
    if (strstr(key_start, "---") || strstr(value_start, "---")) return;

    // Create key/value strings
    size_t key_len = key_end - key_start;
    size_t value_len = value_end - value_start;
    char *key = malloc(key_len + 1);
    char *value = malloc(value_len + 1);

    if (key && value) {
        memcpy(key, key_start, key_len);
        key[key_len] = '\0';
        memcpy(value, value_start, value_len);
        value[value_len] = '\0';

        if (strcasecmp(key, "key") != 0 && strcasecmp(value, "value") != 0) {
            add_env_var(node, key, value);
        }
    }

    free(key);
    free(value);
}

struct cmd_node *parse_markdown_content(const char *content) {
    if (!content) return NULL;

    struct cmd_node *root = create_cmd_node(0, NULL);
    if (!root) return NULL;

    struct cmd_node *current = root;
    char *buffer = strdup(content);
    if (!buffer) {
        free_cmd_node(root);
        return NULL;
    }

    char  *line = strtok(buffer, "\n");
    char  *code_buffer = NULL;
    size_t code_size = 0;
    int    in_code_block = 0;
    char   code_info[256] = {0};
    int    in_table = 0;

    while (line) {
        char *trimmed = line;
        while (isspace(*trimmed)) trimmed++;

        if (in_code_block) {
            if (strncmp(trimmed, "```", 3) == 0) {
                if (code_buffer && current) {
                    if (code_size > 0 && code_buffer[code_size - 1] == '\n') {
                        code_buffer[--code_size] = '\0';
                    }
                    add_code_block(current, code_info, code_buffer);
                }
                free(code_buffer);
                code_buffer = NULL;
                code_size = 0;
                in_code_block = 0;
            } else {
                size_t line_len = strlen(line);
                char *new_buffer = realloc(code_buffer, code_size + line_len + 2);
                if (new_buffer) {
                    code_buffer = new_buffer;
                    memcpy(code_buffer + code_size, line, line_len);
                    code_size += line_len;
                    code_buffer[code_size++] = '\n';
                    code_buffer[code_size] = '\0';
                }
            }
        } else {
            int level = get_heading_level(trimmed);
            if (level > 0) {
                const char *heading_text = trimmed + level;
                while (isspace(*heading_text)) heading_text++;

                struct cmd_node *heading = create_cmd_node(level, heading_text);
                if (heading) {
                    struct cmd_node *parent = current;
                    while (parent && parent != root && parent->level >= level) {
                        parent = parent->parent;
                    }
                    if (!parent) parent = root;

                    heading->parent = parent;
                    if (!parent->children) {
                        parent->children = heading;
                    } else {
                        struct cmd_node *sibling = parent->children;
                        while (sibling->next) sibling = sibling->next;
                        sibling->next = heading;
                    }

                    current = heading;
                }
            } else if (strncmp(trimmed, "```", 3) == 0) {
                in_code_block = 1;
                code_info[0] = '\0';
                is_code_block_start(trimmed, code_info);
            } else if (*trimmed == '|') {
                if (!in_table) {
                    in_table = 1;
                } else if (!strstr(trimmed, "---")) {
                    parse_table_row(current, trimmed);
                }
            } else if (*trimmed == '\0') {
                in_table = 0;
            } else if (!in_table && current && !current->description) {
                current->description = strdup(trimmed);
            }
        }

        line = strtok(NULL, "\n");
    }

    if (code_buffer) {
        free(code_buffer);
    }
    free(buffer);

    return root;
}

int find_and_execute_command(struct cmd_node *root, char **heading_path, int num_headings, char **args, int num_args) {
    if (!root || !heading_path || num_headings <= 0) return 0;

    struct cmd_node *current = root;
    int found;

    for (int i = 0; i < num_headings; i++) {
        found = 0;
        struct cmd_node *child = current->children;

        // First search direct children
        while (child) {
            if (child->heading_text && strcicmp(child->heading_text, heading_path[i]) == 0) {
                current = child;
                found = 1;
                break;
            }
            child = child->next;
        }

        // If not found in direct children, search recursively
        if (!found) {
            child = current->children;
            while (child && !found) {
                struct cmd_node *stack[100];
                int stack_size = 0;
                stack[stack_size++] = child;

                while (stack_size > 0 && !found) {
                    struct cmd_node *node = stack[--stack_size];
                    if (node->heading_text && strcicmp(node->heading_text, heading_path[i]) == 0) {
                        current = node;
                        found = 1;
                        break;
                    }

                    // Add children to stack
                    struct cmd_node *sub = node->children;
                    while (sub && stack_size < 100) {
                        stack[stack_size++] = sub;
                        sub = sub->next;
                    }
                }

                if (!found) child = child->next;
            }
        }

        if (!found) {
            fprintf(stderr, "Heading not found: %s\n", heading_path[i]);
            return 0;
        }
    }

    // Set environment variables from root to current node
    struct cmd_node *stack[100];
    int stack_size = 0;
    struct cmd_node *env_node = current;
    while (env_node) {
        stack[stack_size++] = env_node;
        env_node = env_node->parent;
    }

    for (int i = stack_size - 1; i >= 0; i--) {
        struct env_entry *env = stack[i]->env;
        while (env) {
            setenv(env->key, env->value, 1);
            env = env->next;
        }
    }

    return execute_code_blocks(current, args, num_args);
}