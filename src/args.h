#ifndef ARGS_H
#define ARGS_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    char* json_path;
    char* theme;
    int   show_icons;
    int   verbose;
    int   debug;
} Config;

/**
 * Parses argc/argv into cfg. Sets defaults before parsing.
 * Input:  argc, argv from main(); cfg — pointer to an uninitialised Config.
 * Output: 0 on success; 1 if --help or --version was printed (caller exits 0);
 *         -1 on invalid arguments. On success caller must call config_free().
 */
int config_parse(int argc, char* argv[], Config* cfg);

/**
 * Frees all heap-allocated fields inside cfg. Safe to call on a
 * partially-initialised or NULL pointer.
 * Input:  cfg — pointer to Config (may be NULL).
 * Output: none.
 */
void config_free(Config* cfg);

/**
 * Prints usage information to stdout.
 * Input:  program_name — argv[0] or NULL (falls back to PROGRAM_NAME).
 * Output: none.
 */
void config_print_usage(const char* program_name);

/**
 * Prints version string to stdout.
 * Input:  program_name — argv[0] or NULL.
 * Output: none.
 */
void config_print_version(const char* program_name);

/**
 * Validates that required fields are set and paths are readable.
 * Input:  cfg — fully populated Config.
 * Output: 0 if valid; -1 if a required field is missing or unreadable
 *         (prints an error to stderr).
 */
int config_validate(const Config* cfg);

/**
 * Expands a leading '~' to the value of $HOME.
 * Input:  path — a file-system path string (may be NULL).
 * Output: newly heap-allocated string with '~' replaced; caller must free().
 *         Returns strdup(path) when no expansion is needed.
 *         Returns NULL on allocation failure or when $HOME is unset.
 */
char* path_expand_tilde(const char* path);

/**
 * Checks whether a file exists and is readable by the current process.
 * Input:  path — file-system path (may contain '~').
 * Output: 1 if readable; 0 otherwise.
 */
int file_is_readable(const char* path);

/**
 * Writes a formatted message to stderr when cfg->verbose is non-zero.
 * Input:  cfg — current Config; fmt — printf-style format string; variadic args.
 * Output: none.
 */
void log_verbose(const Config* cfg, const char* fmt, ...) __attribute__((format(printf, 2, 3)));

/**
 * Writes a formatted message to stderr when cfg->debug is non-zero.
 * Input:  cfg — current Config; fmt — printf-style format string; variadic args.
 * Output: none.
 */
void log_debug(const Config* cfg, const char* fmt, ...) __attribute__((format(printf, 2, 3)));

#endif
