//João Carlos Borges Silva nº2019216753
//Pedro Afonso Ferreira Lopes Martins nº2019216826

#include "Header.h"

int main(int argc, char** argv){
	if(argc != 2){
		printf("ERROR: NUMBER OF ARGUMENTS ISN'T CORRECT, TRY AGAIN\n\n");
		printf("./offload_simulator <CONFIG_FILENAME>\n");
		return 1;
	}

	starting(argv[1]);
	signal(SIGINT, terminus);
	signal(SIGTSTP, statistics);
	signal(SIGQUIT, queue);
	
	writelogfile(" OFFLOAD SIMULATOR STARTING\n", 1);
	simulator();
	
	return 0;
}
