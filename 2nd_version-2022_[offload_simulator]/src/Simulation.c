//João Carlos Borges Silva nº2019216753
//Pedro Afonso Ferreira Lopes Martins nº2019216826

#include "Header.h"

void simulator(){
	shared_var->system_manager_ID = getpid();
	
	if(shared_var->system_manager_ID == getpid()){
		if(fork() == 0){
			if(shared_var->system_manager_ID != getpid()){
				shared_var->task_manager_ID = getpid();
				task_manager();
				
				exit(0);
			}
		}
	}
	
	if(shared_var->system_manager_ID == getpid()){
		if(fork() == 0){
			if(shared_var->system_manager_ID != getpid()){
				shared_var->monitor_ID = getpid();
				monitor();
					
				exit(0);
			}
		}
	}
	
	if(shared_var->system_manager_ID == getpid()){
		if(fork() == 0){
			if(shared_var->system_manager_ID != getpid()){	
				shared_var->maintenance_manager_ID = getpid();
				maintenance_manager();
				
				exit(0);
			}
		}
	}
	
	// wait for all created processes
	if(shared_var->system_manager_ID == getpid()){
		for(int i=0; i<3; i++){
			wait(NULL);
		}
	}
}

bool verify_and_add(char* command){
	char* part;
	time_t now_time=time(NULL);
	struct tm * now =localtime(&now_time);

	//Será que deveria fazer uma avaliação antes de adicionar?
	//Às vezes é possível adicionar tasks porque algumas da queue (quando ela está cheia)
	//Já passaram o prazo temporal
	pthread_cond_signal(&sched);
	sem_wait(START);
	
	if(shared_var->actual_queue_size == shared_var->queue_size){
		return false;
	}
	
	for(int i=0; i<shared_var->queue_size; i++){
		if(!strcmp(shared_var->queue[i].id, " ")){
			sem_wait(SHM);
			part = strtok(command, ":");
			strcpy(shared_var->queue[i].id, part);
			part = strtok(NULL, ":");
			shared_var->queue[i].nr_instructions = atoi(part);
			part = strtok(NULL, ":");
			shared_var->queue[i].time_exec = atoi(part);
			shared_var->queue[i].time_arriv = now->tm_mday*24*60*60 + now->tm_hour*60*60 + now->tm_min*60 + now->tm_sec;
			shared_var->actual_queue_size++;
			sem_post(SHM);
			
			
			break;
		}
	}
	
	//printf("%d\n", shared_var->actual_queue_size);
	
	if((float)shared_var->actual_queue_size / (float)shared_var->queue_size > 0.8 && shared_var->monitor_high == 0 && shared_var->min_wait > shared_var->configs->max_wait){
		//puts("1");
		sem_post(MONITOR);
		sem_wait(START);
	}
	
	if((float)shared_var->actual_queue_size / (float)shared_var->queue_size <= 0.2 && shared_var->monitor_high == 1){
		//puts("2");
		sem_post(MONITOR);
		sem_wait(START);
	}
	
	pthread_cond_signal(&shared_var->dispat);
	sem_wait(START);
	//pthread_cond_signal(&sched);
	
	return true;
}

bool verify_command(char* command){
	char* part;
	int count = 0;
	char command_normal[MAXCHAR];
	strcpy(command_normal, command);
	
	part = strtok(command_normal, ":");
	if(!strcmp(part, command)){
		return false;
	}
	
	count++;
	while(part){
		if(count > 3){
			return false;
		}
	
		if((part = strtok(NULL, ":")) != NULL){
			if(atoi_check(part, -1) < 0){
				return false;
			}
		}
		
		count++;
	}
	
	if(count < 3){
		return false;
	}
	
	return true;
}


void* vCPU(void* info){ 
	int* index = (int*) info;
	char bufferRU[2*MAXCHAR];
	char str[2*MAXCHAR];
	char task_info [2][MAXCHAR];
	char* part;

	//printf("ME: %d | VCPU CREATED BY %s\n", getpid(), shared_var->servers[index].name);
	while(shared_var->f != 1){
		pthread_mutex_lock(&shared_var->servers[index[0]].mutex_vCPU[index[1]]);
		if(shared_var->f == 1){
			sem_wait(SHM);
			shared_var->servers[index[0]].vCPU_availability[index[1]] = 2;
			sem_post(SHM);
			
			sem_post(FINISHING);
			break;
		}
		
		read(shared_var->servers[index[0]].fdU[0], bufferRU, sizeof(bufferRU));
		
		part = strtok(bufferRU, ":");
		strcpy(task_info[0], part);
		part = strtok(NULL, ":");
		strcpy(task_info[1], part);
		
		usleep(atof(task_info[1])*1000*1000);
		
		sprintf(str, " %s: %s COMPLETED\n", shared_var->servers[index[0]].name, task_info[0]);
		sem_wait(SHM);
		shared_var->servers[index[0]].tasks_executed++;
		shared_var->statistics.tasks_executed++;
		sem_post(SHM);
		
		writelogfile(str, 1);
		
		if(shared_var->servers[index[0]].state == 0){
			sem_post(SERVER_MAINTENANCE);
		}
		
		if(shared_var->f == 1){
			sem_wait(SHM);
			shared_var->servers[index[0]].vCPU_availability[index[1]] = 2;
			sem_post(SHM);
			
			sem_post(FINISHING);
			break;
		}
		
		sem_wait(SHM);
		shared_var->servers[index[0]].vCPU_availability[index[1]] = 0;
		sem_post(SHM);
		
		pthread_cond_signal(&shared_var->dispat);
		
	}
	
	pthread_exit(NULL);
	return NULL;
}

void* scheduler(){
	//printf("ME: %d | TASK SCHEDULER CREATED\n", getpid());
	time_t now_time;
	struct tm * now;
	int time_now;
	char str[2*MAXCHAR];
	
	sem_wait(START);
	
	pthread_mutex_lock(&mutex_sched);
	while(shared_var->f != 1){	
		pthread_cond_wait(&sched, &mutex_sched);
		if(shared_var->f == 1){
			break;
		}
		
		now_time=time(NULL);
		now =localtime(&now_time);
		time_now = now->tm_mday*24*60*60 + now->tm_hour*60*60 + now->tm_min*60 + now->tm_sec;
		
		// Threads evalution	
		for(int i=0; i < shared_var->queue_size; i++){
			if(strcmp(shared_var->queue[i].id, " ") != 0){
				if(time_now >= shared_var->queue[i].time_exec + shared_var->queue[i].time_arriv){
					sprintf(str, " SCHEDULER: THE DEADLINE FOR TASK %s EXPIRED, REMOVING FROM QUEUE\n", shared_var->queue[i].id);
					writelogfile(str, 2);
					
					sem_wait(SHM);
					strcpy(shared_var->queue[i].id, " ");
					shared_var->queue[i].time_exec = MAXTIME_VERYBIG;
					shared_var->queue[i].nr_instructions = MAXINSTRUCTIONS_VERYBIG;
					shared_var->queue[i].time_arriv = 0;
					shared_var->actual_queue_size--;
					shared_var->statistics.tasks_not_executed++;
					sem_post(SHM);
				}
			}
		}
		
		sem_wait(SHM);
		qsort((void*)shared_var->queue, shared_var->queue_size, sizeof(shared_var->queue[0]), comparator_tasks);
		sem_post(SHM);
		
		sem_post(START);
	}
	pthread_mutex_unlock(&mutex_sched);
	
	pthread_exit(NULL);
	return NULL;
}

void* dispatcher(){
	//printf("ME: %d | TASK SCHEDULER CREATED\n", getpid());
	char task_info[2*MAXCHAR];
	int breakk;
	time_t now_time;
	struct tm * now;
	int time_now;
	float time_to_execute_task;
	float time_to_finish_execute_task;
	char str[2*MAXCHAR];
	
	sem_wait(START);
	
	pthread_mutex_lock(&shared_var->mutex_dispat);
	while(shared_var->f != 1){
		pthread_cond_wait(&shared_var->dispat, &shared_var->mutex_dispat);
		if(shared_var->f == 1){
			break;
		}
		
		for(int i=0; i < shared_var->queue_size; i++){
			now_time=time(NULL);
			now =localtime(&now_time);
			time_now = now->tm_mday*24*60*60 + now->tm_hour*60*60 + now->tm_min*60 + now->tm_sec;
		
			if(strcmp(shared_var->queue[i].id, " ") != 0){
				for(int j=0; j < shared_var->configs->nr_edge_servers; j++){
					//if the server is not off(maintenance)		
					if(shared_var->servers[j].state){
						for(int k=0; k<2; k++){
							time_to_execute_task = (float)(shared_var->queue[i].nr_instructions)/(float)(shared_var->servers[j].vCPU[k]*1000);
							time_to_finish_execute_task = (float)time_now + time_to_execute_task;
							
							//if a vCPU is free 
							if(shared_var->servers[j].vCPU_availability[k] == 0){
								//and have time to execute the task
							 	if(time_to_finish_execute_task <= (float)(shared_var->queue[i].time_exec + shared_var->queue[i].time_arriv)){
							 		sprintf(str, " DISPATCHER: %s SELECTED FOR EXECUTION ON %s\n", shared_var->queue[i].id, shared_var->servers[j].name);
									sprintf(task_info, "%s:%f", shared_var->queue[i].id, (float)(shared_var->queue[i].nr_instructions)/(float)(shared_var->servers[j].vCPU[k]*1000));
									writelogfile(str, 1);
									
									pthread_mutex_unlock(&shared_var->servers[j].mutex_vCPU[k]);		//tell the vCPU that he can receive the task
									write(shared_var->servers[j].fdU[1], task_info, sizeof(task_info));
								
									sem_wait(SHM);
									if(time_to_execute_task > shared_var->min_wait){
										shared_var->min_wait = time_to_execute_task;
									}
									shared_var->servers[j].vCPU_availability[k] = 1;		//the vCPU is not free anymore
									strcpy(shared_var->queue[i].id, " ");
									shared_var->queue[i].time_exec = MAXTIME_VERYBIG;
									shared_var->queue[i].nr_instructions = MAXINSTRUCTIONS_VERYBIG;
									shared_var->queue[i].time_arriv = 0;
									shared_var->actual_queue_size--;
									sem_post(SHM);
									
									if((float)shared_var->actual_queue_size / (float)shared_var->queue_size <= 0.2 && shared_var->monitor_high == 1){
										//puts("2");
										sem_post(MONITOR);
										sem_wait(START);
									}
									
									breakk = 1;
									break;
								}
								else{
									sprintf(str, " DISPATCHER: THE DEADLINE FOR TASK %s EXPIRED, REMOVING FROM QUEUE\n", shared_var->queue[i].id);
									writelogfile(str, 2);
								}
							}
						}
					}
					if(breakk){
						breakk = 0;
						break;
					}
				}
			}
		}
		
		sem_post(START);
	}
	pthread_mutex_unlock(&shared_var->mutex_dispat);
	
	pthread_cond_signal(&sched);
	sem_post(FINISHING);

	pthread_exit(NULL);
	return NULL;
}

int comparator_tasks(const void *task1, const void *task2){
	const task *t1 = (task*)task1;
	const task *t2 = (task*)task2;
	
	if(t1->time_exec == t2->time_exec){
		return t1->time_arriv - t2->time_arriv;
	}

	return t1->time_exec - t2->time_exec;
}

void statistics(int signal){
	printf(DEFAULT);
	puts("");
	
	printf("================================ STATISTICS ================================\n");
	printf("[TASKS EXECUTED]: %d\n", shared_var->statistics.tasks_executed);
	printf("[AVERAGE ANSWER TIME]: %.2f\n", shared_var->statistics.average_time_to_execute);
	printf("[TASKS NOT EXECUTED]: %d\n", shared_var->statistics.tasks_not_executed + shared_var->actual_queue_size);
	for(int i=0; i<shared_var->configs->nr_edge_servers; i++){
		printf("{%s}:  [TASKS EXECUTED]: %d\t[MALFUNCTION OPERATIONS]: %d\n", shared_var->servers[i].name, shared_var->servers[i].tasks_executed, shared_var->servers[i].malfunction_operations);
	}
	printf("============================================================================\n\n");
	
	if(signal == SIGTSTP){
		writelogfile(" SIGNAL SIGTSTP RECEIVED\n", 3);
	}
}

void queue(int signal){
	printf(DEFAULT);
	puts("");
	
	printf("================================= QUEUE ==================================\n");
	for(int i=0; i < shared_var->queue_size; i++){
		printf("{%s}: ", shared_var->queue[i].id);
		printf("[MILLIONS INSTRUCTIONS]: %d", shared_var->queue[i].nr_instructions);
		printf("\t[MAX TIME TO EXECUTE]: %d", shared_var->queue[i].time_exec);
		printf("\t[TIME OF ARRIVING]: %d\n", shared_var->queue[i].time_arriv);
	}
	
	puts("");
	writelogfile(" SIGNAL SIGTQUIT RECEIVED\n", 3);
}
