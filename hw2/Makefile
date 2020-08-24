CC = g++
PTHREAD = -pthread

CLIENT = client.cpp
SERVER = server.cpp
SRC = src/
BIN = bin/
CLI = client
SER = server

all: server client
  
server: $(SRC)$(SERVER)
	$(CC) $(SRC)$(SERVER) -o $(SER)  $(PTHREAD) 
client: $(SRC)$(CLIENT)
	$(CC) $(SRC)$(CLIENT) -o $(CLI)  $(PTHREAD)

.PHONY: clean

test: $(SRC)$(SERVER) $(SRC)$(CLIENT)
	$(CC) $(SRC)$(SERVER) -o $(BIN)$(SER)  $(PTHREAD) 
	$(CC) $(SRC)$(CLIENT) -o $(BIN)$(CLI)  $(PTHREAD)

clean:
	rm $(CLI) $(SER)
