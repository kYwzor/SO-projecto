//includes
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/ipc.h> 
#include <sys/shm.h>
#include <sys/fcntl.h>

//functions
void fecha_server();
void load_conf();
void cria_processos();
void *worker_threads(void);
void cria_threads();
void cria_stat_sm();
void gestor_estatisticas();
void liga_server();
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

typedef struct buffer
{

}Buffer;