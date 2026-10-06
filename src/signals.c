/*
 * StudentOS — Signal handling
 * Week 6 (M3·CO3): Ctrl-C/Z, SIGCHLD async reaper
 *
 * The parent shell ignores interactive signals (SIGINT, SIGTSTP, SIGQUIT,
 * SIGTTOU, SIGTTIN) so Ctrl+C/Z kills only the foreground child, never
 * the shell itself.
 *
 * SIGCHLD is caught with SA_RESTART so that slow system calls (fgets)
 * are not interrupted by child state changes.
 *
 * signals_reset_child() restores SIG_DFL in every child process before
 * execvp(), so executed programs respond normally to Ctrl+C/Z.
 */

#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include "signals.h"

/* Set when SIGCHLD is received — currently informational only;
 * actual reaping is done by the job monitor thread. */
volatile sig_atomic_t g_sigchld_received = 0;

static void handle_sigchld(int sig) {
    (void)sig;
    g_sigchld_received = 1;
}

void setup_signals(void) {
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);

    /* Parent shell: ignore all interactive/terminal signals */
    sa.sa_handler = SIG_IGN;
    sa.sa_flags   = 0;
    sigaction(SIGINT,  &sa, NULL);  /* Ctrl+C  */
    sigaction(SIGTSTP, &sa, NULL);  /* Ctrl+Z  */
    sigaction(SIGQUIT, &sa, NULL);  /* Ctrl+\  */
    sigaction(SIGTTOU, &sa, NULL);  /* bg write to terminal */
    sigaction(SIGTTIN, &sa, NULL);  /* bg read  from terminal */

    /* Catch SIGCHLD with SA_RESTART so fgets() is not interrupted */
    sa.sa_handler = handle_sigchld;
    sa.sa_flags   = SA_RESTART;
    sigaction(SIGCHLD, &sa, NULL);
}

void signals_reset_child(void) {
    struct sigaction sa;
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    /* Child processes see default dispositions — Ctrl+C kills them normally */
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTSTP, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);
    sigaction(SIGCHLD, &sa, NULL);
    sigaction(SIGTTOU, &sa, NULL);
    sigaction(SIGTTIN, &sa, NULL);
}
