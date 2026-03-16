#ifndef MENU_H
#define MENU_H

#include <json-c/json.h>

/**
 * Recursively presents a rofi menu for the given JSON node and executes
 * the selected command. A string node runs the string as a shell command.
 * An object node with "::cmd" runs that command with optional metadata
 * (::confirm, ::env, ::cwd). Any other object node is shown as a sub-menu.
 * Input:  menu — a json_object representing the current menu level.
 * Output: none. Side-effects: may launch rofi, execute shell commands,
 *         change cwd, or set environment variables.
 */
void execute_menu(struct json_object* menu);

/**
 * Sets the global icon and theme defaults used when a menu node does not
 * override them with ::icons or ::theme metadata.
 * Input:  show_icons_global — non-zero to enable icons by default;
 *         theme_global      — path to the default .rasi theme, or NULL.
 * Output: none.
 */
void menu_set_defaults(int show_icons_global, const char* theme_global);

/**
 * Returns the last error message produced by execute_menu, or NULL if no
 * error has occurred. The string is thread-local and valid until the next
 * call to execute_menu on the same thread.
 * Input:  none.
 * Output: pointer to a static error string, or NULL.
 */
const char* menu_get_last_error(void);

#endif
