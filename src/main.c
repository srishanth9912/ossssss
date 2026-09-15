#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <termios.h>

#include "vector.h"
#include "student_string.h"
#include "student_commands.h"
#include "signals.h"
#include "jobs.h"
#include "history.h"

#define INPUT_SIZE 4096

/* Global last exit code ($?) */
int g_last_exit_code = 0;

/*
 * Structure representing a parsed command stage
 */
typedef struct {
    char **argv;
    int argc;
    char *input_file;
    char *output_file;
    int append_output;
    char *error_file;
    int redirect_error_to_stdout;
} ParsedCommand;

/*
 * Helper: Free array of ParsedCommand structs
 */
static void free_commands(ParsedCommand *cmds, int count) {
    if (!cmds) return;
    for (int i = 0; i < count; i++) {
        if (cmds[i].argv) {
            for (int j = 0; j < cmds[i].argc; j++) {
                free(cmds[i].argv[j]);
            }
            free(cmds[i].argv);
        }
        free(cmds[i].input_file);
        free(cmds[i].output_file);
        free(cmds[i].error_file);
    }
    free(cmds);
}

/*
 * Tokenizer: Splits input line into tokens respecting quotes and operators
 */
static StringVector *tokenize_input(const char *input) {
    StringVector *tokens = vector_create();
    if (!tokens) return NULL;

    DynamicString *cur = string_create();
    if (!cur) { vector_free(tokens); return NULL; }

    char quote = '\0';

    for (size_t i = 0; input[i] != '\0'; i++) {
        char c = input[i];

        /* Inside quotes */
        if (quote != '\0') {
            if (c == quote) {
                quote = '\0';
            } else {
                string_append_c(cur, c);
            }
            continue;
        }

        /* Opening quote */
        if (c == '\'' || c == '"') {
            quote = c;
            continue;
        }

        /* Whitespace separator */
        if (c == ' ' || c == '\t') {
            if (cur->length > 0) {
                vector_push(tokens, cur->data);
                string_free(cur);
                cur = string_create();
            }
            continue;
        }

        /* Multi-character operator: 2>&1 */
        if (c == '2' && input[i + 1] == '>' && input[i + 2] == '&' && input[i + 3] == '1') {
            if (cur->length > 0) {
                vector_push(tokens, cur->data);
                string_free(cur);
                cur = string_create();
            }
            vector_push(tokens, "2>&1");
            i += 3;
            continue;
        }

        /* Multi-character operator: 2> */
        if (c == '2' && input[i + 1] == '>') {
            if (cur->length > 0) {
                vector_push(tokens, cur->data);
                string_free(cur);
                cur = string_create();
            }
            vector_push(tokens, "2>");
            i += 1;
            continue;
        }

        /* Multi-character operator: >> */
        if (c == '>' && input[i + 1] == '>') {
            if (cur->length > 0) {
                vector_push(tokens, cur->data);
                string_free(cur);
                cur = string_create();
            }
            vector_push(tokens, ">>");
            i += 1;
            continue;
        }

        /* Single-character operators: |, <, >, & */
        if (c == '|' || c == '<' || c == '>' || c == '&') {
            if (cur->length > 0) {
                vector_push(tokens, cur->data);
                string_free(cur);
                cur = string_create();
            }
            char op[2] = {c, '\0'};
            vector_push(tokens, op);
            continue;
        }

        string_append_c(cur, c);
    }

    if (cur->length > 0) {
        vector_push(tokens, cur->data);
    }
    string_free(cur);

    return tokens;
}

/*
 * Check if command is a built-in
 */
static int is_shell_builtin(const char *cmd) {
    if (!cmd) return 0;
    return (strcmp(cmd, "cd") == 0 ||
            strcmp(cmd, "pwd") == 0 ||
            strcmp(cmd, "echo") == 0 ||
            strcmp(cmd, "help") == 0 ||
            strcmp(cmd, "history") == 0 ||
            strcmp(cmd, "jobs") == 0 ||
            strcmp(cmd, "fg") == 0 ||
            strcmp(cmd, "bg") == 0 ||
            strcmp(cmd, "exit") == 0 ||
            strcmp(cmd, "quit") == 0);
}

/*
 * Execute built-in shell command
 */
static int execute_shell_builtin(char **args, int argc) {
    if (!args || argc <= 0) return 0;
    const char *cmd = args[0];

    if (strcmp(cmd, "cd") == 0) {
        const char *path = (argc > 1) ? args[1] : getenv("HOME");
        if (!path) path = ".";
        if (chdir(path) != 0) {
            perror("cd");
            return 1;
        }
        return 0;
    }

    if (strcmp(cmd, "pwd") == 0) {
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd))) {
            printf("%s\n", cwd);
            return 0;
        }
        perror("pwd");
        return 1;
    }

    if (strcmp(cmd, "echo") == 0) {
        for (int i = 1; i < argc; i++) {
            if (i > 1) printf(" ");
            if (strcmp(args[i], "$?") == 0) {
                printf("%d", g_last_exit_code);
            } else {
                printf("%s", args[i]);
            }
        }
        printf("\n");
        return 0;
    }

    if (strcmp(cmd, "help") == 0) {
        if (argc > 1) print_command_help(args[1]);
        else print_student_help();
        return 0;
    }

    if (strcmp(cmd, "history") == 0) {
        if (argc > 1 && strcmp(args[1], "clear") == 0) history_clear();
        else history_list();
        return 0;
    }

    if (strcmp(cmd, "jobs") == 0) {
        jobs_list();
        return 0;
    }

    if (strcmp(cmd, "fg") == 0) {
        int job_id = 0;
        if (argc < 2) {
            printf("Job ID to bring to foreground : ");
            fflush(stdout);
            char id_buf[32];
            if (!fgets(id_buf, sizeof(id_buf), stdin)) return 1;
            job_id = atoi(id_buf);
        } else {
            job_id = atoi(args[1]);
        }
        if (job_id <= 0) {
            printf("Invalid job ID.\n");
            return 1;
        }
        jobs_fg(job_id);
        return 0;
    }

    if (strcmp(cmd, "bg") == 0) {
        int job_id = 0;
        if (argc < 2) {
            printf("Job ID to resume in background : ");
            fflush(stdout);
            char id_buf[32];
            if (!fgets(id_buf, sizeof(id_buf), stdin)) return 1;
            job_id = atoi(id_buf);
        } else {
            job_id = atoi(args[1]);
        }
        if (job_id <= 0) {
            printf("Invalid job ID.\n");
            return 1;
        }
        jobs_bg(job_id);
        return 0;
    }

    if (strcmp(cmd, "exit") == 0 || strcmp(cmd, "quit") == 0) {
        int code = (argc > 1) ? atoi(args[1]) : g_last_exit_code;
        jobs_cleanup();
        exit(code);
    }

    return 0;
}

/*
 * Redirection Setup: Apply <, >, >>, 2>, 2>&1
 */
static int setup_redirection(const ParsedCommand *cmd) {
    if (cmd->input_file) {
        int fd = open(cmd->input_file, O_RDONLY);
        if (fd < 0) {
            perror(cmd->input_file);
            return -1;
        }
        dup2(fd, STDIN_FILENO);
        close(fd);
    }

    if (cmd->output_file) {
        int flags = O_WRONLY | O_CREAT | (cmd->append_output ? O_APPEND : O_TRUNC);
        int fd = open(cmd->output_file, flags, 0644);
        if (fd < 0) {
            perror(cmd->output_file);
            return -1;
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }

    if (cmd->error_file) {
        int fd = open(cmd->error_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            perror(cmd->error_file);
            return -1;
        }
        dup2(fd, STDERR_FILENO);
        close(fd);
    }

    if (cmd->redirect_error_to_stdout) {
        dup2(STDOUT_FILENO, STDERR_FILENO);
    }

    return 0;
}

/*
 * Main Shell Entry Point
 */
int main(void) {
    char input[INPUT_SIZE];

    /* Initialize signal handling and background job monitor */
    setup_signals();
    jobs_init();

    printf("=================================\n");
    printf("        Welcome to StudentOS\n");
    printf("   A Student-Centric Unix Shell\n");
    printf("=================================\n");

    while (1) {
        /* Non-blocking reap of background jobs */
        jobs_reap();

        /* Format A: Clean prompt */
        printf("StudentOS > ");
        fflush(stdout);

        /* Read input */
        if (fgets(input, sizeof(input), stdin) == NULL) {
            printf("\nGoodbye!\n");
            break;
        }

        /* Strip trailing newlines */
        input[strcspn(input, "\r\n")] = '\0';
        if (input[0] == '\0') continue;

        /* Save to history */
        history_add(input);

        /* Tokenize */
        StringVector *tokens = tokenize_input(input);
        if (!tokens || tokens->size == 0) {
            if (tokens) vector_free(tokens);
            continue;
        }

        /* Check background '&' */
        int is_background = 0;
        size_t token_count = tokens->size;
        if (strcmp(tokens->items[token_count - 1], "&") == 0) {
            is_background = 1;
            token_count--;
        }

        if (token_count == 0) {
            vector_free(tokens);
            continue;
        }

        /* Count pipeline stages */
        int cmd_count = 1;
        for (size_t i = 0; i < token_count; i++) {
            if (strcmp(tokens->items[i], "|") == 0) cmd_count++;
        }

        ParsedCommand *cmds = calloc(cmd_count, sizeof(ParsedCommand));
        int cur_cmd = 0;
        StringVector *arg_builder = vector_create();
        int parse_error = 0;

        for (size_t i = 0; i < token_count; i++) {
            const char *t = tokens->items[i];

            if (strcmp(t, "|") == 0) {
                if (arg_builder->size == 0) {
                    printf("Error: empty command in pipeline\n");
                    parse_error = 1;
                    break;
                }
                cmds[cur_cmd].argc = arg_builder->size;
                cmds[cur_cmd].argv = malloc((cmds[cur_cmd].argc + 1) * sizeof(char *));
                for (size_t j = 0; j < arg_builder->size; j++) {
                    cmds[cur_cmd].argv[j] = strdup(arg_builder->items[j]);
                }
                cmds[cur_cmd].argv[cmds[cur_cmd].argc] = NULL;

                vector_free(arg_builder);
                arg_builder = vector_create();
                cur_cmd++;
                continue;
            }

            if (strcmp(t, "<") == 0 && i + 1 < token_count) {
                cmds[cur_cmd].input_file = strdup(tokens->items[++i]);
                continue;
            }
            if (strcmp(t, ">") == 0 && i + 1 < token_count) {
                cmds[cur_cmd].output_file = strdup(tokens->items[++i]);
                cmds[cur_cmd].append_output = 0;
                continue;
            }
            if (strcmp(t, ">>") == 0 && i + 1 < token_count) {
                cmds[cur_cmd].output_file = strdup(tokens->items[++i]);
                cmds[cur_cmd].append_output = 1;
                continue;
            }
            if (strcmp(t, "2>") == 0 && i + 1 < token_count) {
                cmds[cur_cmd].error_file = strdup(tokens->items[++i]);
                continue;
            }
            if (strcmp(t, "2>&1") == 0) {
                cmds[cur_cmd].redirect_error_to_stdout = 1;
                continue;
            }

            vector_push(arg_builder, t);
        }

        if (!parse_error && arg_builder->size > 0) {
            cmds[cur_cmd].argc = arg_builder->size;
            cmds[cur_cmd].argv = malloc((cmds[cur_cmd].argc + 1) * sizeof(char *));
            for (size_t j = 0; j < arg_builder->size; j++) {
                cmds[cur_cmd].argv[j] = strdup(arg_builder->items[j]);
            }
            cmds[cur_cmd].argv[cmds[cur_cmd].argc] = NULL;
        } else if (arg_builder->size == 0 && !parse_error) {
            printf("Error: empty command in pipeline\n");
            parse_error = 1;
        }

        vector_free(arg_builder);
        vector_free(tokens);

        if (parse_error) {
            free_commands(cmds, cmd_count);
            continue;
        }

        /*
         * Single In-Process Command (Builtin or Student Tool)
         */
        if (cmd_count == 1 && !is_background &&
            (is_shell_builtin(cmds[0].argv[0]) || is_student_command(cmds[0].argv[0]))) {
            int sv_out = -1, sv_in = -1, sv_err = -1;
            if (cmds[0].output_file) sv_out = dup(STDOUT_FILENO);
            if (cmds[0].input_file)  sv_in  = dup(STDIN_FILENO);
            if (cmds[0].error_file || cmds[0].redirect_error_to_stdout)
                sv_err = dup(STDERR_FILENO);

            if (setup_redirection(&cmds[0]) == 0) {
                if (is_shell_builtin(cmds[0].argv[0])) {
                    g_last_exit_code = execute_shell_builtin(cmds[0].argv, cmds[0].argc);
                } else {
                    handle_student_command(cmds[0].argv, cmds[0].argc);
                    g_last_exit_code = 0;
                }
            }

            if (sv_out != -1) { dup2(sv_out, STDOUT_FILENO); close(sv_out); }
            if (sv_in  != -1) { dup2(sv_in,  STDIN_FILENO);  close(sv_in);  }
            if (sv_err != -1) { dup2(sv_err, STDERR_FILENO); close(sv_err); }

            free_commands(cmds, cmd_count);
            continue;
        }

        /*
         * Pipeline & External Command Execution (fork / exec / waitpid)
         */
        int pipe_count = cmd_count - 1;
        int *pipefds = NULL;
        if (pipe_count > 0) {
            pipefds = malloc(2 * pipe_count * sizeof(int));
            for (int i = 0; i < pipe_count; i++) {
                if (pipe(pipefds + i * 2) < 0) {
                    perror("pipe");
                    break;
                }
            }
        }

        pid_t *pids = malloc(cmd_count * sizeof(pid_t));
        pid_t pgid = 0;

        for (int i = 0; i < cmd_count; i++) {
            pid_t pid = fork();

            if (pid == 0) {
                /* Child process */
                pid_t child_pid = getpid();
                if (pgid == 0) pgid = child_pid;
                setpgid(0, pgid);

                /* Pipe wiring */
                if (i > 0 && pipefds) {
                    dup2(pipefds[(i - 1) * 2], STDIN_FILENO);
                }
                if (i < cmd_count - 1 && pipefds) {
                    dup2(pipefds[i * 2 + 1], STDOUT_FILENO);
                }

                if (pipefds) {
                    for (int j = 0; j < 2 * pipe_count; j++) close(pipefds[j]);
                }

                if (setup_redirection(&cmds[i]) != 0) _exit(EXIT_FAILURE);

                signals_reset_child();

                if (is_shell_builtin(cmds[i].argv[0])) {
                    fflush(stdout);
                    int rc = execute_shell_builtin(cmds[i].argv, cmds[i].argc);
                    fflush(stdout);
                    _exit(rc);
                }

                if (is_student_command(cmds[i].argv[0])) {
                    fflush(stdout);
                    handle_student_command(cmds[i].argv, cmds[i].argc);
                    fflush(stdout);
                    _exit(0);
                }

                execvp(cmds[i].argv[0], cmds[i].argv);
                fprintf(stderr, "%s: command not found\n", cmds[i].argv[0]);
                _exit(127);
            }

            if (pid < 0) {
                perror("fork");
                break;
            }

            if (pgid == 0) pgid = pid;
            setpgid(pid, pgid);
            pids[i] = pid;
        }

        /* Parent closes all pipe ends */
        if (pipefds) {
            for (int j = 0; j < 2 * pipe_count; j++) close(pipefds[j]);
            free(pipefds);
        }

        if (is_background) {
            jobs_add(pids[0], pgid, input);
            g_last_exit_code = 0;
        } else {
            if (isatty(STDIN_FILENO)) tcsetpgrp(STDIN_FILENO, pgid);

            for (int i = 0; i < cmd_count; i++) {
                int status;
                waitpid(pids[i], &status, WUNTRACED);
                if (i == cmd_count - 1) {
                    if (WIFEXITED(status)) {
                        g_last_exit_code = WEXITSTATUS(status);
                    } else if (WIFSIGNALED(status)) {
                        g_last_exit_code = 128 + WTERMSIG(status);
                        if (WTERMSIG(status) == SIGINT) printf("\n");
                    } else if (WIFSTOPPED(status)) {
                        g_last_exit_code = 128 + WSTOPSIG(status);
                        jobs_add(pids[i], pgid, input);
                    }
                }
            }

            if (isatty(STDIN_FILENO)) tcsetpgrp(STDIN_FILENO, getpgrp());
        }

        free(pids);
        free_commands(cmds, cmd_count);
    }

    jobs_cleanup();
    return g_last_exit_code;
}
