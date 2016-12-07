all: server console

server: main.c simplehttpd.c
	gcc main.c simplehttpd.c -lpthread -D_REENTRANT -Wall -o server

console: console.c
	gcc console.c -Wall -o console