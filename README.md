# StudentOS

**StudentOS** is a full Unix-style interactive shell built from scratch in C11. It is a 12-week operating systems learning project that implements real low-level POSIX concepts — process creation, inter-process communication through pipes, file descriptor redirection, signal handling, and multithreaded background job control — alongside a suite of student productivity tools built directly into the shell.

The project is designed to be small, readable, and educational. Every feature in StudentOS corresponds to a specific operating systems concept taught in the course, implemented using the actual system calls that a production shell would use.

> **Platform:** Linux or WSL (Windows Subsystem for Linux). The shell requires a POSIX-compatible environment and will not run natively on Windows Command Prompt or PowerShell.

---

## Table of Contents

1. [What is StudentOS and Why it Exists](#1-what-is-studentos-and-why-it-exists)
2. [What the Project Covers](#2-what-the-project-covers)
3. [Project Structure](#3-project-structure)
4. [How the Shell Works Internally](#4-how-the-shell-works-internally)
5. [ShellForge Curriculum — Week by Week](#5-shellforge-curriculum--week-by-week)
6. [Quick Start](#6-quick-start)
7. [Shell Built-in Commands](#7-shell-built-in-commands)
8. [Student Productivity Tools](#8-student-productivity-tools)
9. [OS Inspection and Developer Tools](#9-os-inspection-and-developer-tools)
10. [Pipelines, Redirection, and Job Control](#10-pipelines-redirection-and-job-control)
11. [Data Storage](#11-data-storage)
12. [Testing](#12-testing)
13. [What the Shell Does Not Support](#13-what-the-shell-does-not-support)
14. [Development Guidelines](#14-development-guidelines)

---

## 1. What is StudentOS and Why it Exists

When you open a terminal on any Linux or macOS machine and type a command like `ls | grep .c`, the shell you are using — bash, zsh, or sh — does the following:

1. Reads the line you typed.
2. Breaks it into tokens (words and operators).
3. Creates a child process using `fork()`.
4. Connects the child's stdin/stdout to a pipe using `dup2()`.
5. Replaces the child's process image with the program using `execvp()`.
6. Waits for the child to finish using `waitpid()`.

All of that happens invisibly, every time you press Enter. StudentOS makes the entire process **visible in code** by building that same system from scratch in clean, commented C11.

The goal is not to replace bash. The goal is to understand exactly what bash is doing underneath.

---

## 2. What the Project Covers

| Area | OS Concept | Implementation |
|------|-----------|---------------|
| Memory management | Dynamic strings, resizable arrays, heap allocation and ownership | `vector.c`, `student_string.c` |
| Process control | `fork()`, `execvp()`, `waitpid()`, exit status codes | `main.c` |
| Parsing | Tokenizer, quoted arguments, multi-stage pipeline construction | `main.c` |
| File descriptors | `open()`, `dup2()`, stdin/stdout/stderr redirection | `main.c` |
| Signals | Parent shell ignores SIGINT/SIGTSTP; children restore defaults | `signals.c` |
| Concurrency | POSIX threads, mutex-protected shared job table | `jobs.c` |
| Job control | Process groups, `jobs`, `fg`, `bg`, background pipelines | `jobs.c` |
| Kernel introspection | Reading `/proc/meminfo`, calling `uname()` | `student_commands.c` |
| Persistent storage | File-based notes, assignment, and timetable databases | `student_commands.c`, `history.c` |

---

## 3. Project Structure

```
ossssss/
│
├── Makefile                    Build system — compile, test, clean, strict-check
├── README.md                   This file
├── notes.txt                   Local developer notes
│
├── include/                    Public header files (API declarations)
│   ├── vector.h                StringVector — resizable array of heap-allocated C strings
│   ├── student_string.h        DynamicString — auto-growing character buffer
│   ├── jobs.h                  Job states, job table constants, background job API
│   ├── signals.h               Signal setup API for parent shell and child processes
│   ├── history.h               Command history persistence API
│   └── student_commands.h      Student utility dispatcher (is_student_command, handle_student_command)
│
├── src/                        All C11 source files
│   ├── main.c                  The REPL loop, tokenizer, parser, pipeline engine, built-in execution
│   ├── vector.c                StringVector — malloc, realloc, push, free
│   ├── student_string.c        DynamicString — create, append character/string, free
│   ├── jobs.c                  Background job table, pthread monitor, fg/bg/jobs implementation
│   ├── signals.c               sigaction setup — parent ignores, child resets to SIG_DFL
│   ├── history.c               Append/list/clear command history at ~/.studentos/history.db
│   └── student_commands.c      notes, assignment, timetable, calc, compile/run/test,
│                                 files, memstat, sysinfo, threads
│
├── tests/
│   ├── test_shell.sh           Automated bash test harness (14 regression tests)
│   └── hello.c                 Small C program used as a fixture in compile/run/test tests
│
└── data/                       Local fallback directory for database files when $HOME is unavailable
    ├── history.db
    ├── notes.db
    ├── assignments.db
    └── timetable.db
```

### What each file actually does

**`src/main.c`** — This is the heart of the shell. It contains:
- The REPL (read-eval-print loop) that repeatedly prints the `StudentOS >` prompt, reads a line, processes it, and loops.
- `tokenize_input()` — a character-by-character scanner that correctly handles single and double quotes, whitespace, and multi-character operators like `>>` and `2>&1`.
- The pipeline parser that builds an array of `ParsedCommand` structures, one per stage.
- `setup_redirection()` — opens files and rewires stdin/stdout/stderr using `dup2()`.
- The `fork()` loop that spawns each stage of a pipeline, connects them with `pipe()` file descriptors, and manages process groups.
- Foreground and background execution paths and exit code tracking via `g_last_exit_code`.

**`src/vector.c`** — Implements `StringVector`, a dynamically growing array of `char*` strings. Used by the tokenizer to accumulate tokens. Doubles in capacity when full. Every string pushed into the vector is owned by the vector and freed when `vector_free()` is called.

**`src/student_string.c`** — Implements `DynamicString`, a growable character buffer. Used during tokenization to accumulate characters one at a time while scanning an input line. Doubles in capacity using `realloc()` when needed.

**`src/jobs.c`** — Manages background job control. Contains:
- A static job table of up to 64 jobs, each identified by process group ID.
- A background `pthread` monitor thread that calls `waitpid(WNOHANG)` every 100ms to reap finished background pipelines without blocking the shell.
- `pthread_mutex_t` protection on all reads and writes to the shared job table.
- `jobs_fg()` — transfers terminal control to a job using `tcsetpgrp()`, waits for it to finish, then reclaims the terminal.
- `jobs_bg()` — sends `SIGCONT` to a stopped process group.

**`src/signals.c`** — Sets up signal handling for the parent shell using `sigaction()`. The parent ignores `SIGINT`, `SIGTSTP`, `SIGQUIT`, `SIGTTOU`, and `SIGTTIN` so `Ctrl+C` and `Ctrl+Z` only affect child processes. Before each child calls `execvp()`, `signals_reset_child()` restores all handlers to `SIG_DFL` so the child behaves normally.

**`src/history.c`** — Appends every executed command line to `~/.studentos/history.db` using plain text file I/O. `history list` reads and numbers each line. `history clear` truncates the file.

**`src/student_commands.c`** — All student-facing utilities are implemented here and dispatched through `handle_student_command()`. Each utility is a self-contained static function.

---

## 4. How the Shell Works Internally

Every line you type follows this path through the code:

```
User types a line and presses Enter
         │
         ▼
  history_add(input)         — saved to ~/.studentos/history.db
         │
         ▼
  tokenize_input(input)      — produces a StringVector of tokens
                               handles: "quoted text", 'quotes', |, <, >, >>, 2>, 2>&1, &
         │
         ▼
  Parse tokens               — detects pipeline stages separated by |
                               attaches redirection files to each ParsedCommand
                               detects trailing & for background execution
         │
         ├─── Is it a single built-in or student command (no pipeline, no &)?
         │         │
         │         ▼
         │    Run directly in the parent process
         │    Save stdout/stdin/stderr with dup(), apply redirection with dup2(),
         │    execute the command, restore the saved file descriptors
         │
         └─── Is it a pipeline or external command?
                   │
                   ▼
              Allocate pipe file descriptor pairs: pipe() × (stage_count − 1)
                   │
                   ▼
              for each stage: fork()
                   Child:  setpgid(0, pgid)         — join the process group
                           dup2() pipe ends          — wire stdin/stdout between stages
                           close all pipe FDs        — avoid descriptor leaks
                           setup_redirection()       — apply < > >> 2> 2>&1
                           signals_reset_child()     — restore SIG_DFL
                           execvp()                  — replace with the actual program
                   Parent: setpgid(pid, pgid)        — set group from parent side too
                   │
                   ▼
              Close all pipe ends in the parent
                   │
                   ├─── Foreground:
                   │         tcsetpgrp(STDIN, pgid)  — give terminal to child group
                   │         waitpid() for each stage
                   │         tcsetpgrp(STDIN, shell) — reclaim terminal
                   │
                   └─── Background (&):
                             jobs_add(pgid, stage_count, command)
                             pthread monitor reaps completion asynchronously
```

---

## 5. ShellForge Curriculum — Week by Week

StudentOS is built following the 12-week ShellForge Operating Systems curriculum. Each week's milestone is directly reflected in the codebase.

| Week | Chapter | Module | Milestone | Files Involved |
|------|---------|--------|-----------|----------------|
| 1 | The Machine Beneath the Prompt | M1·CO1 | Working REPL loop, repo, Makefile | `main.c`, `Makefile` |
| 2 | The C Toolchain & Memory Model | M1·CO1 | `DynamicString` and `StringVector` types | `student_string.c`, `vector.c` |
| 3 | The Parser | M2·CO2 | Tokenizer + multi-stage pipeline parser | `main.c` — `tokenize_input()` |
| 4 | Processes & Process Control | M2·CO2 | `fork()`, `execvp()`, `waitpid()` for one command | `main.c`, `student_commands.c` |
| 5 | fork / exec / wait in Anger | M3·CO3 | PATH lookup, built-ins, exit codes via `$?` | `main.c` — `g_last_exit_code` |
| 6 | Signals & Async Control | M3·CO3 | `SIGINT`/`SIGTSTP` isolation, `SIGCHLD` with `SA_RESTART` | `signals.c` |
| 7 | Pipes & Plumbing | M3·CO3 | Arbitrary-length pipelines with `pipe()` and `dup2()` | `main.c` — pipeline execution section |
| 8 | Virtual Memory | M4·CO4 | Valgrind-clean memory; `memstat` via `/proc/meminfo` | `student_commands.c` |
| 9 | Redirection & the File Abstraction | M5·CO5 | Full `<`, `>`, `>>`, `2>`, `2>&1` redirection | `main.c` — `setup_redirection()` |
| 10 | Concurrency I — Mutual Exclusion | M6·CO6 | Background `pthread` monitor + mutex-protected job table | `jobs.c` |
| 11 | Concurrency II — Deadlock & Jobs | M6·CO6 | Complete `jobs`, `fg`, `bg`, `&` with process groups | `jobs.c` |
| 12 | Polish, Hosting & Ascent | All | Regression test suite, documentation, hosted on GitHub | `tests/test_shell.sh`, `README.md` |

---

## 6. Quick Start

### Requirements

You need a Linux system or WSL. The following tools must be installed:

- GCC or Clang with C11 support
- GNU Make
- Bash (for running the test suite)
- POSIX threads library (comes with GCC on Linux)
- Valgrind (optional, for memory checking)

On Ubuntu, Debian, or WSL:

```bash
sudo apt update
sudo apt install build-essential valgrind
```

### Build

```bash
git clone https://github.com/srishanth9912/ossssss.git
cd ossssss
make
```

This produces the `studentos` executable and places `.o` object files in `obj/`.

### Run

```bash
./studentos
```

You will see:

```
=================================
        Welcome to StudentOS
   A Student-Centric Unix Shell
=================================
StudentOS >
```

### First Commands to Try

```bash
# Basic I/O
StudentOS > echo "Hello from StudentOS"
StudentOS > pwd
StudentOS > echo $?

# Pipelines
StudentOS > echo "hello world" | tr a-z A-Z
StudentOS > cat /etc/passwd | cut -d: -f1 | sort | head -5

# Redirection
StudentOS > echo "log entry" > log.txt
StudentOS > echo "second entry" >> log.txt
StudentOS > cat < log.txt

# Student tools
StudentOS > notes add "Study process scheduling"
StudentOS > notes list
StudentOS > calc (20 + 5) * 4
StudentOS > memstat

# Exit
StudentOS > exit
```

### Makefile Targets

| Target | Command | What it does |
|--------|---------|-------------|
| Default build | `make` | Compiles all sources into `studentos` |
| Run shell | `make run` | Builds and immediately launches the shell |
| Run tests | `make test` | Runs the automated regression suite |
| Strict build | `make check` | Rebuilds with `-Werror` (warnings become errors) |
| Clean | `make clean` | Removes `studentos`, `obj/`, and temp files |

---

## 7. Shell Built-in Commands

These commands are handled directly inside the shell process. They do not `fork()` a child.

| Command | Usage | Description |
|---------|-------|-------------|
| `cd` | `cd [directory]` | Change the current working directory. Without an argument, goes to `$HOME`. |
| `pwd` | `pwd` | Print the current working directory path. |
| `echo` | `echo [text ...]` | Print arguments separated by spaces. `echo $?` prints the last exit code. |
| `history` | `history` or `history clear` | List all saved commands, or clear the history file. |
| `jobs` | `jobs` | Display the background job table with ID, PGID, status, and command. |
| `fg` | `fg <job-id>` | Bring a background or stopped job to the foreground. |
| `bg` | `bg <job-id>` | Resume a stopped job and continue it in the background. |
| `help` | `help` or `help <command>` | Print the general help screen, or detailed help for a specific command. |
| `exit` | `exit [code]` or `quit [code]` | Terminate the shell, cleaning up the job monitor thread first. |

---

## 8. Student Productivity Tools

All tools persist their data in `~/.studentos/` across sessions. If `$HOME` is not available, data falls back to the `data/` directory in the project folder.

---

### Notes — `notes`

Take quick study notes and retrieve them later.

**Interactive mode** — prompts you for input:
```
StudentOS > notes add
Usage: notes add [text] | list | clear
Note : Study process scheduling for the exam
Note saved.
```

**One-line mode** — type everything in a single command:
```
StudentOS > notes add "Revise pipe() and dup2() before the viva"
Note saved.
```

**List all notes:**
```
StudentOS > notes list

Saved Notes
--------------------------------------------------
 1. Study process scheduling for the exam
 2. Revise pipe() and dup2() before the viva
--------------------------------------------------
```

**Clear all notes:**
```
StudentOS > notes clear
All notes cleared.
```

---

### Assignments — `assignment`

Track your homework, lab reports, and project deadlines.

**Interactive mode:**
```
StudentOS > assignment add
Title    : OS Shell Project Report
Due Date : 30-09-2026
Assignment added: [1] OS Shell Project Report (Due: 30-09-2026)
```

**One-line mode:**
```
StudentOS > assignment add "OS Shell Project Report" --due 30-09-2026
Assignment added: [1] OS Shell Project Report (Due: 30-09-2026)
```

**List all assignments:**
```
StudentOS > assignment list

Assignments
----------------------------------------------------------------------
ID   Title                            Due Date         Status
----------------------------------------------------------------------
1    OS Shell Project Report          30-09-2026       Pending
----------------------------------------------------------------------
```

**Mark an assignment as done:**
```
StudentOS > assignment done 1
Assignment #1 marked as done.
```

**Clear all assignments:**
```
StudentOS > assignment clear
All assignments cleared.
```

---

### Timetable — `timetable`

Manage your weekly class schedule.

**Interactive mode:**
```
StudentOS > timetable add
Day     : Monday
Time    : 09:00
Subject : Operating Systems
Class added: Monday at 09:00 - Operating Systems
```

**One-line mode:**
```
StudentOS > timetable add Monday 09:00 "Operating Systems"
Class added: Monday at 09:00 - Operating Systems
```

**List the timetable:**
```
StudentOS > timetable list

Weekly Class Timetable
--------------------------------------------------
Day          Time       Subject
--------------------------------------------------
Monday       09:00      Operating Systems
--------------------------------------------------
```

**Clear the timetable:**
```
StudentOS > timetable clear
Timetable cleared.
```

---

## 9. OS Inspection and Developer Tools

---

### Calculator — `calc` or `calculator`

A recursive-descent expression evaluator that correctly handles operator precedence and parentheses. Supports `+`, `-`, `*`, `/`, unary negation, and nested parentheses. Division by zero is detected and reported.

```
StudentOS > calc (20 + 5) * 4
Expression : (20 + 5) * 4
Result     : 100

StudentOS > calculator 100 / 8
Expression : 100 / 8
Result     : 12.5

StudentOS > calc -5 * (3 + 2)
Expression : -5 * (3 + 2)
Result     : -25
```

---

### Compiler and Runner — `compile`, `run`, `test`

These commands drive GCC directly using `fork()` and `execvp()`, so there is no shell injection and arguments are passed literally.

**Compile a C file:**
```
StudentOS > compile tests/hello.c hello
Compiling tests/hello.c -> hello ...
Compilation successful: ./hello
```

**Run a binary:**
```
StudentOS > run ./hello
Hello from StudentOS test program!
```

**One-step compile, run, and clean up** (the binary is deleted after execution):
```
StudentOS > test tests/hello.c
Hello from StudentOS test program!

Test process exited with status 0
```

---

### File Manager — `files`

Perform common file operations from inside the shell.

```
StudentOS > files list .
StudentOS > files list src/

StudentOS > files create output.txt
Created empty file: output.txt

StudentOS > files read output.txt

StudentOS > files info README.md

File Information: README.md
--------------------------------------------------
Size : 17408 bytes
Type : File
--------------------------------------------------

StudentOS > files delete output.txt
Deleted: output.txt
```

---

### Virtual Memory Statistics — `memstat`

Reads and parses `/proc/meminfo` directly to report memory usage. This only works on Linux systems.

```
StudentOS > memstat

Virtual Memory Statistics (memstat)
--------------------------------------------------
Total Physical Memory :   16384000 kB (16000.00 MB)
Used Physical Memory  :    6250000 kB ( 38.15%)
Free Physical Memory  :    4200000 kB
Available Memory      :   10134000 kB
Buffers / Cache       :     512000 kB / 5422000 kB
Swap Total / Free     :    4194304 kB / 4194304 kB
--------------------------------------------------
```

---

### System Information — `sysinfo`

Uses the POSIX `uname()` system call to display kernel and hardware details.

```
StudentOS > sysinfo

StudentOS System Information
--------------------------------------------------
OS Name    : Linux
Node Name  : my-machine
Release    : 5.15.0-91-generic
Version    : #101-Ubuntu SMP Tue Nov 14 13:30:08 UTC 2023
Machine    : x86_64
--------------------------------------------------
```

---

### Thread Concurrency Demo — `threads`

Spawns N POSIX worker threads (default 4, max 16). Each thread increments a shared counter 100,000 times protected by a `pthread_mutex_t`. This demonstrates that mutex locking prevents race conditions.

```
StudentOS > threads 4
Spawning 4 threads, each incrementing counter 100000 times...
Expected count: 400000
Actual count  : 400000
Result: Mutex synchronization successful (no race condition).
```

---

## 10. Pipelines, Redirection, and Job Control

---

### Pipelines

Use `|` to connect commands. The stdout of the left command becomes the stdin of the right command. Any number of stages are supported.

```bash
# Two-stage pipeline
cat /etc/passwd | grep root

# Three-stage pipeline
cat /etc/passwd | cut -d: -f1 | sort

# Four-stage pipeline
echo "hello world from studentos" | tr ' ' '\n' | sort | uniq
```

---

### Redirection

| Syntax | What it does |
|--------|-------------|
| `command > file` | Write stdout to file (creates or truncates) |
| `command >> file` | Append stdout to file |
| `command < file` | Read stdin from file |
| `command 2> file` | Write stderr to file |
| `command 2>&1` | Merge stderr into stdout |

```bash
# Write output to a file
echo "session log" > session.txt

# Append to a file
echo "continued log" >> session.txt

# Read from a file as stdin
wc -l < session.txt

# Capture compiler errors in a file
gcc missing_file.c 2> errors.txt

# Merge stderr into the pipeline
gcc missing_file.c 2>&1 | grep error
```

---

### Background Jobs

Append `&` to run a command in the background. The prompt returns immediately and you can continue using the shell. Background pipelines are tracked as a single job.

```bash
# Start a background job
StudentOS > sleep 30 &
[1] 12345

# View all background jobs
StudentOS > jobs

Active Jobs
----------------------------------------------------------------------
ID   PGID     Status       Command
----------------------------------------------------------------------
1    12345    Running      sleep 30 &
----------------------------------------------------------------------

# Bring job 1 to the foreground
StudentOS > fg 1

# Start a background pipeline
StudentOS > cat /dev/urandom | base64 | head -100 > random.txt &
[2] 12380

# Resume a stopped job in the background (after Ctrl+Z)
StudentOS > bg 1
```

---

### Exit Code Tracking

```bash
StudentOS > ls /nonexistent
ls: cannot access '/nonexistent': No such file or directory
StudentOS > echo $?
2

StudentOS > echo "hello"
hello
StudentOS > echo $?
0
```

---

## 11. Data Storage

StudentOS stores all persistent data in `~/.studentos/`. The directory is created automatically on first use.

```
~/.studentos/
├── history.db         One command per line, appended after each execution
├── notes.db           One note per line
├── assignments.db     Format: id|title|due_date|done_flag
└── timetable.db       Format: day|time|subject
```

If the `HOME` environment variable is not set, the `data/` directory in the project folder is used as a fallback.

---

## 12. Testing

### Automated Regression Suite

Run all 14 automated tests with:

```bash
make test
```

The test harness (`tests/test_shell.sh`) feeds command sequences to the shell through stdin and checks stdout for expected strings. It covers:

| Test | What It Verifies |
|------|-----------------|
| 1 | `echo` output and `$?` exit code display |
| 2 | Output redirection (`>`) and input redirection (`<`) |
| 3 | Append redirection (`>>`) |
| 4 | Three-stage arbitrary pipeline |
| 5 | Space preservation inside double quotes |
| 6 | Parser error on unmatched quote |
| 7 | Parser error on redirection with no file name |
| 8 | `notes add` one-line mode and `notes list` |
| 9 | `notes add` interactive mode |
| 10 | `assignment add` one-line mode and `assignment list` |
| 11 | `assignment add` interactive mode |
| 12 | `timetable add` one-line mode and `timetable list` |
| 13 | `timetable add` interactive mode |
| 14 | Background job launch produces `[1]` output |

### Strict Compilation Check

Rebuilds the project with `-Werror` so all warnings become compile errors:

```bash
make check
```

### Memory Leak Verification

```bash
make
printf 'echo hello\nexit\n' | valgrind --leak-check=full --error-exitcode=1 ./studentos
```

---

## 13. What the Shell Does Not Support

StudentOS implements the syntax and features described above. The following are deliberately not implemented, to keep the code clear and focused on the core OS concepts:

- Shell variables other than `echo $?`
- Wildcard/glob expansion such as `*.c` or `*.txt`
- Command substitution such as `$(date)` or `` `command` ``
- Escape sequences inside quotes such as `\n` or `\t`
- Command separators such as `;` or `&&` or `||`
- Shell scripting constructs such as `if`, `for`, `while`, or function definitions
- Tab-completion or command-line editing (no `readline` dependency)
- Full POSIX terminal job control semantics beyond what is documented above

The parser reports an error and discards the command when:
- A quote is opened but never closed.
- A redirection operator (`<`, `>`, `>>`, `2>`) has no filename following it.
- A pipeline contains an empty stage (e.g., `cmd1 | | cmd3`).

---

## 14. Development Guidelines

- Keep modules small. Each `.c` file has a single responsibility.
- Every `malloc`, `realloc`, `calloc`, and `strdup` call must check for `NULL` and clean up safely on failure.
- Every POSIX system call must check its return value. Failures must produce a descriptive `perror()` message.
- Add a test case to `tests/test_shell.sh` for every parser bug or execution edge case that is fixed.
- Do not commit the `studentos` binary, object files in `obj/`, or local database files. All of these are excluded in `.gitignore`.
- New modules must add their header to `include/` and their source to `SRC` in the `Makefile`.

---

*StudentOS — Built as part of the ShellForge 12-Week Operating Systems curriculum.*
