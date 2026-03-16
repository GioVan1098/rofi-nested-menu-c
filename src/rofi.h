#ifndef ROFI_H
#define ROFI_H

#include <stddef.h>

/**
 * Launches rofi in -dmenu mode, feeds it the given option list via stdin,
 * and returns the string the user selected.
 * Input:  options    — array of option strings to display;
 *         count      — number of elements in options;
 *         prompt     — text for rofi's -p flag, or NULL;
 *         show_icons — non-zero to pass -show-icons to rofi;
 *         theme      — path to a .rasi theme file, or NULL.
 * Output: heap-allocated string with the selected option; caller must free().
 *         Returns NULL if the user cancelled (ESC), rofi failed to launch,
 *         or a memory allocation error occurred.
 */
char* rofi_select_menu(const char* const* options, size_t count,
                       const char* prompt, int show_icons, const char* theme);

/**
 * Executes a shell command synchronously via /bin/sh -c.
 * Input:  command — the command string to execute.
 * Output: exit status of the child process, or -1 on fork/exec failure.
 */
int shell_exec(const char* command);

/**
 * Executes a program directly without a shell (no injection risk).
 * Input:  argv — NULL-terminated argument vector; argv[0] is the program name.
 * Output: exit status of the child process, or -1 on fork/exec failure.
 */
int shell_exec_argv(char* const argv[]);

#endif
