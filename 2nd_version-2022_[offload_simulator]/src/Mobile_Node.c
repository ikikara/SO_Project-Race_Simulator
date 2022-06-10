//João Carlos Borges Silva nº2019216753
//Pedro Afonso Ferreira Lopes Martins nº2019216826

#include <ctype.h>
#include <fcntl.h>
#include <malloc.h> 
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define PIPE_NAME "TASK_PIPE"
#define MAXCHAR 100

int isnumber(char *str) {
    while(*str != '\0'){
        if(!isdigit(*str)){	
            return 0;
        }
        str++;
    }
    return 1;
}

int atoi_check(char* a){	
	if(isnumber(a)){
		return atoi(a);
	}
	
	return -1;
}

int main(int argc, char **argv){
	char* tosend = malloc(sizeof(char)* MAXCHAR);
	int fd_named_pipe;
	int id = 1;
	int flag = 0;
	
	if(argc != 5){
		printf("ERROR: NUMBER OF ARGUMENTS ISN'T CORRECT, TRY AGAIN\n\n");
		printf("./mobile_node <NUMBER_OF_REQUESTS> <TIME_INTERVAL> <INSTRUCTIONS_PER_REQUEST> <MAX_TIME_OF_EXECUTION>\n");
		return 1;
	}
	
	for(int i=1; i<5; i++){
		if(atoi_check(argv[i]) < 0){
			printf("ERROR: THE %dº ARGUMENT ISN'T A NUMBER OR AN INVALID NUMBER (MUST BE BIGGER OR EQUAL 0)\n", i);
			flag = 1;
		}
	}
	
	if(flag){
		return 1;
	}
	
	if((fd_named_pipe = open(PIPE_NAME, O_RDWR))<0){
    	printf("ERROR: THE PIPE DOESN'T EXIST\n");
	}
	else{
		printf("COMMAND SUCCESSFULLY SENTED. NEW TASKS GONNA BE ADDED\n");
		for(int i=0; i < atoi(argv[1]); i++){
			sprintf(tosend, "%d_%d:%s:%s ", getpid(), id, argv[3], argv[4]);
			write(fd_named_pipe, tosend, strlen(tosend));
			printf("TASK %d_%d SENTED\n", getpid(), id++);
			usleep(atoi(argv[2])*1000);
		}
		close(fd_named_pipe);
	}
    
	return 0;
}
