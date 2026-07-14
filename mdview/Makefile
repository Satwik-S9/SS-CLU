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