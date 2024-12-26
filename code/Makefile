# Compiler and Flags
CXX = g++
CXXFLAGS = -Wall -g -I./include `pkg-config --cflags opencv4`

# Source Files
SRCS = Client.cpp Server.cpp
OBJS = $(SRCS:.cpp=.o)

# Targets
TARGETS = Client Server

all: $(TARGETS)

debug: CXXFLAGS += -DDEBUG
debug: all

Client: Client.o
	$(CXX) $(CXXFLAGS) -o Client Client.o -lssl -lcrypto -lmpg123 -lportaudio `pkg-config --libs opencv4`

Server: Server.o
	$(CXX) $(CXXFLAGS) -o Server Server.o -lpthread -lssl -lcrypto -lmpg123 `pkg-config --libs opencv4`

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGETS) certs/*

clean_certs:
	rm -f certs/*
