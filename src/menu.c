#define _POSIX_C_SOURCE 200809L
#include "../include/config.h"
#include "menu.h"
#include "rofi.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>

static int         g_show_icons = 1;
static const char* g_theme      = NULL;
static __thread char g_last_error[256] = {0};

/**
 * Formats an error message into the thread-local buffer and prints it to stderr.
 * Input:  fmt — printf-style format string; variadic args.
 * Output: none.
 */
static void set_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(g_last_error, sizeof(g_last_error), fmt, args);
    va_end(args);
    fprintf(stderr, "menu error: %s\n", g_last_error);
}

const char* menu_get_last_error(void) {
    return g_last_error[0] ? g_last_error : NULL;
}

void menu_set_defaults(int show_icons_global, const char* theme_global) {
    g_show_icons = show_icons_global;
    g_theme      = theme_global;
}

/**
 * Expands a leading '~' to $HOME in path.
 * Input:  path — a file-system path string (may be NULL).
 * Output: heap-allocated expanded string; caller must free().
 *         Returns NULL on failure or when $HOME is unset.
 */
static char* expand_tilde(const char* path) {
    if (!path || !path[0]) return NULL;
    if (path[0] != '~')    return strdup(path);

    const char* home = getenv("HOME");
    if (!home) { set_error("Cannot expand ~: $HOME not set"); return NULL; }

    size_t home_len = strlen(home);
    size_t path_len = strlen(path);
    size_t total    = home_len + path_len;

    char* result = malloc(total + 1);
    if (!result) { set_error("Memory allocation failed"); return NULL; }

    memcpy(result, home, home_len);
    memcpy(result + home_len, path + 1, path_len);
    result[total] = '\0';
    return result;
}

/**
 * Returns non-zero when key starts with the "::" metadata prefix.
 * Input:  key — JSON object key string.
 * Output: 1 if metadata key; 0 otherwise.
 */
static int is_metadata_key(const char* key) {
    return (key && key[0] == ':' && key[1] == ':');
}

/**
 * Retrieves a string value from a JSON object by key, with a fallback.
 * Input:  obj — JSON object to query; key — field name; fallback — value if absent.
 * Output: pointer to the JSON string value, or fallback if not found.
 */
static const char* get_json_string(struct json_object* obj, const char* key,
                                   const char* fallback) {
    struct json_object* val;
    if (json_object_object_get_ex(obj, key, &val) &&
        json_object_get_type(val) == json_type_string)
        return json_object_get_string(val);
    return fallback;
}

/**
 * Retrieves a boolean value from a JSON object by key.
 * Accepts boolean, integer (0/non-0), or string ("true"/"1").
 * Input:  obj — JSON object; key — field name; fallback — default when absent.
 * Output: 1 (true) or 0 (false), or fallback if the key is absent.
 */
static int get_json_bool(struct json_object* obj, const char* key, int fallback) {
    struct json_object* val;
    if (!json_object_object_get_ex(obj, key, &val)) return fallback;

    json_type t = json_object_get_type(val);
    if (t == json_type_boolean) return json_object_get_boolean(val);
    if (t == json_type_string) {
        const char* s = json_object_get_string(val);
        return (s && (strcmp(s, "true") == 0 || strcmp(s, "1") == 0));
    }
    if (t == json_type_int) return json_object_get_int(val) != 0;
    return fallback;
}

/**
 * Returns the "::env" object from a JSON node, or NULL if absent.
 * Input:  obj — JSON object to query.
 * Output: pointer to the inner JSON object for ::env, or NULL.
 */
static struct json_object* get_json_env(struct json_object* obj) {
    struct json_object* val;
    if (json_object_object_get_ex(obj, META_KEY_ENV, &val) &&
        json_object_get_type(val) == json_type_object)
        return val;
    return NULL;
}

/**
 * Iterates a JSON object and calls setenv() for each key-value pair.
 * Input:  env_obj — JSON object whose string values are env-var assignments.
 * Output: 0 on success; -1 if setenv() fails (errno set by setenv).
 */
static int apply_env_vars(struct json_object* env_obj) {
    if (!env_obj) return 0;

    struct json_object_iter iter;
    json_object_object_foreachC(env_obj, iter) {
        if (json_object_get_type(iter.val) != json_type_string) {
            fprintf(stderr, "menu: env var '%s' must be a string, skipping\n", iter.key);
            continue;
        }
        if (setenv(iter.key, json_object_get_string(iter.val), 1) != 0) {
            perror("menu: setenv failed");
            return -1;
        }
    }
    return 0;
}

/**
 * Changes the process working directory to cwd, expanding '~' first.
 * Input:  cwd — target directory path (may contain '~'), or NULL/empty to skip.
 * Output: 0 on success; -1 on chdir failure (error stored via set_error).
 */
static int change_cwd(const char* cwd) {
    if (!cwd || !cwd[0]) return 0;

    char* expanded = expand_tilde(cwd);
    if (!expanded) return -1;

    if (chdir(expanded) != 0) {
        set_error("Failed to chdir to '%s': %s", expanded, strerror(errno));
        free(expanded);
        return -1;
    }

    free(expanded);
    return 0;
}

/**
 * Shows a two-item rofi prompt asking the user to confirm or cancel execution.
 * Input:  command — the command string shown in the prompt text.
 * Output: 1 if the user chose to execute; 0 if cancelled or rofi failed.
 */
static int confirm_execution(const char* command) {
    char prompt[MAX_PROMPT_LEN];
    snprintf(prompt, sizeof(prompt), "Run: %.50s%s",
             command, strlen(command) > 50 ? "..." : "");

    const char* options[] = {DEFAULT_CONFIRM_LABEL, DEFAULT_CANCEL_LABEL};
    char* choice = rofi_select_menu(options, 2, prompt, 0, NULL);
    if (!choice) return 0;

    int confirmed = STREQ(choice, DEFAULT_CONFIRM_LABEL);
    free(choice);
    return confirmed;
}

/**
 * Extracts the command string from a JSON node.
 * A string node returns the string directly. An object node returns the
 * value of its "::cmd" field. Any other type produces an error.
 * Input:  node — a JSON node representing a command or command object.
 * Output: pointer to the command string owned by the JSON tree (do not free),
 *         or NULL on error (error stored via set_error).
 */
static const char* extract_command(struct json_object* node) {
    if (!node) return NULL;

    json_type t = json_object_get_type(node);

    if (t == json_type_string)
        return json_object_get_string(node);

    if (t == json_type_object) {
        struct json_object* cmd_obj;
        if (json_object_object_get_ex(node, META_KEY_CMD, &cmd_obj) &&
            json_object_get_type(cmd_obj) == json_type_string)
            return json_object_get_string(cmd_obj);
        set_error("Command object missing '%s' field", META_KEY_CMD);
        return NULL;
    }

    set_error("Unexpected node type for command: %d", t);
    return NULL;
}

/**
 * Applies metadata from node (::confirm, ::env, ::cwd) and then runs the command.
 * Input:  node  — JSON node containing the command and optional metadata;
 *         label — human-readable name used in error messages (may be NULL).
 * Output: none. Side-effects: may set env vars, change cwd, run a shell command.
 */
static void execute_command_node(struct json_object* node, const char* label) {
    if (!node) return;

    const char* command = extract_command(node);
    if (!command || !command[0]) {
        set_error("No command to execute for '%s'", label ? label : "(unnamed)");
        return;
    }

    if (get_json_bool(node, META_KEY_CONFIRM, 0))
        if (!confirm_execution(command)) return;

    char original_cwd[PATH_MAX];
    int has_cwd = (getcwd(original_cwd, sizeof(original_cwd)) != NULL);

    struct json_object* env_obj = get_json_env(node);
    if (env_obj && apply_env_vars(env_obj) != 0) return;

    const char* cwd = get_json_string(node, META_KEY_CWD, NULL);
    if (cwd && change_cwd(cwd) != 0) return;

    int status = shell_exec(command);
    if (status != 0)
        fprintf(stderr, "menu: command '%s' exited with status %d\n", command, status);

    if (has_cwd && cwd)
        (void)chdir(original_cwd);
}

/**
 * Counts the visible (non-metadata, non-hidden) options in a JSON menu object.
 * Input:  menu — JSON object representing a menu level.
 * Output: number of displayable options.
 */
static size_t count_menu_options(struct json_object* menu) {
    if (!menu || json_object_get_type(menu) != json_type_object) return 0;

    size_t count = 0;
    struct json_object_iter iter;
    json_object_object_foreachC(menu, iter) {
        if (is_metadata_key(iter.key)) continue;
        if (get_json_bool(iter.val, META_KEY_HIDDEN, 0)) continue;
        count++;
    }
    return count;
}

/**
 * Builds an array of option-key strings from a JSON menu object, skipping
 * metadata keys and hidden entries. The returned array holds pointers into
 * the JSON tree and must not outlive it. The array itself must be freed.
 * Input:  menu      — JSON object representing a menu level;
 *         out_count — output: number of elements written to the array.
 * Output: heap-allocated array of const char* pointers, or NULL on error.
 */
static const char** collect_menu_options(struct json_object* menu, size_t* out_count) {
    if (!menu || !out_count) return NULL;

    size_t count = count_menu_options(menu);
    if (count == 0) { *out_count = 0; return NULL; }

    const char** options = calloc(count, sizeof(char*));
    if (!options) {
        set_error("Memory allocation failed for menu options");
        *out_count = 0;
        return NULL;
    }

    size_t idx = 0;
    struct json_object_iter iter;
    json_object_object_foreachC(menu, iter) {
        if (is_metadata_key(iter.key)) continue;
        if (get_json_bool(iter.val, META_KEY_HIDDEN, 0)) continue;
        options[idx++] = iter.key;
    }

    *out_count = count;
    return options;
}

void execute_menu(struct json_object* menu) {
    if (!menu) { set_error("NULL menu object"); return; }

    json_type type = json_object_get_type(menu);

    if (type == json_type_string ||
        (type == json_type_object &&
         json_object_object_get_ex(menu, META_KEY_CMD, NULL))) {
        execute_command_node(menu, NULL);
        return;
    }

    if (type == json_type_object) {
        size_t option_count = 0;
        const char** options = collect_menu_options(menu, &option_count);

        if (option_count == 0) {
            set_error("Menu has no valid options");
            free(options);
            return;
        }

        const char* prompt = get_json_string(menu, META_KEY_PROMPT, NULL);
        const char* theme  = get_json_string(menu, META_KEY_THEME,  NULL);
        int icons = get_json_bool(menu, META_KEY_ICONS, -1);
        if (icons < 0) icons = g_show_icons;

        char* expanded_theme = NULL;
        if (theme) { expanded_theme = expand_tilde(theme); theme = expanded_theme; }
        if (!theme) theme = g_theme;

        char* selected = rofi_select_menu(options, option_count, prompt, icons, theme);
        free(options);
        free(expanded_theme);

        if (!selected) return;

        struct json_object* next;
        if (!json_object_object_get_ex(menu, selected, &next)) {
            set_error("Selected option '%s' not found in menu", selected);
            free(selected);
            return;
        }

        execute_menu(next);
        free(selected);
        return;
    }

    set_error("Unexpected JSON type in menu: %d (%s)", type, json_type_to_name(type));
}
