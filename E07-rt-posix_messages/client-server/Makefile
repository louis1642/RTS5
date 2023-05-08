all: client server

client: client.c
	gcc -o client client.c -lrt
	
server: server.c
	gcc -o server server.c -lrt
	
clean:
	rm client server
