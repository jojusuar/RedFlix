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
	$(CC) -g -o client client.c

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) client

.PHONY: all clean