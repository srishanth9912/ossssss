#ifndef SIGNALS_H
#define SIGNALS_H

/* Initialize signal handlers for parent shell (Week 6 Milestone) */
void setup_signals(void);

/* Reset signal dispositions in child processes before execvp */
void signals_reset_child(void);

#endif /* SIGNALS_H */
