# StudentOS

**StudentOS** is a compact Unix-style shell written in C11 as an operating-systems learning project. It provides a real interactive REPL, external-command execution, pipelines, file redirection, signal handling, background jobs, and a small collection of student-focused utilities.

It is intentionally small enough to study. The code favors explicit POSIX concepts—`fork()`, `execvp()`, `waitpid()`, `pipe()`, `dup2()`, process groups, signals, and pthread mutexes—over hidden framework behavior.

> **Platform:** Linux, WSL, or another POSIX-compatible environment. It is not intended to run natively on Windows Command Prompt or PowerShell.

## Contents

- [What the project demonstrates](#what-the-project-demonstrates)
- [Quick start](#quick-start)
- [How the shell works](#how-the-shell-works)
- [Project structure](#project-structure)
- [Command reference](#command-reference)
- [Pipelines, redirection, and jobs](#pipelines-redirection-and-jobs)
- [Testing and quality checks](#testing-and-quality-checks)
- [Supported behavior and limits](#supported-behavior-and-limits)

## What the project demonstrates

| Area | Concepts used in StudentOS |
| --- | --- |
| C foundations | Dynamic strings, resizable vectors, heap allocation, ownership, cleanup |
| Process control | `fork()`, `execvp()`, `waitpid()`, exit status, PATH lookup |
| Parsing | Quoted arguments, pipelines, redirection operators, background marker |
| File descriptors | `open()`, `close()`, `dup2()`, stdin/stdout/stderr redirection |
| Signals | Parent ignores interactive signals; children restore default behavior |
| Concurrency | pthread job-monitor thread, mutex-protected shared job table |
| Job control | Process groups, `jobs`, `fg`, `bg`, background pipelines |
| Linux inspection | `/proc/meminfo` through `memstat`, `uname()` through `sysinfo` |

## Quick start

### 1. Install requirements

Use a Linux terminal or WSL with the following tools installed:

- GCC or Clang with C11 support
- GNU Make
- Bash
- POSIX threads (normally included with the C toolchain)
- Optional: Valgrind for memory checking

On Ubuntu or Debian/WSL:

```bash
sudo apt update
sudo apt install build-essential valgrind
```

### 2. Build

```bash
git clone https://github.com/srishanth9912/ossssss.git
cd ossssss
make
```

The build creates the `studentos` executable and object files under `obj/`.

### 3. Start the shell

```bash
./studentos
```

Try a few commands:

```text
StudentOS > echo "Hello from StudentOS"
StudentOS > pwd
StudentOS > echo one two three | wc -w
StudentOS > exit
```

## How the shell works

Each command line follows this flow:

```text
user input
   |
   v
tokenizer: words, quotes, |, <, >, >>, 2>, 2>&1, &
   |
   v
parser: command stages + redirection settings
   |
   +--> built-in/student command: runs in the shell process when possible
   |
   +--> external command or pipeline: fork -> connect pipes -> redirect -> execvp
                                                        |
                                                        v
                                              wait / process-group job control
```

For a background pipeline, all child processes are placed in one process group. The job monitor records the group as one job and reaps every stage, avoiding orphaned pipeline children.

## Project structure

```text
.
├── Makefile                    # Build, test, clean, and strict compiler-check targets
├── README.md                   # Project guide and command reference
├── include/                    # Public headers shared between C modules
│   ├── vector.h                # Resizable vector of heap-allocated strings
│   ├── student_string.h        # Growable string type used by the tokenizer
│   ├── jobs.h                  # Job states and background-job API
│   ├── signals.h               # Parent/child signal setup API
│   ├── history.h               # Persistent command-history API
│   └── student_commands.h      # Student utility dispatcher API
├── src/
│   ├── main.c                  # REPL, tokenizer, parser, redirection, pipeline execution
│   ├── vector.c                # StringVector allocation, growth, and cleanup
│   ├── student_string.c        # DynamicString allocation and append operations
│   ├── jobs.c                  # Process-group jobs and mutex-protected monitor thread
│   ├── signals.c               # Signal dispositions for parent and child processes
│   ├── history.c               # History file storage in ~/.studentos/
│   └── student_commands.c      # Notes, assignments, timetable, calculator, files, and tools
├── tests/
│   ├── test_shell.sh           # End-to-end Bash regression suite
│   └── hello.c                 # Small C fixture used by manual testing
└── data/                       # Local sample/runtime data; not required for normal use
```

## Command reference

### Shell built-ins

| Command | What it does | Example |
| --- | --- | --- |
| `cd [directory]` | Changes the shell's working directory. Without an argument, uses `$HOME`. | `cd /tmp` |
| `pwd` | Prints the current working directory. | `pwd` |
| `echo [text...]` | Prints text. `echo $?` prints the previous command's exit code. | `echo $?` |
| `history [clear]` | Lists saved commands or clears the history file. | `history clear` |
| `jobs` | Lists background jobs and their state. | `jobs` |
| `fg <id>` | Brings a background/stopped job to the foreground. | `fg 1` |
| `bg <id>` | Continues a stopped job in the background. | `bg 1` |
| `help [command]` | Shows the available commands or help for a supported student command. | `help notes` |
| `exit [code]` / `quit [code]` | Exits StudentOS. | `exit 0` |

### Student utilities

| Command | What it does | Example |
| --- | --- | --- |
| `notes add <text>` | Saves a study note in `~/.studentos/notes.db`. | `notes add "Revise pipes"` |
| `notes list` / `notes clear` | Lists or clears notes. | `notes list` |
| `assignment add <text> [--due DATE]` | Saves an assignment with an optional due date. | `assignment add "Shell report" --due 30-09-2026` |
| `assignment list`, `assignment done <id>`, `assignment clear` | Manages saved assignments. | `assignment done 1` |
| `timetable add <day> <time> <subject>` | Adds a timetable entry. | `timetable add Monday 09:00 "Operating Systems"` |
| `timetable list` / `timetable clear` | Lists or clears the timetable. | `timetable list` |
| `calculator <expression>` | Evaluates `+`, `-`, `*`, `/`, and parentheses. | `calculator (20 + 5) * 4` |
| `compile <file.c> [output]` | Compiles a C file with GCC through `fork()` and `execvp()`. | `compile tests/hello.c hello` |
| `run <program> [args...]` | Runs a program without invoking a second shell. | `run ./hello` |
| `test <file.c>` | Compiles a C file to a temporary executable and runs it. | `test tests/hello.c` |
| `files list\|create\|read\|delete\|info <path>` | Performs a selected filesystem action. | `files info README.md` |
| `memstat` | Reads Linux memory information from `/proc/meminfo`. | `memstat` |
| `sysinfo` | Prints system information from `uname()`. | `sysinfo` |
| `threads [count]` | Runs a small mutex-protected pthread demonstration. | `threads 4` |

## Pipelines, redirection, and jobs

### Pipelines

Use `|` to pass stdout from one program to stdin of the next. Pipelines can contain more than two stages.

```bash
cat /etc/passwd | cut -d: -f1 | sort
echo test_pipeline_stream | tr '_' ' ' | grep test
```

### Redirection

| Syntax | Meaning |
| --- | --- |
| `command > file` | Write stdout to a new/truncated file. |
| `command >> file` | Append stdout to a file. |
| `command < file` | Read stdin from a file. |
| `command 2> file` | Write stderr to a file. |
| `command 2>&1` | Send stderr to the current stdout destination. |

```bash
echo "session started" > log.txt
echo "another line" >> log.txt
wc -l < log.txt
gcc missing.c 2> compiler-errors.txt
gcc missing.c 2>&1 | grep error
```

### Background jobs

Append `&` to run a command or pipeline in the background.

```bash
sleep 10 &
jobs
fg 1

printf "hello\n" | tr a-z A-Z &
jobs
```

## Testing and quality checks

The automated suite exercises the REPL, exit-code display, redirection, pipelines, quotes, parser failures, student utilities, Linux utilities, and background jobs.

```bash
make test
```

Before committing C changes, run the stricter build. It rebuilds the project with warnings promoted to errors.

```bash
make check
```

For a memory-focused run on Linux/WSL:

```bash
make
printf 'echo hello\nexit\n' | valgrind --leak-check=full --error-exitcode=1 ./studentos
```

## Supported behavior and limits

StudentOS is an educational shell, not a replacement for Bash. It supports the syntax documented above and deliberately does **not** implement:

- shell variables other than `echo $?`
- wildcard expansion such as `*.c`
- command substitution such as `$(date)`
- escape-sequence parsing inside quotes
- command separators (`;`), conditionals (`&&`, `||`), or shell scripts
- full POSIX terminal job-control semantics

The parser rejects unmatched quotes and redirection operators that have no file name. External commands run with the permissions of the user who starts StudentOS.

## Development notes

- Keep new modules small and add their headers to `include/`.
- Check every allocation and POSIX system call; failures must clean up safely.
- Add a regression case in `tests/test_shell.sh` for every parser or execution bug fixed.
- Do not commit `studentos`, `obj/`, or local runtime databases. `.gitignore` excludes these generated files.

## License

No license file is currently included. Add one before distributing or accepting outside contributions.
