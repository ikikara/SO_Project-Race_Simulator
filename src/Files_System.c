//João Carlos Borges Silva     nº 2019216763
//Mário Guilherme Lemos      nº2019216792

#include "Header.h"

//Inicia o programa
// - inicializa memória partilhada
// - inicializa os semáforos
// - abre o ficheiro "Logs.txt"
// - escreve no ficheiro "Logs.txt" indicando que o Simulador iniciou
//Não dá return
void comecus(){	
    char* filename=malloc(sizeof(char)* MAXCHAR);
    int* confs=malloc(sizeof(int)*9);
    
    sem_unlink("SHM");
	sem_unlink("LOG");
	
	SHM=sem_open("SHM", O_CREAT|O_EXCL, 0700,1);
	#ifdef DEBUG
		printf("Semaphore for shared memory created\n");
	#endif
	
	LOG=sem_open("LOG", O_CREAT|O_EXCL, 0700,1);
	#ifdef DEBUG
		printf("Semaphore for write logfile created\n");
	#endif
	
	logs=fopen("Logs.txt", "w+");
	#ifdef DEBUG
		printf("Logfile created\n\n");
	#endif

    printf("Introduce the filename of config file: ");
    fgets(filename, MAXCHAR , stdin);
    filename=strtok(filename,"\n");
	readfile(filename, confs);
	
    if((msqid=msgget(IPC_PRIVATE, IPC_CREAT|0777))<0){
    	perror("Creating message queue");
		exit(0);
	}
	
	#ifdef DEBUG
		printf("\nMessage queue created\n");
	#endif
    
	configs=malloc(sizeof(vals));
	
	data_from_configs(confs);
	#ifdef DEBUG
		printf("Data from Config.txt correctly readed\n\n");
	#endif
	
	create_shared_memory();
	
	writelogfile(" SIMULATOR STARTING\n");
	signal(SIGINT, ctrlcHandler);
	signal(SIGTSTP, ctrlzHandler);
	signal(SIGUSR1, SIG_IGN);
}

//Termina o programa
// - liberta a memória partilhada
// - escreve no ficheiro "Logs.txt" indicando que o Simulador terminou
// - fecha os semáforos
// - fecha o ficheiro "Logs.txt"
//Não dá return
void terminus(){
		writelogfile(" SIMULATOR CLOSING\n");
		
		close(fdN);
		unlink(PIPE_NAME);
		
		for(int i=0; i<shared_var->nteams; i++){
			kill(shared_var->teams[i].id_team, SIGKILL);
		}
		kill(shared_var->raceP, SIGKILL);
		kill(shared_var->malfunctionP, SIGKILL);
		
		sem_unlink("SHM");
		sem_unlink("LOG");
		msgctl(msqid, IPC_RMID, NULL);
		
		shared_var->finish=10;
		shmdt(shared_var);
		shmctl(shmid, IPC_RMID, NULL);
		
		fclose(logs);
		#ifdef DEBUG
			printf("Data Simulator removed\n");
		#endif
		
		exit(0);
}

//Ler ficheiro de configurações
// - filename -> nome do ficheiro de configurações ("Config.txt")
// - array -> para onde serão escritos os dados do ficheiro
//Não dá return
void readfile(char* filename, int* array ){
    FILE* file;
    char str[MAXCHAR];
    char* a;
    int line = 1;
    int elem;

    file=fopen(filename,"r");
    if(file==NULL){
    	char str[MAXCHAR];
		sprintf(str, " COULDN'T OPEN THE FILE: %s\n", filename);
		writelogfile(str);
        exit(1);
    }
    
    int i=0;
    int n=0;
    while (fgets(str, MAXCHAR, file) != NULL) {
    	elem=1;
        a = strtok( str, ", \n" );
        while( a != NULL ) {
            array[i++]=atoi_check(a, line, elem);
            if(i-1==3 && array[i-1]<3){
            	writelogfile(" ERROR: THE NUMBER OF TEAMS MUST BIGGER THAN 3\n");
            	exit(1);
            }      
            a = strtok( NULL, ", \n");
            elem++;
            n++;
        }
        line++;	
    }
    if(n>9){
        writelogfile(" ERROR: EXCESSIVE INFORMATION IN CONFIGS FILE\n");
		exit(1);
	}
	else if(n<9){
		writelogfile(" ERROR: MISSING INFORMATION IN CONFIGS FILE\n");
		exit(1);
	}
    fclose(file);
}

//Escreve no ficheiro "Logs.txt" a mensagem que for passada
// - message -> mensagem a ser escrita no ficheiro "Logs.txt"
//Não dá return
void writelogfile(char* message){
	sem_wait(LOG);
	char* hour=malloc(sizeof(char)*8);
	
	time_t hora=time(NULL);
	struct tm * now =localtime(&hora);
	
	sprintf(hour,"%d:%d:%d", now->tm_hour, now->tm_min, now->tm_sec);
	printf("%s%s",hour,message);
	
	fputs(hour, logs);
	fputs(message, logs);
	fflush(logs);
	sem_post(LOG);
}

//Cria a variável que vai ser colocada na memória partilhada
// - configs -> informação lida do ficheiro de configurações ("Config.txt")
//Dá return da variável criada (do tipo vals)
void data_from_configs(int* confs){
	configs->unittime=confs[0];
    configs->dlap=confs[1];
    configs->nlaps=confs[2];
    configs->nteams=confs[3];
    configs->nmaxcars=confs[4];
    configs->tmalfunction=confs[5];
    configs->tminmalfunction=confs[6];
    configs->tmaxmalfunction=confs[7];
    configs->tank=confs[8];
}

void create_shared_memory(){
	int size = sizeof(shm) + sizeof(team)*configs->nteams + sizeof(carro)*configs->nteams*configs->nmaxcars;
	if((shmid=shmget(IPC_PRIVATE, size ,IPC_CREAT | 0766))<0){
		perror("Error in shmget with IPC_GREAT\n");
		exit(1);
	}

	if((shared_var = (shm*) shmat(shmid,NULL,0))==(shm*)-1){
		perror("Shmat error!");
		exit(1);
	}
	#ifdef DEBUG
		printf("Shared Memory created\n");
	#endif
	

	shared_var->teams = (team*)(shared_var+1);
	shared_var->cars = (carro*)(shared_var->teams + configs->nteams);

	shared_var->table.malfunctions=0;
	shared_var->table.refuels=0;

	for(int i=0;i<configs->nteams;i++){
    	shared_var->teams[i].ncars=0;
    	shared_var->teams[i].box=0;
    	shared_var->teams[i].damaged=0;
    }
    
    for(int k=0; k<configs->nteams; k++){
    	if (pipe(shared_var->teams[k].fdU) == -1){
    		printf("An error as ocurred with opening the pipe\n");
    	}	
    }
	
	shared_var->condition=0;
	shared_var->nteams=0;
	shared_var->finish=0;
	shared_var->ctrlc=0;
	shared_var->id_simulator=getpid();
}

void clean_shared_memory(){
	shared_var->finish = 0;

    for(int i = 0; i<configs->nteams; i++){
    	shared_var->teams[i].id_team=0;
        shared_var->teams[i].name[0] = '\0';
        shared_var->teams[i].ncars=0;
            shared_var->teams[i].box=0;
    }
    for(int j = 0; j<configs->nteams*configs->nmaxcars; j++){
        shared_var->cars[j].team[0] = '\0';
        shared_var->cars[j].n=0;
        shared_var->cars[j].speed=0;
        shared_var->cars[j].consumption=0;
        shared_var->cars[j].reliability=0;
        shared_var->cars[j].state=0;
        shared_var->cars[j].space=0;
        shared_var->cars[j].laps=0;
        shared_var->cars[j].distance=0;
        shared_var->cars[j].time=0;
        shared_var->cars[j].needtogobox=false;
        shared_var->cars[j].tank=configs->tank;
        shared_var->cars[j].reserved=0;
        shared_var->cars[j].finished=0;
        shared_var->cars[j].stops=0;
    }
    
    shared_var->pos=0;
    shared_var->nteams=0;
    shared_var->condition = 0;
    shared_var->table.malfunctions = 0;
    shared_var->table.refuels = 0;
    shared_var->ctrlc=0;
}

void create_car(char* team, int num, int speed, float consump, int reliab){
	strcpy(shared_var->cars[posC_P].team, team);
	shared_var->cars[posC_P].n=num;
	shared_var->cars[posC_P].speed=speed;
	shared_var->cars[posC_P].consumption=consump;
	shared_var->cars[posC_P].reliability=reliab;
	shared_var->cars[posC_P].state=0;
	shared_var->cars[posC_P].space=1;
	shared_var->cars[posC_P].laps=0;
	shared_var->cars[posC_P].distance=0;
	shared_var->cars[posC_P].time=0;
	shared_var->cars[posC_P].needtogobox=false;
	shared_var->cars[posC_P].tank=configs->tank;
	shared_var->cars[posC_P].reserved=0;
	shared_var->cars[posC_P].stops=0;
	shared_var->cars[posC_P].finished=0;
	
	shared_var->pos++;
}

//Converter uma string numérica em inteiro
// - a -> string numérica
// - line -> line onde está escrita a string no ficheiro de configurações ("Config.txt")
// - elem -> ordem do elemento na line em questão
//Dá return do inteiro convertido
int atoi_check(char* a, int line, int elem){	
	if(isnumber(a)==1){
		return atoi(a);
	}
	else{
		char str[MAXCHAR];
		sprintf(str, " ERROR: IN LINE %d OF THE CONFIGS FILE IT WAS NOT POSSIBLE TO CONVERT THE %dº NUMBER\n", line, elem);
		writelogfile(str);
		exit(1);
	}
}

//Verificar se realmente a string é um número
// str -> string a verificar
//Dá return 1 caso seja um número
//Dá return 0 caso não seja um número
int isnumber(char *str) {
    while(*str != '\0'){
        if(!isdigit(*str)){	
            return 0;
        }
        str++;
    }
    return 1;
}
