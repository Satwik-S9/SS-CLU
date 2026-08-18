# Compiler
CC := cc

# Compiler and Linker Flags
CFLAGS := -Wall -Wextra -O2
LDFLAGS := 

# Source and Targets
TARGET_DIR := ./build
TARGET     := ${TARGET_DIR}/be
SOURCE 	   := be.c
PREFIX     ?= $(HOME)/.local/bin

.PHONY: all, install, clean, run

all: ${TARGET}

${TARGET}: ${SOURCE}
	@mkdir -p ${TARGET_DIR}
	gcc -o $@ $< ${CFLAGS}

install: ${TARGET}
	@mkdir -p $(PREFIX)
	install -m 755 ${TARGET} $(PREFIX)/be

clean:
	@rm -rf ${TARGET_DIR}

run:
	${TARGET}
