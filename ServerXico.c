#include "header.h"

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

//Shared Memory Variables
config *config_ptr;
statistic *stat_ptr;
int configid,statid;

void server_startup(){
	load_config();
	process_creator();
	thread_creator();
	stat_sm_creator();
}
void server_shutown(){
	pthread_mutex_destroy(&mutex);
}

void process_creator(){
	pid_t statistic_process;

	if(((statistic_process) = fork()) == 0){
		printf("Starting StatisticManager Process PID: %ld\n",(long)getpid());
		StatisticManager();
	}
	else if((statistic_process) < 0){
		printf("Error creating StatisticManager\n");
	}

	else
		perror("Error creating StatisticManager Process\n");

}

void load_config(){
	FILE *fp;
	fp = fopen("config.txt","r");
	char line[20];
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        
	if((configid = shmget(IPC_PRIVATE, sizeof(config),IPC_CREAT | 0766)) != -1){
		printf("Config ID: %d\n",configid);
		config_ptr = (config*) shmat(configid,NULL,0);
	}
	else
		perror("Error creating configid\n");
	
	fgets(line,20,fp);
	config_ptr->port = atoi(line);
	fgets(line,20,fp);
	config_ptr->scheduling = atoi(line);
	fgets(line,20,fp);
	config_ptr->nthreads = atoi(line);	

}

void thread_creator(){

	pthread_t threads[config_ptr->nthreads];
	int ids[config_ptr->nthreads];
	//creating threads
	for (int i=0;i<config_ptr->nthreads;i++){
			ids[i] = i;
			if(pthread_create(&threads[i],NULL,worker,&ids[i])==0)
				printf("Thread %d was created\n",ids[i]);
			else
				perror("Error creating threads\n");
	}
	//waiting for threads
	for (int i=0;i<config_ptr->nthreads;i++){
		pthread_join(threads[i],NULL);
		printf("Thread %d has joined\n",ids[i]);
	}
}

void stat_sm_creator(){

	if((statid = shmget(IPC_PRIVATE, sizeof(statistic), IPC_CREAT | 0766)) != -1){
		printf("STAT ID: %d\n",statid);
		stat_ptr = (statistic*) shmat(statid,NULL,0);
	}
	else
		perror("Error creating statid\n");
}

void StatisticManager(){
	printf("Hi, I'm StatisticManager Process\n");
	exit(0);
		
}

void *worker(void){
	sleep(2);
	printf("HI!\n");
	pthread_exit(NULL);
}

void Servidor(void){
	server_startup();
	server_shutown();
	wait(NULL);
}

int main(void){
	Servidor();
	exit(0);
}

