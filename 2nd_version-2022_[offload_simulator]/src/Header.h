
//João Carlos Borges Silva nº2019216753
//Pedro Afonso Ferreira Lopes Martins nº2019216826

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h> 
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define MAXCHAR 100
#define MAXTIME_VERYBIG 99999999;
#define MAXINSTRUCTIONS_VERYBIG 99999999;
#define PIPE_NAME "TASK_PIPE"
//#define DEBUG

#define DEBUG_PRINT(text)\
	printf(GREEN);\
	printf(text);\
	printf(DEFAULT);\

#define RED "\033[0;31m"
#define GREEN "\033[0;32m"
#define YELLOW "\033[0;33m"
#define BLUE "\033[0;36m"
#define DEFAULT "\033[0m"

typedef struct{
	int nr_slots;
	int max_wait;
	int nr_edge_servers;
}vals;

typedef struct{
	int tasks_executed;
	double average_time_to_execute;
	int tasks_not_executed;
}stats;

typedef struct{
	char id[MAXCHAR];  		
	int nr_instructions;	
	int time_exec;
	int time_arriv;			//gonna be a count seconds since the year 0
}task;

typedef struct{
	char name[(int)(MAXCHAR/2)];
	int vCPU[2];
	
	pid_t edge_server_ID;
	pthread_t vCPU_ID[2];
	int vCPU_availability[2];    //0 - Free             | 1 - Not Free    | 2 - Off
	int fdU[2];
	int state; 			  		 //0 - Stopped 			| 1 - Working
	
	int tasks_executed;
	int malfunction_operations;
	pthread_mutex_t mutex_vCPU[2];
}server;

typedef struct{
	vals* configs;
	server* servers;
	stats statistics;
	
	int actual_queue_size;
	int queue_size;
	task* queue;
	
	pid_t task_manager_ID;
	pid_t monitor_ID;
	pid_t maintenance_manager_ID;
	pid_t system_manager_ID;
	
	pthread_t scheduler;
	pthread_t dispatcher;
	
	pthread_cond_t dispat;
	pthread_mutex_t mutex_dispat;
	
	int monitor_high;
	int f;
	int fT;
	float min_wait;
}shm;

typedef struct{
	long mtype;			//-1 - maintenance manager 
					    //0, 1, 2, 3, ... - servers 
	char something[1];     
}msq;

//___________________________________________________________________
//Variables like shm, semaphores, fd's, FILE.txt, pid, message queue
vals* configs;

int shmid;           // Shared Memory
shm* shared_var;

sem_t * LOG; 		 // Semaphores
sem_t * SHM;
sem_t * START; 	
sem_t * MONITOR;
sem_t * MAINTENANCE;
sem_t * FINISHING;
sem_t * SERVER_MAINTENANCE;

pthread_cond_t sched;						//Conditional variables and mutexes
pthread_mutex_t mutex_sched;
pthread_condattr_t attrcond;
pthread_mutexattr_t attrmutex;


FILE* logs;			  // File.txt (Config.txt)

int fd_named_pipe;   // File Descriptor of Named Pipe
      
int msqid; 			 // Message Queue
msq MQ;   

//____________________________________________________________________
//Files_System.c
void starting(char* config_filename);

void terminus();

char*** readfile(char* filename, int* confs);

void writelogfile(char* message, int color);

void data_from_configs(int* confs);

void create_shared_memory(int* confs, char*** servers);

int atoi_check(char* a, int line);

int isnumber(char *str);

//____________________________________________________________________
//Simulation.c
void simulator();

bool verify_and_add(char* command);

bool verify_command(char* command);

void* vCPU(void* info);

void* scheduler();

void* dispatcher();

int comparator_tasks(const void *task1, const void *task2);

void statistics();

void queue();

//____________________________________________________________________
//Funcs_Procs.c
void task_manager();

void edge_server();

void monitor();

void maintenance_manager();

