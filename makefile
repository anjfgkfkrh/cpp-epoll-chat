CXX = g++
CXXFLAGS = -Wall -O2 -I lib

SRCS = lib/Socket.cpp lib/Epoll.cpp src/server.cpp lib/Room.cpp
TARGET = server

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS)

clean:
	rm -f $(TARGET)