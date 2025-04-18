#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <libgen.h>
#include <getopt.h>
#include <limits.h>

// Case insensitive string comparison
int strcicmp(const char *a, const char *b) {
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
            return tolower((unsigned char)*a) - tolower((unsigned char)*b);
        }
        a++;
        b++;
    }
    return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}

// Find markdown file in current and parent directories
char* find_markdown_file(const char *program_name) {
    char current_dir[PATH_MAX];
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
    char *dot = strrchr(base_name, '.');
    if (dot) *dot = '\0';

    while (1) {
        DIR *d = opendir(working_dir);
        if (!d) {
            break;
        }

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
        if (strcmp(parent, working_dir) == 0) {
            break;  // Reached root directory
        }
        strncpy(working_dir, parent, PATH_MAX);
    }

cleanup:
    free(working_dir);
    free(base_prog);
    return result;
}

// Environment variable entry
struct env_entry {
    char *key;
    char *value;
    struct env_entry *next;
};

// Code block entry
struct code_block {
    char *info;
    char *content;
    struct code_block *next;
};

// Command node structure (similar to Go's cmdNode)
struct cmd_node {
    // Heading info
    int level;
    char *heading_text;
    
    // Code blocks
    struct code_block *code_blocks;
    
    // Environment variables
    struct env_entry *env;
    
    // Node relationships
    struct cmd_node *parent;
    struct cmd_node *next;  // Next sibling
    struct cmd_node *children;
    
    // Description
    char *description;
};

// Create a new command node
struct cmd_node *create_cmd_node(int level, const char *heading) {
    struct cmd_node *node = calloc(1, sizeof(struct cmd_node));
    if (!node) return NULL;
    
    node->level = level;
    if (heading) {
        node->heading_text = strdup(heading);
    }
    return node;
}

// Add code block to a node
void add_code_block(struct cmd_node *node, const char *info, const char *content) {
    if (!node || !content) return;
    
    struct code_block *block = calloc(1, sizeof(struct code_block));
    if (!block) return;
    
    if (info) block->info = strdup(info);
    if (content) {
        size_t len = strlen(content);
        // Remove trailing newline if present
        while (len > 0 && content[len-1] == '\n') {
            len--;
        }
        block->content = malloc(len + 1);
        if (block->content) {
            memcpy(block->content, content, len);
            block->content[len] = '\0';
        }
    }
    
    // Add to front of list
    block->next = node->code_blocks;
    node->code_blocks = block;
}

// Add environment variable to a node
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

// Free a command node and all its resources
void free_cmd_node(struct cmd_node *node) {
    if (!node) return;

    // Free children first (depth-first cleanup)
    struct cmd_node *child = node->children;
    struct cmd_node *next_child;
    while (child) {
        next_child = child->next;
        child->next = NULL;  // Prevent circular references
        free_cmd_node(child);
        child = next_child;
    }
    node->children = NULL;

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
    node->code_blocks = NULL;

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
    node->env = NULL;

    // Free node's own resources
    if (node->heading_text) free(node->heading_text);
    if (node->description) free(node->description);
    
    // Clear pointers before freeing
    node->parent = NULL;
    node->next = NULL;
    free(node);
}

// Safe string duplication
char *safe_strdup(const char *str) {
    if (!str) return NULL;
    size_t len = strlen(str);
    char *dup = malloc(len + 1);
    if (dup) {
        memcpy(dup, str, len);
        dup[len] = '\0';
    }
    return dup;
}

// Check if a line starts with a heading marker
int get_heading_level(const char *line) {
    int level = 0;
    while (*line == '#') {
        level++;
        line++;
    }
    
    // Must be followed by space and not exceed 6 levels
    return (level > 0 && level <= 6 && isspace(*line)) ? level : 0;
}

// Check if a line starts a code block
int is_code_block_start(const char *line, char *info) {
    while (isspace(*line)) line++;
    if (strncmp(line, "```", 3) != 0) return 0;
    
    // Extract language info if present
    line += 3;
    char *end = strchr(line, '\n');
    if (end) {
        size_t len = end - line;
        while (len > 0 && isspace(line[len-1])) len--;
        if (len > 0 && info) {
            memcpy(info, line, len);
            info[len] = '\0';
        }
    }
    return 1;
}

// Parse a table row into key-value pair
void parse_table_row(struct cmd_node *node, const char *row) {
    if (!node || !row) return;
    
    // Skip leading whitespace
    const char *ptr = row;
    while (*ptr && isspace(*ptr)) ptr++;
    
    // Must start with pipe
    if (*ptr != '|') return;
    ptr++;
    
    // Parse first column (key)
    while (*ptr && isspace(*ptr)) ptr++;
    const char *key_start = ptr;
    
    // Find end of key (next pipe)
    while (*ptr && *ptr != '|') ptr++;
    if (!*ptr) return;
    
    // Trim key
    const char *key_end = ptr - 1;
    while (key_end > key_start && isspace(*key_end)) key_end--;
    key_end++;
    
    // Skip pipe and whitespace
    ptr++;
    while (*ptr && isspace(*ptr)) ptr++;
    
    // Parse second column (value)
    const char *value_start = ptr;
    
    // Find end of value (next pipe or end of string)
    while (*ptr && *ptr != '|') ptr++;
    const char *value_end = ptr - 1;
    while (value_end > value_start && isspace(*value_end)) value_end--;
    value_end++;
    
    // Skip empty or separator rows
    if (key_end <= key_start || value_end <= value_start) return;
    if (strstr(key_start, "---") || strstr(value_start, "---")) return;
    
    // Extract and clean key/value
    size_t key_len = key_end - key_start;
    size_t value_len = value_end - value_start;
    
    char *key = malloc(key_len + 1);
    char *value = malloc(value_len + 1);
    
    if (key && value) {
        memcpy(key, key_start, key_len);
        key[key_len] = '\0';
        memcpy(value, value_start, value_len);
        value[value_len] = '\0';
        
        // Add to environment if not header row
        if (strcasecmp(key, "key") != 0 && strcasecmp(value, "value") != 0) {
            add_env_var(node, key, value);
        }
    }
    
    free(key);
    free(value);
}

// Parse markdown content into command nodes
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

    char *line = strtok(buffer, "\n");
    char *code_buffer = NULL;
    size_t code_size = 0;
    int in_code_block = 0;
    char code_info[256] = {0};
    int in_table = 0;
    
    while (line) {
        char *trimmed = line;
        while (isspace(*trimmed)) trimmed++;

        if (in_code_block) {
            if (strncmp(trimmed, "```", 3) == 0) {
                // End of code block
                if (code_buffer && current) {
                    // Remove trailing newline if present
                    if (code_size > 0 && code_buffer[code_size-1] == '\n') {
                        code_buffer[--code_size] = '\0';
                    }
                    add_code_block(current, code_info, code_buffer);
                }
                free(code_buffer);
                code_buffer = NULL;
                code_size = 0;
                in_code_block = 0;
            } else {
                // Append to code buffer
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
                // Handle heading
                const char *heading_text = trimmed + level;
                while (isspace(*heading_text)) heading_text++;
                
                struct cmd_node *heading = create_cmd_node(level, heading_text);
                if (heading) {
                    // Find correct parent based on heading level
                    struct cmd_node *parent = current;
                    while (parent && parent != root && parent->level >= level) {
                        parent = parent->parent;
                    }
                    if (!parent) parent = root;

                    // Add to parent's children
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
                // Start of code block
                in_code_block = 1;
                code_info[0] = '\0';
                is_code_block_start(trimmed, code_info);
            } else if (*trimmed == '|') {
                // Table row
                if (!in_table) {
                    in_table = 1;
                } else if (!strstr(trimmed, "---")) {
                    parse_table_row(current, trimmed);
                }
            } else if (*trimmed == '\0') {
                // Empty line ends table
                in_table = 0;
            } else if (!in_table && current && !current->description) {
                // Regular text becomes description
                current->description = strdup(trimmed);
            }
        }
        
        line = strtok(NULL, "\n");
    }

    // Clean up
    if (code_buffer) {
        free(code_buffer);
    }
    free(buffer);

    return root;
}

// Print environment variables in A=B format
void print_cmd_tree(struct cmd_node *node, int level) {
    if (!node) return;

    // Print indentation
    for (int i = 0; i < level; i++) printf("  ");

    // Print node info
    if (node->heading_text) {
        printf("Heading(%d): %s\n", node->level, node->heading_text);
    } else {
        printf("(root)\n");
    }

    // Print description if exists
    if (node->description) {
        for (int i = 0; i < level + 1; i++) printf("  ");
        printf("Description: %s\n", node->description);
    }

    // Print environment variables in A=B format
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
        print_cmd_tree(child, level + 1);
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

    struct option long_options[] = {
        {"file", required_argument, NULL, 'f'},
        {NULL, 0, NULL, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "f:", long_options, NULL)) != -1) {
        switch (opt) {
            case 'f':
                markdown_file = optarg;
                break;
            default:
                fprintf(stderr, "Usage: %s [--file <markdown_file>]\n", argv[0]);
                return 1;
        }
    }

    // If no file specified, search for one
    if (!markdown_file) {
        found_file = find_markdown_file(argv[0]);
        if (!found_file) {
            fprintf(stderr, "%s: No markdown file found\n", argv[0]);
            return 1;
        }
        markdown_file = found_file;
    }

    // Read the markdown file
    file = fopen(markdown_file, "r");
    if (!file) {
        perror("Error opening file");
        goto cleanup;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Allocate buffer and read file
    buffer = malloc(size + 1);
    if (!buffer) {
        perror("Memory allocation failed");
        goto cleanup;
    }

    size_t read_size = fread(buffer, 1, size, file);
    buffer[read_size] = '\0';
    fclose(file);
    file = NULL;  // Mark as closed

    // Parse markdown content
    root = parse_markdown_content(buffer);
    if (root) {
        printf("Markdown Document Structure:\n\n");
        print_cmd_tree(root, 0);
        result = 0;  // Success
    }

cleanup:
    // Free resources in reverse order of allocation
    if (root) {
        free_cmd_node(root);
        root = NULL;
    }
    if (buffer) {
        free(buffer);
        buffer = NULL;
    }
    if (file) {
        fclose(file);
        file = NULL;
    }
    if (found_file) {
        free(found_file);
        found_file = NULL;
    }

    return result;
}