#ifndef __dht_peer_h__
#define __dht_peer_h__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#include <pthread.h>
#include <sys/time.h>

#include "mylib.h"
#include "sha1.h"

// size of SHA1 hash value, 160 bits.

#ifndef SHA1ValueSize
#define SHA1ValueSize 20  
#endif

// Message type

#ifndef JOIN
#define JOIN 1  
#endif

#ifndef JOIN_REPLY
#define JOIN_REPLY 2  
#endif

#ifndef UPDATE_SUC
#define UPDATE_SUC 3  
#endif

#ifndef UPDATE_PRE
#define UPDATE_PRE 4  
#endif

#ifndef CRASH
#define CRASH 5  
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


// maximum size of object name in bytes.

#ifndef OBJ_NAME_SIZE
#define OBJ_NAME_SIZE 64
#endif

#ifndef INDEX_NAME_SIZE
#define INDEX_NAME_SIZE 20
#endif

// maximum packet size in bytes.

#ifndef PACKET_SIZE
#define PACKET_SIZE 1500
#endif

#ifndef ID_SIZE
#define ID_SIZE 6
#endif


typedef struct PeerNode {
    // predecessor peer
	unsigned int pre_IP;
	unsigned short pre_Port;
	unsigned short pre_ID;
    
    // own info
	unsigned int cur_IP;
	unsigned short cur_Port;
	unsigned short cur_ID;

	// successor peer
	unsigned int suc_IP;
	unsigned short suc_Port;
	unsigned short suc_ID;
	
} PeerNode;


// main process

void *peerProcess (void *node);
void *rootProcess (void *node);
void *pingPreNode (void *node);

// threads

void threadCreatePeer (PeerNode *my_node);
void threadCreateRoot (PeerNode *my_node);
void threadWait();

// Command operation.

void parseCommand (int argc, char *argv[]);
void peerUserInterface ();

// SHA1.

unsigned short computeSha1(char *ip, char *port);
unsigned short computeSha1Obj(char *obj_name);
void printID (unsigned char *id);

// CHORD

void handleJoinReq (char *msg, PeerNode *root);
void handleJoinRecursive(char *msg, PeerNode *my_node);
void handleCrash(char *msg, PeerNode *my_node);

void handleStoreReqByRoot (char *msg, PeerNode *my_node);
void handleStoreReqByPeer (char *msg, PeerNode *my_node);

void handleRecurRetrvByRoot (char *msg, PeerNode *my_node); 
void handleRecurRetrvByPeer (char *msg, PeerNode *my_node);

void handleIteraRetrv (char *msg, PeerNode *my_node);

void handleData (char *msg);
void handleRetrv (char *msg);

void sendJoinReqToRoot (int sock);
void sendJoinReqToPeer(int sock, unsigned int *ip, unsigned short *port, unsigned short *id);
void sendJoinReply(int sock, unsigned short *id, 
  unsigned int *suc_ip, unsigned short *suc_port, unsigned short *suc_id, 
  unsigned int *pre_ip, unsigned short *pre_port, unsigned short *pre_id);
void sendUpdateInfo (int sock, char msg_type, unsigned int *ip, unsigned short *port, unsigned short *id);

void sendCrashInfo (PeerNode *my_node);
void sendRepairInfo (int sock, PeerNode *my_node);

void sendReply2Client (int sock, char rpl_type, unsigned short *target_id, unsigned int *target_ip, unsigned short *target_port);
void sendStoreReply (int sock, unsigned short *target_id, unsigned int *target_ip, unsigned short *target_port);
void sendRecurReply (int sock, unsigned short *target_id, unsigned int *target_ip, unsigned short *target_port);
void sendIteraReply (int sock, unsigned short *target_id, unsigned int *target_ip, unsigned short *target_port);
void sendIteraNext (int sock, unsigned short *next_id, unsigned int *next_ip, unsigned short *next_port);

void sendReq2Suc (char msg_type, unsigned short obj_id, PeerNode *my_node);
void sendStoreReq2Suc (unsigned short obj_id, PeerNode *my_node);
void sendRecurReq2Suc (unsigned short obj_id, PeerNode *my_node);

void sendAck (unsigned int ip, unsigned short port);
void sendFile2Node (int sock, char *filename, int filesize, char *filedata);

void buildJoinReq (char *res);
void buildJoinReply (char *res, unsigned short *id, 
  unsigned int *suc_ip, unsigned short *suc_port, unsigned short *suc_id, 
  unsigned int *pre_ip, unsigned short *pre_port, unsigned short *pre_id);

void parseJoin (char *msg, unsigned int *ip, unsigned short *port);
void parseJoinByPeer (char *msg, unsigned int *ip, unsigned short *port, unsigned short *id);
void parseJoinReply (char *msg, PeerNode *my_node);
void parseUpdateInfo (char *msg, PeerNode *my_node);

void printNode (PeerNode node);


// index file operations.

void createIndexFile (unsigned short node_id);
void writeIndex (char *new_file);
void updateIndex (int new_pre_sock, PeerNode *my_node);
bool updateOrNot (unsigned short obj_id, PeerNode *my_node);

#endif