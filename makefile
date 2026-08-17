CXX := g++
CXXFLAGS := -Wall -Wextra -std=c++20 -pthread
DEBUGFLAGS := -g -O0
RELEASEFLAGS := -O2

# 헤더가 하위 디렉터리에 흩어져 있고 파일명만으로 include 하므로 전부 추가해야 한다
SERV_INC := -Iserver/src -Iserver/src/core -Iserver/src/network -Iserver/src/user \
            -Iserver/src/room -Iserver/src/event -Iserver/src/protocol -Iserver/src/server
CLIE_INC := -Iclient/src


SERV_TARGET := server/run_server
CLIE_TARGET := client/run_client

SERV_SRC := $(shell find server/src -name "*.cpp") server/run_server.cpp
CLIE_SRC := $(shell find client/src -name "*.cpp") client/run_client.cpp

SERV_OBJ := $(SERV_SRC:.cpp=.o)
CLIE_OBJ := $(CLIE_SRC:.cpp=.o)

all: release

debug: CXXFLAGS += $(DEBUGFLAGS)
debug: $(SERV_TARGET) $(CLIE_TARGET)

release: CXXFLAGS += $(RELEASEFLAGS)
release: $(SERV_TARGET) $(CLIE_TARGET)

server: $(SERV_TARGET)
client: $(CLIE_TARGET)


$(SERV_TARGET): $(SERV_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(SERV_OBJ)

$(CLIE_TARGET): $(CLIE_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(CLIE_OBJ)

server/%.o: server/%.cpp
	$(CXX) $(CXXFLAGS) $(SERV_INC) -c $< -o $@

client/%.o: client/%.cpp
	$(CXX) $(CXXFLAGS) $(CLIE_INC) -c $< -o $@

clean:
	rm -f $(SERV_OBJ) $(CLIE_OBJ)
	rm -f $(SERV_TARGET) $(CLIE_TARGET)

re: clean all

.PHONY: all debug release server client clean re
