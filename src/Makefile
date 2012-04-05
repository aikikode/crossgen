CC=gcc
OPTIMIZE=-g -O0
DEBUG=           # use '-DDEBUG' for debug output

OBJS= main.o

INCLUDES = -I.
CFLAGS=-c -Wall $(INCLUDES)

TARGET=../bin/crossgen

.PHONY: all clean cleanobjs

all: $(TARGET)

%.o: %.c
	$(CC) -c $(OPTIMIZE) $(DEBUG) $(CFLAGS) -o $@ $<

$(TARGET): $(OBJS)
	@if [ ! -d ../bin ]; then mkdir ../bin; fi
	$(CC) -o $@ $(OBJS)

clean: cleanobjs
	$(RM) $(TARGET)

cleanobjs:
	$(RM) *.o
