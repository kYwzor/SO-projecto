/* 
 * -- simplehttpd.c --
 * A (very) simple HTTP server
 *
 * Sistemas Operativos 2014/2015
 */

#include "header.h"

// Processes request from client
void get_request(char* req_buf, int socket)
{
	char buf[SIZE_BUF];
	int i,j;
	int found_get;

	found_get=0;
	while ( read_line(buf,socket,SIZE_BUF) > 0 ) {
		if(!strncmp(buf,GET_EXPR,strlen(GET_EXPR))) {
			// GET received, extract the requested page/script
			found_get=1;
			i=strlen(GET_EXPR);
			j=0;
			while( (buf[i]!=' ') && (buf[i]!='\0') )
				req_buf[j++]=buf[i++];
			req_buf[j]='\0';
		}
	}	

	// Currently only supports GET 
	if(!found_get) {
		printf("get_request: Request from client without a GET. Not supported! Exiting process\n");
		exit(1);
	}
	// If no particular page is requested then we consider htdocs/index.html
	if(!strlen(req_buf))
		sprintf(req_buf,"index.html");

	#if DEBUG
	printf("get_request: client requested the following page: %s\n",req_buf);
	#endif

	return;
}


// Send message header (before html page) to client
void send_header(int socket)
{
	char buf[SIZE_BUF];
	#if DEBUG
	printf("send_header: sending HTTP header to client\n");
	#endif
	sprintf(buf,HEADER_1);
	if((send(socket,buf,strlen(HEADER_1),MSG_NOSIGNAL)==-1) && (errno!= EPIPE)){
		perror("send_header: Error sending HEADER_1");
		exit(1);
	}
	sprintf(buf,SERVER_STRING);
	if((send(socket,buf,strlen(SERVER_STRING),MSG_NOSIGNAL)==-1) && (errno!= EPIPE)){
		perror("send_header: Error sending SERVER_STRING");
		exit(1);
	}
	sprintf(buf,HEADER_2);
	if((send(socket,buf,strlen(HEADER_2),MSG_NOSIGNAL)==-1) && (errno!= EPIPE)){
		perror("send_header: Error sending HEADER_2");
		exit(1);
	}

	return;
}



// Execute script in /cgi-bin
void execute_script(int socket)
{
	// Currently unsupported, return error code to client
	cannot_execute(socket);
	
	return;
}
void send_compressed_page(char* page, int socket)
{
	int pid;
	char buf_tmp[SIZE_BUF], aux[SIZE_BUF];

	// Searchs for page in directory htdocs
	sprintf(buf_tmp,"htdocs/%s",page);

	#if DEBUG
	printf("send_compressed_page: searching for %s\n",buf_tmp);
	#endif

	if(access(buf_tmp, F_OK) == -1) {
		// File not found, send error to client
		printf("send_compressed_page: file %s not found, alerting client\n",buf_tmp);
		not_found(socket);
	}
	else{
		pid = fork();
		if(pid == 0){
			if(execlp("gunzip","gunzip", "-k","-f",buf_tmp,(char*)NULL)==-1){
				perror("send_compressed_page: Error extracting");
			}
		}
		else
			waitpid(pid,NULL,0);

		printf("send_compressed_page: %s extracted\n", page);
		strcpy(aux, page);
		aux[strlen(aux)-3]='\0';
		send_page(aux, socket);

		sprintf(buf_tmp,"htdocs/%s",aux);
		pid = fork();
		if(pid == 0){
			if(execlp("rm","rm",buf_tmp,(char*)NULL)==-1){
				perror("send_compressed_page: Error deleting file");
			}
		}
		else
			waitpid(pid,NULL,0);
	}
}


// Send html page to client
void send_page(char* page, int socket)
{
	FILE * fp;
	char buf_tmp[SIZE_BUF];

	// Searchs for page in directory htdocs
	sprintf(buf_tmp,"htdocs/%s",page);

	#if DEBUG
	printf("send_page: searching for %s\n",buf_tmp);
	#endif

	// Verifies if file exists
	if((fp=fopen(buf_tmp,"rt"))==NULL) {
		// Page not found, send error to client
		printf("send_page: page %s not found, alerting client\n",buf_tmp);
		not_found(socket);
	}
	else {
		// Page found, send to client 
	
		// First send HTTP header back to client
		send_header(socket);

		printf("send_page: sending page %s to client\n",buf_tmp);
		while(fgets(buf_tmp,SIZE_BUF,fp))
			if((send(socket,buf_tmp,strlen(buf_tmp),MSG_NOSIGNAL)==-1) && (errno!= EPIPE)){
				perror("send_page: Error sending page line");
				exit(1);	
			}
		
		// Close file
		fclose(fp);
	}

	return; 

}


// Identifies client (address and port) from socket
void identify(int socket)
{
	char ipstr[INET6_ADDRSTRLEN];
	socklen_t len;
	struct sockaddr_in *s;
	int port;
	struct sockaddr_storage addr;

	len = sizeof addr;
	getpeername(socket, (struct sockaddr*)&addr, &len);

	// Assuming only IPv4
	s = (struct sockaddr_in *)&addr;
	port = ntohs(s->sin_port);
	inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof ipstr);

	printf("identify: received new request from %s port %d\n",ipstr,port);

	return;
}


// Reads a line (of at most 'n' bytes) from socket
int read_line(char* buf, int socket,int n) 
{ 
	int n_read;
	int not_eol; 
	int ret;
	char new_char;

	n_read=0;
	not_eol=1;

	while (n_read<n && not_eol) {
		ret = read(socket,&new_char,sizeof(char));
		if (ret == -1) {
			perror("read_line: Error reading from socket");
			return -1;
		}
		else if (ret == 0) {
			return 0;
		}
		else if (new_char=='\r') {
			not_eol = 0;
			// consumes next byte on buffer (LF)
			if(read(socket,&new_char,sizeof(char))==-1){
				perror("read_line: Error reading from socket");
				return -1;
			}
			continue;
		}		
		else {
			buf[n_read]=new_char;
			n_read++;
		}
	}

	buf[n_read]='\0';
	#if DEBUG
	printf("read_line: new line read from client socket: %s\n",buf);
	#endif
	
	return n_read;
}


// Creates, prepares and returns new socket
int fireup(int port)
{
	int new_sock;
	struct sockaddr_in name;

	// Creates socket
	if ((new_sock = socket(PF_INET, SOCK_STREAM, 0))==-1) {
		printf("fireup: Error creating socket\n");
		return -1;
	}

	// Binds new socket to listening port 
 	name.sin_family = AF_INET;
 	name.sin_port = htons(port);
 	name.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(new_sock, (struct sockaddr *)&name, sizeof(name)) < 0) {
		printf("fireup: Error binding to socket\n");
		return -1;
	}

	// Starts listening on socket
 	if (listen(new_sock, 5) < 0) {
		printf("fireup: Error listening to socket\n");
		return -1;
	}
 
	return(new_sock);
}


// Sends a 404 not found status message to client (page not found)
void not_found(int socket)
{
	char buf[SIZE_BUF];
 	sprintf(buf,"HTTP/1.0 404 NOT FOUND\r\n");
	if((send(socket,buf, strlen(buf), MSG_NOSIGNAL)==-1) && (errno!= EPIPE)){
		perror("not_found: Error sending 404");
		exit(1);
	}
	sprintf(buf,SERVER_STRING);
	if((send(socket,buf, strlen(buf), MSG_NOSIGNAL)==-1) && (errno!= EPIPE)){
		perror("not_found: Error sending 404");
		exit(1);
	}
	sprintf(buf,"Content-Type: text/html\r\n");
	if((send(socket,buf, strlen(buf), MSG_NOSIGNAL)==-1) && (errno!= EPIPE)){
		perror("not_found: Error sending 404");
		exit(1);
	}
	sprintf(buf,"\r\n");
	if((send(socket,buf, strlen(buf), MSG_NOSIGNAL)==-1) && (errno!= EPIPE)){
		perror("not_found: Error sending 404");
		exit(1);
	}
	sprintf(buf,"<HTML><TITLE>Not Found</TITLE>\r\n");
	if((send(socket,buf, strlen(buf), MSG_NOSIGNAL)==-1) && (errno!= EPIPE)){
		perror("not_found: Error sending 404");
		exit(1);
	}
	sprintf(buf,"<BODY><P>Resource unavailable or nonexistent.\r\n");
	if((send(socket,buf, strlen(buf), MSG_NOSIGNAL)==-1) && (errno!= EPIPE)){
		perror("not_found: Error sending 404");
		exit(1);
	}
	sprintf(buf,"</BODY></HTML>\r\n");
	if((send(socket,buf, strlen(buf), MSG_NOSIGNAL)==-1) && (errno!= EPIPE)){
		perror("not_found: Error sending 404");
		exit(1);
	}

	return;
}


// Send a 5000 internal server error (script not configured for execution)
void cannot_execute(int socket)
{
	char buf[SIZE_BUF];
	sprintf(buf,"HTTP/1.0 500 Internal Server Error\r\n");
	if((send(socket,buf, strlen(buf), MSG_NOSIGNAL)==-1) && (errno!= EPIPE)){
		perror("not_found: Error sending 5000 internal server error");
		exit(1);
	}
	sprintf(buf,"Content-type: text/html\r\n");
	if((send(socket,buf, strlen(buf), MSG_NOSIGNAL)==-1) && (errno!= EPIPE)){
		perror("not_found: Error sending 5000 internal server error");
		exit(1);
	}
	sprintf(buf,"\r\n");
	if((send(socket,buf, strlen(buf), MSG_NOSIGNAL)==-1) && (errno!= EPIPE)){
		perror("not_found: Error sending 5000 internal server error");
		exit(1);
	}
	sprintf(buf,"<P>Error prohibited CGI execution.\r\n");
	if((send(socket,buf, strlen(buf), MSG_NOSIGNAL)==-1) && (errno!= EPIPE)){
		perror("not_found: Error sending 5000 internal server error");
		exit(1);
	}

	return;
}