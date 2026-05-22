CC ?= gcc
AR ?= ar

CFLAGS ?= -std=c11 -Wall -Wextra -pedantic -Iinclude

ifdef MSYSTEM
EXEEXT := .exe
PLATFORM := win
RM := C:/msys64/usr/bin/rm.exe
CLEAN_CMD = $(RM) -f build_protocol.o build_gesfich_store.o build_gesfich_service.o build_gesfich_server.o build_gesfich_main.o build_gesprog_store.o build_gesprog_service.o build_gesprog_server.o build_gesprog_main.o build_ejecutor_store.o build_ejecutor_service.o build_ejecutor_server.o build_ejecutor_main.o build_ctrllt_service.o build_ctrllt_server.o build_ctrllt_main.o liblotes.a gesfich gesfich.exe gesprog gesprog.exe ejecutor ejecutor.exe ctrllt ctrllt.exe test_protocol test_protocol.exe test_gesfich_store test_gesfich_store.exe test_gesfich_service test_gesfich_service.exe test_gesprog_store test_gesprog_store.exe test_gesprog_service test_gesprog_service.exe test_ejecutor_store test_ejecutor_store.exe test_ejecutor_service test_ejecutor_service.exe test_ctrllt_service test_ctrllt_service.exe
else ifeq ($(OS),Windows_NT)
EXEEXT := .exe
PLATFORM := win
CLEAN_CMD = del /Q build_protocol.o build_gesfich_store.o build_gesfich_service.o build_gesfich_server.o build_gesfich_main.o build_gesprog_store.o build_gesprog_service.o build_gesprog_server.o build_gesprog_main.o build_ejecutor_store.o build_ejecutor_service.o build_ejecutor_server.o build_ejecutor_main.o build_ctrllt_service.o build_ctrllt_server.o build_ctrllt_main.o liblotes.a gesfich gesfich.exe gesprog gesprog.exe ejecutor ejecutor.exe ctrllt ctrllt.exe test_protocol test_protocol.exe test_gesfich_store test_gesfich_store.exe test_gesfich_service test_gesfich_service.exe test_gesprog_store test_gesprog_store.exe test_gesprog_service test_gesprog_service.exe test_ejecutor_store test_ejecutor_store.exe test_ejecutor_service test_ejecutor_service.exe test_ctrllt_service test_ctrllt_service.exe 2>NUL
else
EXEEXT :=
PLATFORM := linux
CLEAN_CMD = rm -f build_protocol.o build_gesfich_store.o build_gesfich_service.o build_gesfich_server.o build_gesfich_main.o build_gesprog_store.o build_gesprog_service.o build_gesprog_server.o build_gesprog_main.o build_ejecutor_store.o build_ejecutor_service.o build_ejecutor_server.o build_ejecutor_main.o build_ctrllt_service.o build_ctrllt_server.o build_ctrllt_main.o liblotes.a gesfich gesfich.exe gesprog gesprog.exe ejecutor ejecutor.exe ctrllt ctrllt.exe test_protocol test_protocol.exe test_gesfich_store test_gesfich_store.exe test_gesfich_service test_gesfich_service.exe test_gesprog_store test_gesprog_store.exe test_gesprog_service test_gesprog_service.exe test_ejecutor_store test_ejecutor_store.exe test_ejecutor_service test_ejecutor_service.exe test_ctrllt_service test_ctrllt_service.exe
endif

OBJ_PREFIX := build_$(PLATFORM)_
LIBLOTES := liblotes_$(PLATFORM).a

CLEAN_CMD := $(CLEAN_CMD) build_win_protocol.o build_win_gesfich_store.o build_win_gesfich_service.o build_win_gesfich_server.o build_win_gesfich_main.o build_win_gesprog_store.o build_win_gesprog_service.o build_win_gesprog_server.o build_win_gesprog_main.o build_win_ejecutor_store.o build_win_ejecutor_service.o build_win_ejecutor_server.o build_win_ejecutor_main.o build_win_ctrllt_service.o build_win_ctrllt_server.o build_win_ctrllt_main.o liblotes_win.a build_linux_protocol.o build_linux_gesfich_store.o build_linux_gesfich_service.o build_linux_gesfich_server.o build_linux_gesfich_main.o build_linux_gesprog_store.o build_linux_gesprog_service.o build_linux_gesprog_server.o build_linux_gesprog_main.o build_linux_ejecutor_store.o build_linux_ejecutor_service.o build_linux_ejecutor_server.o build_linux_ejecutor_main.o build_linux_ctrllt_service.o build_linux_ctrllt_server.o build_linux_ctrllt_main.o liblotes_linux.a

COMMON_OBJS := $(OBJ_PREFIX)protocol.o $(OBJ_PREFIX)gesfich_store.o $(OBJ_PREFIX)gesfich_service.o $(OBJ_PREFIX)gesprog_store.o $(OBJ_PREFIX)gesprog_service.o $(OBJ_PREFIX)ejecutor_store.o $(OBJ_PREFIX)ejecutor_service.o $(OBJ_PREFIX)ctrllt_service.o

.PHONY: all clean test

all: $(LIBLOTES) gesfich$(EXEEXT) gesprog$(EXEEXT) ejecutor$(EXEEXT) ctrllt$(EXEEXT)

$(OBJ_PREFIX)protocol.o: src/common/protocol.c include/protocol.h
	$(CC) $(CFLAGS) -c src/common/protocol.c -o $@

$(OBJ_PREFIX)gesfich_store.o: src/gesfich/store.c include/gesfich_store.h include/protocol.h
	$(CC) $(CFLAGS) -c src/gesfich/store.c -o $@

$(OBJ_PREFIX)gesfich_service.o: src/gesfich/service.c include/gesfich_service.h include/gesfich_store.h include/protocol.h
	$(CC) $(CFLAGS) -c src/gesfich/service.c -o $@

$(OBJ_PREFIX)gesprog_store.o: src/gesprog/store.c include/gesprog_store.h include/protocol.h
	$(CC) $(CFLAGS) -c src/gesprog/store.c -o $@

$(OBJ_PREFIX)gesprog_service.o: src/gesprog/service.c include/gesprog_service.h include/gesprog_store.h include/protocol.h
	$(CC) $(CFLAGS) -c src/gesprog/service.c -o $@

$(OBJ_PREFIX)ejecutor_store.o: src/ejecutor/store.c include/ejecutor_store.h include/protocol.h
	$(CC) $(CFLAGS) -c src/ejecutor/store.c -o $@

$(OBJ_PREFIX)ejecutor_service.o: src/ejecutor/service.c include/ejecutor_service.h include/ejecutor_store.h include/protocol.h
	$(CC) $(CFLAGS) -c src/ejecutor/service.c -o $@

$(OBJ_PREFIX)ctrllt_service.o: src/ctrllt/service.c include/ctrllt_service.h include/protocol.h
	$(CC) $(CFLAGS) -c src/ctrllt/service.c -o $@

$(LIBLOTES): $(COMMON_OBJS)
	$(AR) rcs $@ $(COMMON_OBJS)

$(OBJ_PREFIX)gesfich_server.o: src/gesfich/server.c include/gesfich_server.h include/gesfich_service.h include/protocol.h
	$(CC) $(CFLAGS) -c src/gesfich/server.c -o $@

$(OBJ_PREFIX)gesfich_main.o: src/gesfich/main.c include/gesfich_server.h
	$(CC) $(CFLAGS) -c src/gesfich/main.c -o $@

gesfich$(EXEEXT): $(OBJ_PREFIX)gesfich_main.o $(OBJ_PREFIX)gesfich_server.o $(LIBLOTES)
	$(CC) $(CFLAGS) $(OBJ_PREFIX)gesfich_main.o $(OBJ_PREFIX)gesfich_server.o $(LIBLOTES) -o $@

$(OBJ_PREFIX)gesprog_server.o: src/gesprog/server.c include/gesprog_server.h include/gesprog_service.h include/protocol.h
	$(CC) $(CFLAGS) -c src/gesprog/server.c -o $@

$(OBJ_PREFIX)gesprog_main.o: src/gesprog/main.c include/gesprog_server.h
	$(CC) $(CFLAGS) -c src/gesprog/main.c -o $@

gesprog$(EXEEXT): $(OBJ_PREFIX)gesprog_main.o $(OBJ_PREFIX)gesprog_server.o $(LIBLOTES)
	$(CC) $(CFLAGS) $(OBJ_PREFIX)gesprog_main.o $(OBJ_PREFIX)gesprog_server.o $(LIBLOTES) -o $@

$(OBJ_PREFIX)ejecutor_server.o: src/ejecutor/server.c include/ejecutor_server.h include/ejecutor_service.h include/protocol.h
	$(CC) $(CFLAGS) -c src/ejecutor/server.c -o $@

$(OBJ_PREFIX)ejecutor_main.o: src/ejecutor/main.c include/ejecutor_server.h
	$(CC) $(CFLAGS) -c src/ejecutor/main.c -o $@

ejecutor$(EXEEXT): $(OBJ_PREFIX)ejecutor_main.o $(OBJ_PREFIX)ejecutor_server.o $(LIBLOTES)
	$(CC) $(CFLAGS) $(OBJ_PREFIX)ejecutor_main.o $(OBJ_PREFIX)ejecutor_server.o $(LIBLOTES) -o $@

$(OBJ_PREFIX)ctrllt_server.o: src/ctrllt/server.c include/ctrllt_server.h include/ctrllt_service.h include/protocol.h
	$(CC) $(CFLAGS) -c src/ctrllt/server.c -o $@

$(OBJ_PREFIX)ctrllt_main.o: src/ctrllt/main.c include/ctrllt_server.h
	$(CC) $(CFLAGS) -c src/ctrllt/main.c -o $@

ctrllt$(EXEEXT): $(OBJ_PREFIX)ctrllt_main.o $(OBJ_PREFIX)ctrllt_server.o $(LIBLOTES)
	$(CC) $(CFLAGS) $(OBJ_PREFIX)ctrllt_main.o $(OBJ_PREFIX)ctrllt_server.o $(LIBLOTES) -o $@

test_protocol$(EXEEXT): tests/test_protocol.c $(LIBLOTES)
	$(CC) $(CFLAGS) tests/test_protocol.c $(LIBLOTES) -o $@

test_gesfich_store$(EXEEXT): tests/test_gesfich_store.c $(LIBLOTES)
	$(CC) $(CFLAGS) tests/test_gesfich_store.c $(LIBLOTES) -o $@

test_gesfich_service$(EXEEXT): tests/test_gesfich_service.c $(LIBLOTES)
	$(CC) $(CFLAGS) tests/test_gesfich_service.c $(LIBLOTES) -o $@

test_gesprog_store$(EXEEXT): tests/test_gesprog_store.c $(LIBLOTES)
	$(CC) $(CFLAGS) tests/test_gesprog_store.c $(LIBLOTES) -o $@

test_gesprog_service$(EXEEXT): tests/test_gesprog_service.c $(LIBLOTES)
	$(CC) $(CFLAGS) tests/test_gesprog_service.c $(LIBLOTES) -o $@

test_ejecutor_store$(EXEEXT): tests/test_ejecutor_store.c $(LIBLOTES)
	$(CC) $(CFLAGS) tests/test_ejecutor_store.c $(LIBLOTES) -o $@

test_ejecutor_service$(EXEEXT): tests/test_ejecutor_service.c $(LIBLOTES)
	$(CC) $(CFLAGS) tests/test_ejecutor_service.c $(LIBLOTES) -o $@

test_ctrllt_service$(EXEEXT): tests/test_ctrllt_service.c $(LIBLOTES)
	$(CC) $(CFLAGS) tests/test_ctrllt_service.c $(LIBLOTES) -o $@

test: test_protocol$(EXEEXT) test_gesfich_store$(EXEEXT) test_gesfich_service$(EXEEXT) test_gesprog_store$(EXEEXT) test_gesprog_service$(EXEEXT) test_ejecutor_store$(EXEEXT) test_ejecutor_service$(EXEEXT) test_ctrllt_service$(EXEEXT) gesfich$(EXEEXT) gesprog$(EXEEXT) ejecutor$(EXEEXT) ctrllt$(EXEEXT)
	./test_protocol$(EXEEXT)
	./test_gesfich_store$(EXEEXT)
	./test_gesfich_service$(EXEEXT)
	./test_gesprog_store$(EXEEXT)
	./test_gesprog_service$(EXEEXT)
	./test_ejecutor_store$(EXEEXT)
	./test_ejecutor_service$(EXEEXT)
	./test_ctrllt_service$(EXEEXT)

clean:
	-$(CLEAN_CMD)
