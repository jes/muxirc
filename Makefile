# Makefile for muxirc
# James Stanley 2012

CFLAGS=-Wall -g
LDFLAGS=
OBJS=src/channel.o src/client.o src/message.o src/muxirc.o src/server.o \
	 src/socket.o src/str.o

.PHONY: all
all: muxirc

muxirc: $(OBJS)
	$(CC) -o muxirc $(OBJS) $(LDFLAGS)

-include $(OBJS:.o=.d)

%.o: %.c
	$(CC) -MMD -o $@ -c $< $(CFLAGS)

.PHONY: clean
clean:
	$(RM) src/*.o src/*.d muxirc
