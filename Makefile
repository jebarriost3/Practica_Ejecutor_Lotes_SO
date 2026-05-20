CC ?= gcc
AR ?= ar

CFLAGS ?= -std=c11 -Wall -Wextra -pedantic -Iinclude

ifdef MSYSTEM
EXEEXT := .exe
PLATFORM := win
RM := C:/msys64/usr/bin/rm.exe
CLEAN_CMD = $(RM) -f build_protocol.o build_gesfich_store.o build_gesfich_service.o build_gesfich_server.o build_gesfich_main.o liblotes.a gesfich gesfich.exe test_protocol test_protocol.exe test_gesfich_store test_gesfich_store.exe test_gesfich_service test_gesfich_service.exe
else ifeq ($(OS),Windows_NT)
EXEEXT := .exe
PLATFORM := win
CLEAN_CMD = del /Q build_protocol.o build_gesfich_store.o build_gesfich_service.o build_gesfich_server.o build_gesfich_main.o liblotes.a gesfich gesfich.exe test_protocol test_protocol.exe test_gesfich_store test_gesfich_store.exe test_gesfich_service test_gesfich_service.exe 2>NUL
else
EXEEXT :=
PLATFORM := linux
CLEAN_CMD = rm -f build_protocol.o build_gesfich_store.o build_gesfich_service.o build_gesfich_server.o build_gesfich_main.o liblotes.a gesfich gesfich.exe test_protocol test_protocol.exe test_gesfich_store test_gesfich_store.exe test_gesfich_service test_gesfich_service.exe
endif

OBJ_PREFIX := build_$(PLATFORM)_
LIBLOTES := liblotes_$(PLATFORM).a

CLEAN_CMD := $(CLEAN_CMD) build_win_protocol.o build_win_gesfich_store.o build_win_gesfich_service.o build_win_gesfich_server.o build_win_gesfich_main.o liblotes_win.a build_linux_protocol.o build_linux_gesfich_store.o build_linux_gesfich_service.o build_linux_gesfich_server.o build_linux_gesfich_main.o liblotes_linux.a

COMMON_OBJS := $(OBJ_PREFIX)protocol.o $(OBJ_PREFIX)gesfich_store.o $(OBJ_PREFIX)gesfich_service.o

.PHONY: all clean test

all: $(LIBLOTES) gesfich$(EXEEXT)

$(OBJ_PREFIX)protocol.o: src/common/protocol.c include/protocol.h
	$(CC) $(CFLAGS) -c src/common/protocol.c -o $@

$(OBJ_PREFIX)gesfich_store.o: src/gesfich/store.c include/gesfich_store.h include/protocol.h
	$(CC) $(CFLAGS) -c src/gesfich/store.c -o $@

$(OBJ_PREFIX)gesfich_service.o: src/gesfich/service.c include/gesfich_service.h include/gesfich_store.h include/protocol.h
	$(CC) $(CFLAGS) -c src/gesfich/service.c -o $@

$(LIBLOTES): $(COMMON_OBJS)
	$(AR) rcs $@ $(COMMON_OBJS)

$(OBJ_PREFIX)gesfich_server.o: src/gesfich/server.c include/gesfich_server.h include/gesfich_service.h include/protocol.h
	$(CC) $(CFLAGS) -c src/gesfich/server.c -o $@

$(OBJ_PREFIX)gesfich_main.o: src/gesfich/main.c include/gesfich_server.h
	$(CC) $(CFLAGS) -c src/gesfich/main.c -o $@

gesfich$(EXEEXT): $(OBJ_PREFIX)gesfich_main.o $(OBJ_PREFIX)gesfich_server.o $(LIBLOTES)
	$(CC) $(CFLAGS) $(OBJ_PREFIX)gesfich_main.o $(OBJ_PREFIX)gesfich_server.o $(LIBLOTES) -o $@

test_protocol$(EXEEXT): tests/test_protocol.c $(LIBLOTES)
	$(CC) $(CFLAGS) tests/test_protocol.c $(LIBLOTES) -o $@

test_gesfich_store$(EXEEXT): tests/test_gesfich_store.c $(LIBLOTES)
	$(CC) $(CFLAGS) tests/test_gesfich_store.c $(LIBLOTES) -o $@

test_gesfich_service$(EXEEXT): tests/test_gesfich_service.c $(LIBLOTES)
	$(CC) $(CFLAGS) tests/test_gesfich_service.c $(LIBLOTES) -o $@

test: test_protocol$(EXEEXT) test_gesfich_store$(EXEEXT) test_gesfich_service$(EXEEXT) gesfich$(EXEEXT)
	./test_protocol$(EXEEXT)
	./test_gesfich_store$(EXEEXT)
	./test_gesfich_service$(EXEEXT)

clean:
	-$(CLEAN_CMD)
