# Makefile — build unipath-9p 的 C++ 埠。
CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -pthread
SRC = server.cpp ninep.cpp ninep_dispatch.cpp env.cpp env_io.cpp
OBJ = $(SRC:.cpp=.o)
BIN = unipath_9p_cpp

$(BIN): $(SRC) *.hpp
	$(CXX) $(CXXFLAGS) $(SRC) -o $(BIN)

clean:
	rm -f $(BIN) $(OBJ)

.PHONY: clean
