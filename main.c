#include "header.h"
#include "simplehttpd.c"

//variaveis globais
Config *config;
Req_list rlist;
int config_sm_id, stat_sm_id;

void stat_manager(){
	#if DEBUG
	printf("Gestor de estatisticas!\n");
	#endif
}

void load_conf(){
	#if DEBUG
	printf("Entrou em load_conf!\n");
	#endif

	FILE *f;
	char line[50], aux[50];
	char *token, **strlist;
	int count=0, i=0;

	f = fopen ("config.txt", "r");
	if(f==NULL){
		perror("Erro na leitura do ficheiro!\n");
	}
	if(fscanf(f, "SERVERPORT=%d\n", &(config->port))!=1){
		perror("Error reading port!\n");
	}

	if(fscanf(f, "SCHEDULING=%s\n", line)!=1){
		perror("Error reading Scheduling!\n");
	}
	if(strcmp(line, "NORMAL")==0) config->sched=0;
	else if(strcmp(line, "ESTATICO")==0) config->sched=1;
	else if(strcmp(line, "COMPRIMIDO")==0) config->sched=2;

	if(fscanf(f, "THREADPOOL=%d\n", &(config->threadp))!=1){
		perror("Error reading threadpool!\n");
	}

	if(fscanf(f, "ALLOWED=%s\n", line)!=1){
		perror("Error reading allowed compressed files");
	}

	strcpy(aux, line);
	token=strtok(aux, ";");
	while(token != NULL){
		count++;
		token = strtok(NULL, ";");
	}

	strlist=(char**) malloc(count*sizeof(char*));

	token=strtok(line, ";");
	while(token != NULL){
		strlist[i]=strdup(token);
		token = strtok(NULL, ";");
		i++;
	}

	#if DEBUG
	printf("Port: %d\n",config->port);
	printf("Schedule: %d\n",config->sched);
	printf("Threadpool: %d\n",config->threadp);
	for(i=0; i<count; i++){
		printf("%s\n", strlist[i]);
	}
	#endif
	//todo: prints de debug com if
	fclose(f);
}

int run_http(){
	struct sockaddr_in client_name;
	socklen_t client_name_len = sizeof(client_name);
	int port, i;
	Req_list aux;

	signal(SIGINT,catch_ctrlc);

	rlist->req=NULL;
	rlist->next=NULL;
	rlist->prev=NULL;
	port=config->port;
	printf("Listening for HTTP requests on port %d\n",port);

	// Configure listening port
	if ((socket_conn=fireup(port))==-1)
		exit(1);

	// Serve requests 
	while (1)
	{
		// Accept connection on socket
		if ( (new_conn = accept(socket_conn,(struct sockaddr *)&client_name,&client_name_len)) == -1 ) {
			printf("Error accepting connection\n");
			exit(1);
		}

		// Identify new client
		identify(new_conn);

		// Process request
		get_request(new_conn);

		aux=rlist;
		while(aux->next!=NULL) aux=aux->next;

	//!!!alocacao dinamica? alocacao memoria partilhada??
		aux->next = (Req_list) malloc(sizeof(Req_list_node));
		(aux->next)->req= (Request*) malloc(sizeof(Request));
		((aux->next)->req)->page=strdup(req_buf);
		((aux->next)->req)->time_requested=time(NULL);
		(aux->next)->next=NULL;
		(aux->next)->prev=aux;

		// Verify if request is for a page or script
		if(!strncmp(req_buf,CGI_EXPR,strlen(CGI_EXPR)))
			execute_script(new_conn);	
		else
			// Search file with html page and send to client
			send_page(new_conn);

		((aux->next)->req)->time_answered=time(NULL);

	//teste:
		#if DEBUG
		aux=rlist;
		i=0;
		while(aux->next!=NULL){
			aux=aux->next;
			printf("Pedido %d: %s, recebido em '%s' tratado em '%s'\n", i, aux->req->page, asctime(gmtime(&(aux->req->time_requested))), asctime(gmtime(&(aux->req->time_answered))));
			i++;
		}
		#endif

		// Terminate connection with client 
		close(new_conn);

	}
}

void start_stat_process(){
	pid_t stat_pid;

	#if DEBUG
	printf("Entrou start_stat_process\n");
	#endif
	
	stat_pid = fork();
	if(stat_pid == -1){
		perror("Error while creating stat process");
	}
	else if(stat_pid == 0){
		#if DEBUG
		printf("Process for statistics created! PID of statistics: %ld\tPID of the father: %ld\n",(long)getpid(), (long)getppid());
		#endif
		stat_manager();
		#if DEBUG
		printf("processo de gestao saiu\n");
		#endif
		exit(0);
	}
}

void *worker_threads(){
	#if DEBUG
	printf("Entrou na worker da thread\n");
	#endif

	usleep(1000);
	pthread_exit(NULL);
}

void start_threads(){
	int i, size=config->threadp;
	pthread_t threads[size];
	int id[size];

	#if DEBUG
	printf("entrou start_threads\n");
	#endif

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

void start_sm(){

	stat_sm_id = shmget(IPC_PRIVATE, sizeof(Req_list_node), IPC_CREAT | 0766);
	if(stat_sm_id != -1){
		#if DEBUG
		printf("Stat shared mem ID: %d\n",stat_sm_id );
		#endif
		rlist = (Req_list) shmat(stat_sm_id,NULL,0);
	}
	else
		perror("Error sm stat!\n");

	config_sm_id = shmget(IPC_PRIVATE, sizeof(Config), IPC_CREAT | 0766);
	if(config_sm_id != -1){
		#if DEBUG
		printf("Config shared mem ID: %d\n",config_sm_id);
		#endif
		config = (Config*) shmat(config_sm_id,NULL,0);
	}
	else
		perror("Error sm config\n");
}

int main(){
	start_sm();
	load_conf();
	start_stat_process();
	start_threads();
	run_http();
	wait(NULL);
	return 0;
}