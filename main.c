#include "header.h"

void stat_manager(){
	signal(SIGINT,SIG_IGN);
	#if DEBUG
	printf("stat_manager: Stat manager started working\n");
	#endif
	while(1){
		#if DEBUG
		printf("stat_manager: Stat manager still running\n");
		#endif
		sleep(5);
	}
}

void load_conf(){
	#if DEBUG
	printf("load_conf: Loading configurations from config.txt\n");
	#endif

	FILE *f;
	char line[50], aux[50];
	char *token;
	int i;

	config = (Config*) malloc(sizeof(Config));

	f = fopen ("config.txt", "r");
	if(f==NULL){
		perror("Error reading config.txt!\n");
		exit(1);
	}
	if(fscanf(f, "SERVERPORT=%d\n", &(config->port))!=1){
		perror("Error reading port!\n");
		exit(1);
	}

	if(fscanf(f, "SCHEDULING=%s\n", line)!=1){
		perror("Error reading Scheduling!\n");
		exit(1);
	}

	if(strcmp(line, "NORMAL")==0)
		config->sched=0;
	else if(strcmp(line, "ESTATICO")==0)
		config->sched=1;
	else if(strcmp(line, "COMPRIMIDO")==0)
		config->sched=2;
	else{
		perror("Type of scheduling not available");
		exit(1);
	}


	if(fscanf(f, "THREADPOOL=%d\n", &(config->threadp))!=1){
		perror("Error reading threadpool!\n");
		exit(1);
	}

	if(fscanf(f, "ALLOWED=%s", line)!=1){
		perror("Error reading allowed compressed files!");
		exit(1);
	}

	strcpy(aux, line);
	token=strtok(aux, ";");
	for(config->nallowed=0; token != NULL; config->nallowed++)
		token = strtok(NULL, ";");


	config->allowed=(char**) malloc(config->nallowed*sizeof(char*));

	token=strtok(line, ";");
	for(i=0; token != NULL; i++){
		config->allowed[i]=strdup(token);
		token = strtok(NULL, ";");
	}


	#if DEBUG
	printf("__________Values read from config.txt__________\n");
	printf("Port: %d\n",config->port);
	printf("Scheduling: %s\n",config->sched==0 ? "FIFO" : config->sched==1 ? "Prioritizing static files" : "Prioritizing compressed files");
	printf("Threadpool: %d\n",config->threadp);
	printf("Allowed compressed files (%d files):\n", config->nallowed);
	for(i=0; i<config->nallowed; i++){
		printf(" -File %d: %s\n", i, config->allowed[i]);
	}
	printf("__________End of values read__________\n");
	#endif
	fclose(f);
}

void run_http(){
	#if DEBUG
	printf("run_http: Starting server\n");
	#endif

	struct sockaddr_in client_name;
	socklen_t client_name_len = sizeof(client_name);
	int port, i;
	Req_list rlist, aux;

	signal(SIGINT,catch_ctrlc);

	rlist=(Req_list) malloc(sizeof(Req_list_node));
	rlist->req=NULL;
	rlist->next=NULL;
	rlist->prev=NULL;
	port=config->port;
	printf("run_http: Listening for HTTP requests on port %d\n",port);

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

		aux->next = (Req_list) malloc(sizeof(Req_list_node));	//Cria nova node
		(aux->next)->req= (Request*) malloc(sizeof(Request));	//cria request
		((aux->next)->req)->time_requested=time(NULL);			//timestamp
		((aux->next)->req)->page=strdup(req_buf);				//guarda pagina pedida
		((aux->next)->req)->socket=new_conn;					//guarda socket
		((aux->next)->req)->answered=0;							//nao respondido por esta altura

		if(strstr(req_buf, ".gz")!=NULL)
			((aux->next)->req)->compressed=1;
		else
			((aux->next)->req)->compressed=0;

		(aux->next)->next=NULL;
		(aux->next)->prev=aux;

		// Verify if request is for a page or script
		if(!strncmp(req_buf,CGI_EXPR,strlen(CGI_EXPR)))
			execute_script(new_conn);	
		else
			// Search file with html page and send to client
			send_page(new_conn);

		((aux->next)->req)->time_answered=time(NULL);
		((aux->next)->req)->answered=1;

		#if DEBUG
		aux=rlist;
		i=0;
		printf("run_http: Printing request buffer\n");
		printf("__________Request buffer__________\n");
		while(aux->next!=NULL){
			aux=aux->next;
			printf("Request #%d:\n\tFile: %s\n\tType: %s\n\tSocket: %d\n\tTime requested: %s\tTime answered: %s\n", i, aux->req->page, aux->req->compressed ? "Compressed" : "Not compressed", aux->req->socket, asctime(gmtime(&(aux->req->time_requested))), asctime(gmtime(&(aux->req->time_answered))));
			i++;
		}
		printf("__________End of buffer__________\n");
		#endif

		// Terminate connection with client 
		close(new_conn);

	}
}

void start_stat_process(){
	#if DEBUG
	printf("start_stat_process: Starting stat process\n");
	#endif

	stat_pid = fork();
	if(stat_pid == -1){
		perror("start_stat_process: Error while creating stat process");
		exit(1);
	}
	else if(stat_pid == 0){
		#if DEBUG
		printf("start_stat_process: Stats process created! Stats PID: %ld, Father PID: %ld\n",(long)getpid(), (long)getppid());
		#endif

		stat_manager();

		#if DEBUG
		printf("start_stat_process: Stats process exited unexpectedly!\n");
		#endif
		exit(1);
	}
}

void *worker_threads(void *id_ptr){
	int id = *((int *)id_ptr);
	#if DEBUG
	printf("worker_threads: Thread %d working!\n", id);
	#endif
	usleep(1000);
	pthread_exit(NULL);
}

void start_threads(){
	#if DEBUG
	printf("start_threads: Starting %d threads\n", config->threadp);
	#endif

	int i, size=config->threadp;

	threads=(pthread_t*) malloc(size*sizeof(pthread_t));
	id=(int*) malloc(size*sizeof(int));

	for(i = 0 ; i<size ;i++) {
		id[i] = i;
		if(pthread_create(&threads[i],NULL,worker_threads,&id[i])==0){
			#if DEBUG
			printf("start_threads: Created thread #%d\n", i);
			#endif
		}
		else{
			perror("start_threads: Error creating threadpool!\n");
			exit(1);
		}
	}
}

void join_threads(){
	//espera pela morte das threads
	for (int i=0;i<config->threadp;i++){
		if(pthread_join(threads[i],NULL) == 0){
			#if DEBUG
			printf("Thread #%d joined.\n",id[i]);
			#endif
		}
		else{
			perror("Error joining threads!\n");
			exit(1);
		}
	}
	#if DEBUG
	printf("join_threads: All threads joined.\n");
	#endif
}

void start_sm(){
	#if DEBUG
	printf("start_sm: Starting shared memory\n");
	#endif

	stat_sm_id = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0777);
	if(stat_sm_id != -1){
		#if DEBUG
		printf("start_sm: Stat shared memory created - ID: %d\n",stat_sm_id);
		#endif
		temporario = (int*) shmat(stat_sm_id,NULL,0);
	}
	else{
		perror("start_sm: Error creating shared memory!\n");
		exit(1);
	}
}

void free_all_alocations(){
	int i;

	//free de todo o buffer
	Req_list aux;
	while(rlist!=NULL){
		free(rlist->req->page);
		free(rlist->req);
		aux=rlist->next;
		free(rlist);
		rlist=aux;
	}

	//free do espaco para pthread_t e int que sao usados pelos workers
	free(threads);
	free(id);

	//free da configuracao
	for (i=0; i<config->nallowed; i++){
		free((config->allowed)[i]);
	}
	free(config->allowed);
	free(config);

	#if DEBUG
	printf("free_all_alocations: All allocations were freed\n");
	#endif
}

// Closes socket before closing
void catch_ctrlc(int sig)
{
	printf("\nServer terminating\n");
	close(socket_conn);
	#if DEBUG
	printf("catch_ctrlc: Main process ended.\n");
	printf("catch_ctrlc: Calling join_threads...\n");
	#endif

	join_threads();

	#if DEBUG
	printf("catch_ctrlc: Killing stat process...\n");
	#endif

	kill(stat_pid, SIGTERM);

	#if DEBUG
	printf("catch_ctrlc: Stat process killed.\n");
	printf("catch_ctrlc: Deleting shared memory...\n");
	#endif

	shmdt(temporario);
	shmctl(stat_sm_id, IPC_RMID, NULL);

	#if DEBUG
	printf("catch_ctrlc: Shared memory deleted.\n");
	printf("catch_ctrlc: Freeing allocated memory\n");
	#endif

	free_all_alocations();

	#if DEBUG
	printf("catch_ctrlc: Everything was cleaned! Bye!\n");
	#endif

	exit(0);
}

int main(){
	load_conf();
	start_sm();
	start_stat_process();
	start_threads();
	run_http();

	#if DEBUG
	printf("main: main process exited unexpectedly!\n");	
	#endif
	exit(1);
	
	return 0;
}
