CC ?= gcc
AR ?= ar

CFLAGS ?= -std=c11 -Wall -Wextra -pedantic -Iinclude

ifdef MSYSTEM
EXEEXT := .exe
RM := C:/msys64/usr/bin/rm.exe
CLEAN_CMD = $(RM) -f build_protocol.o build_gesfich_store.o build_gesfich_service.o build_gesfich_server.o build_gesfich_main.o liblotes.a gesfich$(EXEEXT) test_protocol$(EXEEXT) test_gesfich_store$(EXEEXT) test_gesfich_service$(EXEEXT)
else ifeq ($(OS),Windows_NT)
EXEEXT := .exe
CLEAN_CMD = del /Q build_protocol.o build_gesfich_store.o build_gesfich_service.o build_gesfich_server.o build_gesfich_main.o liblotes.a gesfich$(EXEEXT) test_protocol$(EXEEXT) test_gesfich_store$(EXEEXT) test_gesfich_service$(EXEEXT) 2>NUL
else
EXEEXT :=
CLEAN_CMD = rm -f build_protocol.o build_gesfich_store.o build_gesfich_service.o build_gesfich_server.o build_gesfich_main.o liblotes.a gesfich$(EXEEXT) test_protocol$(EXEEXT) test_gesfich_store$(EXEEXT) test_gesfich_service$(EXEEXT)
endif

COMMON_OBJS := build_protocol.o build_gesfich_store.o build_gesfich_service.o

.PHONY: all clean test

all: liblotes.a gesfich$(EXEEXT)

build_protocol.o: src/common/protocol.c include/protocol.h
	$(CC) $(CFLAGS) -c src/common/protocol.c -o $@

build_gesfich_store.o: src/gesfich/store.c include/gesfich_store.h include/protocol.h
	$(CC) $(CFLAGS) -c src/gesfich/store.c -o $@

build_gesfich_service.o: src/gesfich/service.c include/gesfich_service.h include/gesfich_store.h include/protocol.h
	$(CC) $(CFLAGS) -c src/gesfich/service.c -o $@

liblotes.a: $(COMMON_OBJS)
	$(AR) rcs $@ $(COMMON_OBJS)

build_gesfich_server.o: src/gesfich/server.c include/gesfich_server.h include/gesfich_service.h include/protocol.h
	$(CC) $(CFLAGS) -c src/gesfich/server.c -o $@

build_gesfich_main.o: src/gesfich/main.c include/gesfich_server.h
	$(CC) $(CFLAGS) -c src/gesfich/main.c -o $@

gesfich$(EXEEXT): build_gesfich_main.o build_gesfich_server.o liblotes.a
	$(CC) $(CFLAGS) build_gesfich_main.o build_gesfich_server.o liblotes.a -o $@

test_protocol$(EXEEXT): tests/test_protocol.c liblotes.a
	$(CC) $(CFLAGS) tests/test_protocol.c liblotes.a -o $@

test_gesfich_store$(EXEEXT): tests/test_gesfich_store.c liblotes.a
	$(CC) $(CFLAGS) tests/test_gesfich_store.c liblotes.a -o $@

test_gesfich_service$(EXEEXT): tests/test_gesfich_service.c liblotes.a
	$(CC) $(CFLAGS) tests/test_gesfich_service.c liblotes.a -o $@

test: test_protocol$(EXEEXT) test_gesfich_store$(EXEEXT) test_gesfich_service$(EXEEXT) gesfich$(EXEEXT)
	./test_protocol$(EXEEXT)
	./test_gesfich_store$(EXEEXT)
	./test_gesfich_service$(EXEEXT)

clean:
	-$(CLEAN_CMD)
