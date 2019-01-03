all: server client

server:
	gcc -ggdb -pthread server.c -o server

client:
	gcc -ggdb client.c -o client

clean:
	rm -f server client

.PHONY: server client clean all
	
