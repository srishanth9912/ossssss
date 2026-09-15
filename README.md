# 🎓 StudentOS

[![C11](https://img.shields.io/badge/Language-C11-blue.svg)](https://en.wikipedia.org/wiki/C11_(C_standard_revision))
[![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20WSL-orange.svg)](https://ubuntu.com/wsl)
[![Standard](https://img.shields.io/badge/Standard-POSIX.1--2008-green.svg)](https://pubs.opengroup.org/onlinepubs/9699919799/)
[![Curriculum](https://img.shields.io/badge/Curriculum-ShellForge%2012--Week-purple.svg)](#-shellforge-curriculum-roadmap)
[![License](https://img.shields.io/badge/License-MIT%20%2F%20Educational-lightgrey.svg)](#-license)

**StudentOS** is a compact Unix-style shell and Operating Systems learning lab written in **C11**. It combines a real POSIX interactive shell (supporting pipelines, file redirections, signal isolation, and background job control) with an integrated suite of student productivity tools and OS kernel diagnostic experiments.

---

## 📑 Table of Contents

- [✨ At a Glance (For Beginners)](#-at-a-glance-for-beginners)
- [🎯 Core Features & Use Cases](#-core-features--use-cases)
- [🗺️ Project Architecture & Map](#️-project-architecture--map)
- [📚 ShellForge Curriculum Roadmap](#-shellforge-curriculum-roadmap)
- [⚡ Quick Start Guide](#-quick-start-guide)
- [📖 Complete Command Reference](#-complete-command-reference)
- [🔬 Operating Systems Concepts in Action](#-operating-systems-concepts-in-action)
- [🧪 Testing & Quality Assurance](#-testing--quality-assurance)
- [🛡️ Supported Syntax & Educational Scope](#-supported-syntax--educational-scope)
- [📄 License](#-license)

---

## ✨ At a Glance (For Beginners)

### What is StudentOS?
When you open a terminal on Linux or macOS, you are using a shell (like `bash` or `zsh`) that reads your commands, creates child processes (`fork()`), runs programs (`execvp()`), and connects them with pipes (`pipe()`). 

**StudentOS is that exact same system built from scratch in C**, without any third-party frameworks. It is designed to be small, clean, readable, and educational.

```text
=================================
        Welcome to StudentOS
   A Student-Centric Unix Shell
=================================
StudentOS > echo "Hello Operating Systems" | tr a-z A-Z
HELLO OPERATING SYSTEMS
StudentOS > calc (25 + 75) * 4
Expression : (25 + 75) * 4
Result     : 400
StudentOS > memstat
Virtual Memory Statistics (memstat)
--------------------------------------------------
Total Physical Memory :    8192000 kB (8000.00 MB)
Used Physical Memory  :    3150000 kB ( 38.45%)
--------------------------------------------------
StudentOS > exit
```

---

## 🎯 Core Features & Use Cases

### 1. 🎓 Student Productivity Suite
Organize your academic life without leaving your terminal. All records persist across sessions in `~/.studentos/`.
- **Notes (`notes`)**: Interactive or one-line study notes (`notes add "Study pipes"`, `notes list`, `notes clear`).
- **Assignment Tracker (`assignment`)**: Track deadlines and completion status (`assignment add "Lab 3" --due 30-09-2026`, `assignment done 1`).
- **Weekly Timetable (`timetable`)**: Manage your lecture schedule by day, time, and subject.

### 2. 🧮 Built-in Math & C Development Toolchain
- **Recursive-Descent Calculator (`calc` / `calculator`)**: Evaluates arithmetic expressions with parentheses and operator precedence (`+`, `-`, `*`, `/`) and zero-division protection.
- **In-Shell C Compiler & Runner (`compile`, `run`, `test`)**: Direct, safe `gcc` driver via `fork()`/`execvp()` without shell-injection risk. `test file.c` compiles to a temporary binary, executes it, and cleans it up in one step.

### 3. ⚙️ Operating Systems Lab & Kernel Introspection
- **Virtual Memory Inspector (`memstat`)**: Directly parses the Linux `/proc/meminfo` virtual filesystem to compute physical memory usage, cache, and swap statistics.
- **System Information (`sysinfo`)**: Queries the Linux kernel using the POSIX `uname()` system call.
- **Multithreading & Concurrency Demo (`threads [N]`)**: Spawns $N$ concurrent POSIX worker threads with `pthread_mutex_t` synchronization to demonstrate race condition prevention.

### 4. 🐚 Full POSIX Unix Shell Engine
- **Arbitrary Pipelines (`|`)**: Chain output and input across multiple commands (`cat file | grep pattern | wc -l`).
- **I/O Redirections**: File input (`<`), output overwrite (`>`), output append (`>>`), error redirection (`2>`), and stderr-to-stdout merging (`2>&1`).
- **Process Group Job Control (`&`, `jobs`, `fg`, `bg`)**: Run background jobs and multi-process pipelines. Uses an asynchronous `pthread` monitor to reap completed processes non-blockingly.
- **Signal Protection**: The parent shell safely ignores `Ctrl+C` (`SIGINT`) and `Ctrl+Z` (`SIGTSTP`), while child processes restore default signal dispositions.
- **Exit Code Tracking (`$?`)**: Inspect the exit status of the previous foreground process via `echo $?`.

---

## 🗺️ Project Architecture & Map

### How a Command Flows Through StudentOS

```text
 User Input String
        |
        v
 +--------------+
 | Tokenizer    |  Splits input respecting quotes ("...", '...'),
 | (main.c)     |  whitespace, and operators (|, <, >, >>, 2>, 2>&1, &)
 +-------+------+
         |
         v
 +--------------+
 | Parser       |  Constructs ParsedCommand structures for each stage
 | (main.c)     |  and binds input/output redirection files
 +-------+------+
         |
         +-----------------------------+
         |                             |
  Single Built-in / Student Tool?      Multi-Stage Pipeline or External?
         |                             |
         v                             v
 +---------------+             +---------------+
 | Parent Exec   |             | Pipe Setup    |  Allocates 2 * (N - 1) FDs
 | (save/restore |             +-------+-------+
 |  STDIN/OUT)   |                     |
 +---------------+                     v
                               +---------------+
                               | fork() Stages |  setpgid() creates process group
                               +-------+-------+  signals_reset_child() restores SIG_DFL
                                       |          execvp() replaces child images
                                       v
                     +-----------------+-----------------+
                     |                                   |
              Foreground (& not set)              Background (& set)
                     |                                   |
                     v                                   v
             +---------------+                   +---------------+
             | tcsetpgrp()   |                   | jobs_add()    |
             | waitpid() loop|                   | Monitor Thread|
             | reclaim tty   |                   | reaps (WNOHANG|
             +---------------+                   +---------------+
```

---

### 📂 Directory & File Map

```text
.
├── Makefile                     # Build system with all, clean, run, test, and check targets
├── README.md                    # Project documentation & user guide
├── notes.txt                    # Local scratchpad notes
│
├── include/                     # Public C headers shared across modules
│   ├── vector.h                 # Resizable dynamic string array (StringVector)
│   ├── student_string.h         # Auto-expanding dynamic string buffer (DynamicString)
│   ├── jobs.h                   # Job table, states, process groups, and monitor API
│   ├── signals.h                # POSIX signal configuration (parent ignore / child reset)
│   ├── history.h                # Persistent command history API
│   └── student_commands.h       # Student utilities and OS lab dispatcher API
│
├── src/                         # Core C11 source implementations
│   ├── main.c                   # Shell REPL, tokenizer, parser, pipeline execution engine
│   ├── vector.c                 # StringVector memory management (malloc, realloc, free)
│   ├── student_string.c         # DynamicString buffer expansion and appending
│   ├── jobs.c                   # Threaded background job monitor & process-group job control
│   ├── signals.c                # Signal dispositions (sigaction, SA_RESTART)
│   ├── history.c                # History logger (~/.studentos/history.db)
│   └── student_commands.c       # Notes, assignments, timetable, calc, memstat, sysinfo, threads
│
├── tests/                       # Automated regression testing
│   ├── test_shell.sh            # 14-point automated test harness in Bash
│   └── hello.c                  # Fixture program used for compiler testing
│
└── data/                        # Local fallback storage directory
```

---

## 📚 ShellForge Curriculum Roadmap

StudentOS is structured in 100% compliance with the **12-Week ShellForge OS Curriculum**:

| Week | Module · CO | Chapter Title | ShellForge Milestone | Implemented In |
| :---: | :---: | :--- | :--- | :--- |
| **1** | `M1·CO1` | The Machine Beneath the Prompt | REPL loop, repo, Makefile | [src/main.c](src/main.c), [Makefile](Makefile) |
| **2** | `M1·CO1` | The C Toolchain & Memory Model | Dynamic string/vector types | [src/vector.c](src/vector.c), [src/student_string.c](src/student_string.c) |
| **3** | `M2·CO2` | The Parser | Tokenizer + pipeline AST | [src/main.c](src/main.c) (`tokenize_input`) |
| **4** | `M2·CO2` | Processes & Process Control | Run one command (`fork`/`exec`/`wait`) | [src/main.c](src/main.c), [src/student_commands.c](src/student_commands.c) |
| **5** | `M3·CO3` | fork / exec / wait in Anger | PATH lookup, built-ins, exit codes (`$?`) | [src/main.c](src/main.c) (`g_last_exit_code`) |
| **6** | `M3·CO3` | Signals & Async Control | Ctrl-C/Z isolation, `SIGCHLD` handling | [src/signals.c](src/signals.c) (`sigaction`) |
| **7** | `M3·CO3` | Pipes & Plumbing | Arbitrary-length multi-stage pipelines | [src/main.c](src/main.c) (`pipefds` & `dup2`) |
| **8** | `M4·CO4` | Virtual Memory | Valgrind-clean allocations; `memstat` | [src/student_commands.c](src/student_commands.c) (`/proc/meminfo`) |
| **9** | `M5·CO5` | Redirection & File Abstraction | Full I/O redirection (`<`, `>`, `>>`, `2>`, `2>&1`) | [src/main.c](src/main.c) (`setup_redirection`) |
| **10** | `M6·CO6` | Concurrency I — Mutual Exclusion | Threaded job monitor + mutex container | [src/jobs.c](src/jobs.c) (`pthread_mutex_t`) |
| **11** | `M6·CO6` | Concurrency II — Deadlock & Jobs | Complete job control (`jobs`, `fg`, `bg`, `&`) | [src/jobs.c](src/jobs.c) (`tcsetpgrp`, `kill`) |
| **12** | `All` | Polish, Hosting & Ascent | Automated test suite, documentation, repo | [tests/test_shell.sh](tests/test_shell.sh), [README.md](README.md) |

---

## ⚡ Quick Start Guide

### 1. Requirements
StudentOS requires a POSIX environment:
- **Linux** (Ubuntu, Debian, Fedora, Arch, etc.) or **WSL (Windows Subsystem for Linux)**.
- `gcc` or `clang` (C11 support)
- GNU `make`
- `bash` (for running the automated test suite)
- `valgrind` (optional, for memory checking)

On Ubuntu/Debian/WSL:
```bash
sudo apt update
sudo apt install build-essential valgrind
```

### 2. Clone & Build
```bash
git clone https://github.com/srishanth9912/ossssss.git
cd ossssss
make
```

### 3. Launch StudentOS
```bash
./studentos
```

---

## 📖 Complete Command Reference

### Shell Built-ins
| Command | Syntax | Description | Example |
| :--- | :--- | :--- | :--- |
| `cd` | `cd [dir]` | Change working directory (defaults to `$HOME`) | `cd /tmp` |
| `pwd` | `pwd` | Print current working directory | `pwd` |
| `echo` | `echo [text...]` | Print text; supports `$?` for previous exit code | `echo "Status:" $?` |
| `history` | `history [clear]` | View past commands or clear the history file | `history` |
| `jobs` | `jobs` | List active, stopped, and completed background jobs | `jobs` |
| `fg` | `fg <id>` | Bring a background or stopped job to the foreground | `fg 1` |
| `bg` | `bg <id>` | Resume a stopped job in the background | `bg 1` |
| `help` | `help [cmd]` | Display the interactive help screen | `help notes` |
| `exit` | `exit [code]` | Clean up jobs and exit the shell | `exit 0` |

### Student Productivity Tools
| Command | Mode | Description | Example |
| :--- | :--- | :--- | :--- |
| `notes add` | Interactive | Prompts step-by-step for the note text | Type `notes add` & enter note |
| `notes add <text>` | One-line | Directly appends a note to database | `notes add "Exam on Friday"` |
| `notes list` | Display | Prints all saved notes | `notes list` |
| `notes clear` | Reset | Deletes all saved notes | `notes clear` |
| `assignment add` | Interactive | Prompts for title and due date | Type `assignment add` |
| `assignment add` | One-line | Adds assignment with optional deadline | `assignment add "OS Lab" --due 30-09-2026` |
| `assignment list` | Display | Shows formatted table of assignments & status | `assignment list` |
| `assignment done <id>` | Action | Marks an assignment as completed | `assignment done 1` |
| `timetable add` | Interactive | Prompts for Day, Time, and Subject | Type `timetable add` |
| `timetable add` | One-line | Adds a class schedule entry | `timetable add Monday 09:00 "OS"` |
| `timetable list` | Display | Displays the weekly class schedule | `timetable list` |

### Math & System Tools
| Command | Description | Example |
| :--- | :--- | :--- |
| `calc <expr>` | Evaluates math expressions (`+`, `-`, `*`, `/`, `( )`) | `calc (50 + 25) * 2` |
| `compile <file.c> [bin]` | Compiles a C source file using GCC | `compile tests/hello.c hello` |
| `run <bin> [args...]` | Executes a compiled program directly | `run ./hello` |
| `test <file.c>` | Compiles, runs, and deletes a temporary C test binary | `test tests/hello.c` |
| `files list [path]` | Lists directory contents with directory markers | `files list .` |
| `files create <file>` | Creates a new empty file | `files create new.txt` |
| `files read <file>` | Prints file contents to the terminal | `files read new.txt` |
| `files delete <file>` | Deletes a file | `files delete new.txt` |
| `files info <file>` | Displays file size in bytes and file type | `files info README.md` |
| `memstat` | Displays physical RAM, available memory, and swap usage | `memstat` |
| `sysinfo` | Displays OS name, release, machine architecture | `sysinfo` |
| `threads [N]` | Demonstrates mutex synchronization across $N$ worker threads | `threads 4` |

---

## 🔬 Operating Systems Concepts in Action

| OS Concept | Low-Level POSIX System Call | How StudentOS Uses It |
| :--- | :--- | :--- |
| **Process Creation** | `fork()` | Clones the shell process to execute new commands. |
| **Program Execution** | `execvp()` | Replaces the child process memory image with the target binary. |
| **Process Waiting** | `waitpid()` | Reaps child processes and retrieves their exit codes. |
| **Inter-Process Comm.** | `pipe()` | Creates unidirectional data channels connecting pipeline stages. |
| **File Redirection** | `dup2()`, `open()` | Duplicates file descriptors to standard streams (`STDIN`, `STDOUT`, `STDERR`). |
| **Process Groups** | `setpgid()`, `tcsetpgrp()` | Groups pipeline children together to manage terminal foreground ownership. |
| **Signal Handling** | `sigaction()` | Disables interrupts on the parent shell and restores defaults in children. |
| **Multithreading** | `pthread_create()`, `pthread_join()` | Runs the asynchronous job monitor thread and concurrency demo. |
| **Mutual Exclusion** | `pthread_mutex_lock()`, `pthread_mutex_unlock()` | Protects shared data (the job table and thread counter) against race conditions. |
| **Kernel Introspection** | `fopen("/proc/meminfo")`, `uname()` | Reads Linux virtual filesystem metrics and system kernel parameters. |

---

## 🧪 Testing & Quality Assurance

### Run the Automated Test Suite
StudentOS includes an automated test harness covering all built-ins, pipeline stages, redirection operators, student utilities, and job control:

```bash
make test
```

### Strict Compilation Check
Rebuilds the entire codebase treating all warnings as errors (`-Werror`):
```bash
make check
```

### Valgrind Memory Leak Test
Ensure all heap-allocated vectors, dynamic strings, and pipeline descriptors are freed without memory leaks:
```bash
printf 'echo hello\nexit\n' | valgrind --leak-check=full --error-exitcode=1 ./studentos
```

---

## 🛡️ Supported Syntax & Educational Scope

StudentOS is intentionally crafted as an operating systems learning platform.

### Supported Syntax:
- Quoted strings with spaces: `"string with spaces"` or `'literal text'`
- Pipelines of arbitrary length: `cmd1 | cmd2 | cmd3 | cmd4`
- Multi-mode redirection: `<`, `>`, `>>`, `2>`, `2>&1`
- Background execution: `command &` or `cmd1 | cmd2 &`
- Exit code inspection: `echo $?`

### Intentionally Excluded (To Keep Code Clean & Readable):
- Complex shell scripting syntax (e.g., `for`, `while`, `if/else`, function definitions).
- Shell variable expansions (other than `$?`).
- Globbing / wildcard expansions (e.g., `*.c`).
- Command substitutions (e.g., `$(date)`).

---

## 📄 License

This project is created for educational and operating-systems study purposes. Feel free to use, modify, and learn from it.
