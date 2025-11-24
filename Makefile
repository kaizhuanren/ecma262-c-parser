# Default tools (override via environment)
CC ?= cc
RE2C ?= re2c
# Prefer Homebrew bison (supports %code/GLR); override via environment if needed.
BISON ?= /opt/homebrew/opt/bison/bin/bison
RM ?= rm -f
RMDIR ?= rm -rf
MKDIR_P ?= mkdir -p

# Flags
CFLAGS ?= -std=c17 -Wall -Wextra -Wpedantic -Iinclude -I$(GEN_DIR) -Isrc/lexer -Isrc/parser
LDFLAGS ?=

# Generated sources
GEN_DIR := build/generated
LEXER_RE := src/lexer/js_lexer.re
LEXER_GEN := $(GEN_DIR)/js_lexer.c
PARSER_Y := src/parser/js_parser.y
PARSER_GEN := $(GEN_DIR)/js_parser.c
PARSER_HEADER := $(GEN_DIR)/js_parser.h

# Sources
SRC := \
	src/ast/ast.c \
	src/cli/main.c \
	src/lexer/lexer_driver.c \
	src/parser/parser_driver.c

OBJS := $(SRC:%.c=build/obj/%.o) $(LEXER_GEN:%.c=build/obj/%.o) $(PARSER_GEN:%.c=build/obj/%.o)

TARGET := build/bin/jsparser
VALID_TEST_CASES := $(wildcard tests/cases/valid/*.js)
INVALID_TEST_CASES := $(wildcard tests/cases/invalid/*.js)
SCAN_DIR ?=
SCAN_LOG ?= scan.log

.PHONY: all clean distclean generated_dirs test

all: $(TARGET)

generated_dirs:
	@$(MKDIR_P) build/obj/src/ast
	@$(MKDIR_P) build/obj/src/cli
	@$(MKDIR_P) build/obj/src/lexer
	@$(MKDIR_P) build/obj/src/parser
	@$(MKDIR_P) build/obj/build/generated
	@$(MKDIR_P) build/bin
	@$(MKDIR_P) $(GEN_DIR)

$(LEXER_GEN): $(LEXER_RE) | generated_dirs
	$(RE2C) -o $@ $<

$(PARSER_GEN) $(PARSER_HEADER): $(PARSER_Y) | generated_dirs
	$(BISON) --defines=$(PARSER_HEADER) --output=$(PARSER_GEN) $<

build/obj/%.o: %.c | generated_dirs
	$(CC) $(CFLAGS) -c $< -o $@

build/obj/src/parser/parser_driver.o: $(PARSER_HEADER)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS)

clean:
	$(RMDIR) build/obj
	$(RM) $(TARGET)

distclean: clean
	$(RMDIR) $(GEN_DIR)

test: $(TARGET)
	@set -e; \
	for file in $(VALID_TEST_CASES); do \
		echo "[test][valid] $$file"; \
		$(TARGET) "$$file" >/dev/null; \
	done; \
	for file in $(INVALID_TEST_CASES); do \
		echo "[test][invalid] $$file"; \
		if $(TARGET) "$$file" >/dev/null 2>&1; then \
			echo "Expected failure but parser succeeded: $$file"; \
			exit 1; \
		fi; \
	done

# Recursively parse all .js files under SCAN_DIR and write a log file.
# Usage: make scan SCAN_DIR=path/to/dir [SCAN_LOG=out.log]
scan: $(TARGET)
ifeq ($(strip $(SCAN_DIR)),)
	$(error SCAN_DIR must be set, e.g. 'make scan SCAN_DIR=examples')
endif
	$(TARGET) --scan-dir "$(SCAN_DIR)" --log "$(SCAN_LOG)"
