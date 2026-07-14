CC       := gcc
CFLAGS   := -Wall -Wextra -pedantic -O2 -std=c11
LDFLAGS  :=

TARGET   := csview
BUILDDIR := build
SRC      := csview.c
OBJ      := $(BUILDDIR)/csview.o
BIN      := $(BUILDDIR)/$(TARGET)

# Installation directory — override with:  make install PREFIX=~/bin
PREFIX   ?= $(HOME)/.local/bin

.PHONY: all clean install uninstall

all: $(BIN)

$(BUILDDIR):
	@mkdir -p $@

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^
	@echo "  -> Built: $@"

$(OBJ): $(SRC) | $(BUILDDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf $(BUILDDIR)
	@echo "  -> Cleaned."

# Install the binary to BINDIR (default: ~/.local/bin)
# Override prefix with:  make install PREFIX=/usr/share/bin
install: $(BIN)
	@mkdir -p $(PREFIX)
	install -m 755 $< $(PREFIX)/$(TARGET)
	@echo "  -> Installed to $(PREFIX)/$(TARGET)"

uninstall:
	rm -f $(PREFIX)/$(TARGET)
	@echo "  -> Removed $(PREFIX)/$(TARGET)"
