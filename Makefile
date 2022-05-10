CC=gcc
LD=$(CC)
CPPFLAGS=-g -std=gnu11 -Wpedantic -Wall -Wextra
CFLAGS=-I.
LDFLAGS=
LDLIBS=
PROGRAM=command_line

all: $(PROGRAM)

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(PROGRAM): $(PROGRAM).o jumbo_file_system.o basic_file_system.o raw_disk.o
	$(LD) $(CPPFLAGS) $(LDFLAGS) $(LDLIBS) -o $@ $^

.PHONY:
clean:
	rm -f *.o $(PROGRAM) DISK
