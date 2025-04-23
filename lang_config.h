#ifndef LANG_CONFIG_H
#define LANG_CONFIG_H

#include <stddef.h>

// Language configuration structure
struct language_config {
    const char  *name;
    const char **prefix_args;
    size_t       prefix_args_count;
};

// Get language configuration by name
const struct language_config* get_language_config(const char *lang);

// Check if a language is supported
int is_language_supported(const char *lang);

#endif /* LANG_CONFIG_H */