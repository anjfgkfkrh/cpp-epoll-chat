CXX := g++
CXXFLAGS := -Wall -Wextra -std=c++20 -pthread
DEBUGFLAGS := -g -O0
RELEASEFLAGS := -O2

# 빌드 산출물은 전부 out/ 아래로 모은다 (소스 트리를 오염시키지 않는다)
#   out/server/...  서버 오브젝트 파일 (소스 디렉터리 구조 그대로)
#   out/client/...  클라이언트 오브젝트 파일
#   out/bin/...     실행 파일
OUT_DIR  := out
OBJ_SERV := $(OUT_DIR)/server
OBJ_CLIE := $(OUT_DIR)/client
BIN_DIR  := $(OUT_DIR)/bin

# 헤더가 하위 디렉터리에 흩어져 있고 파일명만으로 include 하므로 전부 추가해야 한다
SERV_INC := -Iserver/src -Iserver/src/core -Iserver/src/network -Iserver/src/user \
            -Iserver/src/room -Iserver/src/event -Iserver/src/protocol -Iserver/src/server
CLIE_INC := -Iclient/src


SERV_TARGET := $(BIN_DIR)/run_server
CLIE_TARGET := $(BIN_DIR)/run_client

SERV_SRC := $(shell find server -name "*.cpp")
CLIE_SRC := $(shell find client -name "*.cpp")

SERV_OBJ := $(patsubst server/%.cpp,$(OBJ_SERV)/%.o,$(SERV_SRC))
CLIE_OBJ := $(patsubst client/%.cpp,$(OBJ_CLIE)/%.o,$(CLIE_SRC))

all: release

debug: CXXFLAGS += $(DEBUGFLAGS)
debug: $(SERV_TARGET) $(CLIE_TARGET)

release: CXXFLAGS += $(RELEASEFLAGS)
release: $(SERV_TARGET) $(CLIE_TARGET)

server: $(SERV_TARGET)
client: $(CLIE_TARGET)


$(SERV_TARGET): $(SERV_OBJ)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $(SERV_OBJ)

$(CLIE_TARGET): $(CLIE_OBJ)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $(CLIE_OBJ)

$(OBJ_SERV)/%.o: server/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(SERV_INC) -c $< -o $@

$(OBJ_CLIE)/%.o: client/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(CLIE_INC) -c $< -o $@

clean:
	rm -rf $(OUT_DIR)

re: clean all

.PHONY: all debug release server client clean re
