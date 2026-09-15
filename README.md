# StudentOS - ShellForge Unix Shell

StudentOS is a modular, educational Unix-like command line shell written in pure C (C11 / POSIX). It strictly adheres to the 12-week **ShellForge** operating system milestone progression, combining foundational POSIX shell mechanics with CLI-only student productivity tools.

---

## 🏛️ Architecture & Milestone Compliance

StudentOS is 100% command-driven (zero menus or interactive prompts). Every feature is accessible through standard command-line syntax:

```
                  ┌─────────────────────────────────────────────────┐
                  │                 REPL Loop (main.c)              │
                  └────────────────────────┬────────────────────────┘
                                           │
                                           ▼
                  ┌─────────────────────────────────────────────────┐
                  │            Tokenizer (tokenizer.c)              │
                  │   Lexer: quotes (', "), operators (|, <, >, &)  │
                  └────────────────────────┬────────────────────────┘
                                           │
                                           ▼
                  ┌─────────────────────────────────────────────────┐
                  │              Parser (parser.c)                  │
                  │   Generates Pipeline AST (include/ast.h)        │
                  └────────────────────────┬────────────────────────┘
                                           │
                                           ▼
                  ┌─────────────────────────────────────────────────┐
                  │             Executor (executor.c)               │
                  │   • Builtins: in-process execution with dup2    │
                  │   • Pipelines: fork(), pipes, pgid, execvp()    │
                  │   • Redirection: <, >, >>, 2>, 2>&1             │
                  └──────────────┬──────────────────┬───────────────┘
                                 │                  │
                                 ▼                  ▼
┌──────────────────────────────────────┐     ┌──────────────────────────────────┐
│     Threaded Job Monitor (jobs.c)    │     │       Student CLI Utilities      │
│  • Dedicated monitor thread (pthread)│     │  • notes, assignments, timetable │
│  • Mutex-protected job container     │     │  • calculator, compiler, memstat │
│  • Signal handling (Ctrl+C / Ctrl+Z) │     │  • persistent in ~/.studentos/   │
└──────────────────────────────────────┘     └──────────────────────────────────┘
```

### ShellForge 12-Week Milestone Matrix

| Week | Chapter | ShellForge Milestone | Implementation in StudentOS |
| :---: | :--- | :--- | :--- |
| **1** | The Machine Beneath the Prompt | REPL loop, repo, Makefile | [src/main.c](src/main.c), [Makefile](Makefile): modular structure, robust prompt with cwd, clean build. |
| **2** | The C Toolchain & Memory Model | Dynamic string/vector types | [include/vector.h](include/vector.h), [src/vector.c](src/vector.c): safe `StringVector` and `StringBuilder`. |
| **3** | The Parser | Tokenizer + pipeline AST | [src/tokenizer.c](src/tokenizer.c), [src/parser.c](src/parser.c), [include/ast.h](include/ast.h): concrete `PipelineAST` with commands and redirections. |
| **4** | Processes & Process Control | Run one command (`fork`/`exec`/`wait`) | [src/executor.c](src/executor.c): `fork()`, `execvp()`, `waitpid()`, child signal reset (`SIG_DFL`). |
| **5** | fork / exec / wait in Anger | PATH, built-ins, exit codes | Builtins (`cd`, `pwd`, `exit`, `echo`, `history`, `help`), PATH search, `$?` exit code tracking. |
| **6** | Signals & Async Control | Ctrl-C/Z, `SIGCHLD` reaper | [src/signals.c](src/signals.c): `sigaction` handling, parent ignores interrupts, children restore them. |
| **7** | Pipes & Plumbing | Arbitrary-length pipelines | [src/executor.c](src/executor.c): $N$-stage pipelines (`cmd1 \| cmd2 \| ... \| cmdN`) with process groups. |
| **8** | Virtual Memory | Valgrind-clean; `memstat` | [src/memory.c](src/memory.c): `/proc/meminfo` virtual memory parser; verified zero leaks under Valgrind. |
| **9** | Redirection & File Abstraction | Full I/O redirection | [src/executor.c](src/executor.c): `<`, `>`, `>>`, `2>`, `2>&1` with clean `dup2` wiring. |
| **10** | Concurrency I — Mutual Exclusion | Threaded job monitor + container | [src/jobs.c](src/jobs.c): background `pthread` monitor polling jobs safely under `pthread_mutex_t`. |
| **11** | Concurrency II — Deadlock & Jobs | Complete job control | [src/jobs.c](src/jobs.c): `jobs`, `fg <id>`, `bg <id>`, `&` asynchronous execution, terminal handing (`tcsetpgrp`). |
| **12** | Polish, Hosting & Ascent | Tests, design doc, host it | [tests/test_shell.sh](tests/test_shell.sh): automated test suite covering all 12 milestones; clean documentation. |

---

## 🚀 Building & Running

### Requirements
- GCC (C11 support)
- POSIX-compliant environment (Ubuntu / Debian / WSL on Windows)
- Make
- pthreads (`-pthread`)

### Build
```bash
make
```

### Run
```bash
./studentos
```

### Run Automated Tests
```bash
make test
# or
bash tests/test_shell.sh ./studentos
```

### Valgrind Memory Leak Verification
```bash
valgrind --leak-check=full --error-exitcode=1 ./studentos
```

---

## 💻 Command Reference

### Shell & System
| Command | Description |
| :--- | :--- |
| `cd [dir]` | Change current working directory (defaults to `$HOME`) |
| `pwd` | Print current working directory |
| `echo [args...]` | Print arguments (supports `$?` for last command exit code) |
| `history [clear]` | View or clear shell command history |
| `jobs` | List active background jobs and their status |
| `fg <job-id>` | Bring background job to foreground |
| `bg <job-id>` | Resume stopped job in the background |
| `memstat` | View Linux virtual and physical memory statistics |
| `sysinfo` | View kernel, OS, and architecture details |
| `threads [n]` | Run POSIX threads mutual exclusion demo |
| `exit [code]` | Exit shell with optional status code |

### Student Productivity (CLI Only)
| Command | Description |
| :--- | :--- |
| `notes add <text>` | Add study note to persistent storage (`~/.studentos/notes.db`) |
| `notes list` | Display saved notes in formatted table |
| `notes clear` | Clear all saved notes |
| `assignment add <text> [--due DATE]` | Add assignment with deadline |
| `assignment list` | Display assignments with ID and status |
| `assignment done <id>` | Mark assignment as completed |
| `assignment clear` | Clear assignments |
| `timetable add <day> <time> <subject>` | Add class schedule entry |
| `timetable list` | Display weekly timetable |
| `timetable clear` | Clear timetable entries |
| `calculator <expression>` | Evaluate arithmetic expressions: `+`, `-`, `*`, `/`, `()` |
| `compile <file.c> [output]` | Compile C code using `gcc` via `fork`/`execvp` |
| `run <program> [args...]` | Execute binary program |
| `test <file.c>` | Compile and run temporary C test code |
| `files list\|create\|read\|delete\|info <path>` | File system management commands |

### Unix Pipelines & Redirections
```bash
# Arbitrary Pipelines
cat /etc/passwd | grep -v nobody | cut -d: -f1 | sort

# Output Redirection (Truncate)
ls -la > listing.txt

# Output Redirection (Append)
echo "entry" >> listing.txt

# Input Redirection
wc -l < listing.txt

# Error Redirection & Combining
gcc non_existent.c 2> errors.txt
gcc non_existent.c 2>&1 | grep "error"

# Background Execution
sleep 5 &
jobs
fg 1
```

---

## 📂 Codebase Layout

```
├── Makefile                # Build configuration (-pthread, -g, -Wall, -Wextra)
├── README.md               # Architecture documentation & milestone guide
├── include/                # Modular C headers
│   ├── ast.h               # Pipeline AST structures
│   ├── assignments.h       # Assignment tracker interface
│   ├── calculator.h        # Recursive descent arithmetic parser
│   ├── compiler.h          # C compiler & test runner
│   ├── executor.h          # AST execution, pipelines & redirections
│   ├── file_manager.h      # POSIX file manager
│   ├── help.h              # Help & CLI manual
│   ├── history.h           # Command history tracking
│   ├── jobs.h              # Threaded job monitor & container
│   ├── memory.h            # /proc/meminfo virtual memory reader
│   ├── notes.h             # Notes utility interface
│   ├── parser.h            # Token-to-AST parser
│   ├── signals.h           # Async signal handlers & child reset
│   ├── studentos.h         # Core REPL entry
│   ├── system_info.h       # POSIX uname inspection
│   ├── threads.h           # Thread synchronization demo
│   ├── timetable.h         # Class schedule interface
│   ├── tokenizer.h         # Shell lexer
│   ├── types.h             # Shell limits, JobState, and exit codes
│   └── vector.h            # Dynamic StringVector & StringBuilder
├── src/                    # C implementation files
│   ├── assignments.c
│   ├── ast.c
│   ├── calculator.c
│   ├── compiler.c
│   ├── executor.c
│   ├── file_manager.c
│   ├── help.c
│   ├── history.c
│   ├── jobs.c
│   ├── main.c
│   ├── memory.c
│   ├── notes.c
│   ├── parser.c
│   ├── signals.c
│   ├── system_info.c
│   ├── threads.c
│   ├── timetable.c
│   ├── tokenizer.c
│   └── vector.c
└── tests/
    ├── hello.c             # C test program
    └── test_shell.sh       # Automated milestone test suite
```
