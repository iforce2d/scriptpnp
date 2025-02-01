MKDIR := mkdir -p
SRC_DIR := .
OBJ_DIR := ./obj
SRC_FILES := $(wildcard ./*.cpp wildcard ../common/*.cpp wildcard ../common/scv/*.cpp)
OBJ_FILES := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRC_FILES))
CXXFLAGS := -std=c++14 -Wall -Icommon -Wno-psabi

LDFLAGS := -lpthread -lzmq -lstdc++fs

all: pnpServer

pnpServer: $(OBJ_FILES)
	g++ -o $@ $^ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@ $(MKDIR) $(@D)
	g++ $(CXXFLAGS) -c -o $@ $<

clean:
	rm -rf $(OBJ_DIR)

