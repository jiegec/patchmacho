all: patchmacho

patchmacho: main.o
	$(CXX) $^ -o $@

%.o: %.cpp
	$(CXX) -c $^ -o $@

install: all
	install -D -m 755 patchmacho $(out)/bin/patchmacho