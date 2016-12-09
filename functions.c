#include "functions.h"
#include "header.h"

void remove_enter(char* str){
	int len;
	len=strlen(str);
	if(len>0 && str[len-1] == '\n'){
		str[len-1] = '\0';
	}
}

int string_to_int(char *string){
	int i, aux;
	aux=0;
	for(i=0; string[i]!='\0'; i++){
		if(isdigit(string[i]))
			aux++;
	}
	if (aux!=i || i==0){
		return -1;
	}
	else if (strlen(string)>9){
		return -2;
	}
	else return atoi(string);
}

int get_int(){
	char string[SIZE_BUF];
	int numero, flag;

	do{
		flag=0;
		printf("Select an option: ");
		fgets(string, SIZE_BUF, stdin);
		remove_enter(string);
		numero=string_to_int(string);

		if (numero==-1){
			printf("Input can only be numbers.\n");
			flag=1;
		}
		else if (numero==-2){
			printf("Only numbers lower than 10 digits allowed as input.\n");
			flag=1;
		}
	}while(flag);

	return numero;
}

int get_scheduling_type(char* type){
	if(strcmp(type, "NORMAL")==0)
		return 0;
	else if(strcmp(type, "ESTATICO")==0)
		return 1;
	else if(strcmp(type, "COMPRIMIDO")==0)
		return 2;
	else
		return -1;
}