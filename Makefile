SERVER=server
CLIENT=client
OBJS=uds.o

CFLAGS=-Wall -O2 #-g
LDFLAGS+=-pthread
#CC=gcc
#RM=rm -f


all: $(SERVER) $(CLIENT)

$(SERVER): $(OBJS) $(SERVER).o
	$(CC) $(LDFLAGS) -o $@ $^

$(CLIENT): $(OBJS) $(CLIENT).o
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<


.PHONY: clean
clean:
	$(RM) *.o *~ $(CLIENT) $(SERVER)

