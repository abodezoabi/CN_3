CC = gcc
CFLAGS = -Wall -g -Wextra -std=c99
LDFLAGS =
LIBS = -lm

.PHONY: all clean

all: RUDP_Sender RUDP_Receiver

RUDP_Sender: RUDP_Sender.o RUDP_API.o
	$(CC) $(LDFLAGS) $^ -o $@ $(LIBS)

RUDP_Receiver: RUDP_Receiver.o RUDP_API.o
	$(CC) $(LDFLAGS) $^ -o $@ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o RUDP_Sender RUDP_Receiver
