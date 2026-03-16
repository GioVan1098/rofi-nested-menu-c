#define _POSIX_C_SOURCE 200809L
#include "../include/config.h"
#include "args.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>

static int  parse_options(int argc, char* argv[], Config* cfg,
                          int* help_flag, int* version_flag);
static void free_config_fields(Config* cfg);

int config_parse(int argc, char* argv[], Config* cfg) {
    if (!cfg) {
        fprintf(stderr, "%s: internal error: NULL config pointer\n", PROGRAM_NAME);
        return -1;
    }

    memset(cfg, 0, sizeof(Config));
    cfg->show_icons = DEFAULT_SHOW_ICONS;

    int help_flag    = 0;
    int version_flag = 0;

    if (parse_options(argc, argv, cfg, &help_flag, &version_flag) != 0) {
        config_free(cfg);
        return -1;
    }

    if (help_flag) {
        config_print_usage(argv[0]);
        config_free(cfg);
        return 1;
    }

    if (version_flag) {
        config_print_version(argv[0]);
        config_free(cfg);
        return 1;
    }

    if (config_validate(cfg) != 0) {
        config_free(cfg);
        return -1;
    }

    return 0;
}

void config_free(Config* cfg) {
    if (!cfg) return;
    free_config_fields(cfg);
}

void config_print_usage(const char* program_name) {
    const char* name = program_name ? program_name : PROGRAM_NAME;
    printf("Usage: %s [OPTIONS] <config.json>\n\n", name);
    printf("A nested menu launcher using rofi and JSON configuration.\n\n");
    printf("Required Arguments:\n");
    printf("  <config.json>          Path to JSON configuration file\n\n");
    printf("Optional Arguments:\n");
    printf("  -t, --theme <path>     Path to rofi theme file (.rasi)\n");
    printf("  -i, --icons            Show icons in rofi menus (default: enabled)\n");
    printf("  -I, --no-icons         Hide icons in rofi menus\n");
    printf("  -v, --verbose          Enable verbose output\n");
    printf("  -d, --debug            Enable debug output (implies verbose)\n");
    printf("  -h, --help             Show this help message and exit\n");
    printf("  -V, --version          Show version information and exit\n\n");
    printf("Examples:\n");
    printf("  %s config.json\n", name);
    printf("  %s -t ~/.config/rofi/my-theme.rasi config.json\n", name);
    printf("  %s --no-icons --verbose config.json\n\n", name);
    printf("Exit Codes:\n");
    printf("  0  Success\n");
    printf("  1  Help/version shown (not an error)\n");
    printf("  2  Invalid arguments or configuration\n");
    printf("  3  Runtime error (file not found, parse error, etc.)\n");
}

void config_print_version(const char* program_name) {
    const char* name = program_name ? program_name : PROGRAM_NAME;
    printf("%s version %s\n", name, PROGRAM_VERSION);
    printf("License: %s\n", PROGRAM_LICENSE);
}

int config_validate(const Config* cfg) {
    if (!cfg) {
        fprintf(stderr, "%s: internal error: NULL config for validation\n", PROGRAM_NAME);
        return -1;
    }

    if (!cfg->json_path || !cfg->json_path[0]) {
        fprintf(stderr, "%s: error: JSON config file path is required\n", PROGRAM_NAME);
        fprintf(stderr, "Try '%s --help' for more information.\n", PROGRAM_NAME);
        return -1;
    }

    if (!file_is_readable(cfg->json_path)) {
        fprintf(stderr, "%s: error: cannot read config file '%s': %s\n",
                PROGRAM_NAME, cfg->json_path, strerror(errno));
        return -1;
    }

    if (cfg->theme && cfg->theme[0] && !file_is_readable(cfg->theme)) {
        fprintf(stderr, "%s: warning: theme file '%s' not found, ignoring\n",
                PROGRAM_NAME, cfg->theme);
        free(cfg->theme);
        ((Config*)cfg)->theme = NULL;
    }

    return 0;
}

char* path_expand_tilde(const char* path) {
    if (!path || !path[0]) return NULL;
    if (path[0] != '~')    return strdup(path);

    const char* home = getenv("HOME");
    if (!home || !home[0]) {
        fprintf(stderr, "%s: warning: cannot expand ~: $HOME not set\n", PROGRAM_NAME);
        return strdup(path);
    }

    size_t home_len   = strlen(home);
    size_t path_len   = strlen(path);
    size_t result_len = home_len + path_len;

    char* result = malloc(result_len + 1);
    if (!result) {
        fprintf(stderr, "%s: error: memory allocation failed\n", PROGRAM_NAME);
        return NULL;
    }

    memcpy(result, home, home_len);
    memcpy(result + home_len, path + 1, path_len);
    result[result_len] = '\0';
    return result;
}

int file_is_readable(const char* path) {
    if (!path || !path[0]) return 0;
    char* expanded = path_expand_tilde(path);
    if (!expanded) return 0;
    int ok = (access(expanded, R_OK) == 0);
    free(expanded);
    return ok;
}

/**
 * Parses getopt_long options into cfg and sets help/version flags.
 * Input:  argc, argv — from main(); cfg — Config to populate;
 *         help_flag, version_flag — output flags set to 1 when requested.
 * Output: 0 on success; -1 on unknown option or missing argument.
 */
static int parse_options(int argc, char* argv[], Config* cfg,
                         int* help_flag, int* version_flag) {
    static struct option long_options[] = {
        {"theme",    required_argument, 0, 't'},
        {"icons",    no_argument,       0, 'i'},
        {"no-icons", no_argument,       0, 'I'},
        {"verbose",  no_argument,       0, 'v'},
        {"debug",    no_argument,       0, 'd'},
        {"help",     no_argument,       0, 'h'},
        {"version",  no_argument,       0, 'V'},
        {0, 0, 0, 0}
    };

    static const char* short_options = "t:iIvdhV";
    int opt;
    int option_index = 0;
    optind = 0;

    while ((opt = getopt_long(argc, argv, short_options,
                              long_options, &option_index)) != -1) {
        switch (opt) {
            case 't':
                if (cfg->theme) free(cfg->theme);
                cfg->theme = path_expand_tilde(optarg);
                if (!cfg->theme) {
                    fprintf(stderr, "%s: error: failed to process theme path '%s'\n",
                            PROGRAM_NAME, optarg);
                    return -1;
                }
                break;
            case 'i': cfg->show_icons = 1; break;
            case 'I': cfg->show_icons = 0; break;
            case 'v': cfg->verbose = 1; break;
            case 'd': cfg->debug = 1; cfg->verbose = 1; break;
            case 'h': *help_flag    = 1; break;
            case 'V': *version_flag = 1; break;
            case '?':
                fprintf(stderr, "Try '%s --help' for more information.\n", PROGRAM_NAME);
                return -1;
            default:
                fprintf(stderr, "%s: error: unhandled option: %c\n", PROGRAM_NAME, opt);
                return -1;
        }
    }

    if (optind >= argc) {
        fprintf(stderr, "%s: error: missing required argument: <config.json>\n", PROGRAM_NAME);
        fprintf(stderr, "Try '%s --help' for more information.\n", PROGRAM_NAME);
        return -1;
    }

    if (optind + 1 < argc) {
        fprintf(stderr, "%s: error: too many arguments\n", PROGRAM_NAME);
        fprintf(stderr, "Try '%s --help' for more information.\n", PROGRAM_NAME);
        return -1;
    }

    cfg->json_path = path_expand_tilde(argv[optind]);
    if (!cfg->json_path) {
        fprintf(stderr, "%s: error: failed to process config path '%s'\n",
                PROGRAM_NAME, argv[optind]);
        return -1;
    }

    return 0;
}

/**
 * Frees heap fields inside cfg and sets them to NULL.
 * Input:  cfg — pointer to Config (may be NULL).
 * Output: none.
 */
static void free_config_fields(Config* cfg) {
    if (!cfg) return;
    SAFE_FREE(cfg->json_path);
    SAFE_FREE(cfg->theme);
}

void log_verbose(const Config* cfg, const char* fmt, ...) {
    if (!cfg || !cfg->verbose || !fmt) return;
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "[INFO]  ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
}

void log_debug(const Config* cfg, const char* fmt, ...) {
    if (!cfg || !cfg->debug || !fmt) return;
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "[DEBUG] ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
}
