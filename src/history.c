#include "history.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static void get_history_path(char *out, size_t max_len) {
    const char *home = getenv("HOME");
    if (home) {
        char dir[1024];
        snprintf(dir, sizeof(dir), "%s/.studentos", home);
        mkdir(dir, 0755);
        snprintf(out, max_len, "%s/.studentos/history.db", home);
    } else {
        snprintf(out, max_len, "data/history.db");
    }
}

void history_add(const char *line) {
    if (!line || !line[0]) return;
    char path[1024];
    get_history_path(path, sizeof(path));

    FILE *f = fopen(path, "a");
    if (f) {
        fprintf(f, "%s\n", line);
        fclose(f);
    }
}

void history_list(void) {
    char path[1024];
    get_history_path(path, sizeof(path));

    FILE *f = fopen(path, "r");
    if (!f) {
        printf("No command history yet.\n");
        return;
    }

    char line[1024];
    int n = 1;
    printf("\nCommand History\n");
    printf("────────────────────────────────────────\n");
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        printf("%3d  %s\n", n++, line);
    }
    if (n == 1) {
        printf("No command history yet.\n");
    }
    fclose(f);
}

void history_clear(void) {
    char path[1024];
    get_history_path(path, sizeof(path));

    FILE *f = fopen(path, "w");
    if (f) fclose(f);
    printf("History cleared.\n");
}
