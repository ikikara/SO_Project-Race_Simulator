CC=gcc
FLAGS=-Wall -pthread
PROG=offload_simulator
PROG2=mobile_node
OBJS=Funcs_Procs.o Simulation.o Files_System.o Main.o
OBJS2=Mobile_Node.o

all: ${PROG} ${PROG2}

clean:
		rm ${OBJS} *~ {PROG}
		rm ${OBJS2} *~ {PROG2}
	
${PROG}: ${OBJS}
		${CC} ${FLAGS} ${OBJS} -lm -o $@
		
${PROG2}: ${OBJS2}
		${CC} ${FLAGS} ${OBJS2} -lm -o $@
	
.c.o:
		${CC} ${FLAGS} $< -c -o $@
		

		
#########################
Main.o: Main.c Header.h
Funcs_Procs.o: Funcs_Procs.c Header.h
Simulation.o: Simulation.c Header.h
Files_System.o: Files_System.c Header.h
Mobile_Node.o: Mobile_Node.c
