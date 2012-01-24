CC = gcc
CFLAGS = -O3 -s -Wall -Wl,--nxcompat -Wl,--dynamicbase -mwindows -lshlwapi -DUNICODE -D_UNICODE

OUTDIR = bin
DBGDIR = dbg
EXE = loader.exe

all: build release

debug: CFLAGS = -ggdb -Wall -mwindows -lshlwapi -DUNICODE -D_UNICODE
debug: build

loader.exe: loader.o resource.rc.o

build: $(EXE)

release: build
	@mkdir -p $(OUTDIR)
	cp *.exe $(OUTDIR)


%.o: %.c
	$(CC) $(CFLAGS) $(LIBRARY) $(INCLUDE) -c $^ -o $@

%.rc.o: %.rc
	windres.exe $^ $@

%.exe:
	$(CC) $^ $(CFLAGS) -o $@

clean :
	$(RM) -rf *.o *.exe

.PHONY: all build clean release
.SUFFIXES:
