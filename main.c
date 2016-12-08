#include "header.h"
#include "functions.h"

void stat_manager(){
	#if DEBUG
	printf("stat_manager: Stat manager started working\n");
	#endif
	while(1){
		#if DEBUG
		printf("stat_manager: Stat manager still running\n");
		#endif
		sleep(20);
	}
}

void load_conf(){
	#if DEBUG
	printf("load_conf: Loading configurations from config.txt\n");
	#endif

	FILE *f;
	char line[50], aux[50];
	char *token;
	int i, aux_int;

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

	aux_int=get_scheduling_type(line);
	if(aux_int==-1){
		perror("Type of scheduling not available");
		exit(1);
	}
	config->sched=aux_int;

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
	printf("Scheduling: %s\n",config->sched==0 ? "First in first out" : config->sched==1 ? "Prioritizing static files" : "Prioritizing compressed files");
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
	Req_list aux;

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

void *listen_console(){
	Message received;
	int aux,i;
	int flag;
	char str_aux[SIZE_BUF];
	char* token;
	while (1) {
    	read(fd_pipe, &received, sizeof(Message));
    	printf("listen_console: Received a command from the console\n");

    	switch(received.type){
    		case 1:
    			printf("listen_console: Console requested a change to the scheduling type.\n");
    			aux = get_scheduling_type(received.value);
    			if(aux==-1){
					printf("listen_console: Scheduling type received nonexistent. Ignoring command.\n");
					continue;
				}
				config->sched=aux;
				printf("listen_console: Scheduling type set to: '%s\n'.",config->sched==0 ? "First in first out" : config->sched==1 ? "Prioritizing static files" : "Prioritizing compressed files");
    			break;
    		case 2:
    			printf("listen_console: Console requested a change to the threadpool\n");
    			aux=string_to_int(received.value);
    			if(aux==0){
    				printf("listen_console: Threadpool size can't be 0. Ignoring command.\n");
    				continue;
    			}
    			else if(aux<0){
    				printf("listen_console: Threadpool size received is not valid. Ignoring command.\n");
    				continue;
    			}
    			config->threadp=aux;
    			printf("listen_console: Threadpool size set to %d.\n", aux);
    			break;
    		case 3:
    			printf("listen_console: Console requested a change to the allowed compressed files.\n");
				
				strcpy(str_aux, received.value);
				flag=0;
				token=strtok(str_aux, ";");
				for(aux=0; token != NULL; aux++){
					i=strlen(token);
					if(strcmp(token+i-3, ".gz")!=0){
						flag=1;
						break;
					}
					token = strtok(NULL, ";");
				}
				printf("passei aqui\n");

				if(flag){
					printf("listen_console: Invalid file names. Ignoring command.\n");
					continue;
				}
				printf("passei aqui1\n");
				free_allowed_files_array();
				config->nallowed=aux;
				config->allowed=(char**) malloc(config->nallowed*sizeof(char*));
				printf("passei aqui2\n");
				token=strtok(received.value, ";");
				for(i=0; token != NULL; i++){
					config->allowed[i]=strdup(token);
					token = strtok(NULL, ";");
				}
				printf("listen_console: %d files allowed: ", config->nallowed);
				for(i=0; i<config->nallowed; i++){
					printf("%s;", config->allowed[i]);
				}
				printf("\n");
    			break;
    		default:
    			printf("listen_console: Received command type not recognised. Ignoring command.\n");		//should never reach this point
    			continue;
    	}
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

	if(pthread_create(&pipe_thread,NULL,listen_console,NULL)==0){
		#if DEBUG
		printf("start_threads: Created pipe thread\n");
		#endif
	}
	else{
		perror("start_threads: Error creating pipe thread!\n");
		exit(1);
	}

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

void free_allowed_files_array(){
	int i;

	for (i=0; i<config->nallowed; i++){
		free((config->allowed)[i]);
	}
	free(config->allowed);
}

void free_all_alocations(){
	//free de todo o buffer
	Req_list aux;
	aux=rlist->next;
	free(rlist);
	rlist=aux;
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
	free_allowed_files_array();
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

void create_buffer(){
	rlist=(Req_list) malloc(sizeof(Req_list_node));
	rlist->req=NULL;
	rlist->next=NULL;
	rlist->prev=NULL;
}

void create_pipe(){
    // Creates the named pipe if it doesn't exist yet
	if ((mkfifo(PIPE_NAME, O_CREAT|O_EXCL|0600)<0) && (errno!= EEXIST)){
		perror("Error creating pipe\n");
		exit(1);
	}

    // Opens the pipe for reading
	if((fd_pipe=open(PIPE_NAME, O_RDWR)) < 0){
		perror("Cannot open pipe for reading: ");
		exit(1);
	}
}


int main(){
	signal(SIGINT,SIG_IGN);
	load_conf();
	start_sm();
	start_stat_process();
	create_pipe();
	start_threads();
	create_buffer();
	signal(SIGINT,catch_ctrlc);
	run_http();

	#if DEBUG
	printf("main: main process exited unexpectedly!\n");	
	#endif
	exit(1);
	
	return 0;
}