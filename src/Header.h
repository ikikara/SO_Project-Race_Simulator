//João Carlos Borges Silva     nº 2019216763
//Mário Guilherme Lemos      nº2019216792

//Bibliotecas e definições usadas
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <pthread.h>
#include <semaphore.h> 
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>


#define MAXCHAR 100 //nº máximo de caracteres lido por linha do ficheiro de configurações ("Config.txt")
                    //nº máximo de caracteres do nome do ficheiro de configurações ("Config.txt")
                    //nº máximo de caracteres numa mensagem a ser enviada para o ficheiro log
             
#define PIPE_NAME "Info"

#define DEBUG

//____________________________________________________________________
//Struct da memória partilhada
typedef struct{
	int unittime;
	int dlap;
	int nlaps;
	int nmaxcars;
	int nteams;
	int tmalfunction;
	int tminmalfunction;
	int tmaxmalfunction;
	float tank;
}vals;

//Struct de um carro
typedef struct{
	pthread_t id_carro;
	char team[25];
	int n;
	int speed;
	float consumption;
	int reliability;
	int state;   // 0 - Corrida / 1 - Segurança / 2 - Box / 3 - Desistência / 4 - Terminado 
	int space;
	float distance;
	int laps;
	int stops;
	float tank;
	float time;
	bool needtogobox;
	int reserved;
	int finished;
}carro;

//Struct de uma team
typedef struct{
	pid_t id_team;
	int ncars;
	char name[20];
	int * positions;
	int box;  // 0 - Livre / 1 - Reservado / 2 - Ocupado
	int damaged; // 0 - Nenhuma malfunction detetada / 1 - malfunction detetada 
	int fdU[2];
}team;

typedef struct{
	int firstN;
	char firstS[20];
	int malfunctions;
	int refuels;
	int carsinrace;
}statistics;

typedef struct{
	int pos;  // Posição do próximo carro (ou número de cars)
	int nteams; 
	statistics table;
	int finish;
	int ctrlc;  // Variável que vai ser usada como condição para poder o ctrlc em certos casos (ou para quando se pode receber comandos)
	int condition; // Variável que vai ser usada para iniciar a corrida
	pid_t id_simulator;
	pid_t raceP;
	pid_t malfunctionP;
	
	team * teams;
	carro * cars;
}shm;


//Struct da mensagem na message queue
typedef struct{
	long mtype;
	char something[MAXCHAR];
}message;

//___________________________________________________________________
//Variáveis para shm, semáforos, fd's, FILE.txt, pid, message queue
vals * configs;        // Configurations

int shmid;             // Shared Memory
shm * shared_var;

sem_t * LOG; 		  // Semaphores
sem_t * SHM; 			

FILE* logs;			//file.txt (Config.txt)

int fdN;     // File descriptor of Named Pipe

message buf;    // Message Queue
int msqid;    


int posT_P;  //Indica a próxima posição livre para criar uma team (também usado para a team saber em que posição se encontra no array de teams)
int posC_P;  //Indica a próxima posição livre para criar um carro

//___________________________________________________________________
//Funcs_Procs
void race_manager();

void team_manager();

void malfunction_manager();

//____________________________________________________________________
//Simulation.c
void simulator();

int verify_can_creat_new_team();

int verify_already_team_created(char* name);

int verify_can_creat_new_car(team team);

int verify_already_car_created(team team, int n);

void verify_enter_in_box();

int finish_himmmmm();

void search_Pos(int* positions, int nr);

void* worker(void* idp);

int verifyCADD(char command[11][(int)MAXCHAR/2]);

int isfloat(char *str);

void sigusr1handler(int sig);

void ctrlcHandler(int sig);

void ctrlzHandler(int sig);

//____________________________________________________________________
//Files_System.c
void comecus();

void terminus();

void readfile(char* filename, int* array );

void writelogfile(char* message);

void data_from_configs(int* configs);

void create_shared_memory();

void clean_shared_memory();

void create_car(char* team, int num, int speed, float consump, int reliab);

int atoi_check(char* a, int linha, int elem);

int isnumber(char *str);


