Weej: Simplicity in static Web serving
======================================

I recently decided to put up a Web site to share all of the fun things
I'm up to.  But rather than using an existing Web server, I thought it
would be more fun to write my own.  After all, the content in my site
(so far) is entirely HTML, JPG images, and PDF files.  All the Web
server needs to do is listen for HTTP requests for these objects and
construct an appropriate response.  So, here it is.  It's written in C
and called weej.

By default, weej serves content on port 8080.  To start, point it at
the objects you want to serve, with the first two objects the HTML
files for the index and 404 page respectively:

    weej index.html 404.html <other html/jpg/pdf files>

At the moment, those are the only three types of files that weej
supports.  At initialization time, all objects are read into memory.
The maximum number of objects weej can serve is fixed at 100.  There
is one thread that listens on a socket.  When an TCP connection
arrives, a fixed amount of data (1 KB) is read from the socket.  If
the incoming data is an HTTP GET that matches one of the objects that
was loaded at initialization time, weej constructs a 200 OK response
with the object data.  Otherwise, weej constructs a 404 Not Found
response with the content of the 404 object loaded at initialization
time.

There is also an upstart configuration file so that you can run weej
as a long-running service.  By default, it serves two files out of the
www subdirectory, but these can be changed by editing weej.conf.  To
install the upstart file just type:

    sudo make install 

Then, to start weej, type:

    sudo service weej start

You'll see a very simple html page (www/sample_index.html) on port
8080.  That's all there is to it!

Written by: Dan Williams <danlythemanly@gmail.com> 1/21/2015
