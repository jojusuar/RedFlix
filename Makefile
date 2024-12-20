CC = gcc
CFLAGS = -g
LFLAGS = -lm
TARGET = server
SRCS = server.c common.c
OBJS = $(SRCS:.c=.o)
HEADERS = common.h

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LFLAGS)
	$(CC) -g -o controls controls.c -lncurses
	$(CC) -g -o visor visor.c

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) controls visor

.PHONY: all clean