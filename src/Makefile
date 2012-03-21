CC=gcc
OPTIMIZE=-g -O0

OBJS= main.o

INCLUDES = -I.
CFLAGS=-c -Wall $(INCLUDES)

TARGET=../bin/crossgen

.PHONY: all clean cleanobjs

all: $(TARGET)

%.o: %.c
	$(CC) -c $(OPTIMIZE) $(CFLAGS) -o $@ $<

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS)

clean: cleanobjs
	$(RM) $(TARGET)

cleanobjs:
	$(RM) *.o
