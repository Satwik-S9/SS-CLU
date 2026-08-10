TARGET_DIR := ./build
TARGET     := ${TARGET_DIR}/be

.PHONY: all, clean, run

all:
	@mkdir -p ${TARGET_DIR}
	gcc -o ${TARGET} main.c -O2 -Wall -Wextra

clean:
	@rm -rf ${TARGET_DIR}

run:
	${TARGET}
