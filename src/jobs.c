/*
 * StudentOS — Threaded background job monitor
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/wait.h>
#include "jobs.h"

extern int g_last_exit_code;

typedef struct {
    int      id;
    pid_t    pid;
    pid_t    pgid;
    char     command[512];
    JobState state;
    int      exit_status;
} Job;

static Job              g_jobs[MAX_JOBS];
static pthread_mutex_t  g_mu       = PTHREAD_MUTEX_INITIALIZER;
static pthread_t        g_monitor;
static volatile int     g_running  = 0;
static int              g_next_id  = 1;

/*
 * Background monitor thread
 */
static void *monitor_worker(void *arg) {
    (void)arg;
    while (g_running) {
        pthread_mutex_lock(&g_mu);
        for (int i = 0; i < MAX_JOBS; i++) {
            if (g_jobs[i].state != JOB_RUNNING &&
                g_jobs[i].state != JOB_STOPPED) continue;

            int status;
            pid_t r = waitpid(g_jobs[i].pid, &status, WNOHANG | WUNTRACED | WCONTINUED);
            if (r <= 0) continue;

            if (WIFEXITED(status) || WIFSIGNALED(status)) {
                g_jobs[i].state       = JOB_DONE;
                g_jobs[i].exit_status = WIFEXITED(status)
                    ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
            } else if (WIFSTOPPED(status)) {
                g_jobs[i].state = JOB_STOPPED;
            } else if (WIFCONTINUED(status)) {
                g_jobs[i].state = JOB_RUNNING;
            }
        }
        pthread_mutex_unlock(&g_mu);
        usleep(100000); /* 100 ms */
    }
    return NULL;
}

int jobs_init(void) {
    pthread_mutex_lock(&g_mu);
    memset(g_jobs, 0, sizeof(g_jobs));
    g_running = 1;
    pthread_mutex_unlock(&g_mu);

    if (pthread_create(&g_monitor, NULL, monitor_worker, NULL) != 0) {
        perror("pthread_create");
        return -1;
    }
    return 0;
}

void jobs_cleanup(void) {
    g_running = 0;
    pthread_join(g_monitor, NULL);
    pthread_mutex_destroy(&g_mu);
}

int jobs_add(pid_t pid, pid_t pgid, const char *command) {
    pthread_mutex_lock(&g_mu);

    int slot = -1;
    for (int i = 0; i < MAX_JOBS; i++) {
        if (g_jobs[i].state == JOB_EMPTY || g_jobs[i].state == JOB_DONE) {
            slot = i; break;
        }
    }

    if (slot == -1) {
        fprintf(stderr, "student-os: job table full\n");
        pthread_mutex_unlock(&g_mu);
        return -1;
    }

    g_jobs[slot].id          = g_next_id++;
    g_jobs[slot].pid         = pid;
    g_jobs[slot].pgid        = pgid;
    g_jobs[slot].state       = JOB_RUNNING;
    g_jobs[slot].exit_status = 0;
    strncpy(g_jobs[slot].command, command ? command : "?", sizeof(g_jobs[slot].command) - 1);
    g_jobs[slot].command[sizeof(g_jobs[slot].command) - 1] = '\0';

    int id = g_jobs[slot].id;
    printf("[%d] %d\n", id, (int)pid);
    pthread_mutex_unlock(&g_mu);
    return id;
}

void jobs_reap(void) {
    pthread_mutex_lock(&g_mu);
    for (int i = 0; i < MAX_JOBS; i++) {
        if (g_jobs[i].state != JOB_RUNNING &&
            g_jobs[i].state != JOB_STOPPED) continue;
        int status;
        pid_t r = waitpid(g_jobs[i].pid, &status, WNOHANG | WUNTRACED | WCONTINUED);
        if (r <= 0) continue;
        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            g_jobs[i].state       = JOB_DONE;
            g_jobs[i].exit_status = WIFEXITED(status)
                ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
        } else if (WIFSTOPPED(status)) {
            g_jobs[i].state = JOB_STOPPED;
        } else if (WIFCONTINUED(status)) {
            g_jobs[i].state = JOB_RUNNING;
        }
    }
    pthread_mutex_unlock(&g_mu);
}

void jobs_list(void) {
    jobs_reap();
    pthread_mutex_lock(&g_mu);

    printf("\nActive Jobs\n");
    printf("----------------------------------------------------------------------\n");
    printf("%-4s %-8s %-12s %-32s\n", "ID", "PID", "Status", "Command");
    printf("----------------------------------------------------------------------\n");

    int count = 0;
    for (int i = 0; i < MAX_JOBS; i++) {
        if (g_jobs[i].state == JOB_EMPTY) continue;

        const char *sc;
        switch (g_jobs[i].state) {
            case JOB_RUNNING: sc = "Running"; break;
            case JOB_STOPPED: sc = "Stopped"; break;
            case JOB_FG:      sc = "Foreground"; break;
            default:          sc = "Done"; break;
        }

        printf("%-4d %-8d %-12s %-32.32s\n",
               g_jobs[i].id, (int)g_jobs[i].pid, sc, g_jobs[i].command);
        count++;

        if (g_jobs[i].state == JOB_DONE) g_jobs[i].state = JOB_EMPTY;
    }

    if (!count) {
        printf("No active jobs.\n");
    }
    printf("----------------------------------------------------------------------\n\n");
    pthread_mutex_unlock(&g_mu);
}

void jobs_fg(int id) {
    pthread_mutex_lock(&g_mu);

    int slot = -1;
    for (int i = 0; i < MAX_JOBS; i++) {
        if (g_jobs[i].state != JOB_EMPTY && g_jobs[i].id == id) {
            slot = i; break;
        }
    }
    if (slot == -1) {
        printf("fg: job %d not found\n", id);
        pthread_mutex_unlock(&g_mu);
        return;
    }

    pid_t pid  = g_jobs[slot].pid;
    pid_t pgid = g_jobs[slot].pgid;
    printf("%s\n", g_jobs[slot].command);

    g_jobs[slot].state = JOB_FG;

    if (isatty(STDIN_FILENO)) tcsetpgrp(STDIN_FILENO, pgid);

    kill(-pgid, SIGCONT);
    pthread_mutex_unlock(&g_mu);

    int status;
    waitpid(pid, &status, WUNTRACED);

    if (isatty(STDIN_FILENO)) tcsetpgrp(STDIN_FILENO, getpgrp());

    pthread_mutex_lock(&g_mu);
    if (WIFSTOPPED(status)) {
        g_jobs[slot].state = JOB_STOPPED;
        printf("\n[%d]+ Stopped %s\n", id, g_jobs[slot].command);
        g_last_exit_code = 128 + WSTOPSIG(status);
    } else {
        g_jobs[slot].state = JOB_EMPTY;
        g_last_exit_code   = WIFEXITED(status)
            ? WEXITSTATUS(status)
            : 128 + WTERMSIG(status);
    }
    pthread_mutex_unlock(&g_mu);
}

void jobs_bg(int id) {
    pthread_mutex_lock(&g_mu);
    int slot = -1;
    for (int i = 0; i < MAX_JOBS; i++) {
        if (g_jobs[i].state != JOB_EMPTY && g_jobs[i].id == id) {
            slot = i; break;
        }
    }
    if (slot == -1) {
        printf("bg: job %d not found\n", id);
        pthread_mutex_unlock(&g_mu);
        return;
    }
    kill(-g_jobs[slot].pgid, SIGCONT);
    g_jobs[slot].state = JOB_RUNNING;
    printf("[%d]+ %s &\n", id, g_jobs[slot].command);
    pthread_mutex_unlock(&g_mu);
}
