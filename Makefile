# Default tools (override via environment)
CC ?= cc
RE2C ?= re2c
BISON ?= bison
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
TEST_CASES := $(wildcard tests/cases/*.js)

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

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS)

clean:
	$(RMDIR) build/obj
	$(RM) $(TARGET)

distclean: clean
	$(RMDIR) $(GEN_DIR)

test: $(TARGET)
	@set -e; \
	for file in $(TEST_CASES); do \
		echo "[test] $$file"; \
		$(TARGET) "$$file" >/dev/null; \
	done
