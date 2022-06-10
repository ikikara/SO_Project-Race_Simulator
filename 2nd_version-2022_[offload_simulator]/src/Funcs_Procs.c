//João Carlos Borges Silva nº2019216753
//Pedro Afonso Ferreira Lopes Martins nº2019216826

#include "Header.h"

void task_manager(){
	signal(SIGINT, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);

	char buffer[MAXCHAR] = "";
	char str[2*MAXCHAR];
	int lenR;

	//printf("ME: %d | FATHER: %d | NAME: task manager\n", getpid(), getppid());
	writelogfile(" PROCESS TASK_MANAGER CREATED\n", 1);
	
	for(int i=0; i<shared_var->configs->nr_edge_servers; i++){
		if(fork() == 0){
			if(shared_var->task_manager_ID != getpid()){
				shared_var->servers[i].edge_server_ID = getpid();
				edge_server(i);
				
				exit(0);
			}
		}
	}
	
	if(shared_var->task_manager_ID == getpid()){
		pthread_create(&shared_var->scheduler, NULL, scheduler, NULL);
		pthread_create(&shared_var->dispatcher, NULL, dispatcher, NULL);
		
		//number of sem_posts = task scheduler + task dispatcher + monitor + maintenance
		for(int i=0; i<4; i++){
			sem_post(START);
		}
		
		while(strcmp(buffer, "EXIT")!=0){
			lenR = read(fd_named_pipe, buffer, sizeof(buffer));
			//puts(buffer);
			
			buffer[lenR-1] = '\0';
			if(!strcmp(buffer, "STATS")){
				statistics();
			}
			else if(!strcmp(buffer, "EXIT")){
				break;
			}
			else{
				if(verify_command(buffer)){		
					if(!verify_and_add(buffer)){
						writelogfile(" THE QUEUE IS ALREADY FULL\n", 2);
					}		
				}
				else{ //dá WRONG COMMAND porque o buffer às vezes dá overflow -> quando o tempo de envio é muito curto
					sprintf(str, " WRONG COMMAND => %s\n", buffer);
					writelogfile(str, 0);
				}
			}	
    			
    		strcpy(buffer, "");
		}
		
		/*if(shared_var->fT != 1){
			terminus();
		}*/
		
		// wait for all created processes
		if(shared_var->task_manager_ID == getpid()){
			for(int i=0; i<shared_var->configs->nr_edge_servers; i++){
				wait(NULL);
			}	
		}
		
		pthread_join(shared_var->scheduler, NULL);
		pthread_join(shared_var->dispatcher, NULL);
		
		writelogfile(" PROCESS TASK MANAGER FINISHING..\n", 1);
	}
}


void edge_server(int index){
	int indexes[2][2];
	char str[MAXCHAR];
	msq message;
	//printf("ME: %d | FATHER: %d | NAME: %s\n", getpid(), getppid(), shared_var->servers[index].name);
	
	sprintf(str, " %s READY\n", shared_var->servers[index].name);
	writelogfile(str, 1); 
	
	shared_var->servers[index].vCPU_availability[0] = 0;
	shared_var->servers[index].vCPU_availability[1] = 2; 
	
	for (int i = 0; i < 2; i++){
		indexes[i][0] = index; indexes[i][1] = i;
		pthread_create(&shared_var->servers[index].vCPU_ID[i], NULL, vCPU, &indexes[i]);
	}
	
	sem_post(START);
	
	strcpy(message.something, "a");
	
	while(shared_var->f != 1){
		msgrcv(msqid, &message, sizeof(message)-sizeof(long), index+1, 0);
		//printf("%s informed\n", shared_var->servers[index].name);
		if(shared_var->f == 1){
			break;
		}
		
		/* put in stopped state */
		sem_wait(SHM);
		shared_var->servers[index].state = 0;
		sem_post(SHM);
		
		
		/* only do this if entering vCPU finish their tasks */	
		for(int i=0; i<2; i++){
			if(shared_var->servers[index].vCPU_availability[i] == 1){
				sem_wait(SERVER_MAINTENANCE);
				//printf("%d | %d\n", index , i);
			}
		}
		
		if(shared_var->f == 1){
			break;
		}
			
		msgsnd(msqid, &message, sizeof(message)-sizeof(long), 0);
		sprintf(str, " %s: ENTERING IN MAINTENANCE..\n", shared_var->servers[index].name);
		writelogfile(str, 2);
		
		sem_wait(MAINTENANCE);
		msgrcv(msqid, &message, sizeof(message)-sizeof(long), index+1, 0);
		
		//printf("%s maintenance over\n", shared_var->servers[index].name);
		
		sem_wait(SHM);
		shared_var->servers[index].malfunction_operations++;
		shared_var->servers[index].state = 1;
		sem_post(SHM);
	}
	
	for (int j = 0; j < 2; j++){
		pthread_join(shared_var->servers[index].vCPU_ID[j], NULL);
	}
	
	sprintf(str, " %s FINISHING..\n", shared_var->servers[index].name);
	writelogfile(str, 1);
	sem_post(FINISHING);
}


void monitor(){
	signal(SIGINT, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);

	//printf("ME: %d | FATHER: %d | NAME: monitor\n", getpid(), getppid());
	writelogfile(" PROCESS MONITOR CREATED\n", 1);
	
	sem_wait(START);

	while(shared_var->f != 1){
		sem_wait(MONITOR);
		
		if(shared_var->f == 1){
			break;
		}
		else{
			sem_wait(SHM);
			shared_var->monitor_high^=1;
			
			/* waiting for all tasks in vcpu 2 finish */
			
			for(int i=0; i<shared_var->configs->nr_edge_servers; i++){
				if(shared_var->monitor_high){		
					shared_var->servers[i].vCPU_availability[1] = 0; 
				}
				else{		
					shared_var->servers[i].vCPU_availability[1] = 2; 
				}
			}
			sem_post(SHM);
			
			if(shared_var->monitor_high){		
				writelogfile(" HIGH PERFORMANCE MODE ON\n", 1);
			}
			else{		
				writelogfile(" HIGH PERFORMANCE MODE OFF\n", 1);
			}
			
			sem_post(START);
		}
	}
	
	writelogfile(" PROCESS MONITOR FINISHING..\n", 1);
	sem_post(FINISHING);
}


void maintenance_manager(){
	signal(SIGINT, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
	
	msq message;
	int maintenance_time;
	int server;

	//printf("ME: %d | FATHER: %d | NAME: maintenance manager\n", getpid(), getppid());
	writelogfile(" PROCESS MAINTENANCE_MANAGER CREATED\n", 1);
	
	for(int i=0; i<shared_var->configs->nr_edge_servers+1; i++){
		sem_wait(START);
	}
	
	strcpy(message.something, "a");
	
	while(shared_var->f != 1){
		if(shared_var->f == 1){
			break;
		}
		
		sleep(1 + rand()%5);
		srand(time(NULL));
		server = rand() % shared_var->configs->nr_edge_servers;
		srand(time(NULL));
		maintenance_time = 1 + rand()%5;
		message.mtype = server+1;
		
		//printf("%d\n", server+1);
		msgsnd(msqid, &message, sizeof(message)-sizeof(long), 0);
		
		msgrcv(msqid, &message, sizeof(message)-sizeof(long), server+1, 0);
		
		//printf("Maintenance starting\n");
		sleep(maintenance_time);
		sem_post(MAINTENANCE);
		//printf("Maintenance ending\n");
		
		msgsnd(msqid, &message, sizeof(message)-sizeof(long), 0);
	}
	
	writelogfile(" PROCESS MAINTENANCE_MANAGER FINISHING..\n", 1);
	sem_post(FINISHING);
}
