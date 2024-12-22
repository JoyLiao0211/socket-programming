# Compiler and Flags
CXX = g++
CXXFLAGS = -Wall -g -I./include

# Source Files
SRCS = Client.cpp Server.cpp
OBJS = $(SRCS:.cpp=.o)

# Targets
TARGETS = Client Server

all: $(TARGETS)

Client: Client.o
	$(CXX) $(CXXFLAGS) -o Client Client.o -lssl -lcrypto

Server: Server.o
	$(CXX) $(CXXFLAGS) -o Server Server.o -lpthread -lssl -lcrypto

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGETS)
