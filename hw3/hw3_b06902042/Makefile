CC = g++
OPENCV =  `pkg-config --cflags --libs opencv`
PTHREAD = -pthread
PARAM = -std=c++17

SENDER = sender.cpp
RECEIVER = receiver.cpp
AGENT = agent.cpp
SRC = src/
BIN = bin/
SND = sender
RCV = receiver
AGT = agent

all: sender receiver agent
  
sender: $(SRC)$(SENDER)
	$(CC) $(SRC)$(SENDER) -o $(SND)  $(OPENCV) $(PTHREAD) $(PARAM)
receiver: $(SRC)$(RECEIVER)
	$(CC) $(SRC)$(RECEIVER) -o $(RCV)  $(OPENCV) $(PTHREAD) $(PARAM)
agent: $(SRC)$(AGENT)
	$(CC) $(SRC)$(AGENT) -o $(AGT) $(PARAM)

.PHONY: clean

test: $(SRC)$(SENDER) $(SRC)$(RECEIVER) $(SRC)$(AGENT)
	$(CC) $(SRC)$(SENDER) -o $(BIN)$(SND)  $(OPENCV) $(PTHREAD) $(PARAM)
	$(CC) $(SRC)$(RECEIVER) -o $(BIN)$(RCV)  $(OPENCV) $(PTHREAD) $(PARAM)
	$(CC) $(SRC)$(AGENT) -o $(BIN)$(AGT) $(PARAM)

tt: src/testcv.cpp
	$(CC) src/testcv.cpp -o bin/testcv $(OPENCV) $(PARAM)

clean:
	rm $(RCV) $(SND) $(AGT)
