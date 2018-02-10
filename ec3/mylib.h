#ifndef __mylib_h__
#define __mylib_h__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>


// Maxium outstanding connection requests
#ifndef MAX_REQUS
#define MAX_REQUS 5
#endif

#ifndef RECV_BUFSIZE
#define RECV_BUFSIZE 1500
#endif

#ifndef PORT_SIZE
#define PORT_SIZE 6  
#endif

// Error message and termination.
void userErrorMessage (const char *msg, const char *err_info);
void systemErrorMessage (const char *msg);
void sha1ErrorMsg(const char *msg, int err);

// string operation
char * combStr (char *s1, char *s2);

// initial socket
int initRecvSock(char *host, char *port, unsigned int *ip4);
int initSendSock (char *server, char *port);
int acceptConnect(int sock);
int tryConnect(char* server, char *port);

void ipBytes2String(unsigned int *ip4, char * ip4_str);
void portNum2String (unsigned short port, char *port_str);

void RecvMsg(int sock, char *recv_buffer, int buff_size);
void SendMsg(int sock, char *msg, int msg_len);

// file operations.
int readObjData (char *res, char *filename);
void writeObjData (char *filename, int filesize, char *filedata);

#endif