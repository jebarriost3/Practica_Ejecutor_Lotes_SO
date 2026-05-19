CC ?= gcc
AR ?= ar

CFLAGS ?= -std=c11 -Wall -Wextra -pedantic -Iinclude

ifdef MSYSTEM
EXEEXT := .exe
CLEAN_CMD = rm -f build_protocol.o liblotes.a test_protocol$(EXEEXT)
else ifeq ($(OS),Windows_NT)
EXEEXT := .exe
CLEAN_CMD = del /Q build_protocol.o liblotes.a test_protocol$(EXEEXT) 2>NUL
else
EXEEXT :=
CLEAN_CMD = rm -f build_protocol.o liblotes.a test_protocol$(EXEEXT)
endif

COMMON_OBJS := build_protocol.o

.PHONY: all clean test

all: liblotes.a

build_protocol.o: src/common/protocol.c include/protocol.h
	$(CC) $(CFLAGS) -c src/common/protocol.c -o $@

liblotes.a: $(COMMON_OBJS)
	$(AR) rcs $@ $(COMMON_OBJS)

test_protocol$(EXEEXT): tests/test_protocol.c liblotes.a
	$(CC) $(CFLAGS) tests/test_protocol.c liblotes.a -o $@

test: test_protocol$(EXEEXT)
	./test_protocol$(EXEEXT)

clean:
	-$(CLEAN_CMD)
