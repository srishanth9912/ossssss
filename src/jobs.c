/* Thread-safe process-group based background job control. */
#include <stdatomic.h>
#include <stdbool.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "jobs.h"

extern int g_last_exit_code;

typedef struct {
    int id;
    pid_t pgid;
    int process_count;
    int finished_count;
    char command[512];
    JobState state;
    int exit_status;
} Job;

static Job g_jobs[MAX_JOBS];
static pthread_mutex_t g_mu = PTHREAD_MUTEX_INITIALIZER;
static pthread_t g_monitor;
static atomic_bool g_running = ATOMIC_VAR_INIT(false);
static int g_monitor_started;
static int g_next_id = 1;

/* Caller holds g_mu. A process group lets one job represent a whole pipeline. */
static void reap_locked(void) {
    for (int i = 0; i < MAX_JOBS; ++i) {
        Job *job = &g_jobs[i];
        if (job->state != JOB_RUNNING && job->state != JOB_STOPPED) continue;
        int status;
        while (waitpid(-job->pgid, &status, WNOHANG | WUNTRACED | WCONTINUED) > 0) {
            if (WIFEXITED(status) || WIFSIGNALED(status)) {
                job->finished_count++;
                job->exit_status = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
                if (job->finished_count >= job->process_count) job->state = JOB_DONE;
            } else if (WIFSTOPPED(status)) job->state = JOB_STOPPED;
            else if (WIFCONTINUED(status)) job->state = JOB_RUNNING;
        }
    }
}

static void *monitor_worker(void *unused) {
    (void)unused;
    while (atomic_load(&g_running)) {
        pthread_mutex_lock(&g_mu);
        reap_locked();
        pthread_mutex_unlock(&g_mu);
        usleep(100000);
    }
    return NULL;
}

int jobs_init(void) {
    pthread_mutex_lock(&g_mu);
    memset(g_jobs, 0, sizeof(g_jobs));
    g_next_id = 1;
    atomic_store(&g_running, true);
    pthread_mutex_unlock(&g_mu);
    if (pthread_create(&g_monitor, NULL, monitor_worker, NULL) != 0) {
        perror("pthread_create");
        atomic_store(&g_running, false);
        return -1;
    }
    g_monitor_started = 1;
    return 0;
}

void jobs_cleanup(void) {
    atomic_store(&g_running, false);
    if (g_monitor_started) {
        pthread_join(g_monitor, NULL);
        g_monitor_started = 0;
    }
}

int jobs_add(pid_t pgid, int process_count, const char *command) {
    if (pgid <= 0 || process_count <= 0) return -1;
    pthread_mutex_lock(&g_mu);
    int slot = -1;
    for (int i = 0; i < MAX_JOBS; ++i) {
        if (g_jobs[i].state == JOB_EMPTY || g_jobs[i].state == JOB_DONE) { slot = i; break; }
    }
    if (slot < 0) { fprintf(stderr, "studentos: job table full\n"); pthread_mutex_unlock(&g_mu); return -1; }
    Job *job = &g_jobs[slot];
    memset(job, 0, sizeof(*job));
    job->id = g_next_id++;
    job->pgid = pgid;
    job->process_count = process_count;
    job->state = JOB_RUNNING;
    snprintf(job->command, sizeof(job->command), "%s", command ? command : "?");
    printf("[%d] %d\n", job->id, (int)pgid);
    pthread_mutex_unlock(&g_mu);
    return job->id;
}

void jobs_reap(void) { pthread_mutex_lock(&g_mu); reap_locked(); pthread_mutex_unlock(&g_mu); }

void jobs_list(void) {
    jobs_reap();
    pthread_mutex_lock(&g_mu);
    printf("\nActive Jobs\n----------------------------------------------------------------------\n");
    printf("%-4s %-8s %-12s %-32s\n", "ID", "PGID", "Status", "Command");
    printf("----------------------------------------------------------------------\n");
    int count = 0;
    for (int i = 0; i < MAX_JOBS; ++i) {
        Job *job = &g_jobs[i];
        if (job->state == JOB_EMPTY) continue;
        const char *state = job->state == JOB_RUNNING ? "Running" : job->state == JOB_STOPPED ? "Stopped" : job->state == JOB_FG ? "Foreground" : "Done";
        printf("%-4d %-8d %-12s %-32.32s\n", job->id, (int)job->pgid, state, job->command);
        ++count;
        if (job->state == JOB_DONE) job->state = JOB_EMPTY;
    }
    if (!count) printf("No active jobs.\n");
    printf("----------------------------------------------------------------------\n\n");
    pthread_mutex_unlock(&g_mu);
}

void jobs_fg(int id) {
    pthread_mutex_lock(&g_mu);
    Job *job = NULL;
    for (int i = 0; i < MAX_JOBS; ++i) if (g_jobs[i].state != JOB_EMPTY && g_jobs[i].id == id) { job = &g_jobs[i]; break; }
    if (!job) { printf("fg: job %d not found\n", id); pthread_mutex_unlock(&g_mu); return; }
    pid_t pgid = job->pgid;
    int process_count = job->process_count;
    int completed = job->finished_count;
    job->state = JOB_FG;
    printf("%s\n", job->command);
    pthread_mutex_unlock(&g_mu);
    if (isatty(STDIN_FILENO)) tcsetpgrp(STDIN_FILENO, pgid);
    if (kill(-pgid, SIGCONT) < 0) perror("fg");
    int status, last_status = 1;
    while (completed < process_count && waitpid(-pgid, &status, WUNTRACED) > 0) {
        if (WIFSTOPPED(status)) break;
        if (WIFEXITED(status) || WIFSIGNALED(status)) { ++completed; last_status = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status); }
    }
    if (isatty(STDIN_FILENO)) tcsetpgrp(STDIN_FILENO, getpgrp());
    pthread_mutex_lock(&g_mu);
    g_last_exit_code = last_status;
    job->state = completed == process_count ? JOB_EMPTY : JOB_STOPPED;
    pthread_mutex_unlock(&g_mu);
}

void jobs_bg(int id) {
    pthread_mutex_lock(&g_mu);
    Job *job = NULL;
    for (int i = 0; i < MAX_JOBS; ++i) if (g_jobs[i].state != JOB_EMPTY && g_jobs[i].id == id) { job = &g_jobs[i]; break; }
    if (!job) { printf("bg: job %d not found\n", id); pthread_mutex_unlock(&g_mu); return; }
    if (kill(-job->pgid, SIGCONT) < 0) perror("bg"); else job->state = JOB_RUNNING;
    pthread_mutex_unlock(&g_mu);
}
