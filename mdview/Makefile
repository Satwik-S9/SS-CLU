# Select the compiler
CC_CANDIDATES := gcc clang cc
CC             := $(firstword $(foreach c,$(CC_CANDIDATES),$(if $(shell $(WHICH) $(c) 2>$(NULL_DEV)),$(c),)))
ifeq ($(CC),)
    $(error No C compiler found. Install gcc or clang, or set CC explicitly)
endif

# Compiler and Linker flags, plus compile/link command templates.
CFLAGS   := -Wall -Wextra -pedantic -O2 -std=c11
LDFLAGS  :=
COMPILE   = $(CC) $(CFLAGS) -c -o $@ $<
LINK      = $(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^
OBJEXT   := o
EXEEXT   :=

# Source and Targets
TARGET_DIR = build
TARGET = ${TARGET_DIR}/mdview
SOURCE = mdview.c
PREFIX ?= $(HOME)/.local/bin

$(shell touch $(SOURCE)) # To avoid the clock skew warning

all: ${TARGET}

${TARGET}: ${SOURCE}
	@mkdir -p ${TARGET_DIR}
	gcc -o ${TARGET} ${SOURCE} -Wall -Wextra -Werror -O2

install: ${TARGET}
	@mkdir -p $(PREFIX)
	install -m 755 ${TARGET} $(PREFIX)/mdview

clean:
	rm -rf ${TARGET_DIR}
