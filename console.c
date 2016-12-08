#include "header.h"
#include "functions.h"

int main(){
	int fd;
	int option;
	Message msg;
	if ((fd=open(PIPE_NAME, O_WRONLY)) < 0){
		perror("Cannot open pipe for writing: ");
		exit(1);
	}

	while(1){
		printf("\nSelect the configuration you want to change\n\n");
		printf("1-Scheduling\n2-Threadpool\n3-Allowed compressed files\n\n");

		option=get_int();
		switch(option){
			case 1:
				msg.type=option;
				printf("Pick scheduling type [NORMAL/ESTATICO/COMPRIMIDO]: ");
				break;		
			case 2:
				msg.type=option;
				printf("Pick the size of the threadpool [>0]: ");
				break;

			case 3:
				msg.type=option;
				printf("Pick the compressed files allowed\n");
				break;

			default:
				printf("Option nonexistent. Pick another\n");
				continue;	
		}
		fgets(msg.value, SIZE_BUF, stdin);
		remove_enter(msg.value);
		write(fd, &msg, sizeof(Message));
		printf("Command sent!\n");
	}
}