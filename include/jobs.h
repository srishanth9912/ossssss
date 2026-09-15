#ifndef JOBS_H
#define JOBS_H

/*
 * StudentOS — Threaded job monitor and job control
 * Week 10 (M6·CO6): Threaded job monitor + container
 * Week 11 (M6·CO6): Complete job control (fg, bg, &)
 *
 * A background pthread monitor thread periodically reaps finished jobs
 * without blocking the REPL. A mutex protects the shared job table.
 */

#include <sys/types.h>

#define MAX_JOBS 64

/* Job state machine */
typedef enum {
    JOB_EMPTY   = 0,  /* Slot is unused                              */
    JOB_RUNNING,      /* Process is running in the background         */
    JOB_STOPPED,      /* Process was stopped (Ctrl+Z / SIGTSTP)       */
    JOB_FG,           /* Currently moved to foreground — skip monitor */
    JOB_DONE          /* Process has exited (will be reaped on 'jobs') */
} JobState;

/* Initialise job table and launch the background monitor thread (Week 10).
 * Returns 0 on success, -1 on failure. */
int  jobs_init(void);

/* Shut down the monitor thread and release resources. */
void jobs_cleanup(void);

/* Register a new background job.
 * Returns the assigned job ID (>= 1), or -1 if the table is full. */
int  jobs_add(pid_t pid, pid_t pgid, const char *command);

/* Print the current job table; reaps finished jobs in the process. */
void jobs_list(void);

/* Bring job <id> to the foreground and wait for it (Week 11). */
void jobs_fg(int id);

/* Send SIGCONT to stopped job <id> and resume it in the background (Week 11). */
void jobs_bg(int id);

/* Non-blocking reap pass — called before printing the prompt. */
void jobs_reap(void);

#endif /* JOBS_H */
