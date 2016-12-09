#include "header.h"
#include "functions.h"

pthread_mutex_t buffer_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t request_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_var = PTHREAD_COND_INITIALIZER;

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
	int port;
	Req_list aux;
	Request* new_req;

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

		new_req = (Request*) malloc(sizeof(Request));			//cria request
		new_req->time_requested=time(NULL);						//timestamp

		strcpy(new_req->page, req_buf);							//guarda pagina pedida
		new_req->socket=new_conn;								//guarda socket
		new_req->read=0;										//nao lido por esta altura
		if(strstr(req_buf, ".gz")!=NULL)						//verifica se e comprimido
			new_req->compressed=1;
		else
			new_req->compressed=0;

		pthread_mutex_lock(&buffer_mutex);
		aux=rlist;
		while(aux->next!=NULL) aux=aux->next;
		aux->next = (Req_list) malloc(sizeof(Req_list_node));	//cria nova node
		(aux->next)->req=new_req;								//coloca request na nova node
		(aux->next)->next=NULL;
		(aux->next)->prev=aux;
		pthread_mutex_unlock(&buffer_mutex);

//		#if DEBUG
//		aux=rlist;
//		int i=0;
//		printf("run_http: Printing request buffer\n");
//		printf("__________Request buffer__________\n");
//		while(aux->next!=NULL){
//			aux=aux->next;
//			printf("Request #%d:\n\tFile: %s\n\tType: %s\n\tSocket: %d\n\tTime requested: %s\tTime answered: %s\n", i, aux->req->page, aux->req->compressed ? "Compressed" : "Not compressed", aux->req->socket, asctime(gmtime(&(aux->req->time_requested))), asctime(gmtime(&(aux->req->time_answered))));
//			i++;
//		}
//		printf("__________End of buffer__________\n");
//		#endif

		// Terminate connection with client 
		//close(new_conn);

	}
}
void *worker_threads(void *id_ptr){
	int id = *((int *)id_ptr);
	#if DEBUG
	printf("worker_threads: Thread %d working!\n", id);
	#endif
	while(1){
		pthread_mutex_lock(&request_mutex);
		while(next_request==NULL){
			printf("worker_threads: Thread %d waiting for a request!\n", id);
			pthread_cond_wait(&cond_var, &request_mutex);
			if(exit_thread_flag==1){
				pthread_mutex_unlock(&request_mutex);
				pthread_exit(NULL);
			}
		}
		printf("worker_threads: Thread %d received a request!\n", id);

		// Verify if request is for a page or script
		if(!strncmp(next_request->page,CGI_EXPR,strlen(CGI_EXPR)))
			execute_script(next_request->socket);	
		else
			// Search file with html page and send to client
			send_page(next_request->socket);

		//MUTEXES COM SHARED MEMORY!!!!!
		strcpy(shared_request->page, next_request->page);
		shared_request->compressed = next_request->compressed;
		shared_request->socket = next_request->socket;
		shared_request->time_requested = next_request->time_requested;
		shared_request->read = next_request->read;
		next_request->time_answered=time(NULL);

		close(next_request->socket);

		//free all request stuff before setting it to NULL!!!!
		next_request=NULL;
		pthread_mutex_unlock(&request_mutex);
		if(exit_thread_flag==1){
			pthread_exit(NULL);
		}
	}
}

void *scheduler(){
	Req_list aux, prev_aux, next_aux;
	while(1){
		pthread_mutex_lock(&buffer_mutex);
		aux=rlist->next;
		if(aux!=NULL){
			printf("scheduler: Found unanswered request on buffer.\n");
			//mutex for config needed???
			switch(config->sched){
				case 0:
					pthread_mutex_lock(&request_mutex);
					next_request=aux->req;
					pthread_cond_signal(&cond_var);
					pthread_mutex_unlock(&request_mutex);

					prev_aux = aux->prev;
					next_aux = aux->next;

					prev_aux->next = next_aux;
					if(next_aux!=NULL)
						next_aux->prev = prev_aux;

					free(aux);

					break;

				case 1:
					break;

				case 2:
					break;
			}
		}
		pthread_mutex_unlock(&buffer_mutex);
		if(exit_thread_flag==1){
			pthread_exit(NULL);
		}
	}
}

void *listen_console(){
	Message received;
	int aux,i;
	int flag;
	char str_aux[SIZE_BUF];
	char* token;
	fd_set read_set;
	struct timeval tv;
	tv.tv_sec=1;
	tv.tv_usec=0;

	while (1) {
		FD_ZERO(&read_set);
		FD_SET(fd_pipe, &read_set);
		if(select(fd_pipe+1, &read_set, NULL, NULL, &tv)>0){
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
					printf("listen_console: Scheduling type set to: '%s'.\n",config->sched==0 ? "First in first out" : config->sched==1 ? "Prioritizing static files" : "Prioritizing compressed files");
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

					if(flag){
						printf("listen_console: Invalid file names. Ignoring command.\n");
						continue;
					}

					free_allowed_files_array();
					config->nallowed=aux;
					config->allowed=(char**) malloc(config->nallowed*sizeof(char*));

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
	    if(exit_thread_flag==1){
	    	pthread_exit(NULL);
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

void start_threads(){
	#if DEBUG
	printf("start_threads: Starting %d threads\n", config->threadp);
	#endif
	int i, size=config->threadp;

	exit_thread_flag=0;

	if(pthread_create(&pipe_thread,NULL,listen_console,NULL)==0){
		#if DEBUG
		printf("start_threads: Created pipe thread\n");
		#endif
	}
	else{
		perror("start_threads: Error creating pipe thread!\n");
		exit(1);
	}

	if(pthread_create(&scheduler_thread,NULL,scheduler,NULL)==0){
		#if DEBUG
		printf("start_threads: Created scheduler thread\n");
		#endif
	}
	else{
		perror("start_threads: Error creating scheduler thread!\n");
		exit(1);
	}

	threads=(pthread_t*) malloc(size*sizeof(pthread_t));
	id=(int*) malloc(size*sizeof(int));

	for(i=0; i<size; i++){
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
	exit_thread_flag=1;
	if(pthread_join(pipe_thread,NULL) == 0){
			#if DEBUG
			printf("Pipe thread joined.\n");
			#endif
	}
	else{
		perror("Error joining pipe thread!\n");
		exit(1);
	}

	if(pthread_join(scheduler_thread,NULL) == 0){
			#if DEBUG
			printf("Scheduler thread joined.\n");
			#endif
	}
	else{
		perror("Error joining scheduler thread!\n");
		exit(1);
	}
	pthread_cond_broadcast(&cond_var);
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

	stat_sm_id = shmget(IPC_PRIVATE, sizeof(Request), IPC_CREAT | 0777);
	if(stat_sm_id != -1){
		#if DEBUG
		printf("start_sm: Stat shared memory created - ID: %d\n",stat_sm_id);
		#endif
		shared_request = (Request*) shmat(stat_sm_id,NULL,0);
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
	printf("catch_ctrlc: Socket closed.\n");
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

	shmdt(shared_request);
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
	create_buffer();
	start_threads();
	signal(SIGINT,catch_ctrlc);
	run_http();

	#if DEBUG
	printf("main: main process exited unexpectedly!\n");	
	#endif
	exit(1);
	
	return 0;
}