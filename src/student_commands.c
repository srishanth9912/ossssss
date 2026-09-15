#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/utsname.h>
#include <pthread.h>
#include <errno.h>

#include "student_commands.h"

/* Run a program without invoking a second shell. This keeps arguments literal. */
static int run_program(char *const argv[]) {
    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return 1; }
    if (pid == 0) {
        execvp(argv[0], argv);
        perror(argv[0]);
        _exit(127);
    }
    int status;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) { perror("waitpid"); return 1; }
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
}

/*
 * Helper: Resolve persistent data path in ~/.studentos/
 */
static void get_data_path(const char *filename, char *out, size_t max_len) {
    const char *home = getenv("HOME");
    if (home != NULL) {
        char dir[1024];
        snprintf(dir, sizeof(dir), "%s/.studentos", home);
        mkdir(dir, 0755);
        snprintf(out, max_len, "%s/.studentos/%s", home, filename);
    } else {
        snprintf(out, max_len, "data/%s", filename);
    }
}

/* =========================================================================
 * NOTES COMMAND
 * ========================================================================= */
static void handle_notes(char **args, int argc) {
    if (argc < 2) {
        printf("Usage: notes add [text] | list | clear\n");
        return;
    }

    char db[1024];
    get_data_path("notes.db", db, sizeof(db));

    if (strcmp(args[1], "add") == 0) {
        char note_text[1024] = "";

        if (argc < 3) {
            /* Interactive step-by-step prompt */
            printf("Note : ");
            fflush(stdout);
            if (!fgets(note_text, sizeof(note_text), stdin)) return;
            note_text[strcspn(note_text, "\r\n")] = '\0';
            if (strlen(note_text) == 0) {
                printf("Note cannot be empty.\n");
                return;
            }
        } else {
            /* Command-line arguments */
            for (int i = 2; i < argc; i++) {
                if (i > 2) strncat(note_text, " ", sizeof(note_text) - strlen(note_text) - 1);
                strncat(note_text, args[i], sizeof(note_text) - strlen(note_text) - 1);
            }
        }

        FILE *f = fopen(db, "a");
        if (!f) { perror("notes"); return; }
        fprintf(f, "%s\n", note_text);
        fclose(f);
        printf("Note saved.\n");
    } else if (strcmp(args[1], "list") == 0) {
        FILE *f = fopen(db, "r");
        if (!f) {
            printf("No notes found.\n");
            return;
        }
        char line[1024];
        int count = 1;
        printf("\nSaved Notes\n");
        printf("--------------------------------------------------\n");
        while (fgets(line, sizeof(line), f)) {
            line[strcspn(line, "\r\n")] = '\0';
            printf("%2d. %s\n", count++, line);
        }
        if (count == 1) printf("No notes found.\n");
        printf("--------------------------------------------------\n");
        fclose(f);
    } else if (strcmp(args[1], "clear") == 0) {
        FILE *f = fopen(db, "w");
        if (f) fclose(f);
        printf("All notes cleared.\n");
    } else {
        printf("Unknown option: %s. Usage: notes add [text] | list | clear\n", args[1]);
    }
}

/* =========================================================================
 * ASSIGNMENT COMMAND
 * ========================================================================= */
static void handle_assignments(char **args, int argc) {
    if (argc < 2) {
        printf("Usage: assignment add [title] [--due DD-MM-YYYY] | list | done <id> | clear\n");
        return;
    }

    char db[1024];
    get_data_path("assignments.db", db, sizeof(db));

    if (strcmp(args[1], "add") == 0) {
        char title[256] = "";
        char due[64] = "No deadline";

        if (argc < 3) {
            /* Interactive step-by-step prompt */
            printf("Title    : ");
            fflush(stdout);
            if (!fgets(title, sizeof(title), stdin)) return;
            title[strcspn(title, "\r\n")] = '\0';
            if (strlen(title) == 0) {
                printf("Title cannot be empty.\n");
                return;
            }

            printf("Due Date : ");
            fflush(stdout);
            char due_in[64];
            if (fgets(due_in, sizeof(due_in), stdin)) {
                due_in[strcspn(due_in, "\r\n")] = '\0';
                if (strlen(due_in) > 0) strncpy(due, due_in, sizeof(due) - 1);
            }
        } else {
            /* Parse arguments */
            for (int i = 2; i < argc; i++) {
                if (strcmp(args[i], "--due") == 0 && i + 1 < argc) {
                    strncpy(due, args[++i], sizeof(due) - 1);
                } else {
                    if (strlen(title) > 0) strncat(title, " ", sizeof(title) - strlen(title) - 1);
                    strncat(title, args[i], sizeof(title) - strlen(title) - 1);
                }
            }
        }

        /* Generate next ID */
        int next_id = 1;
        FILE *rf = fopen(db, "r");
        if (rf) {
            char line[512];
            while (fgets(line, sizeof(line), rf)) {
                int id;
                if (sscanf(line, "%d|", &id) == 1 && id >= next_id) {
                    next_id = id + 1;
                }
            }
            fclose(rf);
        }

        FILE *f = fopen(db, "a");
        if (!f) { perror("assignments"); return; }
        fprintf(f, "%d|%s|%s|0\n", next_id, title, due);
        fclose(f);
        printf("Assignment added: [%d] %s (Due: %s)\n", next_id, title, due);
    } else if (strcmp(args[1], "list") == 0) {
        FILE *f = fopen(db, "r");
        if (!f) {
            printf("No assignments found.\n");
            return;
        }
        char line[512];
        int count = 0;
        printf("\nAssignments\n");
        printf("----------------------------------------------------------------------\n");
        printf("%-4s %-32s %-16s %-10s\n", "ID", "Title", "Due Date", "Status");
        printf("----------------------------------------------------------------------\n");
        while (fgets(line, sizeof(line), f)) {
            int id = 0, done = 0;
            char title[256] = {0}, due[64] = {0};
            char *p1 = strchr(line, '|');
            if (!p1) continue;
            *p1 = '\0';
            id = atoi(line);

            char *p2 = strchr(p1 + 1, '|');
            if (!p2) continue;
            *p2 = '\0';
            strncpy(title, p1 + 1, sizeof(title) - 1);

            char *p3 = strchr(p2 + 1, '|');
            if (!p3) continue;
            *p3 = '\0';
            strncpy(due, p2 + 1, sizeof(due) - 1);
            done = atoi(p3 + 1);

            printf("%-4d %-32s %-16s %-10s\n", id, title, due, done ? "Done" : "Pending");
            count++;
        }
        if (count == 0) printf("No assignments recorded.\n");
        printf("----------------------------------------------------------------------\n");
        fclose(f);
    } else if (strcmp(args[1], "done") == 0) {
        int target_id = 0;
        if (argc < 3) {
            printf("Assignment ID to mark done : ");
            fflush(stdout);
            char id_buf[32];
            if (!fgets(id_buf, sizeof(id_buf), stdin)) return;
            target_id = atoi(id_buf);
        } else {
            target_id = atoi(args[2]);
        }
        if (target_id <= 0) {
            printf("Invalid assignment ID.\n");
            return;
        }
        FILE *rf = fopen(db, "r");
        if (!rf) { printf("No assignments found.\n"); return; }

        char temp_db[1040];
        snprintf(temp_db, sizeof(temp_db), "%s.tmp", db);
        FILE *wf = fopen(temp_db, "w");
        if (!wf) { fclose(rf); perror("assignments"); return; }

        char line[512];
        int found = 0;
        while (fgets(line, sizeof(line), rf)) {
            int id = 0;
            char raw[512];
            strncpy(raw, line, sizeof(raw) - 1);
            char *p1 = strchr(line, '|');
            if (p1) {
                *p1 = '\0';
                id = atoi(line);
                char *p2 = strchr(p1 + 1, '|');
                if (p2) {
                    char *p3 = strchr(p2 + 1, '|');
                    if (p3) {
                        if (id == target_id) {
                            found = 1;
                            *p1 = '|';
                            p3[1] = '1';
                            p3[2] = '\n';
                            p3[3] = '\0';
                            fputs(line, wf);
                            continue;
                        }
                    }
                }
            }
            fputs(raw, wf);
        }
        fclose(rf);
        fclose(wf);
        rename(temp_db, db);
        if (found) printf("Assignment #%d marked as done.\n", target_id);
        else printf("Assignment #%d not found.\n", target_id);
    } else if (strcmp(args[1], "clear") == 0) {
        FILE *f = fopen(db, "w");
        if (f) fclose(f);
        printf("All assignments cleared.\n");
    } else {
        printf("Unknown option: %s. Usage: assignment add | list | done <id> | clear\n", args[1]);
    }
}

/* =========================================================================
 * TIMETABLE COMMAND
 * ========================================================================= */
static void handle_timetable(char **args, int argc) {
    if (argc < 2) {
        printf("Usage: timetable add [day] [time] [subject] | list | clear\n");
        return;
    }

    char db[1024];
    get_data_path("timetable.db", db, sizeof(db));

    if (strcmp(args[1], "add") == 0) {
        char day[64] = "";
        char time_str[64] = "";
        char subject[256] = "";

        if (argc < 4) {
            /* Interactive step-by-step prompt */
            printf("Day     : ");
            fflush(stdout);
            if (!fgets(day, sizeof(day), stdin)) return;
            day[strcspn(day, "\r\n")] = '\0';

            printf("Time    : ");
            fflush(stdout);
            if (!fgets(time_str, sizeof(time_str), stdin)) return;
            time_str[strcspn(time_str, "\r\n")] = '\0';

            printf("Subject : ");
            fflush(stdout);
            if (!fgets(subject, sizeof(subject), stdin)) return;
            subject[strcspn(subject, "\r\n")] = '\0';
        } else {
            /* Command line arguments */
            strncpy(day, args[2], sizeof(day) - 1);
            strncpy(time_str, args[3], sizeof(time_str) - 1);
            for (int i = 4; i < argc; i++) {
                if (i > 4) strncat(subject, " ", sizeof(subject) - strlen(subject) - 1);
                strncat(subject, args[i], sizeof(subject) - strlen(subject) - 1);
            }
        }

        if (strlen(day) == 0 || strlen(time_str) == 0 || strlen(subject) == 0) {
            printf("Day, Time, and Subject are all required.\n");
            return;
        }

        FILE *f = fopen(db, "a");
        if (!f) { perror("timetable"); return; }
        fprintf(f, "%s|%s|%s\n", day, time_str, subject);
        fclose(f);
        printf("Class added: %s at %s - %s\n", day, time_str, subject);
    } else if (strcmp(args[1], "list") == 0) {
        FILE *f = fopen(db, "r");
        if (!f) {
            printf("No timetable entries found.\n");
            return;
        }
        char line[512];
        int count = 0;
        printf("\nWeekly Class Timetable\n");
        printf("--------------------------------------------------\n");
        printf("%-12s %-10s %-25s\n", "Day", "Time", "Subject");
        printf("--------------------------------------------------\n");
        while (fgets(line, sizeof(line), f)) {
            char *p1 = strchr(line, '|');
            if (!p1) continue;
            *p1 = '\0';
            char *p2 = strchr(p1 + 1, '|');
            if (!p2) continue;
            *p2 = '\0';
            char *sub = p2 + 1;
            sub[strcspn(sub, "\r\n")] = '\0';

            printf("%-12s %-10s %-25s\n", line, p1 + 1, sub);
            count++;
        }
        if (count == 0) printf("No classes scheduled yet.\n");
        printf("--------------------------------------------------\n");
        fclose(f);
    } else if (strcmp(args[1], "clear") == 0) {
        FILE *f = fopen(db, "w");
        if (f) fclose(f);
        printf("Timetable cleared.\n");
    } else {
        printf("Unknown option: %s. Usage: timetable add | list | clear\n", args[1]);
    }
}

/* =========================================================================
 * CALCULATOR COMMAND (Recursive-descent evaluator)
 * ========================================================================= */
typedef struct {
    const char *p;
    int error;
    char errmsg[64];
} CalcCtx;

static void skip_ws(CalcCtx *c) {
    while (*c->p == ' ' || *c->p == '\t') c->p++;
}

static double parse_expr(CalcCtx *c);

static double parse_primary(CalcCtx *c) {
    skip_ws(c);
    if (*c->p == '(') {
        c->p++;
        double v = parse_expr(c);
        skip_ws(c);
        if (*c->p == ')') c->p++;
        else {
            c->error = 1;
            snprintf(c->errmsg, sizeof(c->errmsg), "missing closing parenthesis");
        }
        return v;
    }
    if (*c->p == '-' || *c->p == '+') {
        int neg = (*c->p == '-');
        c->p++;
        return neg ? -parse_primary(c) : parse_primary(c);
    }
    if (isdigit((unsigned char)*c->p) || *c->p == '.') {
        char *end;
        double v = strtod(c->p, &end);
        c->p = end;
        return v;
    }
    c->error = 1;
    snprintf(c->errmsg, sizeof(c->errmsg), "syntax error near '%c'", *c->p ? *c->p : ' ');
    return 0;
}

static double parse_term(CalcCtx *c) {
    double v = parse_primary(c);
    while (!c->error) {
        skip_ws(c);
        if (*c->p == '*') { c->p++; v *= parse_primary(c); }
        else if (*c->p == '/') {
            c->p++;
            double d = parse_primary(c);
            if (d == 0.0) {
                c->error = 1;
                snprintf(c->errmsg, sizeof(c->errmsg), "division by zero");
                return 0;
            }
            v /= d;
        } else break;
    }
    return v;
}

static double parse_expr(CalcCtx *c) {
    double v = parse_term(c);
    while (!c->error) {
        skip_ws(c);
        if (*c->p == '+') { c->p++; v += parse_term(c); }
        else if (*c->p == '-') { c->p++; v -= parse_term(c); }
        else break;
    }
    return v;
}

static void handle_calc(char **args, int argc) {
    char expr[1024] = "";
    if (argc < 2) {
        printf("Enter math expression : ");
        fflush(stdout);
        if (!fgets(expr, sizeof(expr), stdin)) return;
        expr[strcspn(expr, "\r\n")] = '\0';
        if (strlen(expr) == 0) return;
    } else {
        for (int i = 1; i < argc; i++) {
            if (i > 1) strncat(expr, " ", sizeof(expr) - strlen(expr) - 1);
            strncat(expr, args[i], sizeof(expr) - strlen(expr) - 1);
        }
    }
    CalcCtx ctx = {expr, 0, ""};
    double result = parse_expr(&ctx);
    skip_ws(&ctx);
    if (*ctx.p && !ctx.error) {
        ctx.error = 1;
        snprintf(ctx.errmsg, sizeof(ctx.errmsg), "unexpected '%c'", *ctx.p);
    }
    if (ctx.error) {
        printf("calc error: %s\n", ctx.errmsg);
    } else {
        printf("\nExpression : %s\nResult     : %g\n", expr, result);
    }
}

/* =========================================================================
 * COMPILER COMMANDS (compile / run / test)
 * ========================================================================= */
static void handle_compiler(char **args, int argc) {
    const char *action = args[0];

    if (strcmp(action, "compile") == 0) {
        char src_buf[256] = "";
        char out_buf[256] = "";
        const char *src = NULL;

        if (argc < 2) {
            printf("Source file (.c) : ");
            fflush(stdout);
            if (!fgets(src_buf, sizeof(src_buf), stdin)) return;
            src_buf[strcspn(src_buf, "\r\n")] = '\0';
            if (strlen(src_buf) == 0) return;
            src = src_buf;

            printf("Output binary name (optional) : ");
            fflush(stdout);
            if (fgets(out_buf, sizeof(out_buf), stdin)) {
                out_buf[strcspn(out_buf, "\r\n")] = '\0';
            }
        } else {
            src = args[1];
            if (argc > 2) strncpy(out_buf, args[2], sizeof(out_buf) - 1);
        }

        char out[256];
        if (strlen(out_buf) > 0) {
            strncpy(out, out_buf, sizeof(out) - 1);
        } else {
            strncpy(out, src, sizeof(out) - 1);
            char *dot = strrchr(out, '.');
            if (dot) *dot = '\0';
            else strncat(out, ".out", sizeof(out) - strlen(out) - 1);
        }

        printf("Compiling %s -> %s ...\n", src, out);
        char *compile_args[] = {"gcc", "-Wall", "-Wextra", "-g", (char *)src, "-o", out, NULL};
        int rc = run_program(compile_args);
        if (rc == 0) printf("Compilation successful: ./%s\n", out);
        else printf("Compilation failed with code %d\n", rc);
    } else if (strcmp(action, "run") == 0) {
        if (argc < 2) {
            char cmd[1024] = "";
            printf("Program to run : ");
            fflush(stdout);
            if (!fgets(cmd, sizeof(cmd), stdin)) return;
            cmd[strcspn(cmd, "\r\n")] = '\0';
            if (strlen(cmd) == 0) return;
            char *run_args[] = {cmd, NULL};
            (void)run_program(run_args);
        } else {
            (void)run_program(&args[1]);
        }
    } else if (strcmp(action, "test") == 0) {
        char src_buf[256] = "";
        const char *src = NULL;
        if (argc < 2) {
            printf("C file to test : ");
            fflush(stdout);
            if (!fgets(src_buf, sizeof(src_buf), stdin)) return;
            src_buf[strcspn(src_buf, "\r\n")] = '\0';
            if (strlen(src_buf) == 0) return;
            src = src_buf;
        } else {
            src = args[1];
        }
        char tmp_bin[256];
        snprintf(tmp_bin, sizeof(tmp_bin), "/tmp/__test_%d", getpid());
        char *compile_args[] = {"gcc", "-Wall", "-Wextra", (char *)src, "-o", tmp_bin, NULL};
        int rc = run_program(compile_args);
        if (rc == 0) {
            char *test_args[] = {tmp_bin, NULL};
            rc = run_program(test_args);
        }
        unlink(tmp_bin);
        printf("\nTest process exited with status %d\n", rc);
    }
}

/* =========================================================================
 * FILES COMMAND
 * ========================================================================= */
static void handle_files(char **args, int argc) {
    char sub_buf[64] = "";
    const char *sub = NULL;

    if (argc < 2) {
        printf("File action (list/create/read/delete/info) : ");
        fflush(stdout);
        if (!fgets(sub_buf, sizeof(sub_buf), stdin)) return;
        sub_buf[strcspn(sub_buf, "\r\n")] = '\0';
        sub = sub_buf;
    } else {
        sub = args[1];
    }

    if (strcmp(sub, "list") == 0) {
        const char *path = (argc > 2) ? args[2] : ".";
        DIR *d = opendir(path);
        if (!d) { perror(path); return; }
        struct dirent *ent;
        printf("\nDirectory: %s\n", path);
        printf("--------------------------------------------------\n");
        while ((ent = readdir(d))) {
            if (ent->d_name[0] == '.') continue;
            printf("  %s%s\n", ent->d_name, (ent->d_type == DT_DIR) ? "/" : "");
        }
        closedir(d);
        printf("--------------------------------------------------\n");
    } else if (strcmp(sub, "create") == 0) {
        char path[256] = "";
        if (argc < 3) {
            printf("File path to create : ");
            fflush(stdout);
            if (!fgets(path, sizeof(path), stdin)) return;
            path[strcspn(path, "\r\n")] = '\0';
        } else {
            strncpy(path, args[2], sizeof(path) - 1);
        }
        if (strlen(path) == 0) return;
        FILE *f = fopen(path, "w");
        if (!f) { perror(path); return; }
        fclose(f);
        printf("Created empty file: %s\n", path);
    } else if (strcmp(sub, "read") == 0) {
        char path[256] = "";
        if (argc < 3) {
            printf("File path to read : ");
            fflush(stdout);
            if (!fgets(path, sizeof(path), stdin)) return;
            path[strcspn(path, "\r\n")] = '\0';
        } else {
            strncpy(path, args[2], sizeof(path) - 1);
        }
        if (strlen(path) == 0) return;
        FILE *f = fopen(path, "r");
        if (!f) { perror(path); return; }
        char line[1024];
        while (fgets(line, sizeof(line), f)) fputs(line, stdout);
        fclose(f);
    } else if (strcmp(sub, "delete") == 0) {
        char path[256] = "";
        if (argc < 3) {
            printf("File path to delete : ");
            fflush(stdout);
            if (!fgets(path, sizeof(path), stdin)) return;
            path[strcspn(path, "\r\n")] = '\0';
        } else {
            strncpy(path, args[2], sizeof(path) - 1);
        }
        if (strlen(path) == 0) return;
        if (unlink(path) == 0) printf("Deleted: %s\n", path);
        else perror(path);
    } else if (strcmp(sub, "info") == 0) {
        char path[256] = "";
        if (argc < 3) {
            printf("File path to inspect : ");
            fflush(stdout);
            if (!fgets(path, sizeof(path), stdin)) return;
            path[strcspn(path, "\r\n")] = '\0';
        } else {
            strncpy(path, args[2], sizeof(path) - 1);
        }
        if (strlen(path) == 0) return;
        struct stat st;
        if (stat(path, &st) != 0) { perror(path); return; }
        printf("\nFile Information: %s\n", path);
        printf("--------------------------------------------------\n");
        printf("Size : %lld bytes\n", (long long)st.st_size);
        printf("Type : %s\n", S_ISDIR(st.st_mode) ? "Directory" : "File");
        printf("--------------------------------------------------\n");
    } else {
        printf("Unknown files option: %s. Options: list, create, read, delete, info\n", sub);
    }
}

/* =========================================================================
 * MEMSTAT COMMAND (/proc/meminfo)
 * ========================================================================= */
static void handle_memstat(void) {
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) {
        printf("Memory statistics are available on Linux systems via /proc/meminfo.\n");
        return;
    }

    char line[256];
    unsigned long mem_total = 0, mem_free = 0, mem_available = 0;
    unsigned long buffers = 0, cached = 0, swap_total = 0, swap_free = 0;

    while (fgets(line, sizeof(line), f)) {
        char key[64];
        unsigned long val = 0;
        if (sscanf(line, "%63s %lu", key, &val) >= 2) {
            if (strcmp(key, "MemTotal:") == 0) mem_total = val;
            else if (strcmp(key, "MemFree:") == 0) mem_free = val;
            else if (strcmp(key, "MemAvailable:") == 0) mem_available = val;
            else if (strcmp(key, "Buffers:") == 0) buffers = val;
            else if (strcmp(key, "Cached:") == 0) cached = val;
            else if (strcmp(key, "SwapTotal:") == 0) swap_total = val;
            else if (strcmp(key, "SwapFree:") == 0) swap_free = val;
        }
    }
    fclose(f);

    unsigned long mem_used = (mem_total > mem_available) ? (mem_total - mem_available) : 0;
    double mem_percent = (mem_total > 0) ? (100.0 * mem_used / mem_total) : 0.0;

    printf("\nVirtual Memory Statistics (memstat)\n");
    printf("--------------------------------------------------\n");
    printf("Total Physical Memory : %10lu kB (%6.2f MB)\n", mem_total, mem_total / 1024.0);
    printf("Used Physical Memory  : %10lu kB (%6.2f%%)\n", mem_used, mem_percent);
    printf("Free Physical Memory  : %10lu kB\n", mem_free);
    printf("Available Memory      : %10lu kB\n", mem_available);
    printf("Buffers / Cache       : %10lu kB / %lu kB\n", buffers, cached);
    if (swap_total > 0) {
        printf("Swap Total / Free     : %10lu kB / %lu kB\n", swap_total, swap_free);
    }
    printf("--------------------------------------------------\n");
}

/* =========================================================================
 * SYSINFO COMMAND (uname)
 * ========================================================================= */
static void handle_sysinfo(void) {
    struct utsname u;
    if (uname(&u) != 0) {
        perror("sysinfo");
        return;
    }
    printf("\nStudentOS System Information\n");
    printf("--------------------------------------------------\n");
    printf("OS Name    : %s\n", u.sysname);
    printf("Node Name  : %s\n", u.nodename);
    printf("Release    : %s\n", u.release);
    printf("Version    : %s\n", u.version);
    printf("Machine    : %s\n", u.machine);
    printf("--------------------------------------------------\n");
}

/* =========================================================================
 * THREADS COMMAND (pthreads mutex demo)
 * ========================================================================= */
typedef struct {
    int id;
    int iters;
    long *counter;
    pthread_mutex_t *mu;
} ThreadData;

static void *thread_worker(void *arg) {
    ThreadData *td = (ThreadData *)arg;
    for (int i = 0; i < td->iters; i++) {
        pthread_mutex_lock(td->mu);
        (*td->counter)++;
        pthread_mutex_unlock(td->mu);
    }
    return NULL;
}

static void handle_threads(char **args, int argc) {
    int n = (argc > 1) ? atoi(args[1]) : 4;
    if (n < 1 || n > 16) n = 4;
    const int iters = 100000;

    pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
    long counter = 0;
    pthread_t threads[16];
    ThreadData td[16];

    printf("Spawning %d threads, each incrementing counter %d times...\n", n, iters);
    for (int i = 0; i < n; i++) {
        td[i] = (ThreadData){i + 1, iters, &counter, &mu};
        pthread_create(&threads[i], NULL, thread_worker, &td[i]);
    }
    for (int i = 0; i < n; i++) {
        pthread_join(threads[i], NULL);
    }

    long expected = (long)n * iters;
    printf("Expected count: %ld\n", expected);
    printf("Actual count  : %ld\n", counter);
    if (counter == expected) {
        printf("Result: Mutex synchronization successful (no race condition).\n");
    } else {
        printf("Result: Race condition detected!\n");
    }
    pthread_mutex_destroy(&mu);
}

/* =========================================================================
 * HELP COMMANDS
 * ========================================================================= */
void print_student_help(void) {
    printf("\nStudent Productivity Tools:\n");
    printf("  notes               Take, view, and clear quick study notes\n");
    printf("  assignment          Track homework deadlines and mark them done\n");
    printf("  timetable           Manage your weekly class schedule\n");
    printf("  calc <expression>   Arithmetic calculator (e.g. calc (20 + 5) * 4)\n");
    printf("  compile <file.c>    Compile C source code with gcc\n");
    printf("  run <program>       Execute compiled binary\n");
    printf("  files               File manager (list, create, read, delete, info)\n");

    printf("\nOS & System Tools:\n");
    printf("  memstat             View physical and virtual RAM statistics\n");
    printf("  sysinfo             Display operating system and hardware details\n");
    printf("  threads [n]         Run multi-threaded race condition demo\n");

    printf("\nShell Commands & Job Control:\n");
    printf("  cd <dir>            Change directory\n");
    printf("  pwd                 Print working directory\n");
    printf("  echo [text]         Print text (supports $? for exit code)\n");
    printf("  history [clear]     View or reset command history\n");
    printf("  jobs                List background jobs\n");
    printf("  fg <id>             Bring a background job to foreground\n");
    printf("  bg <id>             Resume a stopped job in background\n");
    printf("  help [command]      Show help for a specific command\n");
    printf("  exit                Quit StudentOS\n");

    printf("\nUnix Operators:\n");
    printf("  cmd > file          Redirect output to file\n");
    printf("  cmd >> file         Append output to file\n");
    printf("  cmd < file          Read input from file\n");
    printf("  cmd1 | cmd2         Pipe output of cmd1 into cmd2\n");
    printf("  cmd &               Run command in background\n\n");
}

void print_command_help(const char *cmd) {
    if (!cmd) { print_student_help(); return; }

    if (strcmp(cmd, "notes") == 0) {
        printf("\nCommand: notes\n"
               "Usage:\n"
               "  notes add           Interactive step-by-step prompt\n"
               "  notes add <text>    Direct one-line add\n"
               "  notes list          Show all saved notes\n"
               "  notes clear         Clear all notes\n\n");
    } else if (strcmp(cmd, "assignment") == 0) {
        printf("\nCommand: assignment\n"
               "Usage:\n"
               "  assignment add                    Interactive step-by-step prompt\n"
               "  assignment add <t> [--due <d>]    Direct one-line add\n"
               "  assignment list                   Display all assignments\n"
               "  assignment done <id>              Mark assignment complete\n"
               "  assignment clear                  Clear all assignments\n\n");
    } else if (strcmp(cmd, "timetable") == 0) {
        printf("\nCommand: timetable\n"
               "Usage:\n"
               "  timetable add                     Interactive step-by-step prompt\n"
               "  timetable add <day> <time> <sub>  Direct one-line add\n"
               "  timetable list                    Display weekly schedule\n"
               "  timetable clear                   Clear timetable\n\n");
    } else if (strcmp(cmd, "calc") == 0 || strcmp(cmd, "calculator") == 0) {
        printf("\nCommand: calc\n"
               "Usage:\n"
               "  calc <expression>   Supported: + - * / ( )\n"
               "Example: calc (20 + 5) * 4\n\n");
    } else {
        print_student_help();
    }
}

/* =========================================================================
 * DISPATCHER
 * ========================================================================= */
int is_student_command(const char *cmd) {
    if (!cmd) return 0;
    return (strcmp(cmd, "notes") == 0 || strcmp(cmd, "note") == 0 ||
            strcmp(cmd, "assignment") == 0 || strcmp(cmd, "assignments") == 0 ||
            strcmp(cmd, "timetable") == 0 ||
            strcmp(cmd, "calc") == 0 || strcmp(cmd, "calculator") == 0 ||
            strcmp(cmd, "compile") == 0 || strcmp(cmd, "run") == 0 || strcmp(cmd, "test") == 0 ||
            strcmp(cmd, "files") == 0 ||
            strcmp(cmd, "memstat") == 0 ||
            strcmp(cmd, "sysinfo") == 0 ||
            strcmp(cmd, "threads") == 0);
}

int handle_student_command(char **args, int argc) {
    if (!args || argc <= 0) return 0;
    const char *cmd = args[0];

    if (!strcmp(cmd, "notes") || !strcmp(cmd, "note"))
        { handle_notes(args, argc); return 1; }
    if (!strcmp(cmd, "assignment") || !strcmp(cmd, "assignments"))
        { handle_assignments(args, argc); return 1; }
    if (!strcmp(cmd, "timetable"))
        { handle_timetable(args, argc); return 1; }
    if (!strcmp(cmd, "calc") || !strcmp(cmd, "calculator"))
        { handle_calc(args, argc); return 1; }
    if (!strcmp(cmd, "compile") || !strcmp(cmd, "run") || !strcmp(cmd, "test"))
        { handle_compiler(args, argc); return 1; }
    if (!strcmp(cmd, "files"))
        { handle_files(args, argc); return 1; }
    if (!strcmp(cmd, "memstat"))
        { handle_memstat(); return 1; }
    if (!strcmp(cmd, "sysinfo"))
        { handle_sysinfo(); return 1; }
    if (!strcmp(cmd, "threads"))
        { handle_threads(args, argc); return 1; }

    return 0;
}
