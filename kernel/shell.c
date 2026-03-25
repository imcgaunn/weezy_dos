#include "shell.h"
#include "string.h"

static shell_io_t *io = NULL;

void shell_init(shell_io_t *shell_io) {
    io = shell_io;
}

int shell_readline(char *buf, int max_len) {
    int pos = 0;
    while (1) {
        char c = io->getchar();
        if (c == '\n') {
            io->putchar('\n');
            buf[pos] = '\0';
            return pos;
        } else if (c == '\b') {
            if (pos > 0) {
                pos--;
                io->putchar('\b');
                io->putchar(' ');
                io->putchar('\b');
            }
        } else {
            if (pos < max_len - 1) {
                buf[pos++] = c;
                io->putchar(c);
            }
        }
    }
}

void shell_run(void) {
    char line[SHELL_LINE_MAX];

    io->puts("weezyDOS v0.1\n");
    io->puts("640K conventional memory\n\n");

    while (1) {
        io->puts("A:\\> ");
        shell_readline(line, SHELL_LINE_MAX);
        if (wdos_strlen(line) > 0) {
            io->puts(line);
            io->putchar('\n');
        }
    }
}
