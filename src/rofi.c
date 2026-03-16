#define _GNU_SOURCE
#include "../include/config.h"
#include "rofi.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>

#define MAX_ROFI_ARGS  32
#define READ_BUF_SIZE  1024

/**
 * Closes *fd and sets it to -1.
 * Input:  fd — pointer to a file descriptor.
 * Output: none.
 */
static void safe_close(int* fd) {
    if (fd && *fd >= 0) { close(*fd); *fd = -1; }
}

/**
 * Frees *ptr and sets it to NULL.
 * Input:  ptr — pointer to a heap-allocated string pointer.
 * Output: none.
 */
static void safe_free(char** ptr) {
    if (ptr && *ptr) { free(*ptr); *ptr = NULL; }
}

/**
 * Populates args[] with the rofi -dmenu command-line arguments.
 * Input:  args       — output array of at least MAX_ROFI_ARGS char* elements;
 *         prompt     — text for -p, or NULL to omit;
 *         show_icons — non-zero to append -show-icons;
 *         theme      — path for -theme, or NULL to omit.
 * Output: number of arguments written (excluding the trailing NULL),
 *         or -1 if the argument count would overflow MAX_ROFI_ARGS.
 */
static int build_rofi_args(char* args[MAX_ROFI_ARGS],
                           const char* prompt,
                           int show_icons,
                           const char* theme) {
    int i = 0;
    args[i++] = "rofi";
    args[i++] = "-dmenu";
    args[i++] = "-i";

    if (show_icons)          args[i++] = "-show-icons";
    if (prompt && prompt[0]) { args[i++] = "-p";      args[i++] = (char*)prompt; }
    if (theme  && theme[0])  { args[i++] = "-theme";  args[i++] = (char*)theme;  }

    if (i >= MAX_ROFI_ARGS - 1) { fprintf(stderr, "rofi: too many arguments\n"); return -1; }
    args[i] = NULL;
    return i;
}

/**
 * Writes each option string followed by a newline to fd.
 * Input:  fd      — writable file descriptor (rofi's stdin pipe);
 *         options — array of option strings;
 *         count   — number of elements in options.
 * Output: 0 on success; -1 on write error (errno set).
 */
static int write_options(int fd, const char* const* options, size_t count) {
    for (size_t i = 0; i < count; i++) {
        const char* opt = options[i];
        size_t len = strlen(opt);

        ssize_t written = 0;
        while (written < (ssize_t)len) {
            ssize_t n = write(fd, opt + written, len - (size_t)written);
            if (n < 0) {
                if (errno == EINTR) continue;
                perror("rofi: write option failed");
                return -1;
            }
            if (n == 0) break;
            written += n;
        }

        ssize_t n;
        do { n = write(fd, "\n", 1); } while (n < 0 && errno == EINTR);
        if (n <= 0) { perror("rofi: write newline failed"); return -1; }
    }
    return 0;
}

/**
 * Reads one newline-terminated line from fd into a heap-allocated string.
 * Input:  fd — readable file descriptor.
 * Output: heap-allocated string without the trailing newline; caller must free().
 *         Returns NULL on EOF, empty input, or read error.
 */
static char* read_line(int fd) {
    char buf[READ_BUF_SIZE];
    size_t pos = 0;

    while (pos < sizeof(buf) - 1) {
        ssize_t n = read(fd, buf + pos, 1);
        if (n < 0) { if (errno == EINTR) continue; perror("rofi: read failed"); return NULL; }
        if (n == 0) break;
        if (buf[pos] == '\n') { buf[pos] = '\0'; break; }
        pos++;
    }

    if (pos == 0) return NULL;
    char* result = strdup(buf);
    if (!result) perror("rofi: strdup failed");
    return result;
}

char* rofi_select_menu(const char* const* options, size_t count,
                       const char* prompt, int show_icons, const char* theme) {
    if (!options || count == 0) { fprintf(stderr, "rofi: no options\n"); return NULL; }

    int pipe_in[2]  = {-1, -1};
    int pipe_out[2] = {-1, -1};

    if (pipe(pipe_in) == -1 || pipe(pipe_out) == -1) {
        perror("rofi: pipe failed");
        safe_close(&pipe_in[0]);  safe_close(&pipe_in[1]);
        safe_close(&pipe_out[0]); safe_close(&pipe_out[1]);
        return NULL;
    }

    char* args[MAX_ROFI_ARGS] = {NULL};
    if (build_rofi_args(args, prompt, show_icons, theme) < 0) {
        safe_close(&pipe_in[0]);  safe_close(&pipe_in[1]);
        safe_close(&pipe_out[0]); safe_close(&pipe_out[1]);
        return NULL;
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("rofi: fork failed");
        safe_close(&pipe_in[0]);  safe_close(&pipe_in[1]);
        safe_close(&pipe_out[0]); safe_close(&pipe_out[1]);
        return NULL;
    }

    if (pid == 0) {
        if (dup2(pipe_in[0],  STDIN_FILENO)  == -1) { perror("dup2 stdin");  _exit(126); }
        if (dup2(pipe_out[1], STDOUT_FILENO) == -1) { perror("dup2 stdout"); _exit(126); }

        int devnull = open(DEV_NULL_PATH, O_WRONLY);
        if (devnull >= 0) { dup2(devnull, STDERR_FILENO); close(devnull); }

        safe_close(&pipe_in[0]);  safe_close(&pipe_in[1]);
        safe_close(&pipe_out[0]); safe_close(&pipe_out[1]);

        close_range(3, ~0U, 0);

        execvp("rofi", args);
        perror("rofi: execvp failed");
        _exit(127);
    }

    safe_close(&pipe_in[0]);
    safe_close(&pipe_out[1]);

    if (write_options(pipe_in[1], options, count) < 0) {
        safe_close(&pipe_in[1]);
        safe_close(&pipe_out[0]);
        waitpid(pid, NULL, 0);
        return NULL;
    }

    safe_close(&pipe_in[1]);

    char* result = read_line(pipe_out[0]);
    safe_close(&pipe_out[0]);

    int status;
    if (waitpid(pid, &status, 0) == -1) {
        perror("rofi: waitpid failed");
        safe_free(&result);
        return NULL;
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        safe_free(&result);
        return NULL;
    }

    if (result && result[0] == '\0') { free(result); return NULL; }
    return result;
}

int shell_exec(const char* command) {
    if (!command || !command[0]) { fprintf(stderr, "shell_exec: empty command\n"); return -1; }

    pid_t pid = fork();
    if (pid == -1) { perror("shell_exec: fork failed"); return -1; }

    if (pid == 0) {
        execl(DEFAULT_SHELL, "sh", DEFAULT_SHELL_ARGS, command, (char*)NULL);
        perror("shell_exec: execl failed");
        _exit(127);
    }

    int status;
    if (waitpid(pid, &status, 0) == -1) { perror("shell_exec: waitpid failed"); return -1; }
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

int shell_exec_argv(char* const argv[]) {
    if (!argv || !argv[0]) { fprintf(stderr, "shell_exec_argv: no program\n"); return -1; }

    pid_t pid = fork();
    if (pid == -1) { perror("shell_exec_argv: fork failed"); return -1; }

    if (pid == 0) {
        execvp(argv[0], argv);
        perror("shell_exec_argv: execvp failed");
        _exit(127);
    }

    int status;
    if (waitpid(pid, &status, 0) == -1) { perror("shell_exec_argv: waitpid failed"); return -1; }
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}
