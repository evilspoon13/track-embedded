LDFLAGS += -lraylib -lX11 -lm

SRC_DIR = src
COMMON_DIR = ../common
OBJ_DIR = obj
TARGET = graphics-engine

SRCS = $(wildcard $(SRC_DIR)/*.cpp)
COMMON_SRCS = $(COMMON_DIR)/config_parser.cpp $(COMMON_DIR)/shared_memory.cpp

OBJS = $(SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)
COMMON_OBJS = $(COMMON_SRCS:$(COMMON_DIR)/%.cpp=$(OBJ_DIR)/common_%.o)

all: $(TARGET)

$(TARGET): $(OBJS) $(COMMON_OBJS)
	$(CXX) $(OBJS) $(COMMON_OBJS) -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/common_%.o: $(COMMON_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

clean:
	rm -rf $(OBJ_DIR) $(TARGET)

.PHONY: all clean
