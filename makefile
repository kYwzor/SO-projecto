make: main.c header.h
	gcc main.c -lpthread -D_REENTRANT -Wall -o server