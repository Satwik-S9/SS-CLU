# For Windows use NMakefile instead:  nmake /f NMakefile

# Select the compiler (gcc preferred, then clang, then cc)
CC_CANDIDATES := gcc clang cc
CC             := $(firstword $(foreach c,$(CC_CANDIDATES),$(if $(shell command -v $(c) 2>/dev/null),$(c),)))
ifeq ($(CC),)
    $(error No C compiler found. Install gcc or clang, or set CC explicitly)
endif

CFLAGS   := -Wall -Wextra -pedantic -O2 -std=c11
LDFLAGS  :=
COMPILE   = $(CC) $(CFLAGS) -c -o $@ $<
LINK      = $(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^
OBJEXT   := o
EXEEXT   :=

# Source and Target details
SRC      := csview.c
TARGET   := csview
BUILDDIR := build
OBJ      := $(BUILDDIR)/csview.$(OBJEXT)
BIN      := $(BUILDDIR)/$(TARGET)$(EXEEXT)

# Info messages
$(info Detected OS: $(DETECTED_OS))
$(info Using compiler: $(CC))
$(info Building to: $(BUILDDIR))

# Installation directory — override with:  make install PREFIX=~/bin
PREFIX   ?= $(HOME)/.local/bin

.PHONY: all clean install uninstall

all: $(BIN)

$(BUILDDIR):
	@mkdir -p $@

$(BIN): $(OBJ)
	$(LINK)
	@echo "  -> Built: $@"

$(OBJ): $(SRC) | $(BUILDDIR)
	$(COMPILE)

clean:
	rm -rf $(BUILDDIR)
	@echo "  -> Cleaned."

# Install the binary to BINDIR (default: ~/.local/bin)
# Override prefix with:  make install PREFIX=/usr/share/bin
install: $(BIN)
	@mkdir -p $(PREFIX)
	install -m 755 $< $(PREFIX)/$(TARGET)$(EXEEXT)
	@echo "  -> Installed to $(PREFIX)/$(TARGET)$(EXEEXT)"

uninstall:
	rm -f $(PREFIX)/$(TARGET)$(EXEEXT)
	@echo "  -> Removed $(PREFIX)/$(TARGET)$(EXEEXT)"