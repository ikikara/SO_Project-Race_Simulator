CC=gcc
FLAGS=-Wall -pthread
PROG=simulator
OBJS=Funcs_Procs.o Simulation.o Files_System.o Main.o

all: ${PROG}

clean:
		rm ${OBJS} *~ {PROG}
	
${PROG}: ${OBJS}
		${CC} ${FLAGS} ${OBJS} -lm -o $@
	
.c.o:
		${CC} ${FLAGS} $< -c -o $@
		
#########################
Main.o: Main.c Header.h
Funcs_Procs.o: Funcs_Procs.c Header.h
Simulation.o: Simulation.c Header.h
Files_System.o: Files_System.c Header.h

