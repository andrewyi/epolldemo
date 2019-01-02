EXECUTABLE := proxy

all:
	g++  proxy.cpp -ggdb -o $(EXECUTABLE)

clean:
	rm $(EXECUTABLE)

.PHONY: all clean
