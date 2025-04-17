#define _GNU_SOURCE
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
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

// Program name global
char *program_name;

// Language configuration structure
typedef struct {
  const char *cmd_name;
  const char **prefix_args;
  size_t prefix_args_len;
} language_config;

// Code block structure
typedef struct code_block {
  char *info;    // Language identifier
  char *literal; // Code content
  struct code_block *next;
} code_block;

// Command node structure for markdown parsing
typedef struct cmd_node {
  char *heading;
  int level;
  code_block *code_blocks;
  struct cmd_node **children;
  size_t children_len;
  size_t children_cap;
  char **env_keys;
  char **env_values;
  size_t env_len;
  struct cmd_node *parent;
  char *description;
} cmd_node;

// Global language configurations
static const char *AWK_ARGS[] = {"$CODE"};
static const char *SH_ARGS[] = {"-euc", "$CODE", "--"};
static const char *NODE_ARGS[] = {"-e", "$CODE"};
static const char *PYTHON_ARGS[] = {"-c", "$CODE"};
static const char *RUBY_ARGS[] = {"-e", "$CODE"};
static const char *PHP_ARGS[] = {"-r", "$CODE"};

static const language_config LANG_CONFIGS[] = {
    {"awk", AWK_ARGS, 1},         {"sh", SH_ARGS, 3},
    {"bash", SH_ARGS, 3},         {"zsh", SH_ARGS, 3},
    {"fish", SH_ARGS, 3},         {"dash", SH_ARGS, 3},
    {"ksh", SH_ARGS, 3},          {"ash", SH_ARGS, 3},
    {"shell", SH_ARGS, 3},        {"js", NODE_ARGS, 2},
    {"javascript", NODE_ARGS, 2}, {"py", PYTHON_ARGS, 2},
    {"python", PYTHON_ARGS, 2},   {"rb", RUBY_ARGS, 2},
    {"ruby", RUBY_ARGS, 2},       {"php", PHP_ARGS, 2}};

#define LANG_CONFIGS_LEN (sizeof(LANG_CONFIGS) / sizeof(LANG_CONFIGS[0]))

// Cross-platform strcasecmp implementation
#if defined(_WIN32) || defined(_WIN64)
static int strcasecmp(const char *s1, const char *s2) {
  while (*s1 && *s2) {
    int diff = tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
    if (diff != 0)
      return diff;
    s1++;
    s2++;
  }
  return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}
#endif

// Function prototypes
static void error_msg(const char *format, ...);
static char *strdup_safe(const char *s);
static void *realloc_safe(void *ptr, size_t size);
static char *find_doc(void);
static void cleanup_code_block(code_block *cb);
static void cleanup_cmd_node(cmd_node *node);
static cmd_node *parse_doc(const char *content);
static int exec_cmd_node(cmd_node *node, char **args, size_t args_len);
static void show_commands(cmd_node *nodes[], size_t nodes_len, int verbose);
static void show_help(void);
static void set_node_environment(cmd_node *node);
static cmd_node *find_child_by_heading(cmd_node *parent, const char *heading);

// Utility functions
static void error_msg(const char *format, ...) {
  va_list args;
  fprintf(stderr, "%s: ", program_name);
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fprintf(stderr, "\n");
}

static char *strdup_safe(const char *s) {
  char *d = strdup(s);
  if (!d) {
    error_msg("memory allocation failed");
    exit(EXIT_FAILURE);
  }
  return d;
}

static void *realloc_safe(void *ptr, size_t size) {
  void *new_ptr = realloc(ptr, size);
  if (!new_ptr && size != 0) {
    error_msg("memory allocation failed");
    exit(EXIT_FAILURE);
  }
  return new_ptr;
}

static char *find_doc(void) {
  char cwd[PATH_MAX];
  if (!getcwd(cwd, sizeof(cwd))) {
    error_msg("getting current directory: %s", strerror(errno));
    return NULL;
  }

  char path[PATH_MAX];
  struct stat st;
  DIR *dir;
  struct dirent *entry;

  while (1) {
    // Try program_name.md
    snprintf(path, sizeof(path), "%s/%s.md", cwd, program_name);
    if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
      return strdup_safe(path);
    }

    // Try .program_name.md
    snprintf(path, sizeof(path), "%s/.%s.md", cwd, program_name);
    if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
      return strdup_safe(path);
    }

    // Try README.md
    snprintf(path, sizeof(path), "%s/README.md", cwd);
    if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
      return strdup_safe(path);
    }

    // Go up one directory
    char *parent = dirname(cwd);
    if (strcmp(parent, cwd) == 0) {
      break; // Reached root directory
    }
    strcpy(cwd, parent);
  }

  error_msg("%s.md, .%s.md, or README.md not found", program_name,
            program_name);
  return NULL;
}

static void cleanup_code_block(code_block *cb) {
  if (cb) {
    free(cb->info);
    free(cb->literal);
    cleanup_code_block(cb->next);
    free(cb);
  }
}

static void cleanup_cmd_node(cmd_node *node) {
  if (node) {
    free(node->heading);
    cleanup_code_block(node->code_blocks);

    for (size_t i = 0; i < node->children_len; i++) {
      cleanup_cmd_node(node->children[i]);
    }
    free(node->children);

    for (size_t i = 0; i < node->env_len; i++) {
      free(node->env_keys[i]);
      free(node->env_values[i]);
    }
    free(node->env_keys);
    free(node->env_values);

    free(node->description);
    free(node);
  }
}

static int is_heading_line(const char *line) { return line[0] == '#'; }

static int get_heading_level(const char *line) {
  int level = 0;
  while (line[level] == '#')
    level++;
  return level;
}

static char *extract_heading_text(const char *line) {
  // Skip '#' characters
  while (*line == '#')
    line++;
  // Skip spaces
  while (*line == ' ')
    line++;
  return strdup_safe(line);
}

static const language_config *find_language_config(const char *lang) {
  for (size_t i = 0; i < LANG_CONFIGS_LEN; i++) {
    if (strcasecmp(LANG_CONFIGS[i].cmd_name, lang) == 0) {
      return &LANG_CONFIGS[i];
    }
  }
  return NULL;
}

static cmd_node *new_cmd_node(void) {
  cmd_node *node = calloc(1, sizeof(cmd_node));
  if (!node) {
    error_msg("memory allocation failed");
    exit(EXIT_FAILURE);
  }
  return node;
}

static void add_child(cmd_node *parent, cmd_node *child) {
  if (parent->children_len >= parent->children_cap) {
    size_t new_cap = parent->children_cap == 0 ? 4 : parent->children_cap * 2;
    parent->children =
        realloc_safe(parent->children, new_cap * sizeof(cmd_node *));
    parent->children_cap = new_cap;
  }
  parent->children[parent->children_len++] = child;
  child->parent = parent;
}

static void add_env_var(cmd_node *node, const char *key, const char *value) {
  size_t new_size = (node->env_len + 1) * sizeof(char *);
  node->env_keys = realloc_safe(node->env_keys, new_size);
  node->env_values = realloc_safe(node->env_values, new_size);

  node->env_keys[node->env_len] = strdup_safe(key);
  node->env_values[node->env_len] = strdup_safe(value);
  node->env_len++;
}

static void set_node_environment(cmd_node *node) {
  // Set environment variables from root to current node
  cmd_node *curr = node;
  while (curr) {
    for (size_t i = 0; i < curr->env_len; i++) {
      setenv(curr->env_keys[i], curr->env_values[i], 1);
    }
    curr = curr->parent;
  }
}

static cmd_node *parse_doc(const char *content) {
  cmd_node *root = new_cmd_node();
  cmd_node *current = root;
  code_block *current_block = NULL;
  int in_code_block = 0;
  int in_table = 0;
  int table_header = 1;  // First row is header
  int table_separator = 0;
  char *code_lang = NULL;
  char *code_content = NULL;
  size_t code_content_size = 0;

  char *line = NULL;
  size_t len = 0;
  ssize_t read;
  FILE *stream = fmemopen((void *)content, strlen(content), "r");

  if (!stream) {
    error_msg("failed to create memory stream");
    return NULL;
  }

  while ((read = getline(&line, &len, stream)) != -1) {
    // Remove trailing newline and carriage return
    while (read > 0 && (line[read - 1] == '\n' || line[read - 1] == '\r')) {
      line[--read] = '\0';
    }

    // Handle code blocks
    if (strncmp(line, "```", 3) == 0) {
      if (!in_code_block) {
        // Start of code block
        in_code_block = 1;
        code_lang = strdup_safe(line + 3);
        code_content_size = 0;
        code_content = NULL;
        in_table = 0; // Exit table mode if we were in it
      } else {
        // End of code block
        in_code_block = 0;
        if (code_lang && *code_lang && find_language_config(code_lang)) {
          code_block *cb = calloc(1, sizeof(code_block));
          if (!cb) {
            error_msg("memory allocation failed");
            exit(EXIT_FAILURE);
          }
          cb->info = code_lang;
          if (code_content) {
            // Remove trailing newline if present
            size_t len = strlen(code_content);
            if (len > 0 && code_content[len - 1] == '\n') {
              code_content[len - 1] = '\0';
            }
            cb->literal = code_content;
          } else {
            cb->literal = strdup_safe("");
          }

          // Add to current node's code blocks
          if (current->code_blocks) {
            code_block *last = current->code_blocks;
            while (last->next)
              last = last->next;
            last->next = cb;
          } else {
            current->code_blocks = cb;
          }
        } else {
          free(code_lang);
          free(code_content);
        }
        code_lang = NULL;
        code_content = NULL;
      }
      continue;
    }

    if (in_code_block) {
      // Append to code content
      size_t line_len = strlen(line);
      code_content =
          realloc_safe(code_content, code_content_size + line_len + 2);
      memcpy(code_content + code_content_size, line, line_len);
      code_content[code_content_size + line_len] = '\n';
      code_content[code_content_size + line_len + 1] = '\0';
      code_content_size += line_len + 1;
      continue;
    }

    // Handle headings
    if (is_heading_line(line)) {
      int level = get_heading_level(line);
      char *heading = extract_heading_text(line);

      cmd_node *new_node = new_cmd_node();
      new_node->heading = heading;
      new_node->level = level;

      // Find appropriate parent based on heading level
      cmd_node *parent = root;
      for (cmd_node *n = current; n != root; n = n->parent) {
        if (n->level < level) {
          parent = n;
          break;
        }
      }

      add_child(parent, new_node);
      current = new_node;
      in_table = 0; // Exit table mode
      table_header = 1; // Reset table state
      continue;
    }

    // Handle tables
    if (strchr(line, '|')) {
      in_table = 1;

      // Skip separator row
      if (strstr(line, "---") || strstr(line, "===")) {
        table_separator = 1;
        continue;
      }

      // Skip header row
      if (table_header) {
        table_header = 0;
        continue;
      }

      // Skip separator row after header
      if (table_separator) {
        table_separator = 0;
        continue;
      }

      char *row = strdup_safe(line);
      char *key = NULL;
      char *value = NULL;
      char *token = strtok(row, "|");
      int col = 0;

      while (token) {
        // Trim whitespace
        while (*token && isspace(*token))
          token++;
        char *end = token + strlen(token) - 1;
        while (end > token && isspace(*end))
          end--;
        end[1] = '\0';

        if (col == 0 && *token) {
          key = strdup_safe(token);
        } else if (col == 1 && *token) {
          value = strdup_safe(token);
        }
        col++;
        token = strtok(NULL, "|");
      }

      if (key && value) {
        add_env_var(current, key, value);
      }

      free(key);
      free(value);
      free(row);
      continue;
    } else if (in_table) {
      in_table = 0;
      table_header = 1;
    }

    // Handle description text
    if (!current->description && *line && !isspace(*line)) {
      current->description = strdup_safe(line);
    }
  }

  free(line);
  fclose(stream);
  return root;
}

static int exec_cmd_node(cmd_node *node, char **args, size_t args_len) {
  if (!node->code_blocks) {
    error_msg("no code blocks found");
    return -1;
  }

  // Set up environment variables before executing any code blocks
  set_node_environment(node);

  for (code_block *cb = node->code_blocks; cb; cb = cb->next) {
    const language_config *lang_config = find_language_config(cb->info);
    if (!lang_config) {
      error_msg("unsupported language: %s", cb->info);
      continue;
    }

    pid_t pid = fork();
    if (pid < 0) {
      error_msg("fork failed: %s", strerror(errno));
      return -1;
    }

    if (pid == 0) { // Child process
      // Prepare command arguments
      size_t total_args = lang_config->prefix_args_len + args_len + 1;
      char **cmd_args = calloc(total_args + 1, sizeof(char *));
      if (!cmd_args) {
        error_msg("memory allocation failed");
        exit(EXIT_FAILURE);
      }

      cmd_args[0] = (char *)lang_config->cmd_name;
      size_t arg_idx = 1;

      // Add prefix arguments, replacing $CODE with actual code
      for (size_t i = 0; i < lang_config->prefix_args_len; i++) {
        if (strcmp(lang_config->prefix_args[i], "$CODE") == 0) {
          cmd_args[arg_idx++] = cb->literal;
        } else {
          cmd_args[arg_idx++] = (char *)lang_config->prefix_args[i];
        }
      }

      // Add user arguments
      for (size_t i = 0; i < args_len; i++) {
        cmd_args[arg_idx++] = args[i];
      }

      execvp(lang_config->cmd_name, cmd_args);
      error_msg("exec failed: %s", strerror(errno));
      exit(EXIT_FAILURE);
    }

    // Parent process
    int status;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status)) {
      int exit_code = WEXITSTATUS(status);
      if (exit_code != 0) {
        error_msg("command exited with status %d", exit_code);
        return exit_code;
      }
    } else {
      error_msg("command terminated abnormally");
      return -1;
    }
  }

  return 0;
}

static void print_tree_node(cmd_node *node, int level, int verbose) {
  if (!node)
    return;

  // Print indentation
  for (int i = 0; i < level; i++) {
    printf("    ");
  }

  // Print heading
  if (node->heading) {
    printf("└── %s", node->heading);

    if (verbose) {
      // Print description if available
      if (node->description) {
        printf("  %s", node->description);
      }

      // Print environment variables
      for (size_t i = 0; i < node->env_len; i++) {
        printf("\n");
        for (int j = 0; j <= level; j++)
          printf("    ");
        printf("%s=%s", node->env_keys[i], node->env_values[i]);
      }

      // Print code blocks
      for (code_block *cb = node->code_blocks; cb; cb = cb->next) {
        printf("\n");
        for (int j = 0; j <= level; j++)
          printf("    ");
        printf("```%s\n", cb->info);
        for (int j = 0; j <= level; j++)
          printf("    ");
        printf("%s", cb->literal);
        for (int j = 0; j <= level; j++)
          printf("    ");
        printf("```");
      }
    }
    printf("\n");
  }

  // Print children
  for (size_t i = 0; i < node->children_len; i++) {
    print_tree_node(node->children[i], level + 1, verbose);
  }
}

static void show_commands(cmd_node **nodes, size_t nodes_len, int verbose) {
  for (size_t i = 0; i < nodes_len; i++) {
    print_tree_node(nodes[i], 0, verbose);
  }
}

static cmd_node *find_child_by_heading(cmd_node *parent, const char *heading) {
  for (size_t i = 0; i < parent->children_len; i++) {
    if (strcasecmp(parent->children[i]->heading, heading) == 0) {
      return parent->children[i];
    }
    // Recursively search children if not found at this level
    cmd_node *found = find_child_by_heading(parent->children[i], heading);
    if (found)
      return found;
  }
  return NULL;
}

static void show_help(void) {
  printf("Run markdown codeblocks by its heading.\n\n");
  printf("USAGE:\n");
  printf("    %s [--file FILE] <heading...> [-- <args...>]\n\n", program_name);
  printf("FLAGS:\n");
  printf("    -h, --help        Show this help\n");
  printf("    -v, --verbose     Print more information\n\n");
  printf("OPTIONS:\n");
  printf("    -f, --file        MarkDown file to use\n\n");
}

int main(int argc, char *argv[]) {
  program_name = basename(argv[0]);
  int verbose = 0;
  char *file_path = NULL;

  // Parse command line arguments - skip options when looking for commands
  char **heading_path = NULL;
  size_t heading_path_len = 0;
  char **cmd_args = NULL;
  size_t cmd_args_len = 0;
  int i;

  // First pass - handle flags
  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      show_help();
      return 0;
    } else if (strcmp(argv[i], "-v") == 0 ||
               strcmp(argv[i], "--verbose") == 0) {
      verbose = 1;
    } else if ((strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--file") == 0) &&
               i + 1 < argc) {
      file_path = argv[++i];
    } else if (strcmp(argv[i], "--") == 0) {
      break;
    } else {
      // First non-option argument marks start of heading path
      break;
    }
  }

  // Find separator and set up command arguments
  heading_path = &argv[i];
  heading_path_len = 0;

  while (i < argc && strcmp(argv[i], "--") != 0) {
    heading_path_len++;
    i++;
  }

  if (i < argc && strcmp(argv[i], "--") == 0) {
    i++; // Skip "--"
    cmd_args = &argv[i];
    cmd_args_len = argc - i;
  }

  if (!file_path) {
    file_path = find_doc();
    if (!file_path)
      return 1;
  }

  // Read the markdown file
  FILE *file = fopen(file_path, "r");
  if (!file) {
    error_msg("opening file: %s", strerror(errno));
    return 1;
  }

  // Get file size
  fseek(file, 0, SEEK_END);
  long size = ftell(file);
  rewind(file);

  // Read file content
  char *content = malloc(size + 1);
  if (!content) {
    error_msg("memory allocation failed");
    fclose(file);
    return 1;
  }

  if (fread(content, 1, size, file) != size) {
    error_msg("reading file: %s", strerror(errno));
    free(content);
    fclose(file);
    return 1;
  }
  content[size] = '\0';
  fclose(file);

  // Set environment variables
  setenv("MD_EXE", argv[0], 1);
  setenv("MD_FILE", file_path, 1);

  // Parse markdown and execute commands
  cmd_node *root = parse_doc(content);
  if (!root) {
    free(content);
    return 1;
  }

  // Execute command or show tree
  int result = 0;
  if (heading_path_len > 0) {
    cmd_node *current = NULL;
    // Try to find the command at any level
    for (size_t i = 0; i < root->children_len; i++) {
      current = find_child_by_heading(root->children[i], heading_path[0]);
      if (current)
        break;
    }

    if (!current) {
      error_msg("command path '%s' not found", heading_path[0]);
      result = 1;
      goto cleanup;
    }

    // Follow the rest of the path if any
    for (size_t idx = 1; idx < heading_path_len; idx++) {
      cmd_node *found = find_child_by_heading(current, heading_path[idx]);
      if (!found) {
        error_msg("command path '%s' not found", heading_path[idx]);
        result = 1;
        goto cleanup;
      }
      current = found;
    }

    result = exec_cmd_node(current, cmd_args, cmd_args_len);
  } else {
    show_commands(&root, 1, verbose);
  }

cleanup:
  // Cleanup
  cleanup_cmd_node(root);
  free(content);
  free(file_path);

  return result;
}