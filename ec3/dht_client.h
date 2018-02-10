#ifndef __dht_client_h__
#define __dht_client_h__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mylib.h"
#include "sha1.h"

#ifndef STORE
#define STORE 's'
#endif

#ifndef STORE_REPLY
#define STORE_REPLY 'S'
#endif

#ifndef ITERATIVE
#define ITERATIVE 'i'
#endif

#ifndef ITERA_REPLY
#define ITERA_REPLY 'I'
#endif

#ifndef ITERA_NEXT
#define ITERA_NEXT 'N'
#endif

#ifndef RECURSIVE
#define RECURSIVE 'r'
#endif

#ifndef RECUR_REPLY
#define RECUR_REPLY 'R'
#endif

#ifndef EXIT
#define EXIT 'e'
#endif

#ifndef DATA
#define DATA 6  
#endif

#ifndef ACK
#define ACK 7  
#endif

#ifndef RETRIEVE
#define RETRIEVE 8  
#endif

// maximum size of object name in bytes.
#ifndef OBJ_NAME_SIZE
#define OBJ_NAME_SIZE 64
#endif

// maximum packet size in bytes.
#ifndef PACKET_SIZE
#define PACKET_SIZE 1500
#endif

// size of SHA1 hash value, 160 bits.
#ifndef SHA1ValueSize
#define SHA1ValueSize 20  
#endif

// command line
void parseCommand (int argc, char *argv[]);
char clientUserInterface ();

// sha1
unsigned short computeSha1 (char *obj_name);
void printID (unsigned char *id);

// CHORD
void sendReq2Node (char req_type, unsigned short obj_id, char *ip, char *port);
void sendReq2Root (char req_type, unsigned short obj_id);
void sendStoreReq2Root (unsigned short obj_id);
void sendRecurRetrv2Root (unsigned short obj_id);
void sendIteraRetrv2Root (unsigned short obj_id);
void sendIteraRetrv2Node (unsigned short obj_id, char *node_ip, char *node_port);

void sendRetrv2Target (int sock, unsigned short obj_id, char *filename);

void sendFileToTarget (int sock, char *filename, int filesize, char *filedata);
void sendAck (unsigned int ip, unsigned short port);

void handleStoreReply (int recv_sock, char *obj_name);
void handleRecurReply (int recv_sock, unsigned short obj_id, char *obj_name);
void handleIteraReply (int recv_sock, unsigned short obj_id, char *obj_name);
void handleData (char *msg); 

#endif