#include "header.h"

void remove_enter(char* str){
	int len;
	len=strlen(str);
	if(len>0 && str[len-1] == '\n'){
		str[len-1] = '\0';
	}
}

int get_int(){
	char string[SIZE_BUF];
	int i, aux, flag;

	do{
		flag=0;
		printf("Select an option: ");
		fgets(string, SIZE_BUF, stdin);
		remove_enter(string);

		aux=0;
		for(i=0; string[i]!='\0'; i++){
			if(isdigit(string[i]))
				aux++;
		}
		if (aux!=i || i==0){
			printf("Input can only be numbers.\n");
			flag=1;
		}
		else if (strlen(string)>9){
			printf("Only numbers lower than 10 digits allowed as input.\n");
			flag=1;
		}
	}while(flag);

	return atoi(string);
}

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

		//DO NOT MAKE THESE TESTS HERE, RECEIVING FUNCTION SHOULD TEST AND IGNORE ON FAILURE
		option=get_int();
		switch(option){
			case 1:
				msg.type=option;
				printf("Pick scheduling type [NORMAL/ESTATICO/COMPRIMIDO]: ");
				break;
//					if((strcmp(msg.value, "NORMAL")!=0) && (strcmp(msg.value, "ESTATICO")!=0) && (strcmp(msg.value, "COMPRIMIDO")!=0)){
//						printf("Scheduling type nonexistent. Pick another\n");				
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