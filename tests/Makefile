CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -I../common
LDFLAGS = -lrt -lpthread

OBJ_DIR = obj
TARGET = queue_reader

SRCS = queue_reader.cpp
OBJS = $(OBJ_DIR)/queue_reader.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)

$(OBJ_DIR)/queue_reader.o: queue_reader.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

clean:
	rm -rf $(OBJ_DIR) $(TARGET)

.PHONY: all clean
