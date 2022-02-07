//João Carlos Borges Silva     nº 2019216763
//Mário Guilherme Lemos      nº2019216792

#include "Header.h"

void race_manager(){
	signal(SIGTSTP,SIG_IGN);
	signal(SIGINT, SIG_IGN);
	signal(SIGUSR1, sigusr1handler);
	
	fd_set read_set;
	int lenR;
    int count;
    char* aux;
    char* aux2;
	char command[11][(int)MAXCHAR/2];
	char bufferR[MAXCHAR];
	char bufferRaux[MAXCHAR];
	char toLog[MAXCHAR+20];    
	char aux4[(int)(MAXCHAR*1.5)];        
  	shared_var->raceP=getpid(); 
  	
  	printf("\nUse this PID if you want to stop the actual race: %d\n\n", shared_var->raceP);     
          
    while(shared_var->finish!=10){
    	FD_ZERO(&read_set);
	
    	for(int k=0; k<configs->nteams; k++){
    		FD_SET(shared_var->teams[k].fdU[0], &read_set);
    	}
    	FD_SET(fdN,&read_set);
    	
    	if(select(fdN+1,&read_set, NULL,NULL,NULL)>0){
    		if(FD_ISSET(fdN,&read_set) ){
    			count=0;	
    			lenR=read(fdN, bufferR, sizeof(bufferR));
    			bufferR[lenR-1]='\0';
    			if(shared_var->ctrlc==1){
    				strcpy(bufferR, " REJECT, RACE ALREADY STARTED\n");
    				writelogfile(bufferR);
    			}	
    			else{
    				strcpy(bufferRaux, bufferR);
        				
    				aux=strtok(bufferR,", ");
     				while(aux!=NULL || count>11){
        				strcpy(command[count++],aux);
        				aux=strtok(NULL,", ");
    				} 
        				
					if(strcmp(command[0],"START")==0 && strcmp(bufferRaux,"START RACE!")==0 && shared_var->condition==0){ 
						if(shared_var->nteams>=3){
							shared_var->ctrlc=1;
							shared_var->table.carsinrace=shared_var->pos;
        					writelogfile(" NEW COMMAND RECEIVED: START RACE!\n");
        					shared_var->condition=1;
        				}
        				else{
        					writelogfile(" IS NOT POSSIBLE TO START THE RACE, NEED 3 TEAMS MINIMUM\n");
        				}
					}
					else if(strcmp(command[0],"ADDCAR")==0 && verifyCADD(command)){
    					char aux3[strlen(bufferRaux)+1];
    					int l;
						sprintf(aux3,"%s %s %s",command[0], command[1], command[2]);
						
    					for(int i=3;i<11;i+=1){
        					if(i%2==1){
        						strcat(aux3,", ");
        					}
        					else{
        						strcat(aux3," ");
        					}
        					strcat(aux3, command[i]);
						}
        		        						
						if(strcmp(aux3,bufferRaux)==0){
        					aux2 = strchr(bufferRaux,' ');	
        					if((posT_P=verify_already_team_created(command[2]))!=-1){
        						if((l=verify_already_car_created(shared_var->teams[posT_P], atoi(command[4])))==0 && (posC_P=verify_can_creat_new_car(shared_var->teams[posT_P]))!=-1){
        							sprintf(toLog," NEW CAR LOADED=>%s\n", aux2);
        							create_car(command[2],atoi(command[4]), atoi(command[6]), atof(command[8]), atoi(command[10]));
        							shared_var->teams[posT_P].ncars++;
        							writelogfile(toLog);	
        						}
        						else{
        							if(l==0){
        								writelogfile(" THE TEAM IS FULL\n");
        							}
        							else{
        								writelogfile(" THE CAR ALREADY EXISTS IN THIS TEAM\n");
        							}
        						}
        					}
        					else{
        						if((posT_P=verify_can_creat_new_team())!=-1){
        							sprintf(toLog," NEW TEAM CREATED WITH NAME: %s\n", command[2]);
        							writelogfile(toLog);      						
        							sprintf(toLog," NEW CAR LOADED=>%s\n", aux2);  	
        							strcpy(shared_var->teams[posT_P].name,command[2]); 	
        	     		
        	     					posC_P=verify_can_creat_new_car(shared_var->teams[posT_P]);
        							create_car(command[2],atoi(command[4]), atoi(command[6]), atof(command[8]), atoi(command[10]));
        							shared_var->teams[posT_P].ncars++;
        							
        							shared_var->nteams++;
									writelogfile(toLog);
	
        							if(fork()==0){
        								signal(SIGINT,SIG_IGN);
    									signal(SIGTSTP,SIG_IGN);
    									signal(SIGUSR1, SIG_IGN);
        								struct sigaction action;
    									action.sa_handler = verify_enter_in_box;
    									action.sa_flags = SA_RESTART;
    									sigemptyset(&action.sa_mask);									
										
										if ( sigaction(SIGUSR2, &action, NULL) == -1 ){
        									printf( "Erro\n" );
        									abort();
    									}
        							
        								shared_var->teams[posT_P].id_team=getpid();
        								team_manager();
        								exit(0);
        							}			
        						}
        						else{
        							writelogfile(" THE LIMIT FOR NEW TEAMS OR CARS HAS BEEN REACHED\n"); 
        						}
        					}
						}
						else{
							sprintf(toLog," WRONG COMMAND=> %s\n", bufferRaux);
        					writelogfile(toLog); 
						}
					}
    				else{
        				sprintf(toLog," WRONG COMMAND=> %s\n", bufferRaux);
        				writelogfile(toLog); 
    				}
    			}
    		}
    		for(int k=0; k<configs->nteams; k++){
    			if(FD_ISSET(shared_var->teams[k].fdU[0],&read_set)){
    				lenR=read(shared_var->teams[k].fdU[0], &aux4, sizeof(aux4));
    				aux4[lenR]='\0';
    				writelogfile(aux4);
    			}
    		}
    	}
    	fflush(stdout);
    	
    	if(finish_himmmmm()==2){	
    		kill(shared_var->id_simulator,SIGTSTP);
    		writelogfile(" ALL CARS FINISHED THE RACE!\n");
    		sleep(1);
    		sprintf(toLog, " CAR %d OF TEAM %s WINS THE RACE!!\n", shared_var->table.firstN, shared_var->table.firstS);
    		writelogfile(toLog);
    		for(int i=0; i<shared_var->nteams; i++){
				kill(shared_var->teams[i].id_team, SIGKILL);
			}
			
			clean_shared_memory();
    	}	
    }	

    
      
}

void team_manager(){
	while(shared_var->condition==0){
		sleep(1);
	}
	
	int * positions = malloc(sizeof(int)*shared_var->teams[posT_P].ncars);
	search_Pos(positions, posT_P);
	
	for(int i=0; i<shared_var->teams[posT_P].ncars; i++){
		pthread_create(&shared_var->cars[positions[i]].id_carro,NULL,worker,&positions[i]);
	}

	for(int i=0; i<shared_var->teams[posT_P].ncars; i++){
     	pthread_join(shared_var->cars[positions[i]].id_carro, NULL);
   	}
}

void malfunction_manager(){
	signal(SIGINT,SIG_IGN);
    signal(SIGTSTP,SIG_IGN);
    signal(SIGUSR1, SIG_IGN);
	shared_var->malfunctionP=getpid();
	message M;
	int prob;
	
	while(shared_var->condition==0){
		sleep(1);
	}
	
	while(1){
		srand(time(NULL));
		
		for(int i=0; i<shared_var->pos; i++){
			prob=rand() % 100 + 1;	
			if(prob>shared_var->cars[i].reliability && shared_var->cars[i].needtogobox==false){
				M.mtype=i+1;
				strcpy(M.something, "AVARIOU");
				msgsnd(msqid, &M, sizeof(M)-sizeof(long), 0);
			}
		}
		sleep(configs->tmalfunction*configs->unittime);
	}	
}
