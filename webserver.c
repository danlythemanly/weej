#include <stdio.h>
#include <stdlib.h>

#include <sys/socket.h>
#include <netinet/in.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#define MAX_OBJECTS 100

#define PORT        8080
#define BACKLOG     1024
#define MAX_REQ_LEN 1024
#define FATAL(x...) do {			\
		printf(x);			\
		exit(-1);			\
	} while(0)
#define ERROR(x...) do {			\
		printf(x);			\
		return -1;			\
	} while(0)
#define NULLERROR(x...) do {			\
		printf(x);			\
		return NULL;			\
	} while(0)




struct object {
	char *filename;
	char *content_type;
	unsigned char *content;
	int content_len;
};

static struct object objects[MAX_OBJECTS];
static int num_objects;

static char *html;
static long htmlsize;

static void print_object(struct object *obj) {
	printf("OBJECT: %s\n", obj->filename);
	printf("        %s", obj->content_type);
	printf("        %d\n", obj->content_len);
}

static int add_object(char *filename) {
	FILE *fp;
	struct stat stats;
	struct object *obj;

	if ( strlen(filename) < 5 )
		ERROR("filename is too short\n");

	if ( stat(filename, &stats) < 0 )
		ERROR("couldn't stat %s\n", filename);

	obj = &objects[num_objects];

	obj->filename = filename;

	if ( !strncmp(filename + strlen(filename) - 4, "html", 4) )
		obj->content_type = "Content-Type: text/html\n";
	else if ( !strncmp(filename + strlen(filename) - 3, "jpg", 3) )
		obj->content_type = "Content-Type: image/jpeg\n";
	else if ( !strncmp(filename + strlen(filename) - 3, "pdf", 3) )
		obj->content_type = "Content-Type: application/pdf\n";
	else
		ERROR("unrecognized type: %s\n", filename);

	obj->content_len = stats.st_size;

	fp = fopen(filename, "r");
	if ( !fp )
		ERROR("couldn't open file %s\n", filename);
	
	obj->content = (char *)malloc(obj->content_len);
	if (!obj->content)
		ERROR("couldn't malloc %d\n", obj->content_len);

	if ( fread(obj->content, obj->content_len, 1, fp) != 1 ) {
		free(obj->content);
		ERROR("couldn't fread\n");
	}

	num_objects++;

	return 0;
}

struct object *get_object(char *req, int reqlen){
	char *reqfile;
	int i = 0;

	if ( strncmp(req, "GET ", 4) )
		NULLERROR("not a GET\n");
	
	if ( !strncmp(req, "GET / ", 6) )
		return &objects[0];

	reqfile = req + 5;

	for ( i = 0; i < num_objects; i++ ) {
		struct object *obj = &objects[i];
		char *objfile = obj->filename;
		int len = strlen(objfile);

		if (( !strncmp(objfile, reqfile, len) ) 
		    && (reqfile[len] == ' ') )
			return obj;
	}

	return NULL;
}


static int start_server(void) {
	int lfd;
	struct sockaddr_in saddr;

	lfd = socket(AF_INET,SOCK_STREAM,0);
	if ( lfd < 0 )
		FATAL("socket error\n");

	memset(&saddr, 0, sizeof(saddr));

	saddr.sin_family = AF_INET;
	saddr.sin_addr.s_addr = htonl(INADDR_ANY);
	saddr.sin_port = htons(PORT);
	if ( bind(lfd, (struct sockaddr *)&saddr, sizeof(saddr)) < 0 )
		FATAL("bind error\n");

	if ( listen(lfd, BACKLOG) < 0 )
		FATAL("listen error\n");

	for(;;) {
		int fd;
		struct sockaddr_in caddr;
		int clen = sizeof(caddr);
		char buf[MAX_REQ_LEN + 1];
		int recvd;
		int i;
		struct object *obj;

		fd = accept(lfd, (struct sockaddr *)&caddr, &clen);

		memset(buf, 0, MAX_REQ_LEN + 1);
		recvd = recvfrom(fd, buf, MAX_REQ_LEN, 0,
				 (struct sockaddr *)&caddr, &clen);

		obj = get_object(buf, recvd);

		if (obj != NULL) {
			int ret;
#define STATUS_OK "HTTP/1.1 200 OK\n"
#define LENGTH_FMT "Content-Length: %d\n\n"
#define MAX_LENGTH_STR 256

			char content_length[MAX_LENGTH_STR];

			printf("serving %s\n", obj->filename);

			ret = sendto(fd, STATUS_OK, 
				      strlen(STATUS_OK), 0, 
				      (struct sockaddr *)&caddr, clen);
			if ( ret < 0 )
				FATAL("couldn't send status\n");
			
			ret = sendto(fd, obj->content_type, 
				      strlen(obj->content_type), 0, 
				      (struct sockaddr *)&caddr, clen);
			if ( ret < 0 )
				FATAL("couldn't send content type\n");

			memset(content_length, 0, MAX_LENGTH_STR);
			ret = snprintf(content_length, MAX_LENGTH_STR, 
				LENGTH_FMT, obj->content_len);
			if ( ret > MAX_LENGTH_STR )
				FATAL("couldn't sprintf length str\n");

			ret = sendto(fd, content_length, 
				      strlen(content_length), 0, 
				      (struct sockaddr *)&caddr, clen);
			if ( ret < 0 )
				FATAL("couldn't send content length\n");

			ret = sendto(fd, obj->content, 
				      obj->content_len, 0, 
				      (struct sockaddr *)&caddr, clen);
			if ( ret < 0 )
				FATAL("couldn't send html\n");
			
		}
#if 0
		if ( strncmp("GET / ", buf, strlen("GET / ")) == 0 ) {
			int sent;
			printf("received GET\n");

			#define STATUS_OK "HTTP/1.1 200 OK\n"
			#define CONT_TYPE "Content-Type: text/html;\n\n"

			sent = sendto(fd, STATUS_OK, 
				      strlen(STATUS_OK), 0, 
				      (struct sockaddr *)&caddr, clen);
			if ( sent < 0 )
				FATAL("couldn't send status\n");
			
			sent = sendto(fd, CONT_TYPE, 
				      strlen(CONT_TYPE), 0, 
				      (struct sockaddr *)&caddr, clen);
			if ( sent < 0 )
				FATAL("couldn't send content type\n");

			sent = sendto(fd, html, htmlsize, 0, 
				      (struct sockaddr *)&caddr, clen);
			if ( sent < 0 )
				FATAL("couldn't send html\n");
			
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
#endif
		close(fd);
	}
}

int main(int argc, char **argv) {
	int i;

	if ( argc < 2 )
		FATAL("Usage %s index.html <html/jpg/pdf files>\n", 
		      argv[0]);

	for ( i = 1 ; i < argc; i++ )
		add_object(argv[i]);

	start_server();
}
