make: main.c simplehttpd.c
	gcc main.c simplehttpd.c -lpthread -D_REENTRANT -Wall -o server
