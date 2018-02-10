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

#ifndef PORT_SIZE
#define PORT_SIZE 6  
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
#define CRASH 9  
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
void printID (unsigned char *id);

// CHORD
void handleJoinMsg (char *msg, PeerNode *root);
void handleJoinRecursive(char *msg, PeerNode *my_node);
void handleCrash(char *msg, PeerNode *my_node);

void buildJoinReq (char *res);
void buildJoinReply (char *res, unsigned short *id, 
  unsigned int *suc_ip, unsigned short *suc_port, unsigned short *suc_id, 
  unsigned int *pre_ip, unsigned short *pre_port, unsigned short *pre_id);

void sendJoinReqToRoot (int sock);
void sendJoinReqToPeer(int sock, unsigned int *ip, unsigned short *port, unsigned short *id);
void sendJoinReply(int sock, unsigned short *id, 
  unsigned int *suc_ip, unsigned short *suc_port, unsigned short *suc_id, 
  unsigned int *pre_ip, unsigned short *pre_port, unsigned short *pre_id);
void sendUpdateInfo (int sock, char msg_type, unsigned int *ip, unsigned short *port, unsigned short *id);
void sendCrashInfo (PeerNode *my_node);
void sendRepairMsg (int sock, PeerNode *my_node);

void parseJoin (char *msg, unsigned int *ip, unsigned short *port);
void parseJoinByPeer (char *msg, unsigned int *ip, unsigned short *port, unsigned short *id);
void parseJoinReply (char *msg, PeerNode *my_node);
void parseUpdateInfo (char *msg, PeerNode *my_node);

void printNode (PeerNode node);

#endif