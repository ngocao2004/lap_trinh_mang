CC = gcc
CFLAGS = -Wall -Wextra -g

CLIENT_SRC = TCPClient/client.c
SERVER_SRC = TCPServer/server.c

CLIENT_BIN = client
SERVER_BIN = server

all: $(CLIENT_BIN) $(SERVER_BIN)

$(CLIENT_BIN): $(CLIENT_SRC)
	$(CC) $(CFLAGS) -o $@ $^

$(SERVER_BIN): $(SERVER_SRC)
	$(CC) $(CFLAGS) -o $@ $^ -pthread

clean:
	rm -f $(CLIENT_BIN) $(SERVER_BIN)

