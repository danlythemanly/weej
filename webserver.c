#include <stdio.h>
#include <stdlib.h>

#include <sys/socket.h>
#include <netinet/in.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#define PORT        8082
#define BACKLOG     1024
#define MAX_REQ_LEN 1024
#define ERROR(x...) do {			\
	printf(x);				\
	exit(-1);				\
	} while(0)


static char *html;
static long htmlsize;

static int start_server(void) {
	int lfd;
	struct sockaddr_in saddr;

	lfd = socket(AF_INET,SOCK_STREAM,0);
	if ( lfd < 0 )
		ERROR("socket error\n");

	memset(&saddr, 0, sizeof(saddr));

	saddr.sin_family = AF_INET;
	saddr.sin_addr.s_addr = htonl(INADDR_ANY);
	saddr.sin_port = htons(PORT);
	if ( bind(lfd, (struct sockaddr *)&saddr, sizeof(saddr)) < 0 )
		ERROR("bind error\n");

	if ( listen(lfd, BACKLOG) < 0 )
		ERROR("listen error\n");

	for(;;) {
		int fd;
		struct sockaddr_in caddr;
		int clen = sizeof(caddr);
		char buf[MAX_REQ_LEN];
		int recvd;
		int i;

		fd = accept(lfd, (struct sockaddr *)&caddr, &clen);
		
		recvd = recvfrom(fd, buf, MAX_REQ_LEN, 0,
				 (struct sockaddr *)&caddr, &clen);

		if ( strncmp("GET / ", buf, strlen("GET / ")) == 0 ) {
			int sent;
			printf("received GET\n");

			#define STATUS_OK "HTTP/1.1 200 OK\n"
			#define CONT_TYPE "Content-Type: text/html;\n\n"

			sent = sendto(fd, STATUS_OK, 
				      strlen(STATUS_OK), 0, 
				      (struct sockaddr *)&caddr, clen);
			if ( sent < 0 )
				ERROR("couldn't send status\n");
			
			sent = sendto(fd, CONT_TYPE, 
				      strlen(CONT_TYPE), 0, 
				      (struct sockaddr *)&caddr, clen);
			if ( sent < 0 )
				ERROR("couldn't send content type\n");

			sent = sendto(fd, html, htmlsize, 0, 
				      (struct sockaddr *)&caddr, clen);
			if ( sent < 0 )
				ERROR("couldn't send html\n");
			
		}
		
		for (i = 0; i < recvd; i++ ) {
			printf("%02x ", buf[i]);
			if (i % 8 == 7 )
				printf(" ");
			if (i % 16 == 15 )
				printf("\n");
		}
		printf("got %d bytes\n", recvd);

		for (i = 0; i < recvd; i++ ) {
			printf("%c", buf[i]);
			if (i % 16 == 15 )
				printf("\n");
		}
		printf("got %d bytes\n", recvd);

		close(fd);
	}
}

int main(int argc, char **argv) {
	FILE *fp;
	struct stat stats;

	if ( argc < 2 )
		ERROR("Usage %s <index.html>\n", argv[0]);

	if ( stat(argv[1], &stats) < 0 )
		ERROR("couldn't stat %s\n", argv[1]);

	htmlsize = stats.st_size;

	fp = fopen(argv[1], "r");
	if ( !fp )
		ERROR("couldn't open file %s\n", argv[1]);
	
	printf("got %ld\n", htmlsize);
	
	html = (char *)malloc(htmlsize);
	if (!html)
		ERROR("couldn't malloc\n");

	if ( fread(html, htmlsize, 1, fp) != 1 )
		ERROR("couldn't fread\n");

	printf("got %ld bytes of %s\n", htmlsize, argv[1]);

	start_server();
}
