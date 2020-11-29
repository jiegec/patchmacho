all: patchmacho

patchmacho: main.o
	$(CXX) $^ -o $@

%.o: %.cpp
	$(CXX) -c $^ -o $@