all: server console

server: main.c simplehttpd.c functions.c
	gcc main.c simplehttpd.c functions.c -lpthread -D_REENTRANT -Wall -o server

console: console.c functions.c
	gcc console.c functions.c -Wall -o console