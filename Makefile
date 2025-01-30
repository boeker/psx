SRC_DIR := src
OBJ_DIR := obj
BIN_DIR := .

SRC_SUBDIRS = $(sort $(dir $(wildcard $(SRC_DIR)/*/)))
OBJ_SUBDIRS := $(SRC_SUBDIRS:$(SRC_DIR)/%=$(OBJ_DIR)/%)

#$(info	Source subdirs: $(SRC_SUBDIRS))

EXE := $(BIN_DIR)/psx
SRC := $(wildcard $(SRC_DIR)/*.cpp $(SRC_DIR)/*/*.cpp)
OBJ := $(SRC:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)

#$(info	Objects files: $(OBJ))

CXX := g++
CPPFLAGS := -Iinclude -I$(SRC_DIR) -MMD -MP
CXXFLAGS := -std=c++20 -O2 -Wall
LDFLAGS :=
LDLIBS := -lglfw

.PHONY: all clean

all: $(EXE)

$(EXE): $(OBJ) | $(BIN_DIR)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR) $(OBJ_SUBDIRS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(BIN_DIR) $(OBJ_DIR) $(OBJ_SUBDIRS):
	mkdir -p $@

clean:
	@$(RM) -rv $(EXE) $(BIN_DIR) $(OBJ_DIR) 2>/dev/null || true

-include $(OBJ:.o=.d)

