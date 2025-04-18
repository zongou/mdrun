#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cmark.h>

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
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <markdown_file>\n", argv[0]);
        return 1;
    }

    // Read the markdown file
    FILE *file = fopen(argv[1], "r");
    if (!file) {
        perror("Error opening file");
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

    return 0;
}