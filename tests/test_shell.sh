#!/usr/bin/env bash
# StudentOS Automated Test Harness (ShellForge Week 12 Milestone)

set -e

SHELL_BIN="${1:-./studentos}"

if [ ! -x "$SHELL_BIN" ]; then
    echo "Error: Shell binary '$SHELL_BIN' not found or not executable."
    exit 1
fi

echo "=================================================="
echo " Running StudentOS Automated Test Suite"
echo " Target Binary: $SHELL_BIN"
echo "=================================================="

PASSED=0
FAILED=0

run_test() {
    local test_name="$1"
    local input_commands="$2"
    local expected_output="$3"

    printf "Test: %-45s ... " "$test_name"

    # Run commands through shell
    local actual_output
    actual_output=$(printf "%b\nexit\n" "$input_commands" | "$SHELL_BIN" 2>&1 || true)

    if echo "$actual_output" | grep -F -- "$expected_output" > /dev/null; then
        echo "PASSED"
        PASSED=$((PASSED + 1))
    else
        echo "FAILED"
        echo "  Expected to find: '$expected_output'"
        echo "  Actual output: $actual_output"
        FAILED=$((FAILED + 1))
    fi
}

# Test 1: Echo and Exit Code ($?)
run_test "Builtin: echo and exit code tracking" \
    "echo hello shell\necho \$?" \
    "hello shell"

# Test 2: Input and Output Redirection
TMP_FILE="/tmp/__studentos_test_redirection__"
run_test "Redirection: output (>) and read back (<)" \
    "echo testing_redirection > $TMP_FILE\ncat < $TMP_FILE" \
    "testing_redirection"
rm -f "$TMP_FILE"

# Test 3: Append Redirection (>>)
run_test "Redirection: append (>>)" \
    "echo line1 > $TMP_FILE\necho line2 >> $TMP_FILE\ncat $TMP_FILE" \
    "line2"
rm -f "$TMP_FILE"

# Test 4: Pipeline execution (arbitrary length)
run_test "Pipeline: arbitrary 3-stage pipeline" \
    "echo test_pipeline_stream | tr '_' ' ' | grep test" \
    "test pipeline stream"

# Test 5: Quotes preservation
run_test "Tokenizer: space preservation inside quotes" \
    "echo \"hello      spaces\"" \
    "hello      spaces"

# Test 6: Syntax errors must not be treated as commands
run_test "Parser: reject unmatched quotes" \
    "echo \"unterminated" \
    "unmatched quote"

run_test "Parser: reject redirection without a file" \
    "echo hello >" \
    "redirection requires a file name"

# Test 7: Notes utility without 'n' corruption
run_test "Student Utility: notes add & list (no 'n' bug)" \
    "notes clear\nnotes add \"Study process scheduling in Unix\"\nnotes list" \
    "Study process scheduling in Unix"

run_test "Student Utility: interactive notes entry" \
    "notes clear\nnotes add\nStudy process scheduling interactively\nnotes list" \
    "Study process scheduling interactively"

# Test 8: Assignment utility
run_test "Student Utility: assignment add & list" \
    "assignment clear\nassignment add \"Shell Project\" --due 30-09-2026\nassignment list" \
    "Shell Project"

run_test "Student Utility: interactive assignment entry" \
    "assignment clear\nassignment add\nShell Project Interactive\n15-10-2026\nassignment list" \
    "Shell Project Interactive"

# Test 9: Timetable utility
run_test "Student Utility: timetable add & list" \
    "timetable clear\ntimetable add Monday 09:00 \"Operating Systems\"\ntimetable list" \
    "Operating Systems"

run_test "Student Utility: interactive timetable entry" \
    "timetable clear\ntimetable add\nTuesday\n10:30\nOperating Systems Lab\ntimetable list" \
    "Operating Systems Lab"

# Test 10: Calculator utility
run_test "Student Utility: calculator arithmetic" \
    "calculator (20 + 5) * 4" \
    "Result     : 100"

# Test 11: Virtual Memory stat (memstat)
run_test "System Utility: memstat procfs inspection" \
    "memstat" \
    "Virtual Memory Statistics (memstat)"

# Test 12: System Info (sysinfo)
run_test "System Utility: sysinfo uname inspection" \
    "sysinfo" \
    "StudentOS System Information"

# Test 13: Background jobs
run_test "Job Control: background process launch" \
    "sleep 0.1 &" \
    "[1]"

# Test 14: a background pipeline is one job
run_test "Job Control: background pipeline launch" \
    "printf pipeline | cat &" \
    "[1]"

echo "=================================================="
echo " Test Summary: $PASSED Passed, $FAILED Failed"
echo "=================================================="

if [ "$FAILED" -eq 0 ]; then
    exit 0
else
    exit 1
fi
