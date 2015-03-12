SERVER=server
CLIENT=client
OBJS=uds.o

CFLAGS=-Wall -O2
LDFLAGS+=-pthread

all: $(SERVER) $(CLIENT)

$(SERVER): $(OBJS) $(SERVER).o
	$(CC) -o $@ $^ $(LDFLAGS)

$(CLIENT): $(OBJS) $(CLIENT).o
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<


.PHONY: clean
clean:
	$(RM) *.o *~ $(CLIENT) $(SERVER)
