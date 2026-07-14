CXX = g++
CXXFLAGS = -Wall -O2 -std=c++20 -I lib -I common

SERV_LIB_SRCS = lib/Socket.cpp lib/Epoll.cpp lib/Room.cpp lib/Server.cpp lib/User.cpp
CLIE_LIB_SRCS = lib/Socket.cpp lib/Epoll.cpp lib/Room.cpp lib/Client.cpp

all: server client

server: src/run_server.cpp $(SERV_LIB_SRCS)
	$(CXX) $(CXXFLAGS) -o server src/run_server.cpp $(SERV_LIB_SRCS)

client: src/run_client.cpp $(CLIE_LIB_SRCS)
	$(CXX) $(CXXFLAGS) -o client src/run_client.cpp $(CLIE_LIB_SRCS)

clean:
	rm -f server client