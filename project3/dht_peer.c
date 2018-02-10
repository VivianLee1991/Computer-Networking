// Project 3: CHORD Distributed Hash Table.
// Written by W. Li 
// Date completed: 12-03-2017

// This file is a DHT Peer program.

// Root host name: toque.ccs.neu.edu
// Root Port: 15070 ~ 15074

// Peer host names:
// Peer ports: 15070 ~ 15074

#include "dht_peer.h"

static int type = 0;  // 0: regular peer (default); 1: root.
static char *p_port;  // the peer's own port
static char *p_host;  // the peer's own hostname
static char *r_port;  // the root's port
static char *r_host;  // the root's hostname

static unsigned int   my_ip   = 0;  // local ip addr in 4 bytes
static unsigned short my_port = 0;  // local port in 2 bytes
static unsigned short my_id   = 0;  // local node ID in 2 bytes
static int recv_sock = 0;  // socket for receving message at given local port

static int s_char  = sizeof(char);
static int s_int   = sizeof(unsigned int);
static int s_short = sizeof(unsigned short);

pthread_t thread[3];
pthread_mutex_t mut;

static int msg_num = 0;


int main(int argc, char *argv[]) {
  
  // Parse the command line.
	parseCommand(argc, argv);

  // Initialize a receiving socket, and my_ip.
  recv_sock = initRecvSock(p_host, p_port, &my_ip);
  
  // Initialize my_port.
  my_port = (unsigned short) strtoul(p_port, NULL, 0);

  // Initialize mutex.
  pthread_mutex_init(&mut, NULL);  


  // PEER MODE:

  if (type == 0) 
  {
    // Prompt to user.
    peerUserInterface();
    
    // Construct local node information.
    PeerNode myNode = {0, 0, 0, my_ip, my_port, 0, 0, 0, 0};  // current peer node info
    printNode (myNode);
     
    // Send Join request to root.
    int root_sock = initSendSock(r_host, r_port);
    sendJoinReqToRoot(root_sock);
    close(root_sock);
    
    // main process for peer.

    threadCreatePeer(&myNode);
       
  }

  // ROOT MODE:

  if (type == 1)
  {  
    printf("This is a root.\n\n");

    char root_ip_str[INET_ADDRSTRLEN];      // root ip in string format
    ipBytes2String(&my_ip, root_ip_str);    // convert ip bytes to string
    my_id = computeSha1(root_ip_str, p_port);  // compute root's nodeID

    // Initialize the root node.
    PeerNode rootNode = {0, 0, 0, my_ip, my_port, my_id, 0, 0, 0};
    printNode(rootNode);

    // Main process for root.
    
    threadCreateRoot(&rootNode);

  }

  threadWait();

	return 0;
}


//************************     Helper Functions      ***************************

// Create multiple threads.

void threadCreatePeer (PeerNode *my_node) {

  memset(&thread, 0, sizeof(thread));

  if ( pthread_create(&thread[0], NULL, peerProcess, my_node)) 
  {
    userErrorMessage("pthread_create() failed", 
      "Thread creation failure for peerProcess()");
  }

  if ( pthread_create(&thread[2], NULL, pingPreNode, my_node))
  {
    userErrorMessage("pthread_create() failed", 
      "Thread creation failure for pingPreNode()");
  }
}


void threadCreateRoot (PeerNode *my_node) {

  memset(&thread, 0, sizeof(thread));
  
  if ( pthread_create(&thread[1], NULL, rootProcess, my_node))
  {
    userErrorMessage("pthread_create() failed", 
      "Thread creation failure for rootProcess()");
  }

  if ( pthread_create(&thread[2], NULL, pingPreNode, my_node))
  {
    userErrorMessage("pthread_create() failed", 
      "Thread creation failure for pingPreNode()");
  }
}


// Wait for the thread program to end

void threadWait() {

  if (thread[0] != 0)
  {
    pthread_join(thread[0], NULL);
    printf("%s\n", "peerProcess() ended.");
  }

  if (thread[1] != 0)
  {
    pthread_join(thread[1], NULL);
    printf("%s\n", "rootProcess() ended.");
  }

  if (thread[2] != 0)
  {
    pthread_join(thread[2], NULL);
    printf("%s\n", "pingPreNode() ended.");
  }

}


// peer node keeps receiving messages.

void *peerProcess (void *node) {

  PeerNode *my_node = (PeerNode *) node;

  while(1) {

    char recv_buffer[RECV_BUFSIZE];        // buffer to store received message.
    memset(recv_buffer, 0, RECV_BUFSIZE);
    
    int cur_sock = acceptConnect(recv_sock);
    RecvMsg(cur_sock, recv_buffer, RECV_BUFSIZE);

    char msg_type;
    memcpy(&msg_type, recv_buffer, s_char);

    pthread_mutex_lock(&mut);

    if (msg_type == JOIN_REPLY)
    {
      printf("%s\n", "Received a JOIN REPLY!");
      parseJoinReply (recv_buffer, my_node);
      my_id = my_node->cur_ID;
      printNode (*my_node);     
    }

    if (msg_type == UPDATE_SUC || msg_type == UPDATE_PRE)
    {
      printf("%s\n", "Received an UPDATE!");
      parseUpdateInfo(recv_buffer, my_node);
      printNode (*my_node);
    }

    if (msg_type == JOIN)
    {
      handleJoinRecursive(recv_buffer, my_node);
      printNode(*my_node);
    }

    if (msg_type == CRASH)
    {
      handleCrash(recv_buffer, my_node);
      printNode(*my_node);
    }

    close(cur_sock);
    pthread_mutex_unlock(&mut);
  }
}


// root node keeps receiving messages.

void *rootProcess (void *node) {

  PeerNode *my_node = (PeerNode *) node;

  while(1) {

    //msg_num++;
    //printf("Receive %d message\n", msg_num);

    char recv_buffer[RECV_BUFSIZE];        // buffer to store received message.
    memset(recv_buffer, 0, RECV_BUFSIZE);
    
    int cur_sock = acceptConnect(recv_sock);
    RecvMsg(cur_sock, recv_buffer, RECV_BUFSIZE);

    char msg_type;
    memcpy(&msg_type, recv_buffer, s_char);  // detect type of message

    pthread_mutex_lock(&mut);

    if (msg_type == JOIN)
    {
      handleJoinMsg(recv_buffer, my_node);
      printNode(*my_node);
    }

    if (msg_type == UPDATE_PRE)
    {
      printf("%s\n", "Received an UPDATE!");
      parseUpdateInfo(recv_buffer, my_node);
      printNode (*my_node);
    }

    if (msg_type == CRASH)
    {
      handleCrash(recv_buffer, my_node);
      printNode(*my_node);
    }

    close(cur_sock);
    pthread_mutex_unlock(&mut);
  }
}


// Periodically ping this node's predecessor to detect a node Leave 
//or Crash in this Ring

void *pingPreNode (void *node) {

  PeerNode *my_node = (PeerNode *) node;

  while(1) {

    // Constuct predecessor's IP and Port

    char pre_ip_str[INET_ADDRSTRLEN];  // ip addr in string format
    char pre_port_str[PORT_SIZE];      // port in string format
    ipBytes2String(&(my_node->pre_IP), pre_ip_str);    // convert ip addr to string
    sprintf(pre_port_str, "%u", my_node->pre_Port);  // convert port number to string
     
     // Ping the predecessor.

    int pang = 0;
    if (my_node->pre_ID != 0)
    {
       pang = tryConnect(pre_ip_str, pre_port_str);
    }

    if (pang < 0) // detect a node crash
    {
      // repair the Ring
      if (my_node->suc_ID != my_node->pre_ID)
      {
        sendCrashInfo(my_node);
      }
      else {
        // the Ring only has me in it.
        my_node->pre_IP   = 0;
        my_node->pre_Port = 0;
        my_node->pre_ID   = 0;

        my_node->suc_IP   = 0;
        my_node->suc_Port = 0;
        my_node->suc_ID   = 0;
        printNode(*my_node);
      }
    }
    
    sleep(2);
  }
}


// GIVEN : the number of arguments on command line (argc), 
//         and the commands (argv).
// EFFECT: find the type of the peer, the peer's own port and hostname,
//         and the root's port and hostname.

void parseCommand(int argc, char *argv[]) {

  // Test for correct number of arguments in command line.

  if ((argc != 11) && (argc != 9) && (argc != 7)) {
    userErrorMessage("Command Error", 
      "Input format is : ./dht_peer <-m type> <-p own_port> \
      <-h own_hostname> <-r root_port> <-R root_hostname>");
  }

  for (int i = 1; i < argc; i++) {

    if (strcmp(argv[i], "-m") == 0) {
      type = atoi(argv[i+1]);
      continue;
    }

    if (strcmp(argv[i], "-p") == 0) {
      p_port = argv[i+1];
      continue;
    }

    if (strcmp(argv[i], "-h") == 0) {
      p_host = argv[i+1];
      continue;
    }

    if (strcmp(argv[i], "-r") == 0) {
      r_port = argv[i+1];
      continue;
    }

    if (strcmp(argv[i], "-R") == 0) {
      r_host = argv[i+1];
      continue;
    }
  }

  // Test for parseCommand().
  //printf("type is %d\n", type);
  //printf("own port is %s\n", p_port);
  //printf("own hostname is %s\n", p_host);
  //printf("root port is %s\n", r_port);
  //printf("root hostname is %s\n", r_host);
}

// user interface to control the peer

void peerUserInterface() {

  char key[2];  // input from command line.

  printf("This is a peer.\n\nMenu\nPress '1': Join\nPress '0': Leave\n");
  fgets(key, 2, stdin);

  if (key[0] != '1') { 
    exit(1); 
  }
}

// GIVEN  : an IP address or name of an object, and the port.
// RETURNS: the nodeID or objID (16-bit integer) for the given string 
//          based on SHA1.

unsigned short computeSha1(char *ip, char *port) {

  SHA1Context sha;
  uint8_t Message_Digest[SHA1ValueSize];  // to store the 160-bit hash value
  unsigned short res;  // the 16-bit node/obj ID.
  int i, err;

  char *input = combStr(ip, port);  // combine ip and port as the input of SHA1
  //printf("Input of SHA1: %s\n", input);

  err = SHA1Reset(&sha);
  if (err) {
    sha1ErrorMsg("SHA1Reset Error",err);
  }

  err = SHA1Input(&sha, (const unsigned char *) input, strlen(input));
  if (err) {
    sha1ErrorMsg("SHA1Input Error",err);
  }

  err = SHA1Result(&sha, Message_Digest);
  if (err) {
    sha1ErrorMsg("SHA1Result Error",err);
  }
  
  // print result to check.
  //printID(Message_Digest);

  // convert computed result to unsigned short (0 ~ 65535).
  memcpy(&res, Message_Digest, sizeof(unsigned short));
  //printf("Node ID: %d\n", res);

  return res;
}


// print the 160-bit hash value in hex format.

void printID(unsigned char *id) {

  for(int i = 0; i < SHA1ValueSize ; ++i) { 
    printf("%02X ", id[i]);
  }
  printf("\n");
}


// Handle the received message

void handleJoinMsg (char *msg, PeerNode *root) {

  char type;
  memcpy(&type, msg, sizeof(char));

  if (type == JOIN)
  {
    // Retrieve new node information

    unsigned int new_IP;
    unsigned short new_Port;
    unsigned short new_ID = 0;

    parseJoin(msg, &new_IP, &new_Port);  // Retrieve new node's IP and Port

    char new_ip_str[INET_ADDRSTRLEN];  // ip addr in string format
    char new_port_str[PORT_SIZE];      // port in string format

    ipBytes2String(&new_IP, new_ip_str);    // convert ip addr to string
    sprintf(new_port_str, "%u", new_Port);  // convert port number to string

    new_ID = computeSha1(new_ip_str, new_port_str);  // compute nodeID for the new node
    printf("Receive a JOIN from: IP - %s, Port - %s, Node ID - %d.\n", new_ip_str, new_port_str, new_ID);


    if (root->suc_ID == 0)  // Initial state of the Ring (only with a root node).
    { 
      // update root node info
      root->suc_IP   = new_IP;
      root->suc_Port = new_Port;
      root->suc_ID   = new_ID;

      root->pre_IP   = new_IP;
      root->pre_Port = new_Port;
      root->pre_ID   = new_ID;
      
      // send reply to the new node.
      int new_sock = initSendSock(new_ip_str, new_port_str);
      sendJoinReply(new_sock, &new_ID, &my_ip, &my_port, &my_id, &my_ip, &my_port, &my_id);
      close(new_sock);
    }


    else if (root->pre_ID == root->suc_ID)  // Only 2 nodes in the Ring.
    {
      char suc_ip_str[INET_ADDRSTRLEN];  // ip addr in string format
      char suc_port_str[PORT_SIZE];      // port in string format
      ipBytes2String(&(root->suc_IP), suc_ip_str);  // convert ip addr to string
      sprintf(suc_port_str, "%u", root->suc_Port);  // convert port number to string

      if (((root->cur_ID < root->suc_ID) && ((new_ID < root->cur_ID) || (new_ID > root->suc_ID))) 
        ||((root->cur_ID > root->suc_ID) && (new_ID < root->cur_ID) && (new_ID > root->suc_ID)))
      {
        // send reply to the new node.

        int new_sock = initSendSock(new_ip_str, new_port_str);
        // Set root as new node's suc, set root.suc as new node's pre.
        sendJoinReply(new_sock, &new_ID, &my_ip, &my_port, &my_id, &(root->suc_IP), &(root->suc_Port), &(root->suc_ID));
        close(new_sock);

        // update root's successor 
        int suc_sock = initSendSock(suc_ip_str, suc_port_str);
        // set new node as root.suc's suc.
        sendUpdateInfo(suc_sock, UPDATE_SUC, &new_IP, &new_Port, &new_ID);
        close(suc_sock);

        // update root's predecessor as new node

        root->pre_IP   = new_IP;
        root->pre_Port = new_Port;
        root->pre_ID   = new_ID;
      }

      else {
        // send reply to the new node.

        int new_sock = initSendSock(new_ip_str, new_port_str);
        // Set root as new node's pre, set root.suc as new node's suc.
        sendJoinReply(new_sock, &new_ID, &(root->suc_IP), &(root->suc_Port), &(root->suc_ID), &my_ip, &my_port, &my_id);
        close(new_sock);

        // update root's successor
        int suc_sock = initSendSock(suc_ip_str, suc_port_str);
        // set new node as root.suc's pre.
        sendUpdateInfo(suc_sock, UPDATE_PRE, &new_IP, &new_Port, &new_ID);
        close(suc_sock);

        // update root's successor as new node

        root->suc_IP   = new_IP;
        root->suc_Port = new_Port;
        root->suc_ID   = new_ID;
      }
    }


    else if (root->pre_ID != root->suc_ID)  // 3 or more nodes in the Ring
    {
      char suc_ip_str[INET_ADDRSTRLEN];  // ip addr in string format
      char suc_port_str[PORT_SIZE];      // port in string format
      ipBytes2String(&(root->suc_IP), suc_ip_str);  // convert ip addr to string
      sprintf(suc_port_str, "%u", root->suc_Port);  // convert port number to string

      bool mid = (root->cur_ID < new_ID) && (root->suc_ID > new_ID); // mid node ID
      bool min = (root->cur_ID > new_ID) && (root->suc_ID > new_ID) && (root->cur_ID > root->suc_ID); // new min node ID
      bool max = (root->cur_ID < new_ID) && (root->suc_ID < new_ID) && (root->cur_ID > root->suc_ID); // new max node ID 

      if (mid || min || max)  
      {
        // send reply to the new node.

        int new_sock = initSendSock(new_ip_str, new_port_str);
        // Set root as new node's pre, set root.suc as new node's suc.
        sendJoinReply(new_sock, &new_ID, &(root->suc_IP), &(root->suc_Port), &(root->suc_ID), &my_ip, &my_port, &my_id);
        close(new_sock);

        // update root's successor
        int suc_sock = initSendSock(suc_ip_str, suc_port_str);
        // set new node as root.suc's pre.
        sendUpdateInfo(suc_sock, UPDATE_PRE, &new_IP, &new_Port, &new_ID);
        close(suc_sock);

        // update root's successor as new node

        root->suc_IP   = new_IP;
        root->suc_Port = new_Port;
        root->suc_ID   = new_ID;
      }

      else {
        // Pass on join request to the successor node
        int suc_sock = initSendSock(suc_ip_str, suc_port_str);

        sendJoinReqToPeer(suc_sock, &new_IP, &new_Port, &new_ID);
        close(suc_sock);
      }
    }
  }
}


void handleJoinRecursive(char *msg, PeerNode *my_node) {

  unsigned int new_IP = 0;
  unsigned short new_Port = 0;
  unsigned short new_ID = 0;

  parseJoinByPeer (msg, &new_IP, &new_Port, &new_ID);

  // new node IP and Port
  char new_ip_str[INET_ADDRSTRLEN];  // ip addr in string format
  char new_port_str[PORT_SIZE];      // port in string format
  ipBytes2String(&new_IP, new_ip_str);    // convert ip addr to string
  sprintf(new_port_str, "%u", new_Port);  // convert port number to string

  // suc node IP and Port
  char suc_ip_str[INET_ADDRSTRLEN];  // ip addr in string format
  char suc_port_str[PORT_SIZE];      // port in string format
  ipBytes2String(&(my_node->suc_IP), suc_ip_str);  // convert ip addr to string
  sprintf(suc_port_str, "%u", my_node->suc_Port);  // convert port number to string

  bool mid = (my_node->cur_ID < new_ID) && (my_node->suc_ID > new_ID); // mid node ID
  bool min = (my_node->cur_ID > new_ID) && (my_node->suc_ID > new_ID) && (my_node->cur_ID > my_node->suc_ID); // new min node ID
  bool max = (my_node->cur_ID < new_ID) && (my_node->suc_ID < new_ID) && (my_node->cur_ID > my_node->suc_ID); // new max node ID 

  if (mid || min || max)  
  {
    // send reply to the new node.

    int new_sock = initSendSock(new_ip_str, new_port_str);
    // Set my_node as new node's pre, set my_node.suc as new node's suc.
    sendJoinReply(new_sock, &new_ID, &(my_node->suc_IP), &(my_node->suc_Port), &(my_node->suc_ID), &my_ip, &my_port, &my_id);
    close(new_sock);

    // update my_node's successor

    int suc_sock = initSendSock(suc_ip_str, suc_port_str);
    // set new node as my_node.suc's pre.
    sendUpdateInfo(suc_sock, UPDATE_PRE, &new_IP, &new_Port, &new_ID);
    close(suc_sock);

    // update my_node's successor as new node

    my_node->suc_IP   = new_IP;
    my_node->suc_Port = new_Port;
    my_node->suc_ID   = new_ID;
  }

  else {
    // Pass on join request to the successor node
    int suc_sock = initSendSock(suc_ip_str, suc_port_str);

    sendJoinReqToPeer(suc_sock, &new_IP, &new_Port, &new_ID);
    close(suc_sock);
  }
}


void handleCrash(char *msg, PeerNode *my_node) {

  unsigned short crash_ID = 0;
  unsigned int   src_IP   = 0;
  unsigned short src_Port = 0;
  unsigned short src_ID   = 0;

  // parse the CRASH message.
  const int info_size = s_char + s_int + 3 * s_short;

  memcpy(&crash_ID, msg + s_char, s_short);
  memcpy(&src_IP, msg + s_char + s_short, s_int);
  memcpy(&src_Port, msg + s_char + s_short + s_int, s_short);
  memcpy(&src_ID, msg + s_char + s_short + s_int + s_short, s_short);

  if (crash_ID == my_node->suc_ID)  // I am the crash node's predecessor
  {
    // connect the crash node's suc
    // update my successor as the src node who create the original crash info.
    my_node->suc_IP   = src_IP;
    my_node->suc_Port = src_Port;
    my_node->suc_ID   = src_ID;

    // send REPAIR message to the src node.
    char src_ip_str[INET_ADDRSTRLEN];  // ip addr in string format
    char src_port_str[PORT_SIZE];      // port in string format
    ipBytes2String(&src_IP, src_ip_str);     // convert ip addr to string
    sprintf(src_port_str, "%u", src_Port);  // convert port number to string

    int src_sock = initSendSock(src_ip_str, src_port_str);
    sendRepairMsg (src_sock, my_node);
    close(src_sock);

    printf("%s\n", "Repair message sent.");

  }
  else {
    // pass on the crash message
    char suc_ip_str[INET_ADDRSTRLEN];  // ip addr in string format
    char suc_port_str[PORT_SIZE];      // port in string format
    ipBytes2String(&(my_node->suc_IP), suc_ip_str);  // convert ip addr to string
    sprintf(suc_port_str, "%u", my_node->suc_Port);  // convert port number to string
    
    int suc_sock = initSendSock(suc_ip_str, suc_port_str);
    SendMsg(suc_sock, msg, info_size);
    close(suc_sock);

    printf("%s\n", "Crash message passed on.");
  } 

}


// construct a join request message, which includes the message type,
// the peer's IP address, and its port.
// Join Request:
// JOIN + new_IP + new_Port

void buildJoinReq (char *res) {

  char msg_type = JOIN;
 
  memcpy(res, &msg_type, sizeof(char));
  memcpy(res + sizeof(char), &my_ip, sizeof(unsigned int));
  memcpy(res + sizeof(char) + sizeof(unsigned int), &my_port, sizeof(unsigned short));
  
}


// send a join request to the root.

void sendJoinReqToRoot(int sock) {

  // intitial a buffer for join request
  const int req_size = sizeof(char) + sizeof(unsigned int) + sizeof(unsigned short);
  char join_request[req_size];

  buildJoinReq(join_request);

  SendMsg(sock, join_request, req_size);
  printf("%s\n", "JOIN sent.");
}

// pass on a join request to the successor node

void sendJoinReqToPeer(int sock, unsigned int *ip, unsigned short *port, unsigned short *id) {

  // intitial a buffer for join request
  const int req_size = s_char + s_int + 2 * s_short;
  char join_request[req_size];

  char msg_type = JOIN;
  memcpy(join_request, &msg_type, s_char);
  memcpy(join_request + s_char, ip, s_int);
  memcpy(join_request + s_char + s_int, port, s_short);
  memcpy(join_request + s_char + s_int + s_short, id, s_short);

  SendMsg(sock, join_request, req_size);
  printf("%s\n", "JOIN sent to successor.");
}


// Join Reply:
// JOIN_REPLY + NodeID + suc_IP + suc_Port + suc_ID + pre_IP + pre_Port + pre_ID

void buildJoinReply (char *res, unsigned short *id, 
  unsigned int *suc_ip, unsigned short *suc_port, unsigned short *suc_id, 
  unsigned int *pre_ip, unsigned short *pre_port, unsigned short *pre_id) {

  char msg_type = JOIN_REPLY;

  memcpy(res, &msg_type, s_char);

  memcpy(res + s_char, id, s_short);

  memcpy(res + s_char + s_short, suc_ip, s_int);
  memcpy(res + s_char + s_short + s_int, suc_port, s_short);
  memcpy(res + s_char + s_short + s_int + s_short, suc_id, s_short);

  memcpy(res + s_char + s_short + s_int + s_short + s_short, pre_ip, s_int);
  memcpy(res + s_char + s_short + s_int + s_short + s_short + s_int, pre_port, s_short);
  memcpy(res + s_char + s_short + s_int + s_short + s_short + s_int + s_short, pre_id, s_short);

}


// send JOIN reply to the new node.

void sendJoinReply(int sock, unsigned short *id, 
  unsigned int *suc_ip, unsigned short *suc_port, unsigned short *suc_id, 
  unsigned int *pre_ip, unsigned short *pre_port, unsigned short *pre_id) {

  // intitial a buffer for join reply
  const int rep_size = sizeof(char) + 2 * sizeof(unsigned int) + 5 * sizeof(unsigned short);
  char join_reply[rep_size];

  buildJoinReply(join_reply, id, suc_ip, suc_port, suc_id, pre_ip, pre_port, pre_id);

  SendMsg(sock, join_reply, rep_size);
  printf("%s\n", "JOIN_REPLY sent.");
}


// send an UPDATE information to an existing node.
// Update Info:
// UPDATE_SUC/UPDATE_PRE + IP + Port + ID.
// msg_type = UPDATE_SUC : update the successor as specified
// msg_type = UPDATE_PRE : update the predecessor as specified

void sendUpdateInfo (int sock, char msg_type, unsigned int *ip, unsigned short *port, unsigned short *id) {

  const int info_size = s_char + s_int + 2 * s_short;
  char update_msg[info_size];
  memset(update_msg, 0, info_size);

  char update_type = msg_type;

  memcpy(update_msg, &update_type, s_char);
  memcpy(update_msg + s_char, ip, s_int);
  memcpy(update_msg + s_char + s_int, port, s_short);
  memcpy(update_msg + s_char + s_int + s_short, id, s_short);

  SendMsg(sock, update_msg, info_size);
  printf("%s\n", "Update message sent.");

}


// Send crash info to the successor
// Crash Info:
// CRASH + crash_ID + my_IP + my_Port + my_ID

void sendCrashInfo (PeerNode *my_node) {

  char msg_type = CRASH;

  const int info_size = s_char + s_int + 3 * s_short;
  char crash_msg[info_size];
  memset(crash_msg, 0, info_size);

  // construct crash message
  memcpy(crash_msg, &msg_type, s_char);
  memcpy(crash_msg + s_char, &(my_node->pre_ID), s_short);
  memcpy(crash_msg + s_char + s_short, &(my_node->cur_IP), s_int);
  memcpy(crash_msg + s_char + s_short + s_int, &(my_node->cur_Port), s_short);
  memcpy(crash_msg + s_char + s_short + s_int + s_short, &(my_node->cur_ID), s_short);

  char suc_ip_str[INET_ADDRSTRLEN];  // ip addr in string format
  char suc_port_str[PORT_SIZE];      // port in string format
  ipBytes2String(&(my_node->suc_IP), suc_ip_str);  // convert ip addr to string
  sprintf(suc_port_str, "%u", my_node->suc_Port);  // convert port number to string

  int suc_sock = initSendSock(suc_ip_str, suc_port_str);
  SendMsg(suc_sock, crash_msg, info_size);
  close(suc_sock);

  printf("%s\n", "Crash message sent.");

}


// send a repair message to the src node.

void sendRepairMsg (int sock, PeerNode *my_node) {
  sendUpdateInfo (sock, UPDATE_PRE, &(my_node->cur_IP), &(my_node->cur_Port), &(my_node->cur_ID));
}


// Retrieve the IP and port of the peer who has sent the JOIN request.

void parseJoin (char *msg, unsigned int *ip, unsigned short *port) {
  
  memcpy(ip, msg + sizeof(char), sizeof(unsigned int));
  memcpy(port, msg + sizeof(char) + sizeof(unsigned int), sizeof(unsigned short));

}


// Retrieve the IP, port and node ID of the peer who wants to join the Ring.

void parseJoinByPeer (char *msg, unsigned int *ip, unsigned short *port, unsigned short *id) {
  
  memcpy(ip, msg + s_char, s_int);
  memcpy(port, msg + s_char + s_int, s_short);
  memcpy(id, msg + s_char + s_int + s_short, s_short);

}


void parseJoinReply (char *msg, PeerNode *my_node) {

  memcpy(&(my_node->cur_ID), msg + s_char, s_short);

  memcpy(&(my_node->suc_IP), msg + s_char + s_short, s_int);
  memcpy(&(my_node->suc_Port), msg + s_char + s_short + s_int, s_short);
  memcpy(&(my_node->suc_ID), msg + s_char + s_short + s_int + s_short, s_short);

  memcpy(&(my_node->pre_IP), msg + s_char + s_short + s_int + s_short + s_short, s_int);
  memcpy(&(my_node->pre_Port), msg + s_char + s_short + s_int + s_short + s_short + s_int, s_short);
  memcpy(&(my_node->pre_ID), msg + s_char + s_short + s_int + s_short + s_short + s_int + s_short, s_short);

}


void parseUpdateInfo (char *msg, PeerNode *my_node) {

  char update_type = 0;
  memcpy(&update_type, msg, s_char);

  if (update_type == UPDATE_SUC)  // update the successor
  {
    memcpy(&(my_node->suc_IP), msg + s_char, s_int);
    memcpy(&(my_node->suc_Port), msg + s_char + s_int, s_short);
    memcpy(&(my_node->suc_ID), msg + s_char + s_int + s_short, s_short);
  }

  if (update_type == UPDATE_PRE)  // update the predecessor
  {
    memcpy(&(my_node->pre_IP), msg + s_char, s_int);
    memcpy(&(my_node->pre_Port), msg + s_char + s_int, s_short);
    memcpy(&(my_node->pre_ID), msg + s_char + s_int + s_short, s_short);
  }
}


// print the node information

void printNode (PeerNode node) {

  // Print predecessor.
  printf("Predecesor:\t%d\n", node.pre_ID);
  if (node.pre_IP != 0)
  {
    char ip1[INET_ADDRSTRLEN];  // ip addr in string.
    ipBytes2String(&node.pre_IP, ip1);
  }
  else {
    printf("IP = %d\n", node.pre_IP);
  }
  printf("Port = %d\n", node.pre_Port);

  // Print current node.
  printf("Current Node:\t%d\n", node.cur_ID);
  if (node.cur_IP != 0)
  {
    char ip2[INET_ADDRSTRLEN];  // ip addr in string.
    ipBytes2String(&node.cur_IP, ip2);
  }
  else {
    printf("IP = %d\n", node.cur_IP);
  }
  printf("Port = %d\n", node.cur_Port);

  // Print successor.
  printf("Successor:\t%d\n", node.suc_ID);
  if (node.suc_IP != 0)
  {
    char ip3[INET_ADDRSTRLEN];  // ip addr in string.
    ipBytes2String(&node.suc_IP, ip3);
  }
  else {
    printf("IP = %d\n", node.suc_IP);
  }
  printf("Port = %d\n", node.suc_Port);
  
}
