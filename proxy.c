/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the 
 *     GET method to serve static and dynamic content.
 */
#include "csapp.h"
#include <string.h>

void doit(int fd);
void parser(char* s, int fd);
void forward(int returnfd, char* host, char* port, char* path);

#define MAX_CACHE_SIZE = 16777216;
#define MAX_OBJECT_SIZE = 8388608;
/*
typedef struct listNode{
        int node_size;
        struct listNode *next;
        struct listNode *prev;
        char* data;
	char* request;
} listNode;

typedef struct doublyLinkedList {
        listNode *head;
        listNode *tail;
        int cache_size;
} doublyLinkedList;

void insertHead(doublyLinkedList *listPtr, int data_size, char* request, char* data){
        listNode *newNodePtr = (listNode *) malloc(sizeof(listNode));
        newNodePtr->data = data;
	newNodePtr->request = request;
        newNodePtr->node_size = data_size;
        newNodePtr->prev = NULL;

        if (listPtr->head == NULL){
                listPtr->head = newNodePtr;
                listPtr->tail = newNodePtr;
                newNodePtr->next = NULL;
        }
        else{
                newNodePtr->next = listPtr->head;
                listPtr->head->prev = newNodePtr;
                listPtr->head = newNodePtr;
        }
}

char* removeTail(doublyLinkedList *listPtr){
        listNode *tail = listPtr->tail;
        char* returnValue = tail->data;
        if (listPtr->head == listPtr->tail){
                listPtr->head = NULL;
                listPtr->tail = NULL;
        }

        else{
                listPtr->tail = listPtr->tail->prev;
                listPtr->tail->next = NULL;
        }
        free(tail);
        return returnValue;
}

void freeList(doublyLinkedList *listPtr){
        listNode *head = listPtr->head;
        listNode *temp;
        while(head != NULL){
                temp = head->next;
                free(head);
                head = temp;
        }
        listPtr->head = NULL;
        listPtr->tail = NULL;
}

struct doublyLinkedList *cache;
*/

int main(int argc, char **argv) 
{
    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    
    /* Check command line args */
    if (argc != 2) {
	fprintf(stderr, "usage: %s <port>\n", argv[0]);
	exit(1);
    }

    Signal(SIGPIPE, SIG_IGN);
    listenfd = Open_listenfd(argv[1]);
    while (1) {
	clientlen = sizeof(clientaddr);
	connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); //line:netp:tiny:accept
        Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, 
                    port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);
	
	doit(connfd);                                             //line:netp:tiny:doit
	Close(connfd);                                            //line:netp:tiny:close
    }
}
/* $end tinymain */

void parser(char* s, int fd){
	//printf("s: %s\n", s);
	char host[MAXLINE];
	char port[MAXLINE];
	char path[MAXLINE];
	char cmpGet[3];
	int colonexists = 1;

	strncpy(cmpGet, s, 3);
	if (strcmp(cmpGet, "GET") == 0){
		s += 4;
	}
	else{
		return;
	}

	char cmpHttp[7];
	strncpy(cmpHttp, s, 7);
	if (strcmp(cmpHttp, "http://") == 0){
		s += 7;
	}
		
	if (strstr(s, ":") != NULL){	
		int i = 0;
		while (s[i] != ':'){
			i++;
		}
		strncpy(host, s, i);
		host[i] = '\0';
		s += i + 1;	
	}
	else{
		int i = 0;
		while (s[i] != '/'){
	    		i++;
		}
		strncpy(host, s, i);
		host[i] = '\0';
		s += i + 1;
		strncpy(port, "80", strlen("80"));
		port[strlen("80")] = '\0';
		colonexists = 0;
	}

	if (strstr(s, "/") != NULL && colonexists){
		int j = 0;
		while (s[j] != '/'){
			j++;
		}
	
		strncpy(port, s, j);
		port[j] = '\0';
		s += j;
	}
	
	if (strstr(s, "HTTP/1.1") != NULL){
		int k = 0;
		char httpSequence[8];
		while(strncpy(httpSequence, s + k, 8) && (strcmp(httpSequence, "HTTP/1.1") != 0)){
			k++;
		} 

		strncpy(path, s, k);
		path[k] = '\0';
	}
	
	if (path[0] != '/'){
		int z;
		path[strlen(path) + 1] = '\0';
		for(z = strlen(path); z >= 0; z--){
			path[z] = path[z - 1];
		}
		path[0] = '/';
	}

	forward(fd, host, port, path);
}

int parseHeaderForContentLength(char* h){
	char sequence[14];
	char* newPointer = h;
	int i = 0;
	while(strncpy(sequence, h + i, 14) && (strcmp(sequence, "Content-length") != 0 && strcmp(sequence, "Content-Length")) != 0){
		newPointer += 1;
		i ++;
	}
	newPointer += 15;	
	int j = 0;
	while (newPointer[j] != '\n'){
		j++;
	}

	char size[MAXLINE];
	strncpy(size, newPointer, j);
	return atoi(size);	
}

void forward(int returnfd, char* host, char* port, char* path){
	int clientfd = open_clientfd(host, port);
	rio_t rio;
	rio_readinitb(&rio, clientfd);
		
	char request[MAXLINE];
	sprintf(request, "GET %s HTTP/1.0\nHost: %s\nConnection: close\nProxy-Connection: close \r\n\r\n", path, host);

	
	rio_writen(clientfd, request, strlen(request));
	
	char headerBuf[strlen("\r\n")];
	char buf[MAXLINE];
	int bytes;

	char header[MAXLINE];
	
	while((bytes = rio_readlineb(&rio, headerBuf, strlen("\r\n") + 1)) != 0){
		rio_writen(returnfd, headerBuf, bytes);
		strcat(header, headerBuf);
		if (strcmp(headerBuf, "\r\n") == 0){
			break;
		}
	}
	
	int data_size = 0;
	
	if (strstr(header, "Content-length") != NULL || strstr(header, "Content-Length") != NULL){
		data_size = parseHeaderForContentLength(header);
	}
	
	char data[data_size];	
	memset(data, 0, data_size);
		
	int size = 0;

	while((bytes = rio_readnb(&rio, buf, MAXLINE)) != 0){
		size += bytes;
		strncat(data, buf, bytes);
		rio_writen(returnfd, buf, bytes);
	}
	//For some reason int size is bigger than the actual size of data.

	//printf("Size that data should be: %d\n", data_size);
	//printf("Size that data actually is: %d\n", strlen(data));
        //printf("Data: %s\n", data);
	
	memset(header, 0, strlen(header));
	memset(data, 0, strlen(data));
	Close(clientfd);
}
/*
 * doit - handle one HTTP request/response transaction
 */
/* $begin doit */
void doit(int fd) 
{
    char buf[MAXLINE];
    rio_t rio;

    /* Read request line and headers */
    rio_readinitb(&rio, fd); 
    if (!rio_readlineb(&rio, buf, MAXLINE))  //line:netp:doit:readrequest
        return;

    if (strstr(buf, "CONNECT") == NULL){
    	printf("%s\n", buf);
    }

    parser(buf, fd);
}
/* $end doit */
