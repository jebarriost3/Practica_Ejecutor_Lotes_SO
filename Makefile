CC ?= gcc
AR ?= ar

CFLAGS ?= -std=c11 -Wall -Wextra -pedantic -Iinclude

ifdef MSYSTEM
EXEEXT := .exe
RM := C:/msys64/usr/bin/rm.exe
CLEAN_CMD = $(RM) -f build_protocol.o build_gesfich_store.o liblotes.a test_protocol$(EXEEXT) test_gesfich_store$(EXEEXT)
else ifeq ($(OS),Windows_NT)
EXEEXT := .exe
CLEAN_CMD = del /Q build_protocol.o build_gesfich_store.o liblotes.a test_protocol$(EXEEXT) test_gesfich_store$(EXEEXT) 2>NUL
else
EXEEXT :=
CLEAN_CMD = rm -f build_protocol.o build_gesfich_store.o liblotes.a test_protocol$(EXEEXT) test_gesfich_store$(EXEEXT)
endif

COMMON_OBJS := build_protocol.o build_gesfich_store.o

.PHONY: all clean test

all: liblotes.a

build_protocol.o: src/common/protocol.c include/protocol.h
	$(CC) $(CFLAGS) -c src/common/protocol.c -o $@

build_gesfich_store.o: src/gesfich/store.c include/gesfich_store.h include/protocol.h
	$(CC) $(CFLAGS) -c src/gesfich/store.c -o $@

liblotes.a: $(COMMON_OBJS)
	$(AR) rcs $@ $(COMMON_OBJS)

test_protocol$(EXEEXT): tests/test_protocol.c liblotes.a
	$(CC) $(CFLAGS) tests/test_protocol.c liblotes.a -o $@

test_gesfich_store$(EXEEXT): tests/test_gesfich_store.c liblotes.a
	$(CC) $(CFLAGS) tests/test_gesfich_store.c liblotes.a -o $@

test: test_protocol$(EXEEXT) test_gesfich_store$(EXEEXT)
	./test_protocol$(EXEEXT)
	./test_gesfich_store$(EXEEXT)

clean:
	-$(CLEAN_CMD)
