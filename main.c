#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cmark.h>
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

void print_node(cmark_node *node, int level) {
    for (int i = 0; i < level; i++) {
        printf("  ");
    }

    // Print node type
    printf("%s", cmark_node_get_type_string(node));

    // Print node content if available
    const char *literal = cmark_node_get_literal(node);
    if (literal) {
        printf(": \"%s\"", literal);
    }
    printf("\n");

    // Recursively print children
    cmark_node *child = cmark_node_first_child(node);
    while (child) {
        print_node(child, level + 1);
        child = cmark_node_next(child);
    }
}

int main(int argc, char *argv[]) {
    char *markdown_file = NULL;
    int opt;
    struct option long_options[] = {
        {"file", required_argument, NULL, 'f'},
        {NULL, 0, NULL, 0}
    };

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
        markdown_file = find_markdown_file(argv[0]);
        if (!markdown_file) {
            fprintf(stderr, "%s: No markdown file found\n", argv[0]);
            return 1;
        }
    }

    // Read the markdown file
    FILE *file = fopen(markdown_file, "r");
    if (!file) {
        perror("Error opening file");
        if (markdown_file != optarg) free(markdown_file);
        return 1;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Allocate buffer and read file
    char *buffer = malloc(size + 1);
    if (!buffer) {
        perror("Memory allocation failed");
        fclose(file);
        if (markdown_file != optarg) free(markdown_file);
        return 1;
    }

    size_t read_size = fread(buffer, 1, size, file);
    buffer[read_size] = '\0';
    fclose(file);

    // Parse markdown
    cmark_parser *parser = cmark_parser_new(CMARK_OPT_DEFAULT);
    cmark_parser_feed(parser, buffer, strlen(buffer));
    cmark_node *doc = cmark_parser_finish(parser);

    // Print AST
    if (doc) {
        printf("Markdown AST:\n");
        print_node(doc, 0);
        cmark_node_free(doc);
    }

    // Cleanup
    cmark_parser_free(parser);
    free(buffer);
    if (markdown_file != optarg) free(markdown_file);

    return 0;
}