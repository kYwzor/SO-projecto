#include "header.h"

//variaveis globais
Config *config;
Stats *stat;
int config_sm_id, stat_sm_id;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void fecha_server(){
	pthread_mutex_destroy(&mutex);
}

void gestor_estatisticas(){
	printf("Gestor de estatisticas!\n");
	exit(0);
}

void load_conf(){
	printf("Loaf Config!\n");
	FILE *f;
	char line[50];

	f = fopen ("config.txt", "r");
	if(f==NULL){
		perror("Erro na leitura do ficheiro!\n");
		exit(0);
	}

	if(fscanf(f, "SERVERPORT=%d\n", &(config->port))!=1){
		perror("Error reading port!\n");
	}
	printf("Port: %d\n",config->port);

	if(fscanf(f, "SCHEDULING=%s\n", line)!=1){
		perror("Error reading Scheduling!\n");
	}
	if(strcmp(line, "NORMAL")==0) config->sched=0;
	else if(strcmp(line, "ESTATICO")==0) config->sched=1;
	else if(strcmp(line, "COMPRIMIDO")==0) config->sched=2;

	printf("Schedule: %d\n",config->sched);

	if(fscanf(f, "THREADPOOL=%d\n", &(config->threadp))!=1){
		perror("Error reading threadpool!\n");
	}
	printf("Threadpool: %d\n",config->threadp);

	//falta ler os ficheiros permitidos para um array que ainda nao esta criado na estrutura Config;

	fclose(f);
	exit(0);
}

void cria_processos(){
	pid_t stat_pid, config_pid;
	printf("Entrou cria processos\n");
	stat_pid = fork();
	if(stat_pid == -1){
		perror("Error while creating stat process");
	}
	else if(stat_pid == 0){
		printf("Process for statistics created! PID of statistics: %ld\tPID of the father: %ld\n",(long)getpid(), (long)getppid());
		gestor_estatisticas();
		printf("processo de gestao saiu\n");
	}
	config_pid = fork();
	if(config_pid == -1){
		perror("Error while creating stat process");
	}
	else if(config_pid == 0){
		printf("Process for config created! PID of config: %ld\tPID of the father: %ld\n",(long)getpid(), (long)getppid());
		load_conf();
		printf("processo de configuracao saiu\n");
	}
	printf("isto nao pode repetir\n");
}

void *worker_threads(void){
	printf("Entrou na worker da thread\n");
	usleep(1000);
	pthread_exit(NULL);
}

void cria_threads(){
	int i, size=config->threadp;
	pthread_t threads[size];
	int id[size];

	printf("entrei no cria threads\n");
	for(i = 0 ; i<size ;i++) {
		id[i] = i;
		if(pthread_create(&threads[i],NULL,worker_threads,&id[i])==0){
			printf("Thread %d criada!\n", i);
		}
		else
			perror("Erro a criar a thread!\n");
	}
	//espera pela morte das threads
	for (i=0;i<size;i++){
		if(pthread_join(threads[i],NULL) == 0){
			printf("Thread %d juntou-se\n",id[i]);
		}
		else{
			perror("Erro a juntar threads!\n");
		}
	}
}

void cria_sm(){

	stat_sm_id = shmget(IPC_PRIVATE, 1, IPC_CREAT | 0766);
	if( stat_sm_id != -1){
		printf("Stat shared mem ID: %d\n",stat_sm_id );
		int stat_ptr = shmat(stat_sm_id,NULL,0);
	}
	else
		perror("Error sm stat!\n");

	if((config_sm_id = shmget(IPC_PRIVATE, sizeof(Config),IPC_CREAT | 0766)) != -1){
		printf("Config shared mem ID: %d\n",config_sm_id);
		config = (Config*) shmat(config_sm_id,NULL,0);
	}
	else
		perror("Error sm config\n");
}


void liga_server(){
	setup_server();
	fecha_server();
}

void setup_server(){
	cria_sm();
	cria_processos();
	printf("apos cria processos\n");
	cria_threads();
	wait(NULL);
}

int main(){
	liga_server();
	return 0;
}
