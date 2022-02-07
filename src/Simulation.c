//João Carlos Borges Silva     nº 2019216763
//Mário Guilherme Lemos      nº2019216792

#include "Header.h"

//Cria os processos de Gestor de Corrida e malfunctions
// - executa-se um fork() no processo do Simulador para criar o processo de Gestor de Corrida
// - é aqui chamada a função para criar processos para as teams
// - executa-se um 2º fork() ainda no processo do Simulador para criar o processo de Gestor de malfunctions
//Não dá return
//Nesta primeira meta esta função simplesmente dá print dos ids dos processos criados para verificar que eles estão a ser criados
void simulator(){
	if((mkfifo(PIPE_NAME, O_CREAT|O_EXCL|0600)<0) && (errno!=EEXIST)){
    	perror("Cannot create pipe: ");
    	exit(0);
	}
	
	if((fdN = open(PIPE_NAME, O_RDWR))<0){
		perror("Cannot open pipe for writing: ");
		exit(0);
	}
	
	if(fork()==0){
		race_manager();
    	exit(0);
    }
    else{
    	if(fork()==0){
    		signal(SIGTSTP, SIG_IGN);
        	malfunction_manager();
        	exit(0);
    	}
    	else{
			while(shared_var->finish!=10){
				pause();
			}		
    	}
    }
}


int verify_can_creat_new_team(){
	for(int i=0; i<configs->nteams; i++){
		if(shared_var->teams[i].ncars==0){
			return i;
		}
	}
	return -1;
}

int verify_already_team_created(char* name){
	for(int i=0; i<configs->nteams; i++){
		if(strcmp(shared_var->teams[i].name,name)==0){
			return i;
		}
	}
	return -1;
}

int verify_can_creat_new_car(team team){
	if(team.ncars==configs->nmaxcars){
		return -1;
	}
	for(int i=0; i<configs->nmaxcars*configs->nteams; i++){
		if(shared_var->cars[i].space==0){
			return i;
		}
	}
	return -1;
}

int verify_already_car_created(team team, int n){
	for(int i=0; i<configs->nmaxcars*configs->nteams; i++){
		if(shared_var->cars[i].n==n && strcmp(shared_var->cars[i].team, team.name)==0){
			return 1;
		}
	}
	return 0;
}

void verify_enter_in_box(){
	char toLog[MAXCHAR];

	sprintf(toLog," SIGNAL SIGUSR2 RECEIVED BY TEAM %s\n", shared_var->teams[posT_P].name);
	writelogfile(toLog);
	if(shared_var->teams[posT_P].damaged==1){
		shared_var->teams[posT_P].box=2;
	}
	
		if(shared_var->teams[posT_P].box==0){
			shared_var->teams[posT_P].box=1;
		}
		else if (shared_var->teams[posT_P].box==2){
			shared_var->teams[posT_P].box=0;
		}
}

void* worker(void* idp){
	int posi=*((int*)idp);
	message M;
	float timeA;
	float timeF;
	float distance=shared_var->cars[posi].speed;
	float distanceTM;
	char stateC[(int)(MAXCHAR*1.5)];
	
	 
	while(shared_var->cars[posi].tank>0 && shared_var->cars[posi].state!=4){
		msgrcv(msqid, &M, sizeof(M)-sizeof(long), posi+1, IPC_NOWAIT);
		if(strlen(M.something)>0 && strcmp(M.something,"AVARIOU")==0 ){
			sem_wait(SHM);
			shared_var->cars[posi].needtogobox=true;
			shared_var->cars[posi].state=1;
			shared_var->teams[posT_P].damaged=1;
			sem_post(SHM);
			sprintf(stateC, " NEW PROBLEM IN CAR Nº %d OF TEAM %s\n", shared_var->cars[posi].n, shared_var->cars[posi].team);
			writelogfile(stateC);
			shared_var->table.malfunctions++;
			sprintf(stateC, " THE CAR Nº %d OF TEAM %s CHANGE HIS STATE TO segurança\n", shared_var->cars[posi].n, shared_var->cars[posi].team);
			write(shared_var->teams[posT_P].fdU[1], stateC, strlen(stateC));
			M.something[0]='\0';
		}
		
		if(shared_var->cars[posi].distance+distance > configs->dlap*(shared_var->cars[posi].laps+1) && (shared_var->cars[posi].laps+1)!=configs->nlaps){  //NOT SURE
			distanceTM=(configs->dlap*(shared_var->cars[posi].laps+1) - shared_var->cars[posi].distance) ;
			timeF=(distanceTM/distance)*configs->unittime;
			sem_wait(SHM);
			shared_var->cars[posi].time+=timeF;
			shared_var->cars[posi].distance+=distanceTM;
			sem_post(SHM);
			sleep(timeF);
			
			if(shared_var->finish==1){
				shared_var->cars[posi].state=4;
				sprintf(stateC, " THE CAR Nº %d OF TEAM %s CHANGE HIS STATE TO terminado\n", shared_var->cars[posi].n, shared_var->cars[posi].team);
				write(shared_var->teams[posT_P].fdU[1], stateC, strlen(stateC));		
			}
			
			if(shared_var->finish==0 && (shared_var->teams[posT_P].box==0 || shared_var->teams[posT_P].box==1) && shared_var->cars[posi].needtogobox==true){
				shared_var->cars[posi].state=2;	
				sprintf(stateC, " THE CAR Nº %d OF TEAM %s CHANGE HIS STATE TO box\n", shared_var->cars[posi].n, shared_var->cars[posi].team);
				write(shared_var->teams[posT_P].fdU[1], stateC, strlen(stateC));
			}	
		}
		
		//VERIFICAR GASOLINA PARA 4 laps
		if(shared_var->cars[posi].tank<=configs->dlap*4*shared_var->cars[posi].consumption/shared_var->cars[posi].speed && shared_var->cars[posi].state==0){
			sem_wait(SHM);
			shared_var->cars[posi].needtogobox=true;
			sem_post(SHM);
		}
		
		//VERIFICAR GASOLINA PARA 2 laps
		if(shared_var->cars[posi].tank<=configs->dlap*2*shared_var->cars[posi].consumption/shared_var->cars[posi].speed && shared_var->cars[posi].state==0){
			sem_wait(SHM);
			shared_var->cars[posi].state=1;
			sem_post(SHM);
			sprintf(stateC, " THE CAR Nº %d OF TEAM %s CHANGE HIS STATE TO segurança\n", shared_var->cars[posi].n, shared_var->cars[posi].team);
			write(shared_var->teams[posT_P].fdU[1], stateC, strlen(stateC));
			
			if(shared_var->teams[posT_P].box==0){
				sem_wait(SHM);
				shared_var->cars[posC_P].reserved=1;
				raise(SIGUSR2);
				sem_post(SHM);
			}	
		}
		
		if(shared_var->cars[posi].state==0){
			sem_wait(SHM);
			shared_var->cars[posi].tank-=shared_var->cars[posi].consumption;
			distance=shared_var->cars[posi].speed;
			shared_var->cars[posi].time+=configs->unittime;
			shared_var->cars[posi].distance+=distance;
			sem_post(SHM);
			sleep(configs->unittime);	
		}
		else if(shared_var->cars[posi].state==1){
			sem_wait(SHM);
			shared_var->cars[posi].tank-=shared_var->cars[posi].consumption*0.4;
			distance=shared_var->cars[posi].speed*0.3;
			shared_var->cars[posi].time+=configs->unittime;
			shared_var->cars[posi].distance+=distance;
			sem_post(SHM);
			sleep(configs->unittime);
		}
		else if(shared_var->cars[posi].state==2){
			sem_wait(SHM);
			raise(SIGUSR2);
			shared_var->cars[posi].stops++;
			sem_post(SHM);
			if(shared_var->teams[posT_P].damaged==1){
				timeA=(rand() % (configs->tmaxmalfunction-configs->tminmalfunction+1))  + configs->tminmalfunction;			
			}
			else{
				shared_var->table.refuels++;
				shared_var->cars[posi].tank=configs->tank;
				timeA=2;
			}
			sem_wait(SHM);
			shared_var->cars[posi].tank=configs->tank;
			shared_var->cars[posi].time+=timeA*configs->unittime;
			sem_post(SHM);
			
			sleep(timeA*configs->unittime);
			
			sem_wait(SHM);
			shared_var->cars[posi].state=0;
			sem_post(SHM);

			sprintf(stateC, " THE CAR Nº %d OF TEAM %s CHANGE HIS STATE TO corrida\n", shared_var->cars[posi].n, shared_var->cars[posi].team);
			write(shared_var->teams[posT_P].fdU[1], stateC, strlen(stateC));

			sem_wait(SHM);
			shared_var->cars[posi].reserved=0;
			shared_var->teams[posT_P].box=0;
			shared_var->teams[posT_P].damaged=0;
			sem_post(SHM);
		}
		
		//QUANDO finish SEM PROBLEMAS
		if(shared_var->cars[posi].distance+distance >=  configs->dlap*configs->nlaps){  
			//calculo que demora a finish a corrida quando ainda distance é menor que o percurso da corrida
			timeF=((configs->dlap*configs->nlaps - shared_var->cars[posi].distance) / distance)*configs->unittime;
			
			sem_wait(SHM);
			shared_var->cars[posi].distance+=configs->dlap*configs->nlaps - shared_var->cars[posi].distance;
			shared_var->cars[posi].time+= timeF;
			sem_post(SHM);
						
			sleep(timeF);
			
			sem_wait(SHM);
			shared_var->cars[posi].state=4;
			sem_post(SHM);
			sprintf(stateC, " THE CAR Nº %d OF TEAM %s CHANGE HIS STATE TO terminado\n", shared_var->cars[posi].n, shared_var->cars[posi].team);
			write(shared_var->teams[posT_P].fdU[1], stateC, strlen(stateC));
		}
		
		//INCREMENTA A lap
		if(shared_var->cars[posi].distance>=configs->dlap*(shared_var->cars[posi].laps+1)){
			sem_wait(SHM);
			shared_var->cars[posi].laps++;		
			sem_post(SHM);
		}
	}
	
	shared_var->table.carsinrace--;
	
	if(shared_var->cars[posi].tank<=0){
		sem_wait(SHM);
		shared_var->cars[posi].state=3;
		sem_post(SHM);
		sprintf(stateC, " THE CAR Nº %d OF TEAM %s CHANGE HIS STATE TO desistência\n", shared_var->cars[posi].n, shared_var->cars[posi].team);
		write(shared_var->teams[posT_P].fdU[1], stateC, strlen(stateC));
	}
	
	sem_wait(SHM);
	shared_var->cars[posi].finished=1;
	sem_post(SHM);
	
	pthread_exit(NULL);
}

int finish_himmmmm(){
	int count=0;
	if(shared_var->pos>0){	
		for(int i=0; i<shared_var->pos; i++){
			if(shared_var->cars[i].finished==1){
				count++;
			}
		}
		
		if(count==shared_var->pos){
			if(shared_var->finish==0){
				return 2;
			}
			else{
				return 1;
			}
		}
		else {
			return 0;
		}	
	}
	else{
		if(shared_var->ctrlc==0){
			return 1;
		}
		return 0;
	}
}

void search_Pos(int* positions, int nr){  
	int aux=0;
	int max=shared_var->teams[nr].ncars;
	for(int i=0; i<shared_var->pos; i++){
		if(aux==max){
			break;
		}
		if(strcmp(shared_var->cars[i].team, shared_var->teams[nr].name)==0){
			positions[aux++]=i;
		}
	}
}

int verifyCADD(char command[11][(int)MAXCHAR/2]){
	if(strcmp(command[1],"TEAM:")==0 && strcmp(command[3],"CAR:")==0 && strcmp(command[5],"SPEED:")==0 && strcmp(command[7],"CONSUMPTION:")==0 && strcmp(command[9],"RELIABILITY:")==0){
		if(isnumber(command[4]) && isnumber(command[6]) && isfloat(command[8]) && isnumber(command[10])){
			return 1;
		}
		else{
			return 0;
		}
	}
	else{
		return 0;
	}
}

int isfloat(char *str) {
	if(!isdigit(*str)){
		return 0;
	}
	else{
		str++;
		if(*str!='.'){
			return 0;
		}
		str++;
	}

    while(*str != '\0'){
        if(!isdigit(*str)){	
            return 0;
        }
        str++;
    }
    return 1;
}

void sigusr1handler(int sig){
	writelogfile(" SIGNAL SIGUSR1 RECEIVED\n");

	shared_var->finish=1;

	while(finish_himmmmm()!=1);
	
	kill(shared_var->id_simulator, SIGTSTP);

	for(int i=0; i<shared_var->nteams; i++){
		kill(shared_var->teams[i].id_team, SIGKILL);
	}
	clean_shared_memory();
}

void ctrlcHandler(int sig){
	writelogfile(" SIGNAL SIGINT RECEIVED\n");

	shared_var->finish=1;

	while(finish_himmmmm()!=1);

	if(shared_var->ctrlc==1){
		raise(SIGTSTP);
	}
	terminus();
}


int cmpfuncT (const void * a, const void * b) {
	carro *A = (carro *)a;
	carro *B = (carro *)b;

	int condD=B->distance - A->distance;
	int condV=B->laps - A->laps;
	
   	if ( condV !=0){
   		return condV ;
   	}
   	else{
   		if(condD !=0){
   			return condD;
   		}
   		else{
   			return  (A->time - B->time);
   		}
   	}
   	
   	return  (A->time - B->time);
}

void ctrlzHandler(int sig){
		printf("\n");
		writelogfile(" SIGNAL SIGTSTP RECEIVED\n");
		carro cars[shared_var->pos];
		
		for(int i=0; i<shared_var->pos; i++){
			memcpy(&cars[i], &shared_var->cars[i], sizeof(carro));
		}
		
		qsort(cars, shared_var->pos, sizeof(carro), cmpfuncT);
		
		shared_var->table.firstN=cars[0].n;
		strcpy(shared_var->table.firstS, cars[0].team);
		
		printf("-------------------------------------------------------------------------------------------\n");
		printf("|                                       LEADERBOARD                                        \n");
		for(int i=0; i<5; i++){
			printf("| %dº - Car nº%d of team %s is on %dº lap and has performed %d stops. \n", i+1, cars[i].n, cars[i].team, cars[i].laps, cars[i].stops);
		}
		printf("| Last position: Car nº%d of team %s is on %dº lap and has performed %d stops. \n", cars[shared_var->pos-1].n, cars[shared_var->pos-1].team, cars[shared_var->pos-1].laps, cars[shared_var->pos-1].stops);
		printf("| Total of malfunctions: %d    \n", shared_var->table.malfunctions);
		printf("| Total of refuels: %d    \n", shared_var->table.refuels);
		printf("| Number of cars in race: %d    \n", shared_var->table.carsinrace);
		printf("-------------------------------------------------------------------------------------------\n");
}

