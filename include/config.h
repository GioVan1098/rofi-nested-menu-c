#ifndef CONFIG_H
#define CONFIG_H

#include <limits.h>
#include <stddef.h>

#define PROGRAM_NAME        "rofi-menu"
#define PROGRAM_VERSION     "1.0.0"
#define PROGRAM_LICENSE     "MIT"
#define PROGRAM_DESCRIPTION "Nested menu launcher using rofi and JSON configuration"

#define FEATURE_ICONS           1
#define FEATURE_THEMES          1
#define FEATURE_ENV_VARS        1
#define FEATURE_CWD_CHANGE      1
#define FEATURE_CONFIRM_PROMPT  1
#define FEATURE_HIDDEN_OPTIONS  1
#define FEATURE_PATH_EXPANSION  1
#define FEATURE_VERBOSE_LOGGING 1
#define FEATURE_DEBUG_LOGGING   1

#if defined(__linux__)
    #define PLATFORM_LINUX 1
    #define PLATFORM_UNIX  1
#elif defined(__APPLE__) && defined(__MACH__)
    #define PLATFORM_MACOS 1
    #define PLATFORM_UNIX  1
#elif defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
    #define PLATFORM_BSD   1
    #define PLATFORM_UNIX  1
#else
    #warning "Unknown platform - some features may not work"
    #define PLATFORM_UNKNOWN 1
#endif

#if defined(__clang__)
    #define COMPILER_CLANG 1
    #define LIKELY(x)   __builtin_expect(!!(x), 1)
    #define UNLIKELY(x) __builtin_expect(!!(x), 0)
#elif defined(__GNUC__)
    #define COMPILER_GCC 1
    #define LIKELY(x)   __builtin_expect(!!(x), 1)
    #define UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
    #define COMPILER_UNKNOWN 1
    #define LIKELY(x)   (x)
    #define UNLIKELY(x) (x)
#endif

#define MAX_PATH_LEN            PATH_MAX
#define MAX_CMD_LEN             4096
#define MAX_PROMPT_LEN          256
#define MAX_THEME_PATH_LEN      1024
#define MAX_ENV_VAR_NAME_LEN    256
#define MAX_ENV_VAR_VALUE_LEN   4096
#define MAX_ERROR_MSG_LEN       512
#define MAX_LOG_MSG_LEN         2048

#define MAX_MENU_OPTIONS        256
#define MAX_MENU_DEPTH          16
#define MAX_OPTION_LABEL_LEN    512
#define MAX_COMMAND_ARGS        64

#define ROFI_EXECUTABLE         "rofi"
#define ROFI_DEFAULT_FLAGS      "-dmenu -i"
#define ROFI_TIMEOUT_SECONDS    0

#define DEFAULT_CONFIG_FILENAME "config.json"
#define CONFIG_FILE_MAX_SIZE    (10 * 1024 * 1024)
#define TEMP_FILE_PREFIX        "/tmp/rofi-menu-"
#define DEV_NULL_PATH           "/dev/null"

#define INITIAL_ALLOC_SIZE      256
#define REALLOC_GROWTH_FACTOR   2
#define MAX_ALLOC_SIZE          (100 * 1024 * 1024)

#define DEFAULT_SHOW_ICONS      1
#define DEFAULT_CASE_SENSITIVE  0
#define DEFAULT_NO_LAZY_GRAB    0
#define DEFAULT_MENU_PROMPT     "Select"
#define DEFAULT_CONFIRM_LABEL   "✓ Execute"
#define DEFAULT_CANCEL_LABEL    "✗ Cancel"
#define DEFAULT_HOME_ENV        "HOME"
#define DEFAULT_SHELL           "/bin/sh"
#define DEFAULT_SHELL_ARGS      "-c"

#define META_KEY_PREFIX  "::"
#define META_KEY_PROMPT  "::prompt"
#define META_KEY_THEME   "::theme"
#define META_KEY_ICONS   "::icons"
#define META_KEY_ENV     "::env"
#define META_KEY_CWD     "::cwd"
#define META_KEY_CONFIRM "::confirm"
#define META_KEY_CMD     "::cmd"
#define META_KEY_HIDDEN  "::hidden"
#define META_KEY_PREVIEW "::preview"
#define META_KEY_TIMEOUT "::timeout"

#define ARRAY_SIZE(arr)  (sizeof(arr) / sizeof((arr)[0]))

#define STREQ(a, b)             (strcmp((a), (b)) == 0)
#define STRNEQ(a, b, n)         (strncmp((a), (b), (n)) == 0)
#define STR_EMPTY(s)            (!(s) || !(s)[0])
#define STR_PREFIX(s, prefix)   (STRNEQ((s), (prefix), strlen(prefix)))

#define SAFE_FREE(ptr)            do { if (ptr) { free(ptr); (ptr) = NULL; } } while(0)
#define SAFE_CLOSE(fd)            do { if ((fd) >= 0) { close(fd); (fd) = -1; } } while(0)
#define ZALLOC(type, count)       ((type*)calloc((count), sizeof(type)))
#define ALLOC(type, count)        ((type*)malloc((count) * sizeof(type)))
#define REALLOC(ptr, type, count) ((type*)realloc((ptr), (count) * sizeof(type)))

#define RETURN_IF_NULL(ptr, ret)   do { if (UNLIKELY(!(ptr))) return (ret); } while(0)
#define RETURN_IF_ERROR(cond, ret) do { if (UNLIKELY(cond))  return (ret); } while(0)
#define GOTO_IF_NULL(ptr, label)   do { if (UNLIKELY(!(ptr))) goto label; } while(0)
#define GOTO_IF_ERROR(cond, label) do { if (UNLIKELY(cond))  goto label; } while(0)

#if MAX_MENU_OPTIONS > 1024
    #error "MAX_MENU_OPTIONS too large - may cause stack overflow"
#endif
#if MAX_MENU_DEPTH > 32
    #error "MAX_MENU_DEPTH too large - may cause stack overflow"
#endif
#if MAX_ALLOC_SIZE > (1024 * 1024 * 1024)
    #error "MAX_ALLOC_SIZE too large - risk of memory exhaustion"
#endif

#endif
