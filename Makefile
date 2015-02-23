
all: weej

weej: weej.c
	gcc -Wall -o weej weej.c

install: weej weej.conf
	@cat weej.conf | sed "s PWD $(CURDIR) " > /etc/init/weej.conf
