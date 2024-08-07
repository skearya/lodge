client:
	gcc ./src/client.c -o ./out/client && ./out/client

server:
	gcc ./src/server.c -o ./out/server && ./out/server
