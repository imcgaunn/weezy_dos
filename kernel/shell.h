#ifndef WEEZYDOS_SHELL_H
#define WEEZYDOS_SHELL_H

#include "types.h"

#define SHELL_LINE_MAX 256

typedef struct {
    char (*getchar)(void);
    void (*putchar)(char c);
    void (*puts)(const char *str);
} shell_io_t;

void shell_init(shell_io_t *io);
int shell_readline(char *buf, int max_len);
void shell_run(void);

#endif
