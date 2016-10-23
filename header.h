//includes
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/ipc.h> 
#include <sys/shm.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <string.h>

//functions
void close_server();
void stat_manager();
void load_conf();
void start_stat_process();
void *worker_thread();
void start_threads();
void start_sm();
void setup_server();
int main();


//structs
typedef struct config{
	int port;
	int sched;
	int threadp;

}Config;

typedef struct stats{

}Stats;

typedef struct buffer{
	struct buffer *next;
	struct buffer *prev;	
}Buffer;

//	gcc simplehttpd.c semlib.c -lpthread -D_REENTRANT -Wall -o run