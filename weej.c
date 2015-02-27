/* weej.c 
 * 
 * Weej is a simple static Web server.  It loads all objects into
 * memory at initialization time and can serve HTTP for HTML, JPG and
 * PDF files from a single thread.
 *
 * Written by: Dan Williams <danlythemanly@gmail.com> 1/21/2015
 */

#include <stdio.h>
#include <stdlib.h>

#include <sys/socket.h>
#include <netinet/in.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>


#define PORT        8080

#define BACKLOG     1024   /* parameter for socket listen   */
#define MAX_REQ_LEN 1024   /* parameter for socket recvfrom */

#define USAGE() do {							\
		fprintf(log, "Usage: %s "				\
			"index.html 404.html <html/css/jpg/pdf files>\n", \
			argv[0]);					\
		fflush(log);						\
		return -1;						\
	} while(0);

#define ERROR(r, x...) do {					\
		fprintf(log, "%s:%d: ", __FILE__, __LINE__);	\
		fprintf(log, x);				\
		fflush(log);					\
		return (r);					\
	} while(0)

#define WEEJLOG "/tmp/weejlog"
static FILE *log;

/* This webserver serves objects represented with the following
   structure.  For simplicity, we just keep a static array of objects.
   There are a couple of special objects in the array, so far just the
   index object (e.g., index.html) and the 404 object.
 */
struct object {
	char *filename;
	char *content_type;
	unsigned char *content;
	int content_len;
	char *content_len_str;
};
#define OBJECT_INDEX 0  /* index.html */
#define OBJECT_404   1  /* 404 page */

#define MAX_OBJECTS 100
static struct object objects[MAX_OBJECTS];
static int num_objects;

/* We only ever have success or 404 */
#define HTTP_STATUS_OK    "HTTP/1.1 200 OK\n"
#define HTTP_STATUS_404   "HTTP/1.1 404 Not Found\n"

/* We only serve .html, .css, .jpg, and .pdf files */
#define HTTP_CONTENT_HTML "Content-Type: text/html\n"
#define HTTP_CONTENT_CSS "Content-Type: text/css\n"
#define HTTP_CONTENT_JPG  "Content-Type: image/jpeg\n"
#define HTTP_CONTENT_PDF  "Content-Type: application/pdf\n"

#define HTTP_LENGTH_FMT   "Content-Length: %d\n\n"


/* Adding objects to the array only happens at init time. */
static int add_object(char *filename) {
	FILE *fp;
	struct stat stats;
	struct object *obj;
	int sz, ret;

	if ( strlen(filename) < 5 )
		ERROR(-1, "filename is too short\n");

	if ( stat(filename, &stats) < 0 )
		ERROR(-1, "couldn't stat %s\n", filename);

	obj = &objects[num_objects];

	obj->filename = filename;

	if ( !strncmp(filename + strlen(filename) - 4, "html", 4) )
		obj->content_type = HTTP_CONTENT_HTML;
	else if ( !strncmp(filename + strlen(filename) - 3, "css", 3) )
		obj->content_type = HTTP_CONTENT_CSS;
	else if ( !strncmp(filename + strlen(filename) - 3, "jpg", 3) )
		obj->content_type = HTTP_CONTENT_JPG;
	else if ( !strncmp(filename + strlen(filename) - 3, "pdf", 3) )
		obj->content_type = HTTP_CONTENT_PDF;
	else
		ERROR(-1, "unrecognized type: %s\n", filename);

	obj->content_len = stats.st_size;

	fp = fopen(filename, "r");
	if ( !fp )
		ERROR(-1, "couldn't open file %s\n", filename);
	
	obj->content = (unsigned char *)malloc(obj->content_len);
	if ( !obj->content )
		ERROR(-1, "couldn't malloc %d\n", obj->content_len);

	if ( fread(obj->content, obj->content_len, 1, fp) != 1 ) {
		free(obj->content);
		ERROR(-1, "couldn't fread\n");
	}

	sz = snprintf(NULL, 0, HTTP_LENGTH_FMT, obj->content_len);
	obj->content_len_str = (char *)malloc(sz + 1);
	if ( !obj->content_len_str ) {
		free(obj->content);
		ERROR(-1, "couldn't malloc %d\n", obj->content_len);
	}
	
	ret = snprintf(obj->content_len_str, sz + 1,
		       HTTP_LENGTH_FMT, obj->content_len);
	if ( ret != sz ) {
		free(obj->content);
		free(obj->content_len_str);
		ERROR(-1, "couldn't sprintf %d into %d bytes\n", 
		      obj->content_len, sz);
	}

	num_objects++;

	return 0;
}


/* We only support GETs of our objects. */
struct object *get_object(char *req, int reqlen){
	char *reqfile;
	int i = 0;

	if ( strncmp(req, "GET ", 4) )
		ERROR(NULL, "not a GET\n");
	
	if ( !strncmp(req, "GET / ", 6) )
		return &objects[OBJECT_INDEX];

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

/* Serve the object (with success or 404) */
static int serve_obj(int fd, struct sockaddr *caddr, socklen_t clen,
		     char *status, struct object *obj) {
	int ret;

	ret = sendto(fd, status, strlen(status), 0, caddr, clen);
	if ( ret < 0 )
		ERROR(-1, "couldn't send status\n");
			
	ret = sendto(fd, obj->content_type, strlen(obj->content_type), 
		     0, caddr, clen);
	if ( ret < 0 )
		ERROR(-1, "couldn't send content type\n");

	ret = sendto(fd, obj->content_len_str, 
		     strlen(obj->content_len_str), 0, caddr, clen);
	if ( ret < 0 )
		ERROR(-1, "couldn't send content length\n");

	ret = sendto(fd, obj->content, obj->content_len, 0, caddr, clen);
	if ( ret < 0 )
		ERROR(-1, "couldn't send content\n");

	return 0;
}

static int handle_connection(int fd, 
			     struct sockaddr *caddr, 
			     socklen_t clen) {
	char buf[MAX_REQ_LEN + 1];
	int recvd;
	struct object *obj;

	memset(buf, 0, MAX_REQ_LEN + 1);
	recvd = recvfrom(fd, buf, MAX_REQ_LEN, 0, caddr, &clen);

	obj = get_object(buf, recvd);

	if (obj != NULL) {
		fprintf(log, "serving %s\n", obj->filename);
		fflush(log);						
		return serve_obj(fd, caddr, clen, HTTP_STATUS_OK, obj);
	} 
	
	return serve_obj(fd, caddr, clen, 
			 HTTP_STATUS_404, &objects[OBJECT_404]);
}

static int start_server(void) {
	int lfd;
	struct sockaddr_in saddr;
	int so_reuseaddr = 1;

	lfd = socket(AF_INET,SOCK_STREAM,0);
	if ( lfd < 0 )
		ERROR(-1, "socket error\n");

        if ( setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR,
			&so_reuseaddr, sizeof(so_reuseaddr)) )
		ERROR(-1, "couldn't set sock opt\n");

	memset(&saddr, 0, sizeof(saddr));

	saddr.sin_family = AF_INET;
	saddr.sin_addr.s_addr = htonl(INADDR_ANY);
	saddr.sin_port = htons(PORT);
	if ( bind(lfd, (struct sockaddr *)&saddr, sizeof(saddr)) < 0 )
		ERROR(-1, "bind error\n");

	if ( listen(lfd, BACKLOG) < 0 )
		ERROR(-1, "listen error\n");

	for(;;) {
		int fd;
		struct sockaddr_in caddr;
		socklen_t clen = sizeof(caddr);

		fd = accept(lfd, (struct sockaddr *)&caddr, &clen);
		if ( fd > 0 ) {
			handle_connection(fd, 
					  (struct sockaddr *)&caddr, 
					  clen);
			close(fd);
		}
	}
}

int main(int argc, char **argv) {
	int i;

	if ( argc < 3 )
		USAGE();

	log = fopen(WEEJLOG, "w");

	for ( i = 1 ; i < argc; i++ )
		add_object(argv[i]);

	return start_server();
}
