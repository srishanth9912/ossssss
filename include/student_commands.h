#ifndef STUDENT_COMMANDS_H
#define STUDENT_COMMANDS_H

/* Check if command is a student feature handled in-shell */
int is_student_command(const char *cmd);

/* Execute a student feature */
int handle_student_command(char **args, int argc);

/* Display full student help screen */
void print_student_help(void);

/* Display help for a specific command */
void print_command_help(const char *cmd);

#endif /* STUDENT_COMMANDS_H */
