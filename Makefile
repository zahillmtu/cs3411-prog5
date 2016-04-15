.PHONY: all

CC = gcc
CCFLAGS = -std=c99 -g -Wall -Wextra -lreadline
SERV = server
CLI = client
OBJS = server.o 
CLOBJ = client.o

all: ${SERV} ${CLI}

${SERV}: ${OBJS}
	${CC} ${CCFLAGS} -o ${SERV} ${OBJS}

${CLI}: ${CLOBJ}
	${CC} ${CCFLAGS} -o ${CLI} ${CLOBJ}

.c.o:
	${CC} ${CCFLAGS} -c $<

server.o: server.c

client.o: client.c

clean:
	rm -f ${SERV} ${CLI} ${OBJS} ${CLOBJ}
