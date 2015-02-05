
all: webserver

webserver: webserver.c
	gcc -Wall -o webserver webserver.c

