CC ?= gcc
CPPFLAGS = -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L -Iinclude
CFLAGS ?= -Wall -Wextra -Wpedantic -std=c11 -g
LDLIBS = -pthread

TARGET = studentos

SRC = src/main.c \
      src/student_string.c \
      src/vector.c \
      src/student_commands.c \
      src/jobs.c \
      src/signals.c \
      src/history.c

OBJ = $(SRC:src/%.c=obj/%.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $(TARGET) $(LDLIBS)

obj/%.o: src/%.c
	@mkdir -p obj
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(TARGET) obj tests/__studentos_test__ data/*.tmp

run: all
	./$(TARGET)

test: all
	bash tests/test_shell.sh ./$(TARGET)

check:
	$(MAKE) clean
	$(MAKE) CFLAGS="$(CFLAGS) -Werror" all

.PHONY: all clean run test check
