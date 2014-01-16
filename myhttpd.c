

/* INCLUDE FILES */

#include <stdio.h>          
#include <stdlib.h>         
#include<unistd.h>
#include <string.h>         
#include <fcntl.h> 
#include<time.h>         
#include <sys/stat.h>       
#include <sys/types.h>
#include <sys/socket.h>                                                               // SOCKET SYSTEM CALLS //
#include <arpa/inet.h>                                                                // BINDING SOCKET SYSTEM CALLS //
#include <pthread.h>                                                                  // IMPLEMENTING P - THREAD //
#include <semaphore.h>                                                                // P - THREAD SEMAPHORES //
#include <errno.h>
#include <dirent.h>
#include <netinet/in.h>

/* DEFINITONS */

#define PENDING_CONNECTIONS   100    
#define BUFFER_SIZE           1024    
#define TRUE                   1
#define FALSE                  0
#define SJF                    1
#define FCFS                   0


/* HTTP MESSAGES */

#define OK_MESSAGE  "HTTP/1.0 200 OK\n"
#define OK_IMAGE    "HTTP/1.0 200 OK\n"
#define NOTOK_404   "HTTP/1.0 404 NOT FOUND\n"
#define MESS_404    "<html><body><h1>FILE NOT FOUND</h1></body></html>\n"



/* VALUES OF INITIAL OPTIONS */

int PORT_NUMBER = 8080;
int sched = FCFS;
int t = 60;
int debug = 0;
int NTHREADS = 4;
char *dir;
char *logfile;
int f = 0;


/* SYNCHRONIZATION VARIABLES */

pthread_mutex_t qLock;                                                               //REQUEST READY QUEUE // 
pthread_mutex_t wLock;
pthread_cond_t wContd;                                                               // INITIALIZING THREADS //
sem_t count, idle_threads, *working_threads;
struct requestQueue *q[PENDING_CONNECTIONS];
struct currentThread* threads;


/* WORKING THREAD */

struct currentThread
{
        pthread_t thread;
        struct requestQueue* pointer;
        int flag;
};


/* QUEUING THREAD */

struct requestQueue
{
	int client_id;
	int reqType;
	char* reqPath;
	int len;
	time_t receiveTime;
	time_t assignTime;
};


void* HTTPresponse(void* id);
struct requestQueue* process(char* p);
void* initSJF(void* ptr);
void* initFCFS(void* ptr);


int main(int argc, char** argv)
{
	printf("WEB SERVER INITIALIZATION\n");
	opterr = 0;
       int option;
	while ((option = getopt(argc, argv, "d:h:I:p:r:t:n:s")) != -1)
       {
		switch (option) 
               {

               case 'd':
                        debug = 1;
                        break;

		case 'h':
                        printf("The usage summary is \n");

               case 'I':
			logfile = (char *) malloc(sizeof optarg);
			strcpy(logfile, optarg);
			f = 1;
			break;

		case 'p':
			PORT_NUMBER = atoi(optarg);
			break;

		case 'r':
			dir = (char *) malloc(sizeof optarg);
			strcpy(dir, optarg);
			break;

		case 't':
			t = atoi(optarg);
			break;

		case 'n':
			NTHREADS = atoi(optarg);
			break;

		case 's':
			sched = atoi(optarg);
			break;

			printf(
					"-d          : Enter debugging mode. That is, do not daemonize, only accept one connection at a time and enable logging to stdout. Without this option, the web server should run as a daemon process in the background.\n");
			printf(
					"-h          : Print a usage summary with all options and exit.\n");
			printf(
                                      "-I file     : Log all requests to the given file.\n");
			printf(
					"-p port     : Listen on the given port. If not provided, myhttpd will listen on port 8080.\n");
			printf(
					"-r dir      : Set the root directory for the http server to dir\n");
			printf(
					"-t time     : Set the queuing time to time seconds. The default should be 60 seconds\n");
			printf(
					"-n threadnum: Set number of threads waiting ready in the execution thread pool to threadnum. The default should be 4 execution threads.\n");
			printf(
					"-s sched    : Set the scheduling policy. It can be either FCFS or SJF. The default will be FCFS.\n");
			exit(1);
			break;
		default:
			printf("Other Options\n");
		}
	}
	char path[80];


/* VALUES OF PRINT OPTIONS */

	if (debug == 1) 
       {
		printf("The port number is %d\n", PORT_NUMBER);
               printf("Enter the debugging mode.\n");
		if (dir != NULL) 
               {
			printf("The root directory is %s\n", dir);
		} 
                        else 
               {
			getcwd(path, sizeof(path));
			dir = (char *) malloc(sizeof path);
			strcpy(dir, path);
			printf("The root directory is %s\n", dir);
		}
		printf("The queueing time is %ds\n", t);
		NTHREADS = 1;
	}
                else 
               {
		if (f == 1) 
               {
			printf("The log file name is %s\n", logfile);
		}
		printf("The port number is %d\n", PORT_NUMBER);
		if (dir != NULL) 
               {
			printf("The root directory is %s\n", dir);
		}
                        else 
               {
			char cwd[1024];
       if (getcwd(cwd, sizeof(cwd)) != NULL)
           fprintf(stdout, "The current working directory is %s\n", cwd);
       else
           perror("getcwd() error");
			getcwd(path, sizeof(path));
			dir = (char *) malloc(sizeof path);
			strcpy(path, dir);
			printf("The root directory is %s\n", cwd);
		}
		
		printf("The number of executable threads are %d\n", NTHREADS);
               printf("The queueing time is %ds\n", t);
		if (sched == 0) 
               {
			printf("The scheduling policy is FCFS\n");
		} 
                        else 
               {
			printf("The scheduling policy is SJF\n");
		}
	}



/* DAEMON PROCESS */

	if (debug == 0) 
       {
	      pid_t pid;
             if (pid != 0)
             exit(0);                                                                       // PARENT GOES //                                                                      
             else if ((pid = fork()) < 0)
             return (-1);
	      setsid();                                                                     // BECOMES SESSION LEADER //
	      chdir(dir);                                                                   // CWD //
	      umask(0);
	}


/* LOCAL VARIABLES FOR SOCKET CONNECTION  */

      unsigned int client_socket;                                                            // CLIENT SOCKET DESCRIPTOR //
      struct sockaddr_in client_address;                                                     // CLIENT INTERNET ADDRESS //
      socklen_t address_len;                                                                 // INTERNET ADDRESS LENGTH //
      unsigned int server_socket;                                                            // SERVER SOCKET DESCRIPTOR //
      struct sockaddr_in server_address;                                                     // SERVER INTERNET ADDRESS //
	
	
/* CREATING A NEW SOCKET */

	server_socket = socket(AF_INET, SOCK_STREAM, 0);

/* LISTENS & ACCEPTS CONNECTIONS */

	listen(server_socket, PENDING_CONNECTIONS);


/* BINDING ADDRESS INFORMATION */

	server_address.sin_port = htons(PORT_NUMBER);
       server_address.sin_addr.s_addr = htonl(INADDR_ANY);
       server_address.sin_family = AF_INET;
	bind(server_socket, (struct sockaddr *) &server_address, sizeof(server_address));



/* REQUEST QUEING ARRAY INITIALIZATION */

	int a=0;
	while (a < PENDING_CONNECTIONS) 
	{
		q[a] = (struct requestQueue*) malloc(sizeof(struct requestQueue));
		a++;
	}

        working_threads = (sem_t *) malloc(NTHREADS * sizeof(sem_t));
	threads = (struct currentThread*) malloc(NTHREADS * sizeof(struct currentThread));
	


/* MUTEX AND SEMAPHORE INITIALIZATION */


        pthread_mutex_init(&qLock, NULL);
        pthread_cond_init(&wContd, NULL);
        pthread_mutex_init(&wLock, NULL);
	sem_init(&count, 0, 0);
        sem_init(&idle_threads, 0, NTHREADS);
	
	int k=0;

	while(k < NTHREADS)
	{
		sem_init(&working_threads[k], 0, 0);
		 k++;
	}


/* N THREADS THREADPOOL CREATION */

	int index = 0;
	while ( index < NTHREADS )
	{
		pthread_cond_wait(&wContd, &wLock);
               pthread_create(&((threads + index)->thread), NULL, HTTPresponse, &index);
		(threads + index)->flag = 1;
		pthread_mutex_lock(&wLock);
               pthread_mutex_unlock(&wLock);
		index++;
	}


/* INTIAL SCHEDULING THREAD */

	pthread_t scheduler;
	if (sched == 0) 
	{
		pthread_create(&scheduler, NULL, initFCFS, NULL);
	} 
	else 
	{
		pthread_create(&scheduler, NULL, initSJF, NULL);
	}
	printf(
			"MY HTTPD WEBSERVER\n");

	time_t rTime;
       char in_buf[BUFFER_SIZE];                                                                   // INPUT BUFFER FOR HTTP REQUEST //


/* WEB SERVER INITIALIZATION */

	while (TRUE)
	{
		client_socket = accept(server_socket, (struct sockaddr *) &client_address,
				&address_len);                                                       // WAITS FOR THE CLIENT ARRIVAL // 
               address_len = sizeof(client_address);
		if (client_socket == FALSE) 
		{
			printf("ERROR - Unable to create socket \n");
			exit(FALSE);
		} 
              else
              {
			printf("NEW REQUEST ARRIVES.\n");
			recv(client_socket, in_buf, BUFFER_SIZE, 0);
			pthread_mutex_lock(&qLock);                                                 // ADDS TO THE REQUEST QUEUE // 
			int j=0;
			while ( j < PENDING_CONNECTIONS) 
			{
				if (q[j]->client_id == 0) 
				{
					time(&rTime);
				      q[j]->receiveTime = rTime;
                                      q[j] = process(in_buf);
					q[j]->client_id = client_socket;
					sem_post(&count);
					break;
				}
				j++;
			}
			pthread_mutex_unlock(&qLock);
		}
	}
}



/* EXECUTION THREAD */

void* HTTPresponse(void* id) 
{
       int index = *(int*) id;
       
       pthread_cond_signal(&wContd);
       pthread_mutex_unlock(&wLock);
       pthread_mutex_lock(&wLock);
       while (1) 
       printf("Thread %d  is ready.\n", index + 1);
       {
		sem_wait(&working_threads[index]);
		pthread_mutex_lock(&wLock);
		char *file_name;                                                                    // NAME OF THE FILE //
               char out_buf[BUFFER_SIZE];                                                          // OUTPUT BUFFER FOR HTML RESPONSE // 
		unsigned int fd;                                                                    // FILE DESCRIPTOR /
		unsigned int buffer_length;                                                         // BUFFER LENGTH //
		unsigned int fh;                                                                    // FILE HANDLE // 
		char buffer[100] = { '\0' };
		
               char * pch;
	       char ipstr[100] = { '\0' };
	       socklen_t len;
	       struct sockaddr_storage addr;
	       FILE *pfile = NULL;
	       struct tm *aptm, *rptm;
               struct dirent **namelist;
		struct stat buf;
               time_t current_time;
               int pos, n;
		
               if (strstr(file_name, ".") == NULL)
 {
				n = scandir(file_name, &namelist, 0, alphasort);
				if (n < 0) 
                              { 
                                        strcpy(out_buf, MESS_404);
					send(fd, out_buf, strlen(out_buf), 0);
					strcpy(out_buf, NOTOK_404);
					send(fd, out_buf, strlen(out_buf), 0);
					if ((f == 1) && (debug == 0)) 
                                      {
						if ((threads + index)->pointer->reqType == 0) 
                                              {
						fprintf(pfile, " 404 %d\n",
									(threads + index)->pointer->len);	
                                                fprintf(pfile, " \"GET /%s HTTP1.0\"", file_name);
							
						}
                                             else
                                             {
							
							fprintf(pfile, " 404 %d\n",
									(threads + index)->pointer->len);
                                                        fprintf(pfile, " \"HEAD /%s HTTP1.0\"", file_name);
						}
						fclose(pfile);
					} 
                                     else 
                                     {
						if ((threads + index)->pointer->reqType == 0) 
                                             {
						       printf(" 404 %d\n",
									(threads + index)->pointer->len);
                                                       printf(" \"GET /%s HTTP1.0\"", file_name);
							
						} 
                                             else 
                                             {          
                                                        printf(" 404 %d\n",
									(threads + index)->pointer->len);
							printf(" \"HEAD /%s HTTP1.0\"", file_name);
							
						}
					}
				} 
                              else 
                              {
					strcpy(out_buf, "<html>\n<body>\n");
                                        strcpy(out_buf, OK_MESSAGE);
					strcat(out_buf, "Content-Type:  text/html\n\n");
					send(fd, out_buf, strlen(out_buf), 0);
					while (n--) 
                                      {
						pch = strchr(namelist[n]->d_name, '.');
						pos = pch - namelist[n]->d_name + 1;
						if (pos != 1) 
                                             {
							strcat(out_buf, namelist[n]->d_name);
							strcat(out_buf, "</p>\n");
                                                        strcat(out_buf, "<p>");
							free(namelist[n]);
						}
					}
					free(namelist);
					strcat(out_buf, "</body>\n</html>\n");
					send(fd, out_buf, strlen(out_buf), 0);

					if ((f == 1) && (debug == 0)) 
                                      {
						if ((threads + index)->pointer->reqType == 0) 
                                             {
							fprintf(pfile, " 200 %d\n",
									(threads + index)->pointer->len);
                                                        fprintf(pfile, " \"GET /%s HTTP1.0\"", file_name);
						} 
                                             else 
                                             {
							fprintf(pfile, " 200 %d\n",
									(threads + index)->pointer->len);
                                                        fprintf(pfile, " \"HEAD /%s HTTP1.0\"", file_name);							
						}
						fclose(pfile);
					}  
                                      else
                                      {
						if ((threads + index)->pointer->reqType == 0) 
                                             {
							printf(" \"GET /%s HTTP1.0\"", file_name);
							printf(" 200 %d\n",
									(threads + index)->pointer->len);
						}
                                             else 
                                             {
							printf(" \"HEAD /%s HTTP1.0\"", file_name);
							printf(" 200 %d\n",
									(threads + index)->pointer->len);
						}
					}
				}
				close(fd);                                                                // CLIENT CONNECTION CLOSES //
			}  
                      else 
                      {
				fh = open(&file_name[0], O_RDONLY);
		



               if ((threads + index)->flag == 0)                                                   // SCHEDULED THREAD //
               {                                                                                   
			 len = sizeof addr;
                       fd = (threads + index)->pointer->client_id;
                       file_name = (threads + index)->pointer->reqPath;                            
			getpeername(fd, (struct sockaddr*) &addr, &len);                            // REMOTE IP ADDRESS //
			struct sockaddr_in *s = (struct sockaddr_in *) &addr;
			inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof ipstr);

			if ((f == 1) && (debug == 0))
                       {
				if (!(pfile = fopen(logfile, "a")))
                              {
					printf("The log file cannot be opened.");
					exit(1);
				}
				fputs(ipstr, pfile);
				fprintf(pfile, " - ");
				
				aptm = gmtime(&(threads + index)->pointer->assignTime);              // THE TIME AT WHICH THE REQUEST WAS ASSIGNED TO AN EXECUTION THREAD BY THE SCHEDULER (IN GMT) //
				
                              rptm = gmtime(&(threads + index)->pointer->receiveTime);             // THE TIME AT WHICH THE REQUEST WAS RECEIVED BY THE QUEUING THREAD (IN GMT) //
				fprintf(pfile, "[%d/%d/%d:%d:%d:%d -0400]", rptm->tm_mday,
						rptm->tm_mon + 1, rptm->tm_year + 1900, rptm->tm_hour,
						rptm->tm_min, rptm->tm_sec);
                              fprintf(pfile, " [%d/%d/%d:%d:%d:%d -0400]", aptm->tm_mday,
						aptm->tm_mon + 1, aptm->tm_year + 1900, aptm->tm_hour,
						aptm->tm_min, aptm->tm_sec);
			} 
                      else 
                      {
				printf("%s", ipstr);
				printf(" - ");
                              rptm = gmtime(&(threads + index)->pointer->receiveTime);
				printf("[%d/%d/%d:%d:%d:%d -0400]", rptm->tm_mday,
						rptm->tm_mon + 1, rptm->tm_year + 1900, rptm->tm_hour,
						rptm->tm_min, rptm->tm_sec);
                              aptm = gmtime(&(threads + index)->pointer->assignTime);
				printf(" [%d/%d/%d:%d:%d:%d -0400]", aptm->tm_mday,
						aptm->tm_mon + 1, aptm->tm_year + 1900, aptm->tm_hour,
						aptm->tm_min, aptm->tm_sec);
			}

			
				

/* GENERATION AND SENDING OF HTTP RESPONSE */
				if (fh == -1) 
                              {
					printf("The file %s is not found - sending an HTTP 404 \n",
							&file_name[0]);
					strcpy(out_buf, NOTOK_404);
					send(fd, out_buf, strlen(out_buf), 0);
					time(&current_time);
				      strcpy(
							out_buf,
							"Server:        WEB SERVER Version 1.0\n\n");
				      send(fd, out_buf, strlen(out_buf), 0);
					strcpy(out_buf, "Date:          ");
					send(fd, out_buf, strlen(out_buf), 0);
				        strcpy(out_buf, ctime(&current_time));
				      send(fd, out_buf, strlen(out_buf), 0);	
					
					if ((f == 1) && (debug == 0)) 
                                     {
						if ((threads + index)->pointer->reqType == 0) 
                                             {
							fprintf(pfile, " 404 %d\n",
									(threads + index)->pointer->len);
                                                     fprintf(pfile, " \"GET /%s HTTP1.0\"", file_name);
							
						} 
                                             else 
                                             {
							fprintf(pfile, " 404 %d\n",
									(threads + index)->pointer->len);
						}
                                                     fprintf(pfile, " \"HEAD /%s HTTP1.0\"", file_name);
							
						fclose(pfile);
					}
                                     else
                                     {
						if ((threads + index)->pointer->reqType == 0) 
                                             {
							printf(" 404 %d\n",
									(threads + index)->pointer->len);
                                                     printf(" \"GET /%s HTTP1.0\"", file_name);
							
						}
                                             else
                                             {
							printf(" 404 %d\n",
									(threads + index)->pointer->len);
                                                     printf(" \"HEAD /%s HTTP1.0\"", file_name);
							
						}
					}
					close(fd);                                                        // CLIENT CONNECTION CLOSES //
				} 
                              else
                              {
					if ((strstr(file_name, ".jpg") != NULL)
							|| (strstr(file_name, ".gif") != NULL))
                                             {
						strcpy(out_buf, OK_IMAGE);
						time(&current_time);
						stat(&file_name[0], &buf);
                                             strcat(
								out_buf,
								"Server:        WEB SERVER Version 1.0\n");
					     strcat(out_buf, "Last-Modified: ");
					     strcat(out_buf, ctime(&buf.st_mtime));
						strcat(out_buf, "Content-Type:  image/gif\n");
                                             strcat(out_buf, "Date:          ");
					     strcat(out_buf, ctime(&current_time));
						strcat(out_buf, "Content-Length:");
						sprintf(buffer, "%d",
								(threads + index)->pointer->len);
						strcat(out_buf, buffer);
						strcat(out_buf, "\n\n");
						send(fd, out_buf, strlen(out_buf), 0);
					} 
                                     else
                                     {
						strcpy(out_buf, OK_MESSAGE);
						time(&current_time);
						stat(&file_name[0], &buf);
                                             strcat(out_buf, ctime(&current_time));
						strcat(out_buf, "Date:          ");
						strcat(out_buf, "Last-Modified: ");
						
                                             strcat(
								out_buf,
								"Server:        WEB SERVER Version 1.0\n");
						strcat(out_buf, "Content-Length:");
						sprintf(buffer, "%d",
								(threads + index)->pointer->len);
						strcat(out_buf, buffer);
                                             strcat(out_buf, ctime(&buf.st_mtime));
					     strcat(out_buf, "Content-Type:  text/html\n");
						strcat(out_buf, "\n\n");
						send(fd, out_buf, strlen(out_buf), 0);
					}
                                      if ((threads + index)->pointer->reqType == 0) 
                                     {
						strcpy(out_buf, MESS_404);
						send(fd, out_buf, strlen(out_buf), 0);
					}
					if ((threads + index)->pointer->reqType == 0) 
                                     {
						buffer_length = 1;
						while (buffer_length > 0) 
                                             {
							buffer_length = read(fh, out_buf, BUFFER_SIZE);
							if (buffer_length > 0) 
                                                     {
								send(fd, out_buf, buffer_length, 0);
							}
						}
						printf("The file %s is being sent \n", &file_name[0]);
					}
					close(fh); 
					if ((f == 1) && (debug == 0)) 
                                      {
						if ((threads + index)->pointer->reqType == 0) 
                                             {
						       fprintf(pfile, " 200 %d\n",
									(threads + index)->pointer->len);
                                                    fprintf(pfile, " \"GET /%s HTTP1.0\"", file_name);
							
						} 
                                             else 
                                             {
						       fprintf(pfile, " 200 %d\n",
									(threads + index)->pointer->len);
                                                    fprintf(pfile, " \"HEAD /%s HTTP1.0\"", file_name);
							
						}
						fclose(pfile);
					} 
                                     else
                                     {
						if ((threads + index)->pointer->reqType == 0) 
                                             {
						       printf(" 200 %d\n",
									(threads + index)->pointer->len);
                                                    printf(" \"GET /%s HTTP1.0\"", file_name);
							
						}
                                             else
                                             {
						       printf(" 200 %d\n",
									(threads + index)->pointer->len);
                                                    printf(" \"HEAD /%s HTTP1.0\"", file_name);
							
						}
					}
					close(fd);                                                      // CLIENT CONNECTION CLOSES //
				}
			}
			(threads + index)->flag = 1;
			sem_post(&idle_threads);
			pthread_mutex_unlock(&wLock);
		}
	}
}


/* SJF SCHEDULE THREAD */
void* initSJF(void* ptr) 
{
	sleep(t);
	while (1) 
       {
		sem_wait(&count);
		pthread_mutex_lock(&qLock);                                                            // GETS REQUEST FROM THE REQUEST QUEUE //
		struct requestQueue* request = q[0];                                                  // GETS THE FIRST REQUEST IN THE QUEUE //
		int i=1, k = 0; 
		while ( i < PENDING_CONNECTIONS) 
               {
				
		      if ((q[i]->len) < (request->len)) 
                      {
				request = q[i];
				k = i;
			}
                        break;
                        if (q[i]->client_id == 0)
			i++;
		}
		pthread_mutex_unlock(&qLock);

		sem_wait(&idle_threads);                                                              // ASSIGNS THE REQUEST TO AN IDLE EXECUTION THREAD //
		pthread_mutex_lock(&wLock);
                time_t aTime;
		int s=0;
		
		while ( s < NTHREADS) 
               {
			if ((threads + s)->flag == 1) 
                       {
				(threads + s)->flag = 0;
                                (threads + s)->pointer = request;
				printf("The file %s is assigned to thread %d\n",
						(threads + s)->pointer->reqPath, s + 1);
				time(&aTime);
				(threads + s)->pointer->assignTime = aTime;
				sem_post(&working_threads[s]);
				break;
			}
			 s++;
		}
		pthread_mutex_unlock(&wLock);

		pthread_mutex_lock(&qLock);
		int j;
		j = k;
		while ( j < PENDING_CONNECTIONS) 
               {
			q[j] = q[j + 1];
			if (q[j + 1]->client_id == 0) 
                       {
				break;
			}
			 j++;
		}
		pthread_mutex_unlock(&qLock);
	}
}

/* HTTP REQUEST PROCESSING */ 
struct requestQueue* process(char* p) 
{
	char *file_name, *request_type;                                                             // FILE NAME & REQUEST TYPE //
	int len, len1, len2, len3, fh;
        struct stat buf;
        struct requestQueue* req = (struct requestQueue*) malloc(sizeof(struct requestQueue));
	request_type = strtok(p, " ");
	if (strcmp(request_type, "GET") == 0) 
       {
		req->reqType = 0;
	} 
       else 
       {
		req->reqType = 1;
	}

	file_name = strtok(NULL, " ");

	if (strstr(file_name, "~") != NULL) 
       {
		if (strstr(file_name, ".") != NULL)
               {
			len2 = strlen(dir);
                       len1 = strlen(&file_name[2]);
			req->reqPath = malloc(len1 + len2 + 2);
			strcpy(req->reqPath, dir);
			strcat(req->reqPath, &file_name[2]);
                        strcat(req->reqPath, "/");
			req->len = 0;
			return req;
		}
               else 
               {
			len2 = strlen(dir);
			len3 = strlen(&file_name[2]);
                        len1 = strlen("index.html");
			req->reqPath = malloc(len1 + len2 + len3 + 3);
			strcpy(req->reqPath, dir);
                        strcat(req->reqPath, &file_name[2]);
			strcat(req->reqPath, "/");
			strcat(req->reqPath, "index.html");
			strcat(req->reqPath, "/");	
	        	fh = open(req->reqPath, O_RDONLY);
			if (fh != -1) 
                       {
				stat(req->reqPath, &buf);
				req->len = buf.st_size;
				return req;
			} 
                      else 
                      {
				len2 = strlen(dir);
                              len1 = strlen(&file_name[2]);
				req->reqPath = malloc(len1 + len2 + 2);
				strcpy(req->reqPath, dir);
                              strcat(req->reqPath, &file_name[2]);
				strcat(req->reqPath, "/");	
				req->len = 0;
				return req;
			}
		}
	} 
       else
       {
		if (strstr(file_name, ".") != NULL) 
               {
			len = strlen(&file_name[1]);
			req->reqPath = malloc(len + 1);
                       stat(&file_name[1], &buf);
			strcpy(req->reqPath, &file_name[1]);
			
			req->len = buf.st_size;
			return req;
		}
              else
              {
                       len2 = strlen(dir);
			len3 = strlen(&file_name[1]);
			len1 = strlen("index.html");
			req->reqPath = malloc(len1 + len2 + len3 + 3);
			strcpy(req->reqPath, dir);
                        strcat(req->reqPath, "/");
			strcat(req->reqPath, "index.html");
			strcat(req->reqPath, "/");
			strcat(req->reqPath, &file_name[1]);
			
			fh = open(req->reqPath, O_RDONLY);
			if (fh != -1) 
			{
				stat(req->reqPath, &buf);
				req->len = buf.st_size;
				return req;
			} 
			else
			{
				len2 = strlen(dir);
                                len = strlen(&file_name[1]);
				req->reqPath = malloc(len + len2 + 2);
				strcpy(req->reqPath, dir);
				strcat(req->reqPath, "/");
				strcpy(req->reqPath, &file_name[1]);
				req->len = 0;
				return req;
			}
		}
	}
}


/* FCFS SCHEDULE THREAD */
void* initFCFS(void* ptr) 
{
       sleep(t);
       while (1) 
	
	{
		sem_wait(&count);

/* REQUEST FROM THE READY QUEUE */

		int i=0;
               struct requestQueue* request = q[0];                                                  // GETS THE FIRST REQUEST IN THE QUEUE // 
               pthread_mutex_lock(&qLock); 
		
		while ( i < PENDING_CONNECTIONS) 
               {
			q[i] = q[i + 1];
			if (q[i + 1]->client_id == 0) 
                      {
				break;
			}
			 i++;
		}
		pthread_mutex_unlock(&qLock); 


/* REQUEST ASSIGNMENT TO EXECUTION THREAD */
		sem_wait(&idle_threads);
		pthread_mutex_lock(&wLock);
		int s=0;
		time_t aTime;
		
		while (s < NTHREADS) 
               {
			if ((threads + s)->flag == 1) 
                       {
				(threads + s)->flag = 0;
                              (threads + s)->pointer = request;	
				printf("The file %s is assigned to thread %d\n",
						(threads + s)->pointer->reqPath, s + 1);
				time(&aTime);
				(threads + s)->pointer->assignTime = aTime;
				sem_post(&working_threads[s]);
				break;
			}
			s++;
		}
		pthread_mutex_unlock(&wLock);
	}
}

