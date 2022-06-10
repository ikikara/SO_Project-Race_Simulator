//João Carlos Borges Silva nº2019216753
//Pedro Afonso Ferreira Lopes Martins nº2019216826

#include "Header.h"

void starting(char* config_filename){
    int* confs = malloc(sizeof(int)*3);
    char*** servers;
    
	unlink(PIPE_NAME);
    sem_unlink("SHM");
	sem_unlink("LOG");
	sem_unlink("START");
	sem_unlink("MONITOR");
	sem_unlink("MAINTENANCE");
	sem_unlink("FINISHING");
	sem_unlink("SERVER_MAINTENANCE");
	
	SHM=sem_open("SHM", O_CREAT|O_EXCL, 0700,1);
	#ifdef DEBUG
		DEBUG_PRINT("SEMAPHORE TO SHARED MEMORY CREATED\n");
	#endif
	
	LOG=sem_open("LOG", O_CREAT|O_EXCL, 0700,1);
	#ifdef DEBUG
		DEBUG_PRINT("SEMAPHORE TO WRITE LOGFILE CREATED\n");
	#endif
	
	START=sem_open("START", O_CREAT|O_EXCL, 0700,0);
	#ifdef DEBUG
		DEBUG_PRINT("SEMAPHORE TO START SOME PROCESSES CREATED\n");
	#endif
	
	MONITOR=sem_open("MONITOR", O_CREAT|O_EXCL, 0700,0);
	#ifdef DEBUG
		DEBUG_PRINT("SEMAPHORE TO MONITOR CHANGE STATUS CREATED\n");
	#endif
	
	MAINTENANCE=sem_open("MAINTENANCE", O_CREAT|O_EXCL, 0700,0);
	#ifdef DEBUG
		DEBUG_PRINT("SEMAPHORE TO MAINTENANCE CREATED\n");
	#endif
	
	FINISHING=sem_open("FINISHING", O_CREAT|O_EXCL, 0700,0);
	#ifdef DEBUG
		DEBUG_PRINT("SEMAPHORE TO FINISH CREATED\n");
	#endif
	
	SERVER_MAINTENANCE=sem_open("SERVER_MAINTENANCE", O_CREAT|O_EXCL, 0700,0);
	#ifdef DEBUG
		DEBUG_PRINT("SEMAPHORE TO MAINTENANCE IN SOME SERVER CREATED\n");
	#endif
	
	logs=fopen("Logs.txt", "w+");
	#ifdef DEBUG
		DEBUG_PRINT("LOGFILE CREATED\n");
	#endif	
	
	sched = (pthread_cond_t)PTHREAD_COND_INITIALIZER; 
	mutex_sched = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
    
    if((servers = readfile(config_filename, confs)) == NULL){
    	exit(1);
    }
    
    create_shared_memory(confs, servers);
    
    if((mkfifo(PIPE_NAME, O_CREAT|O_EXCL|0600)<0) && (errno!=EEXIST)){
    	perror("Cannot create pipe: ");
    	exit(1);
	}
	
	if((fd_named_pipe = open(PIPE_NAME, O_RDWR))<0){
		perror("Cannot open pipe for writing: ");
		exit(1);
	}
	
	#ifdef DEBUG
		DEBUG_PRINT("NAMED PIPE CREATED\n");
	#endif
	
	if((msqid=msgget(IPC_PRIVATE, IPC_CREAT|0777))<0){
    	perror("Cannot create message queue");
		exit(1);
	}
	
	#ifdef DEBUG
		DEBUG_PRINT("MESSAGE QUEUE CREATED\n\n");
	#endif
}

void terminus(int signal){
	msq message;

	sem_wait(SHM);
	shared_var->f = 1;
	sem_post(SHM);
	
	puts("");
	
	if(signal == SIGINT){
		sem_wait(SHM);
		shared_var->fT = 1;
		sem_post(SHM);
		
		writelogfile(" SIGNAL SIGINT RECEIVED\n", 3);
	}
	writelogfile(" SIMULATOR WAITING FOR LAST TASKS TO FINISH..\n", 1);
	
	write(fd_named_pipe, "EXIT", sizeof("EXIT"));
	close(fd_named_pipe);
	unlink(PIPE_NAME);
	
	for(int i=0; i<shared_var->configs->nr_edge_servers; i++){
		message.mtype = i+1;
		msgsnd(msqid, &message, sizeof(message)-sizeof(long), 0);
		for(int j=0; j<2; j++){
			pthread_mutex_unlock(&shared_var->servers[i].mutex_vCPU[j]);
			close(shared_var->servers[i].fdU[j]);
		}
	}

	pthread_cond_signal(&shared_var->dispat);
	
	for(int i=0; i<shared_var->configs->nr_edge_servers*(2 + 1) + 1 + 1; i++){
		sem_wait(FINISHING);
	}
	
	sem_post(MONITOR);
	sem_wait(FINISHING);
	
	for(int i=0; i<shared_var->configs->nr_edge_servers; i++){
		kill(shared_var->servers[i].edge_server_ID, SIGKILL);
	}
	kill(shared_var->task_manager_ID, SIGKILL);
	kill(shared_var->monitor_ID, SIGKILL);
	kill(shared_var->maintenance_manager_ID, SIGKILL);
	
	statistics();
	writelogfile(" SIMULATOR CLOSING\n", 1);
	
	pthread_mutex_destroy(&mutex_sched);
	pthread_mutex_destroy(&shared_var->mutex_dispat);
	
	pthread_cond_destroy(&sched);
	//pthread_cond_destroy(&shared_var->dispat);
	
	sem_close(SHM);
	sem_close(LOG);
	sem_close(START);
	sem_close(MONITOR);
	sem_close(MAINTENANCE);
	sem_close(FINISHING);
	
	sem_unlink("SHM");
	sem_unlink("LOG");
	sem_unlink("START");
	sem_unlink("MONITOR");
	sem_unlink("MAINTENANCE");
	sem_unlink("FINISHING");
	sem_unlink("SERVER_MAINTENANCE");
	
	msgctl(msqid, IPC_RMID, NULL);
	
	shmdt(shared_var);
	shmctl(shmid, IPC_RMID, NULL);
	
	fclose(logs);
	#ifdef DEBUG
		DEBUG_PRINT("\nOFFLOAD SIMULATOR REMOVED\n");
	#endif
	
	exit(0);
}

char*** readfile(char* filename, int* confs){
    FILE* file;
    char str[MAXCHAR];
    char* geral_info;
    char* server_info;
    char*** servers_aux;
    int line = 1;

    file=fopen(filename,"r");
    if(file==NULL){
		sprintf(str, " ERROR: COULDN'T OPEN THE FILE: %s\n", filename);
		writelogfile(str, 0);
        return NULL;
    }
    
    int i=0;
    int j=0, k=0;
    
    while (fgets(str, MAXCHAR, file) != NULL) {
    	if(!strcmp(str, "\n")){
    		sprintf(str, " ERROR: THE LINE %d IS EMPTY\n", line+1);
    		writelogfile(str, 0);
    		return NULL;
    	}
    	
    	if(i<3){
    		str[strlen(str)-1] = '\0';
        	geral_info = str;
        	confs[i++]=atoi_check(geral_info, line);
        	
        	if(i-1==2){
        		if(confs[i-1] < 2){
					writelogfile(" ERROR: THE NUMBER OF EDGE SERVERS MUST 2 OR BIGGER\n", 0);
					return NULL;
            	}
            	else{
            		servers_aux = malloc(sizeof(char)*confs[2]*3*(int)(MAXCHAR/2));
            	}
        	}
        	else{
        		if(confs[i-1] < 0){
        			return NULL;
        		}
        	}
        }
        else{
        	str[strlen(str)-1] = '\0';
        	
        	servers_aux[j] = malloc(sizeof(char)*(int)(MAXCHAR/2)*3);
        	
        	servers_aux[j][k] = malloc(sizeof(char)*(int)(MAXCHAR/2));
        	server_info = strtok(str, ",");
        	strcpy(servers_aux[j][k], server_info);
        	
        	k++;
        	
        	while(server_info){
        		if(k>3){
        			sprintf(str," ERROR: EXCESSIVE INFORMATION ABOUT THE SERVER: %s\n", servers_aux[j][0]);
        			writelogfile(str, 0);
        			return NULL;
        		}
        	
        		servers_aux[j][k] = malloc(sizeof(char)*(int)(MAXCHAR/2));
        		server_info = strtok(NULL, ",");
        		
        		
        		if(server_info){    		
	        		atoi_check(server_info, line);
        			strcpy(servers_aux[j][k], server_info);
        		}
        		
        		k++;
        	}
        	
        	if(k<4){
        		sprintf(str," ERROR: MISSING INFORMATION ABOUT THE SERVER: %s\n", servers_aux[j][0]);
        		writelogfile(str, 0);
        		return NULL;
        	}
        	
        	k = 0;
        	j++;
        }
        
        line++;
    }
    
    if(i<3){
    	writelogfile(" ERROR: MISSING INFORMATION ABOUT THE SIMULATOR\n", 0);
    	return NULL;
    }
    
    if(confs[2] != j){
    	writelogfile(" ERROR: THE NUMBER OF EDGE SERVERS MUST BE THE SPECIFIED\n", 0);
    	return NULL;
    }
    
    fclose(file);
    
    return servers_aux;
}

void writelogfile(char* message, int color){
	sem_wait(LOG);
	char* hour=malloc(sizeof(char)*8);
	
	time_t now_time=time(NULL);
	struct tm * now =localtime(&now_time);
	
	sprintf(hour,"[%d-%d-%d]-[%02d:%02d:%02d]", now->tm_mday, now->tm_mon+1, now->tm_year+1900, now->tm_hour, now->tm_min, now->tm_sec);
	if(color == 0) printf(RED); 
	else if(color == 1) printf(DEFAULT);
	else if(color == 2) printf(YELLOW);
	else if(color == 3) printf(BLUE);
	
	printf("%s%s",hour,message);
	printf(DEFAULT);
	
	fputs(hour, logs);
	fputs(message, logs);
	fflush(logs);
	sem_post(LOG);
}

void create_shared_memory(int* confs, char*** servers){
	int size = sizeof(shm) + sizeof(confs) + sizeof(servers)*confs[2] + sizeof(task)*confs[0];
	
	if((shmid=shmget(IPC_PRIVATE, size ,IPC_CREAT | 0766))<0){
		perror("Error in shmget with IPC_GREAT\n");
		exit(1);
	}
	
	if((shared_var = (shm*) shmat(shmid,NULL,0))==(shm*)-1){
		perror("Shmat error!");
		exit(1);
	}
	#ifdef DEBUG
		DEBUG_PRINT("SHARED MEMORY CREATED\n");
	#endif
	
	shared_var->monitor_high = 0;
	shared_var->f = 0;
	shared_var->fT = 0;
	shared_var->min_wait = 0.0;
	
	shared_var->configs = (vals*)(shared_var + 1);
	shared_var->servers = (server*)(shared_var->configs + 1);
	shared_var->queue = (task*)(shared_var->servers + confs[2]);
	
	shared_var->configs->nr_slots = confs[0];
	shared_var->configs->max_wait = confs[1];
	shared_var->configs->nr_edge_servers = confs[2];
	
	if (pthread_mutexattr_init(&attrmutex) != 0){
        perror("ERROR: Failed to initialize mutex atribute\n");
        exit(1);
    }
    
    if (pthread_mutexattr_setpshared(&attrmutex, PTHREAD_PROCESS_SHARED) != 0){
        perror("ERROR: Failed to share mutex\n");
        exit(1);
    }
    
    if (pthread_mutex_init(&shared_var->mutex_dispat, &attrmutex) != 0){
        perror("ERROR: Failed to initialize mutex_dispat\n");
        exit(1);
    }
    
    if (pthread_condattr_init(&attrcond) != 0){
        perror("ERROR: Failed to initialize condition variable\n");
        exit(1);
    }
    
    if (pthread_condattr_setpshared(&attrcond, PTHREAD_PROCESS_SHARED) != 0){
        perror("ERROR: Failed to share condition variable\n");
        exit(1);
    }
    
    if (pthread_cond_init(&shared_var->dispat, &attrcond )!= 0){
        perror("ERROR: Failed to initialize condition variable\n");
        exit(1);
    }
	
	//servers
	for(int i=0; i<shared_var->configs->nr_edge_servers; i++){
		strcpy(shared_var->servers[i].name, servers[i][0]); 
		shared_var->servers[i].vCPU[0] = atoi(servers[i][1]);
		shared_var->servers[i].vCPU[1] = atoi(servers[i][2]);
		shared_var->servers[i].state = 1;
		for(int j=0; j<2; j++){
			if (pthread_mutex_init(&shared_var->servers[i].mutex_vCPU[j], &attrmutex) != 0){
            	perror("ERROR: Failed to initialize mutex_box_state\n");
            	exit(1);
        	}
        	pthread_mutex_lock(&shared_var->servers[i].mutex_vCPU[j]);
		}
		
    	if (pipe(shared_var->servers[i].fdU) == -1){
    		printf("An error as ocurred with opening the unnamed pipe\n");
    	}	
	}
	
	//queue
	shared_var->queue_size = shared_var->configs->nr_slots;
	shared_var->actual_queue_size = 0;
	//shared_var->queue = malloc(sizeof(task)*queue_size);
	
	for(int i=0; i < shared_var->queue_size; i++){
		strcpy(shared_var->queue[i].id, " ");
		shared_var->queue[i].nr_instructions = MAXINSTRUCTIONS_VERYBIG;
		shared_var->queue[i].time_exec = MAXTIME_VERYBIG;
		shared_var->queue[i].time_arriv = 0;
	}
}

int atoi_check(char* a, int line){	
	if(isnumber(a)==1){
		return atoi(a);
	}
	else{
		if(line != -1){
			char str[2*MAXCHAR];
			sprintf(str, " ERROR: IN LINE %d OF THE CONFIGS FILE IT WAS NOT POSSIBLE TO CONVERT TO NUMBER (OR THIS NUMBER IS LESS THAN 0)\n", line);
			writelogfile(str, 0);
		}
		return -1;
	}
}

int isnumber(char *str) {
    while(*str != '\0'){
        if(!isdigit(*str)){	
            return 0;
        }
        str++;
    }
    return 1;
}
