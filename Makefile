
myhttpd:myhttpd.o
	gcc -o myhttpd myhttpd.o -pthread
myhttpd.o:myhttpd.c
	gcc -c myhttpd.c 
