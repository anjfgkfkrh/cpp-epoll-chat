CXX := g++
CXXFLAGS := -Wall -O2 -std=c++20 -Wextra
DEBUGFLAGS := -g -O0
RELEASEFLAGS := -02

SERV_INC := -Iserver/src
CLIE_INC := -Iclient/src


SERV_TARGET := server/run_server
CLIE_TARGET := client/run_client

SERV_SRC := $(shell find server/src -name "*.cpp") server/run_server.cpp
CLIE_SRC := $(shell find client/src -name "*.cpp") client/run_client.cpp

SERV_OBJ := $(SERV_SRC:.cpp=.o)
CLIE_OBJ := $(CLIE_SRC:.cpp=.o)

all: release

debug: CXXFLAGS += $(DEBUGFLAGS)
debug: $(SERVER_TARGET) $(CLIENT_TARGET)

release: CXXFLAGS += $(RELEASEFLAGS)
release: $(SERVER_TARGET) $(CLIENT_TARGET)


$(SERVER_TARGET): $(SERVER_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(SERVER_OBJ)

$(CLIENT_TARGET): $(CLIENT_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(CLIENT_OBJ)

server/src/%.o: server/src/%.cpp
	$(CXX) $(CXXFLAGS) $(SERVER_INC) -c $< -o $@

client/src/%.o: client/src/%.cpp
	$(CXX) $(CXXFLAGS) $(CLIENT_INC) -c $< -o $@

clean:
	rm -f $(SERVER_OBJ) $(CLIENT_OBJ)
	rm -f $(SERVER_TARGET) $(CLIENT_TARGET)

re: clean all

.PHONY: all debug release clean re