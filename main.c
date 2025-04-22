#include <ctype.h>
#include <dirent.h>
#include <getopt.h>
#include <libgen.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

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

// Language configuration structure
struct language_config {
    const char  *name;
    const char **prefix_args;
    size_t       prefix_args_count;
};

// Language configuration argument arrays
static const char *sh_args[]         = {"$NAME", "-euc", "$CODE", "--"};
static const char *awk_args[]        = {"awk", "$CODE"};
static const char *node_args[]       = {"node", "-e", "$CODE"};
static const char *python_args[]     = {"python", "-c", "$CODE"};
static const char *ruby_args[]       = {"ruby", "-e", "$CODE"};
static const char *php_args[]        = {"php", "-r", "$CODE"};
static const char *cmd_args[]        = {"cmd.exe", "/c", "$CODE"};
static const char *powershell_args[] = {"powershell.exe", "-c", "$CODE"};

// Language configuration mappings
static const struct language_config language_configs[] = {
    {"sh", sh_args, 4},
    {"bash", sh_args, 4},
    {"zsh", sh_args, 4},
    {"fish", sh_args, 4},
    {"dash", sh_args, 4},
    {"ksh", sh_args, 4},
    {"ash", sh_args, 4},
    {"shell", sh_args, 4},
    {"awk", awk_args, 2},
    {"js", node_args, 3},
    {"javascript", node_args, 3},
    {"py", python_args, 3},
    {"python", python_args, 3},
    {"rb", ruby_args, 3},
    {"ruby", ruby_args, 3},
    {"php", php_args, 3},
    {"cmd", cmd_args, 3},
    {"batch", cmd_args, 3},
    {"powershell", powershell_args, 3}};

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

// Command node structure (similar to Go's cmdNode)
struct cmd_node {
    // Heading info
    int   level;
    char *heading_text;

    // Code blocks
    struct code_block *code_blocks;

    // Environment variables
    struct env_entry *env;

    // Node relationships
    struct cmd_node *parent;
    struct cmd_node *next; // Next sibling
    struct cmd_node *children;

    // Description
    char *description;
};

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
            break; // Reached root directory
        }
        strncpy(working_dir, parent, PATH_MAX);
    }

cleanup:
    free(working_dir);
    free(base_prog);
    return result;
}

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

    // Skip if language is not supported
    if (info) {
        int supported = 0;
        for (size_t i = 0; i < sizeof(language_configs) / sizeof(language_configs[0]); i++) {
            if (strcasecmp(language_configs[i].name, info) == 0) {
                supported = 1;
                break;
            }
        }
        if (!supported) return; // Skip unsupported language
    }

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

// Add environment variable to a node
void add_env_var(struct cmd_node *node, const char *key, const char *value) {
    if (!node || !key || !value) return;

    struct env_entry *entry = calloc(1, sizeof(struct env_entry));
    if (!entry) return;

    entry->key   = strdup(key);
    entry->value = strdup(value);

    // Add to front of list
    entry->next = node->env;
    node->env   = entry;
}

// Free a command node and all its resources
void free_cmd_node(struct cmd_node *node) {
    if (!node) return;

    // Free children first (depth-first cleanup)
    struct cmd_node *child = node->children;
    struct cmd_node *next_child;
    while (child) {
        next_child  = child->next;
        child->next = NULL; // Prevent circular references
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
    node->next   = NULL;
    free(node);
}

// Safe string duplication
char *safe_strdup(const char *str) {
    if (!str) return NULL;
    size_t len = strlen(str);
    char  *dup = malloc(len + 1);
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
    size_t key_len   = key_end - key_start;
    size_t value_len = value_end - value_start;

    char *key   = malloc(key_len + 1);
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
    char            *buffer  = strdup(content);
    if (!buffer) {
        free_cmd_node(root);
        return NULL;
    }

    char  *line           = strtok(buffer, "\n");
    char  *code_buffer    = NULL;
    size_t code_size      = 0;
    int    in_code_block  = 0;
    char   code_info[256] = {0};
    int    in_table       = 0;

    while (line) {
        char *trimmed = line;
        while (isspace(*trimmed)) trimmed++;

        if (in_code_block) {
            if (strncmp(trimmed, "```", 3) == 0) {
                // End of code block
                if (code_buffer && current) {
                    // Remove trailing newline if present
                    if (code_size > 0 && code_buffer[code_size - 1] == '\n') {
                        code_buffer[--code_size] = '\0';
                    }
                    add_code_block(current, code_info, code_buffer);
                }
                free(code_buffer);
                code_buffer   = NULL;
                code_size     = 0;
                in_code_block = 0;
            } else {
                // Append to code buffer
                size_t line_len   = strlen(line);
                char  *new_buffer = realloc(code_buffer, code_size + line_len + 2);
                if (new_buffer) {
                    code_buffer = new_buffer;
                    memcpy(code_buffer + code_size, line, line_len);
                    code_size += line_len;
                    code_buffer[code_size++] = '\n';
                    code_buffer[code_size]   = '\0';
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
                code_info[0]  = '\0';
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
        print_cmd_node(child, level + 1);
        child = child->next;
    }
}

// Execute code blocks for a given node
int execute_code_blocks(struct cmd_node *node, char **args, int num_args) {
    if (!node) return 0;

    struct code_block *block = node->code_blocks;

    while (block) {
        if (block->info && block->content) {
            const char                   *lang   = block->info;
            const struct language_config *config = NULL;

            verbosePrintf("Executing %s code block:\n%s\n", lang, block->content);

            // Find language configuration
            for (size_t i = 0; i < sizeof(language_configs) / sizeof(language_configs[0]); i++) {
                if (strcasecmp(language_configs[i].name, lang) == 0) {
                    config = &language_configs[i];
                    break;
                }
            }

            if (config) {
                verbosePrintf("Using language config: %s\n", config->name);

                // Fork and execute
                pid_t pid = fork();
                if (pid == -1) {
                    perror("fork failed");
                    return 0;
                }

                if (pid == 0) {
                    // Child process
                    // Calculate number of arguments needed
                    int total_args = config->prefix_args_count; // Prefix arguments
                    if (num_args > 0) total_args += num_args;   // User arguments

                    // Allocate argument array
                    char **exec_args = calloc(total_args + 1, sizeof(char *));
                    if (!exec_args) {
                        _exit(1);
                    }

                    // Fill argument array with prefix args first
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
                    } else {
                        verbosePrintf("Command completed successfully\n");
                    }
                }
            } else {
                fprintf(stderr, "Unsupported language: %s\n", lang);
                return 0;
            }
        }
        block = block->next;
    }
    return 1;
}

// Find and execute command under specified heading path
int find_and_execute_command(struct cmd_node *root, char **heading_path, int num_headings, char **args, int num_args) {
    if (!root || !heading_path || num_headings <= 0) return 0;

    verbosePrintf("Looking for command path:");
    for (int i = 0; i < num_headings; i++) {
        verbosePrintf(" %s", heading_path[i]);
    }
    verbosePrintf("\n");

    struct cmd_node *current = root;
    int              found;

    // Search through all children recursively
    for (int i = 0; i < num_headings; i++) {
        found                  = 0;
        struct cmd_node *child = current->children;

        verbosePrintf("Looking under '%s' for heading '%s'\n",
                      current->heading_text ? current->heading_text : "(root)",
                      heading_path[i]);

        // First search direct children
        while (child) {
            if (child->heading_text && strcicmp(child->heading_text, heading_path[i]) == 0) {
                current = child;
                found   = 1;
                verbosePrintf("Found heading: %s\n", child->heading_text);
                break;
            }
            child = child->next;
        }

        // If not found in direct children, search recursively through each child's children
        if (!found) {
            child = current->children;
            while (child && !found) {
                // Recursively search this child's subtree
                struct cmd_node *stack[100]; // Max depth of 100 should be enough
                int              stack_size = 0;
                stack[stack_size++]         = child;

                while (stack_size > 0 && !found) {
                    struct cmd_node *node = stack[--stack_size];

                    if (node->heading_text && strcicmp(node->heading_text, heading_path[i]) == 0) {
                        current = node;
                        found   = 1;
                        verbosePrintf("Found heading: %s (nested)\n", node->heading_text);
                        break;
                    }

                    // Add children to stack
                    struct cmd_node *sub = node->children;
                    while (sub && stack_size < 100) {
                        stack[stack_size++] = sub;
                        sub                 = sub->next;
                    }
                }

                if (!found) {
                    child = child->next;
                }
            }
        }

        if (!found) {
            fprintf(stderr, "Heading not found: %s\n", heading_path[i]);
            return 0;
        }
    }

    verbosePrintf("Setting up environment variables\n");

    // First collect all nodes from root to target in a stack
    struct cmd_node *stack[100]; // Assuming max depth of 100
    int              stack_size = 0;
    struct cmd_node *env_node   = current;
    while (env_node) {
        stack[stack_size++] = env_node;
        env_node            = env_node->parent;
    }

    // Now set environment variables from root to leaf (reverse order of stack)
    for (int i = stack_size - 1; i >= 0; i--) {
        struct env_entry *env = stack[i]->env;
        while (env) {
            verbosePrintf("Setting %s=%s\n", env->key, env->value);
            setenv(env->key, env->value, 1);
            env = env->next;
        }
    }

    // Execute code blocks under the found heading
    return execute_code_blocks(current, args, num_args);
}

int main(int argc, char *argv[]) {
    int index = 0;
    while (index < argc) {
        if (strcmp(argv[index++], "--") == 0) {
            break;
        }
    }

    char            *markdown_file = NULL;
    char            *found_file    = NULL;
    char            *buffer        = NULL;
    FILE            *file          = NULL;
    struct cmd_node *root          = NULL;
    int              result        = 1;

    struct option long_options[] = {
        {"file", required_argument, NULL, 'f'},
        {"help", no_argument, NULL, 'h'},
        {"verbose", no_argument, NULL, 'v'},
        {NULL, 0, NULL, 0}};

    int opt;
    while ((opt = getopt_long(argc, argv, "f:hv", long_options, NULL)) != -1) {
        switch (opt) {
            case 0:
                printf("option %s", long_options[optind - 1].name);
                break;
            case 'f':
                markdown_file = optarg;
                break;
            case 'h':
                fprintf(stderr, "Usage: %s [--file <markdown_file>] [--verbose] <heading...> [-- <args...>]\n", argv[0]);
                fprintf(stderr, "Options:\n");
                fprintf(stderr, "  -f, --file     Specify markdown file to use\n");
                fprintf(stderr, "  -v, --verbose  Enable verbose output\n");
                fprintf(stderr, "  -h, --help     Show this help message\n");
                return 0;
            case 'v':
                VERBOSE_PRINT = 1;
                break;
            default:
                fprintf(stderr, "Usage: %s [--file <markdown_file>] [--verbose] <heading...> [-- <args...>]\n", argv[0]);
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

    verbosePrintf("Using markdown file: %s\n", markdown_file);

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

    size_t read_size  = fread(buffer, 1, size, file);
    buffer[read_size] = '\0';
    fclose(file);
    file = NULL;

    // Set environment variables
    setenv("MD_EXE", argv[0], 1);
    setenv("MD_FILE", markdown_file, 1);

    // Parse markdown content
    root = parse_markdown_content(buffer);
    if (!root) {
        fprintf(stderr, "Failed to parse markdown content\n");
        goto cleanup;
    }

    if (optind < argc) {
        char **heading_path = argv + optind;
        int    num_headings = index - optind;
        char **cmd_args     = argv + index;
        int    num_args     = argc - index;

        verbosePrintf("Executing command with %d heading(s) and %d argument(s)\n", num_headings, num_args);

        result = find_and_execute_command(root, heading_path, num_headings, cmd_args, num_args) ? 0 : 1;
    } else {
        verbosePrintf("No command specified, printing tree\n");
        print_cmd_node(root, 0);
    }

cleanup:
    if (root) free_cmd_node(root);
    if (buffer) free(buffer);
    if (file) fclose(file);
    if (found_file) free(found_file);

    return result;
}