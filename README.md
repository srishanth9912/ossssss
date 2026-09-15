# StudentOS

StudentOS is a small educational Unix-style shell written in C11 for Linux, WSL, or another POSIX environment. It is designed as a first-year systems project: the source is deliberately compact, but it uses real POSIX primitives including `fork`, `execvp`, `pipe`, `dup2`, process groups, signals, and pthreads.

## Build and test

Requirements: GCC or Clang, `make`, Bash, POSIX threads, and a POSIX-compatible runtime.

```bash
make
make test
./studentos
```

For a memory check on Linux/WSL:

```bash
valgrind --leak-check=full --error-exitcode=1 ./studentos
```

## What is implemented

- Interactive REPL with `cd`, `pwd`, `echo`, `history`, `jobs`, `fg`, `bg`, `help`, and `exit`.
- Tokenization for single/double quotes and `|`, `<`, `>`, `>>`, `2>`, `2>&1`, and final `&`.
- Arbitrary-length pipelines, standard redirection, and external commands through `execvp`.
- Foreground and background process groups. A background pipeline is tracked and reaped as one job.
- Signal setup so child processes receive normal Ctrl-C/Ctrl-Z behavior.
- A mutex-protected monitor thread for completed background jobs.
- Student utilities: notes, assignments, timetable, calculator, compiler helper, file helper, `memstat`, `sysinfo`, and a mutex demonstration.

## Deliberate limits

This is an educational shell, not Bash. It does not implement variable expansion (other than `echo $?`), command substitution, globbing, semicolons, escape sequences, `&&`/`||`, or complete POSIX job-control semantics. Unterminated quotes and redirects without a file are rejected with a clear syntax error.

## Project layout

```text
include/             public C headers
src/main.c           REPL, tokenizer, parser, redirection, process execution
src/jobs.c           process-group job tracking and monitor thread
src/signals.c        parent/child signal configuration
src/vector.c         resizable string-vector implementation
src/student_string.c dynamic string implementation
src/student_commands.c student-facing utility commands
src/history.c        persistent history helper
tests/test_shell.sh  Bash end-to-end regression tests
```

## Example commands

```text
echo "hello world" | tr a-z A-Z
wc -l < README.md
ls > listing.txt 2>&1
sleep 5 &
jobs
fg 1
notes add "revise process groups"
calculator (20 + 5) * 4
```

## Safety notes

The `files delete`, `compile`, `run`, and `test` utilities act on paths or programs supplied by the user. Use them only with files and commands you intend to access. The shell runs external commands with your current user permissions.
