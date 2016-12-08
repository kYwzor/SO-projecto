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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <sys/stat.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>

// Produce debug information (if 0 no information is shown)
#define DEBUG	  	1	

// Header of HTTP reply to client 
#define	SERVER_STRING 	"Server: simpleserver/0.1.0\r\n"
#define HEADER_1	"HTTP/1.0 200 OK\r\n"
#define HEADER_2	"Content-Type: text/html\r\n\r\n"

#define GET_EXPR	"GET /"
#define CGI_EXPR	"cgi-bin/"
#define SIZE_BUF	1024

// Pipe name
#define PIPE_NAME   "pipe_console"

//structs
typedef struct config_struct{
	int port;					//port definida em config.txt
	int sched;					//0->Scheduling FIFO, 1->Prioridade páginas estáticas, 2->Prioridade páginas comprimidas
	int threadp;				//numero de threads criadas
	int nallowed;				//numero de ficheiros permitidos
	char** allowed;				//lista de ficheiros permitidos (alocacao dinamica)
}Config;

typedef struct request_struct{
	char *page;					//exemplo: "index.html" (alocacao dinamica)
	int compressed;				//compressed=1 -> pedido de pagina comprimida
	int socket;					//guarda socket do pedido		
	time_t time_requested;		//timestamp request
	int answered;				//0->not answered / 1->answered
	time_t time_answered;		//timestamp answer -- only check if answered = 1
}Request;

typedef struct lnode_req *Req_list;
typedef struct lnode_req{
	Request *req;				//alocacao dinamica
	Req_list next;
	Req_list prev;
}Req_list_node;

typedef struct msg_struct{
	int type;
	char value[SIZE_BUF];
}Message;

//functions main.c
void stat_manager();
void load_conf();
void run_http();
void *listen_console();
void start_stat_process();
void *worker_threads();
void start_threads();
void join_threads();
void start_sm();
void free_allowed_files_array();
void free_all_alocations();
void catch_ctrlc(int);
void create_buffer();
void create_pipe();

//functions simplehttpd.c
int  fireup(int port);
void identify(int socket);
void get_request(int socket);
int  read_line(int socket, int n);
void send_header(int socket);
void send_page(int socket);
void execute_script(int socket);
void not_found(int socket);
void cannot_execute(int socket);

//variaveis globais main.c
Config *config;					//alocacao dinamica
Req_list rlist;					//alocacao dinamica
int stat_sm_id;
int *temporario;				//shared memory
pthread_t *threads;				//alocacao dinamica
pthread_t pipe_thread;
int *id;						//alocacao dinamica
pid_t stat_pid;
int fd_pipe;

//variaveis globais simplehttpd.c
char buf[SIZE_BUF];
char req_buf[SIZE_BUF];
char buf_tmp[SIZE_BUF];
int port,socket_conn,new_conn;