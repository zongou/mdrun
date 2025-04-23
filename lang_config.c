#include "lang_config.h"
#include <string.h>
#include <strings.h>

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
    {"powershell", powershell_args, 3}
};

const struct language_config* get_language_config(const char *lang) {
    if (!lang) return NULL;
    
    for (size_t i = 0; i < sizeof(language_configs) / sizeof(language_configs[0]); i++) {
        if (strcasecmp(language_configs[i].name, lang) == 0) {
            return &language_configs[i];
        }
    }
    return NULL;
}

int is_language_supported(const char *lang) {
    return get_language_config(lang) != NULL;
}