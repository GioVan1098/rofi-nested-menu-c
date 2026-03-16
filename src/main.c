#define _POSIX_C_SOURCE 200809L
#include "../include/config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <json-c/json.h>

#include "args.h"
#include "menu.h"
#include "rofi.h"

int main(int argc, char* argv[]) {
    Config cfg;
    int result = config_parse(argc, argv, &cfg);

    if (result != 0)
        return (result == 1) ? 0 : 2;

    log_verbose(&cfg, "Config: json=%s, theme=%s, icons=%d, verbose=%d, debug=%d",
                cfg.json_path, cfg.theme ? cfg.theme : "(none)",
                cfg.show_icons, cfg.verbose, cfg.debug);

    menu_set_defaults(cfg.show_icons, cfg.theme);

    FILE* f = fopen(cfg.json_path, "r");
    if (!f) {
        fprintf(stderr, "%s: error: failed to open '%s': %s\n",
                PROGRAM_NAME, cfg.json_path, strerror(errno));
        config_free(&cfg);
        return 3;
    }

    log_debug(&cfg, "Opened config file: %s", cfg.json_path);

    if (fseek(f, 0, SEEK_END) != 0) {
        perror("fseek");
        fclose(f);
        config_free(&cfg);
        return 3;
    }

    long fsize = ftell(f);
    if (fsize < 0 || fsize > CONFIG_FILE_MAX_SIZE) {
        fprintf(stderr, "%s: error: config file too large or unreadable\n", PROGRAM_NAME);
        fclose(f);
        config_free(&cfg);
        return 3;
    }

    rewind(f);

    char* json_str = malloc((size_t)fsize + 1);
    if (!json_str) {
        fprintf(stderr, "%s: error: memory allocation failed\n", PROGRAM_NAME);
        fclose(f);
        config_free(&cfg);
        return 3;
    }

    if (fread(json_str, 1, (size_t)fsize, f) != (size_t)fsize) {
        fprintf(stderr, "%s: error: failed to read config file\n", PROGRAM_NAME);
        free(json_str);
        fclose(f);
        config_free(&cfg);
        return 3;
    }
    json_str[fsize] = '\0';
    fclose(f);

    log_debug(&cfg, "Read %ld bytes from config", fsize);

    struct json_object* parsed = json_tokener_parse(json_str);
    free(json_str);

    if (!parsed) {
        fprintf(stderr, "%s: error: failed to parse JSON\n", PROGRAM_NAME);
        config_free(&cfg);
        return 3;
    }

    log_debug(&cfg, "Parsed JSON successfully");
    log_verbose(&cfg, "Starting menu execution");

    execute_menu(parsed);

    const char* menu_err = menu_get_last_error();
    if (menu_err)
        fprintf(stderr, "%s: menu error: %s\n", PROGRAM_NAME, menu_err);

    json_object_put(parsed);
    log_verbose(&cfg, "Exiting");
    config_free(&cfg);

    return 0;
}
