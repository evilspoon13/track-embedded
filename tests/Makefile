CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -I../common -I/usr/include -I/usr/local/include
LDFLAGS = -lrt -lpthread

OBJ_DIR = obj
TARGETS = queue_reader queue_creator graph_config_test


all: $(TARGETS)

queue_reader: $(OBJ_DIR)/queue_reader.o $(OBJ_DIR)/shared_memory.o
	$(CXX) $^ -o $@ $(LDFLAGS)

queue_creator: $(OBJ_DIR)/queue_creator.o $(OBJ_DIR)/shared_memory.o
	$(CXX) $^ -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: %.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

graph_config_test: $(OBJ_DIR)/graph_config_test.o $(OBJ_DIR)/config_parser.o
	$(CXX) $^ -o $@ $(LDFLAGS)

$(OBJ_DIR)/config_parser.o: ../common/config_parser.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(TARGETS)

.PHONY: all clean
