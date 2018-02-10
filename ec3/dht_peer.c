// Project 3: CHORD Distributed Hash Table.
// Written by Wenwen Li (NEU ID: 001830563)
// Date completed: 11-21-2017

// This file is a DHT Peer program.

// Root host name:
// Root Port: 15074

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

static int recv_sock = 0;           // socket for receving message at given local port
static char *index_file;            // index file name for local node.

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

    createIndexFile (my_id);

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
    close(cur_sock);

    char msg_type;  // detect message type.
    memcpy(&msg_type, recv_buffer, s_char);

    pthread_mutex_lock(&mut);

    if (msg_type == JOIN_REPLY)
    {
      printf("Received a JOIN REPLY!\n");
      parseJoinReply (recv_buffer, my_node);
      my_id = my_node->cur_ID;
      createIndexFile (my_id);
      printNode (*my_node);
    }

    if (msg_type == UPDATE_SUC || msg_type == UPDATE_PRE)
    {
      printf("Received an UPDATE!\n");
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

    if (msg_type == STORE)
    {
      handleStoreReqByPeer(recv_buffer, my_node);
    }

    if (msg_type == DATA)
    {
      printf("Received DATA.\n");
      handleData(recv_buffer);
    }

    if (msg_type == RECURSIVE)
    {
      handleRecurRetrvByPeer(recv_buffer, my_node);
    }

    if (msg_type == ITERATIVE)
    {
      printf("Received a ITERATIVE Retrieve request.\n");
      handleIteraRetrv(recv_buffer, my_node);
    }

    if (msg_type == RETRIEVE)
    {
      handleRetrv(recv_buffer);
    }
    
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
    close(cur_sock);

    char msg_type;
    memcpy(&msg_type, recv_buffer, s_char);  // detect type of message

    pthread_mutex_lock(&mut);

    if (msg_type == JOIN)
    {
      handleJoinReq(recv_buffer, my_node);
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

    if (msg_type == STORE)
    {
      printf("Received a STORE request.\n");
      handleStoreReqByRoot(recv_buffer, my_node);
    }

    if (msg_type == DATA)
    {
      handleData(recv_buffer);
    }

    if (msg_type == RECURSIVE)
    {
      printf("Received a RECURSIVE Retrieve request.\n");
      handleRecurRetrvByRoot(recv_buffer, my_node);
    }

    if (msg_type == ITERATIVE)
    {
      printf("Received a ITERATIVE Retrieve request.\n");
      handleIteraRetrv(recv_buffer, my_node);
    }

    if (msg_type == RETRIEVE)
    {
      handleRetrv(recv_buffer);
    }

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
    sprintf(pre_port_str, "%u", my_node->pre_Port);    // convert port number to string
     
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
    /*
    if ((my_node->pre_ID != 0) && !(pang < 0)) 
    {
      // Update index file.
      int pre_sock = initSendSock(pre_ip_str, pre_port_str);
      updateIndex (pre_sock, my_node);
      close(pre_sock);
    }
    */
    
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


// GIVEN  : an object name.
// RETURNS: the objID (16-bit integer) for the given object based on SHA1.

unsigned short computeSha1Obj(char *obj_name) {

  SHA1Context sha;
  uint8_t Message_Digest[SHA1ValueSize];  // to store the 160-bit hash value
  unsigned short res;  // the 16-bit node/obj ID.
  int i, err;

  err = SHA1Reset(&sha);
  if (err) {
    sha1ErrorMsg("SHA1Reset Error",err);
  }

  err = SHA1Input(&sha, (const unsigned char *) obj_name, strlen(obj_name));
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


// create a index file for each node.

void createIndexFile (unsigned short node_id) {

  char node_id_str[INDEX_NAME_SIZE];
  memset(node_id_str, 0, INDEX_NAME_SIZE);

  sprintf(node_id_str, "%u", node_id);
  index_file = combStr (node_id_str, ".txt");  // index file name, end with '.txt'

  char *dir = combStr ("./", index_file);

  if (access(dir, F_OK) == 0) 
  {
      printf("Index file < %s > can be accessed.\n", index_file);
  }
  else 
  {
    FILE *fh = NULL;
    fh = fopen(index_file, "w");  // create a new index file
    fclose(fh);
    fh = NULL;

    printf("Index file <%s> is created.\n", index_file);
  }
  //printf("index_file: %s\n", index_file);
}


// write the given file name 'new_file' to the local index file.

void writeIndex (char *obj_name) {

  const char new_line = '\n';
  const char tab = '\t';

  unsigned short obj_id = computeSha1Obj(obj_name);
  printf("obj_ID: %d\n", obj_id);

  // convert id num to string
  char obj_id_str[ID_SIZE];
  portNum2String (obj_id, obj_id_str);

  FILE *fh = NULL;
  fh = fopen(index_file, "a+"); // append new content to the end of the file, delete original EOF.
  if (fh == NULL ) {
    systemErrorMessage("3. File open error!");
  }

  fwrite(obj_id_str, s_char, ID_SIZE, fh);
  fwrite(&tab, s_char, 1, fh);

  fwrite(obj_name, s_char, strlen(obj_name), fh);
  fwrite(&new_line, s_char, 1, fh);

  fclose(fh);
  fh = NULL;

}


void updateIndex (int new_pre_sock, PeerNode *my_node) {
 
  FILE *fh = NULL;
  fh = fopen(index_file, "r");
  if (fh == NULL ) {
    systemErrorMessage("1. File open error!");
  }

  char *replica = combStr("replica", index_file);

  FILE *fh2 = NULL;
  fh2 = fopen(replica, "w");
  if (fh2 == NULL ) {
    systemErrorMessage("2. File open error!");
  }

  int line_size = ID_SIZE + s_char + OBJ_NAME_SIZE + s_char;  
  char line[line_size];          // buffer to store a line from index file.

  char obj_id_str[ID_SIZE];
  unsigned short obj_id = 0;     // objID retrieved from each line.

  char obj_name_buffer[OBJ_NAME_SIZE];  // object filename end with '\n'

  char obj_data[PACKET_SIZE];    // buffer to store object file data. 

  // traverse index to find: nodeID < new_pre_id

  while (! feof(fh)) 
  {
    memset(line, 0, line_size);

    if (fgets(line, line_size, fh) != NULL)  // read a line.
    {
      memcpy(obj_id_str, line, ID_SIZE);        // Retrieve an objID
      obj_id = (unsigned short) strtoul(obj_id_str, NULL, 0);

      bool update_cond = updateOrNot(obj_id, my_node);

      if (update_cond)                  // this object should be transferred to new pre node.
      {
        // retrieve obj name.
        memcpy(obj_name_buffer, line + ID_SIZE + s_char, OBJ_NAME_SIZE);  
        char *obj_name = strtok(obj_name_buffer, "\n"); 
        
        memset(obj_data, 0, PACKET_SIZE);  // clear out data buffer.                          

        // read and send object file to new predecessor.
        int obj_size = readObjData (obj_data, obj_name);
        if (obj_size > 0)
        {
          sendFile2Node(new_pre_sock, obj_name, obj_size, obj_data);
          printf("File <%s> transferred to %d.\n", obj_name, my_node->pre_ID);

          // delete object from disk.
          if (remove(obj_name) < 0) {
            systemErrorMessage("Remove file error!");
          } 
          else {
            printf("File <%s> deleted!\n", obj_name);
          }
        } 
      }

      else {
        fwrite(line, s_char, line_size, fh2);  // keep entry in replica.txt
      }
    }
  }

  if (remove(index_file) < 0)        // remove old index file
  {
    systemErrorMessage("remove old index file error");
  }

  rename(replica, index_file); // create new index file.

  fclose(fh);
  fclose(fh2);
  fh = NULL;
  fh2 = NULL;
}


// decide whether the given object should be assigned to my predecessor.

bool updateOrNot (unsigned short obj_id, PeerNode *my_node) {

  if (my_node->pre_ID < my_node->cur_ID) 
  {
      return ((obj_id > my_node->cur_ID) || (obj_id < my_node->pre_ID));
  }

  else  // my_node->pre_ID > my_node->cur_ID
  {
      return ((obj_id > my_node->cur_ID) && (obj_id < my_node->pre_ID));
  }
}


// Handle the received message
// This is only called by root node.

void handleJoinReq (char *msg, PeerNode *root) {

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


// This is called by peer node.

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


// This is called by any node, root or peer.

void handleCrash(char *msg, PeerNode *my_node) {

  unsigned short crash_ID = 0;
  unsigned int   src_IP   = 0;
  unsigned short src_Port = 0;
  unsigned short src_ID   = 0;

  // parse the CRASH message.
  const int msg_size = s_char + s_int + 3 * s_short;

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
    sendRepairInfo (src_sock, my_node);
    close(src_sock);

    printf("%s\n", "Repair message sent.");

  }
  else {
    // pass on the crash message to my successor.

    char suc_ip_str[INET_ADDRSTRLEN];  // ip addr in string format
    char suc_port_str[PORT_SIZE];      // port in string format
    ipBytes2String(&(my_node->suc_IP), suc_ip_str);  // convert ip addr to string
    sprintf(suc_port_str, "%u", my_node->suc_Port);  // convert port number to string
    
    int suc_sock = initSendSock(suc_ip_str, suc_port_str);
    SendMsg(suc_sock, msg, msg_size);
    close(suc_sock);

    printf("%s\n", "Crash message passed on.");
  } 

}


// This is called by a root.
// Store request (from client):
// STORE + obj_ID + client_IP + client_Port

void handleStoreReqByRoot (char *msg, PeerNode *my_node) {

  // Retrieve information from STORE message.

  unsigned short obj_ID      = 0;
  unsigned int   client_IP   = 0;
  unsigned short client_Port = 0;

  memcpy(&obj_ID, msg + s_char, s_short);
  memcpy(&client_IP, msg + s_char + s_short, s_int);
  memcpy(&client_Port, msg + s_char + s_short + s_int, s_short);

  // construct client IP and Port for socket creation.

  char clt_ip_str[INET_ADDRSTRLEN];        // clinet ip addr 
  char clt_port_str[PORT_SIZE];            // client port 
  ipBytes2String (&client_IP, clt_ip_str);  
  portNum2String (client_Port, clt_port_str);

  int clt_sock = initSendSock(clt_ip_str, clt_port_str);  // socket to reply client.


  if (my_node->suc_ID == 0)                     // only one root node in the ring
  {
    // store object on root
    sendStoreReply(clt_sock, &(my_node->cur_ID), &(my_node->cur_IP), &(my_node->cur_Port));
  }


  else if (my_node->suc_ID == my_node->pre_ID)  // 2 nodes in the ring, only called by root.
  {
    // store object on current node.

    if (((my_node->cur_ID < my_node->suc_ID) && ((obj_ID < my_node->cur_ID) || (obj_ID > my_node->suc_ID))) 
      ||((my_node->cur_ID > my_node->suc_ID) && (obj_ID < my_node->cur_ID) && (obj_ID > my_node->suc_ID))) 
    {
      sendStoreReply(clt_sock, &(my_node->cur_ID), &(my_node->cur_IP), &(my_node->cur_Port));
    }
    
    // store object on successor node.

    else 
    {
      sendStoreReply(clt_sock, &(my_node->suc_ID), &(my_node->suc_IP), &(my_node->suc_Port));
    }
  }


  else if (my_node->suc_ID != my_node->pre_ID)   // 3 or more nodes in the ring
  {
    bool mid = (my_node->cur_ID < obj_ID) && (my_node->suc_ID > obj_ID); // mid node ID
    bool min = (my_node->cur_ID > obj_ID) && (my_node->suc_ID > obj_ID) && (my_node->cur_ID > my_node->suc_ID); // new min node ID
    bool max = (my_node->cur_ID < obj_ID) && (my_node->suc_ID < obj_ID) && (my_node->cur_ID > my_node->suc_ID); // new max node ID 

    if (mid || min || max) 
    {   
      // store object on successor node
      sendStoreReply(clt_sock, &(my_node->suc_ID), &(my_node->suc_IP), &(my_node->suc_Port)); 
    }

    else {

      // pass on the store request to successor node

      sendStoreReq2Suc(obj_ID, my_node);

      // wait for store reply from a peer.
      char msg_type = 0;
      char recv_buffer[RECV_BUFSIZE];

      while (msg_type == 0) {
        
        memset(recv_buffer, 0, RECV_BUFSIZE);
        
        int cur_sock = acceptConnect(recv_sock);
        RecvMsg(cur_sock, recv_buffer, RECV_BUFSIZE);
        close(cur_sock);

        memcpy(&msg_type, recv_buffer, s_char);

      }

      // send store reply to client.

      if (msg_type == STORE_REPLY)
      {
        const int rpl_size = s_char + s_int + 2 * s_short;
        SendMsg(clt_sock, recv_buffer, rpl_size);
        printf("Store Reply message sent.\n");
      }
    }
  }

  close(clt_sock);
}


// This is called by a root.
// Recursive Retrieve:
// RECURSIVE + obj_ID + client_IP + client_Port

void handleRecurRetrvByRoot (char *msg, PeerNode *my_node) {

  // Retrieve information from RECURSIVE message.
  unsigned short obj_ID      = 0;
  unsigned int   client_IP   = 0;
  unsigned short client_Port = 0;

  memcpy(&obj_ID, msg + s_char, s_short);
  memcpy(&client_IP, msg + s_char + s_short, s_int);
  memcpy(&client_Port, msg + s_char + s_short + s_int, s_short);

  // construct client IP and Port for socket creation.

  char clt_ip_str[INET_ADDRSTRLEN];        // clinet ip addr 
  char clt_port_str[PORT_SIZE];            // client port 
  ipBytes2String (&client_IP, clt_ip_str);  
  portNum2String (client_Port, clt_port_str);

  int clt_sock = initSendSock(clt_ip_str, clt_port_str);  // socket to reply client.

  // find target node.

  if (my_node->suc_ID == 0)                     // only one root node in the ring
  {
    // retrieve object on root
    sendRecurReply(clt_sock, &(my_node->cur_ID), &(my_node->cur_IP), &(my_node->cur_Port));
  }


  else if (my_node->suc_ID == my_node->pre_ID)  // 2 nodes in the ring, only called by root.
  {
    // retrieve object on current node.
    if (((my_node->cur_ID < my_node->suc_ID) && ((obj_ID < my_node->cur_ID) || (obj_ID > my_node->suc_ID))) 
      ||((my_node->cur_ID > my_node->suc_ID) && (obj_ID < my_node->cur_ID) && (obj_ID > my_node->suc_ID))) 
    {
      sendRecurReply(clt_sock, &(my_node->cur_ID), &(my_node->cur_IP), &(my_node->cur_Port));
    }
    
    // retrieve object on successor node.
    else {
      sendRecurReply(clt_sock, &(my_node->suc_ID), &(my_node->suc_IP), &(my_node->suc_Port));
    }
  }


  else if (my_node->suc_ID != my_node->pre_ID)   // 3 or more nodes in the ring
  {
    bool mid = (my_node->cur_ID < obj_ID) && (my_node->suc_ID > obj_ID); // mid node ID
    bool min = (my_node->cur_ID > obj_ID) && (my_node->suc_ID > obj_ID) && (my_node->cur_ID > my_node->suc_ID); // new min node ID
    bool max = (my_node->cur_ID < obj_ID) && (my_node->suc_ID < obj_ID) && (my_node->cur_ID > my_node->suc_ID); // new max node ID 

    if (mid || min || max) {   
      // retrieve object on successor node
      sendRecurReply(clt_sock, &(my_node->suc_ID), &(my_node->suc_IP), &(my_node->suc_Port)); 
    }

    else {

      // pass on the RECURSIVE request to successor node
      sendRecurReq2Suc(obj_ID, my_node);

      // wait for Recursive reply from a peer.
      char msg_type = 0;
      char recv_buffer[RECV_BUFSIZE];

      while (msg_type == 0) {

        memset(recv_buffer, 0, RECV_BUFSIZE);
        
        int cur_sock = acceptConnect(recv_sock);
        RecvMsg(cur_sock, recv_buffer, RECV_BUFSIZE);
        close(cur_sock);

        memcpy(&msg_type, recv_buffer, s_char);
      }

      // send RECURSIVE reply to client.
      if (msg_type == RECUR_REPLY)
      {
        const int rpl_size = s_char + s_int + 2 * s_short;
        SendMsg(clt_sock, recv_buffer, rpl_size);
        printf("RECURSIVE Reply message sent.\n");
      }
    }
  }

  close(clt_sock);
}


// This is called by a root / peer.
// Iterative Retrieve:
// ITERATIVE + obj_ID + client_IP + client_Port

void handleIteraRetrv (char *msg, PeerNode *my_node) 
{

  // Retrieve information from RECURSIVE message.
  unsigned short obj_ID      = 0;
  unsigned int   client_IP   = 0;
  unsigned short client_Port = 0;

  memcpy(&obj_ID, msg + s_char, s_short);
  memcpy(&client_IP, msg + s_char + s_short, s_int);
  memcpy(&client_Port, msg + s_char + s_short + s_int, s_short);

  // construct client IP and Port for socket creation.

  char clt_ip_str[INET_ADDRSTRLEN];        // clinet ip addr 
  char clt_port_str[PORT_SIZE];            // client port 
  ipBytes2String (&client_IP, clt_ip_str);  
  portNum2String (client_Port, clt_port_str);

  int clt_sock = initSendSock(clt_ip_str, clt_port_str);  // socket to reply client.

  // find target node.

  if (my_node->suc_ID == 0)                     // only one root node in the ring
  {
    // retrieve object from root
    sendIteraReply(clt_sock, &(my_node->cur_ID), &(my_node->cur_IP), &(my_node->cur_Port));
  }


  else if (my_node->suc_ID == my_node->pre_ID)  // 2 nodes in the ring, only called by root.
  {
    // retrieve object on current node.
    if (((my_node->cur_ID < my_node->suc_ID) && ((obj_ID < my_node->cur_ID) || (obj_ID > my_node->suc_ID))) 
      ||((my_node->cur_ID > my_node->suc_ID) && (obj_ID < my_node->cur_ID) && (obj_ID > my_node->suc_ID))) 
    {
      sendIteraReply(clt_sock, &(my_node->cur_ID), &(my_node->cur_IP), &(my_node->cur_Port));
    }
    
    // retrieve object on successor node.
    else {
      sendIteraReply(clt_sock, &(my_node->suc_ID), &(my_node->suc_IP), &(my_node->suc_Port));
    }
  }

  else if (my_node->suc_ID != my_node->pre_ID)   // 3 or more nodes in the ring
  {
    bool mid = (my_node->cur_ID < obj_ID) && (my_node->suc_ID > obj_ID); // mid node ID
    bool min = (my_node->cur_ID > obj_ID) && (my_node->suc_ID > obj_ID) && (my_node->cur_ID > my_node->suc_ID); // new min node ID
    bool max = (my_node->cur_ID < obj_ID) && (my_node->suc_ID < obj_ID) && (my_node->cur_ID > my_node->suc_ID); // new max node ID 

    if (mid || min || max) {   
      // retrieve object on successor node
      sendIteraReply(clt_sock, &(my_node->suc_ID), &(my_node->suc_IP), &(my_node->suc_Port)); 
    }

    else {
      // Continue Iterative lookup on suc node.
      sendIteraNext(clt_sock, &(my_node->suc_ID), &(my_node->suc_IP), &(my_node->suc_Port));
    }

    close(clt_sock);
  }
}


// This is called by a peer.
// Store request (from peer/root):
// STORE + obj_ID

void handleStoreReqByPeer (char *msg, PeerNode *my_node) {

  // retrieve object ID
  unsigned short obj_ID = 0;
  memcpy(&obj_ID, msg + s_char, s_short);

  bool mid = (my_node->cur_ID < obj_ID) && (my_node->suc_ID > obj_ID); // mid node ID
  bool min = (my_node->cur_ID > obj_ID) && (my_node->suc_ID > obj_ID) && (my_node->cur_ID > my_node->suc_ID); // new min node ID
  bool max = (my_node->cur_ID < obj_ID) && (my_node->suc_ID < obj_ID) && (my_node->cur_ID > my_node->suc_ID); // new max node ID 

  if (mid || min || max) 
  {   
    // store object on successor node
    int root_sock = initSendSock(r_host, r_port);
    sendStoreReply(root_sock, &(my_node->suc_ID), &(my_node->suc_IP), &(my_node->suc_Port));
    close(root_sock); 
  }

  else {
    // pass on the store request to successor node
    sendStoreReq2Suc(obj_ID, my_node);
  }
}


// This is called by a peer.
// Recursive Retrieve (from peer/root):
// RECURSIVE + obj_ID

void handleRecurRetrvByPeer (char *msg, PeerNode *my_node) {

  // retrieve object ID
  unsigned short obj_ID = 0;
  memcpy(&obj_ID, msg + s_char, s_short);

  bool mid = (my_node->cur_ID < obj_ID) && (my_node->suc_ID > obj_ID); // mid node ID
  bool min = (my_node->cur_ID > obj_ID) && (my_node->suc_ID > obj_ID) && (my_node->cur_ID > my_node->suc_ID); // new min node ID
  bool max = (my_node->cur_ID < obj_ID) && (my_node->suc_ID < obj_ID) && (my_node->cur_ID > my_node->suc_ID); // new max node ID 

  if (mid || min || max) 
  {   
    // store object on successor node
    int root_sock = initSendSock(r_host, r_port);
    sendRecurReply (root_sock, &(my_node->suc_ID), &(my_node->suc_IP), &(my_node->suc_Port));
    close(root_sock); 
  }

  else {
    // pass on the store request to successor node
    sendRecurReq2Suc(obj_ID, my_node);
  }
}


// Process data message sent from client
// Data Message:
// DATA + client_IP + client_Port + filename + filesize + filedata 

void handleData (char *msg) {

  unsigned int   client_IP   = 0;
  unsigned short client_Port = 0;
  char filename[OBJ_NAME_SIZE];
  int filesize = 0;
  char filedata[PACKET_SIZE];

  // retrieve info.
  memcpy(&client_IP, msg + s_char, s_int);
  memcpy(&client_Port, msg + s_char + s_int, s_short);

  memcpy(filename, msg + s_char + s_int + s_short, OBJ_NAME_SIZE);
  memcpy(&filesize, msg + s_char + s_int + s_short + OBJ_NAME_SIZE, s_int);
  memcpy(filedata, msg + s_char + s_int + s_short + OBJ_NAME_SIZE + s_int, filesize);

  // store object on node.
  writeObjData (filename, filesize, filedata);

  // update index file.
  writeIndex (filename);

  // send ACK to client
  sendAck(client_IP, client_Port);

}


// handle RETRIEVE message sent from client.
// RETRIEVE Message:
// RETRIEVE + obj_ID + filename + client_IP + client_Port

void handleRetrv (char *msg) {

  unsigned short obj_ID = 0;
  char obj_name[OBJ_NAME_SIZE];
  unsigned int   client_IP   = 0;
  unsigned short client_Port = 0;

  memcpy(&obj_ID, msg + s_char, s_short);
  memcpy(obj_name, msg + s_char + s_short, OBJ_NAME_SIZE);
  memcpy(&client_IP, msg + s_char + s_short + OBJ_NAME_SIZE, s_int);
  memcpy(&client_Port, msg + s_char + s_short + OBJ_NAME_SIZE + s_int, s_short);

  // search object file in index.

  // construct client socket.
  char client_ip_str[INET_ADDRSTRLEN];
  char client_port_str[PORT_SIZE];
  ipBytes2String (&client_IP, client_ip_str);
  portNum2String (client_Port, client_port_str);

  int clt_sock = initSendSock(client_ip_str, client_port_str);

  // read object file to buffer.
  char obj_data[PACKET_SIZE];
  memset(obj_data, 0, PACKET_SIZE);

  int obj_size = readObjData (obj_data, obj_name);
  if (obj_size > 0)
  {
    // send object file to client.
    sendFile2Node (clt_sock, obj_name, obj_size, obj_data);
    printf("%s\n", "File sent to client!");
  }

  close(clt_sock);

  // wait for ACK.
  char recv_buffer[RECV_BUFSIZE];
  char ack_type = 0;

  while(ack_type == 0) {

    memset(recv_buffer, 0, RECV_BUFSIZE);

    int ack_sock = acceptConnect(recv_sock);
    RecvMsg(ack_sock, recv_buffer, RECV_BUFSIZE);
    close(ack_sock);
    
    memcpy(&ack_type, recv_buffer, s_char);
  }
  
  if (ack_type == ACK)
  {
    printf("%s\n", "File ACKed.");
  }
  else {
    printf("%s\n", "File transmission failure.");
  }
}


// Send object file to node specified by the given socket.
// Data Message:
// DATA + my_IP + my_Port + filename + filesize + filedata 

void sendFile2Node (int sock, char *filename, int filesize, char *filedata) {
  
  char msg_type = DATA;
  int  msg_size = s_char + s_int + s_short + OBJ_NAME_SIZE + s_int + filesize;
  char data_msg[msg_size];

  memset(data_msg, 0, msg_size);
  
  // construct data packet.

  memcpy(data_msg, &msg_type, s_char);

  memcpy(data_msg + s_char, &my_ip, s_int);
  memcpy(data_msg + s_char + s_int, &my_port, s_short);
  
  memcpy(data_msg + s_char + s_int + s_short, filename, strlen(filename));
  memcpy(data_msg + s_char + s_int + s_short + OBJ_NAME_SIZE, &filesize, s_int);
  memcpy(data_msg + s_char + s_int + s_short + OBJ_NAME_SIZE + s_int, filedata, filesize);

  SendMsg(sock, data_msg, msg_size);

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

  const int msg_size = s_char + s_int + 2 * s_short;
  char update_msg[msg_size];
  memset(update_msg, 0, msg_size);

  char update_type = msg_type;

  memcpy(update_msg, &update_type, s_char);
  memcpy(update_msg + s_char, ip, s_int);
  memcpy(update_msg + s_char + s_int, port, s_short);
  memcpy(update_msg + s_char + s_int + s_short, id, s_short);

  SendMsg(sock, update_msg, msg_size);
  printf("%s\n", "Update message sent.");

}


// Send crash info to the successor
// Crash Info:
// CRASH + crash_ID + my_IP + my_Port + my_ID

void sendCrashInfo (PeerNode *my_node) {

  char msg_type = CRASH;

  const int msg_size = s_char + s_int + 3 * s_short;
  char crash_msg[msg_size];
  memset(crash_msg, 0, msg_size);

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
  SendMsg(suc_sock, crash_msg, msg_size);
  close(suc_sock);

  printf("%s\n", "Crash message sent.");

}


// send a repair message to the src node.
// special case of Update predecessor.

void sendRepairInfo (int sock, PeerNode *my_node) {

  sendUpdateInfo (sock, UPDATE_PRE, &(my_node->cur_IP), &(my_node->cur_Port), &(my_node->cur_ID));
}


// Send given type of reply ('rpl_type') to client. 

void sendReply2Client (int sock, char rpl_type, unsigned short *target_id, unsigned int *target_ip, unsigned short *target_port) 
{
  char msg_type = rpl_type;

  const int msg_size = s_char + s_int + 2 * s_short;
  char msg[msg_size];
  memset(msg, 0, msg_size);

  memcpy(msg, &msg_type, s_char);
  memcpy(msg + s_char, target_id, s_short);
  memcpy(msg + s_char + s_short, target_ip, s_int);
  memcpy(msg + s_char + s_short + s_int, target_port, s_short);

  SendMsg(sock, msg, msg_size);
}


// root send a STORE reply to client
// Store Reply:
// STORE_REPLY + target_ID + target_IP + target_Port

void sendStoreReply (int sock, unsigned short *target_id, unsigned int *target_ip, unsigned short *target_port) {

  char msg_type = STORE_REPLY;
  sendReply2Client(sock, msg_type, target_id, target_ip, target_port);

  printf("%s\n", "STORE Reply message sent.");
}


// root send a RECURSIVE reply to client
// Recursive Reply:
// RECUR_REPLY + target_ID + target_IP + target_Port

void sendRecurReply (int sock, unsigned short *target_id, unsigned int *target_ip, unsigned short *target_port) {

  char msg_type = RECUR_REPLY;
  sendReply2Client(sock, msg_type, target_id, target_ip, target_port);

  printf("%s\n", "RECURSIVE Reply message sent.");
}


// send a ITERATIVE reply to client
// Iterative Reply:
// ITERA_REPLY + target_ID + target_IP + target_Port

void sendIteraReply (int sock, unsigned short *target_id, unsigned int *target_ip, unsigned short *target_port) {

  char msg_type = ITERA_REPLY;
  sendReply2Client(sock, msg_type, target_id, target_ip, target_port);

  printf("%s\n", "ITERATIVE Reply message sent.");
}


// send a ITERATIVE Next to client
// Iterative Next:
// ITERA_NEXT + next_ID + next_IP + next_Port

void sendIteraNext (int sock, unsigned short *next_id, unsigned int *next_ip, unsigned short *next_port) {

  char msg_type = ITERA_NEXT;
  sendReply2Client(sock, msg_type, next_id, next_ip, next_port);

  printf("%s\n", "ITERATIVE Next message sent.");
}


// Send given type of request to successor node.

void sendReq2Suc (char msg_type, unsigned short obj_id, PeerNode *my_node) {

  // construct suc IP and Port for socket creation.
  char suc_ip_str[INET_ADDRSTRLEN];        // successor ip addr 
  char suc_port_str[PORT_SIZE];            // successor port 
  ipBytes2String (&(my_node->suc_IP), suc_ip_str); 
  portNum2String (my_node->suc_Port, suc_port_str);

  int suc_sock = initSendSock(suc_ip_str, suc_port_str);

  // construct Store Request message.
  int msg_size = s_char + s_short;
  char msg[msg_size];

  memcpy(msg, &msg_type, s_char);
  memcpy(msg + s_char, &obj_id, s_short);

  SendMsg(suc_sock, msg, msg_size);

  close(suc_sock);
}


// This is called by a peer or root.
// Store request (from peer/root):
// STORE + obj_ID

void sendStoreReq2Suc (unsigned short obj_id, PeerNode *my_node) {

  char msg_type = STORE;
  sendReq2Suc (msg_type, obj_id, my_node);
  
  printf("STORE request passed on.\n");
}


// This is called by a peer or root.
// Recursive request (from peer/root):
// RECURSIVE + obj_ID

void sendRecurReq2Suc (unsigned short obj_id, PeerNode *my_node) {

  char msg_type = RECURSIVE;
  sendReq2Suc (msg_type, obj_id, my_node);
  
  printf("RECURSIVE request passed on.\n");
}


// when successfully store an object file on local node,
// send an ACK to the given IP and Port.

void sendAck (unsigned int ip, unsigned short port) {

  char ip_str[INET_ADDRSTRLEN];  
  char port_str[PORT_SIZE];      
  ipBytes2String(&ip, ip_str);  
  portNum2String(port, port_str);  

  int sock = initSendSock(ip_str, port_str);  // socket to reply client.

  char msg_type = ACK;
  
  char ack_msg[1];
  memcpy(ack_msg, &msg_type, s_char);

  SendMsg(sock, ack_msg, 1);
  close(sock);

  printf("ACK sent.\n");
}


// construct a join request message, which includes the message type,
// the peer's IP address, and its port.
// Join Request:
// JOIN + new_IP + new_Port

void buildJoinReq (char *res) {

  char msg_type = JOIN;
 
  memcpy(res, &msg_type, s_char);
  memcpy(res + s_char, &my_ip, s_int);
  memcpy(res + s_char + s_int, &my_port, s_short);
  
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


// Retrieve the IP and port of the peer who has sent the JOIN request.

void parseJoin (char *msg, unsigned int *ip, unsigned short *port) {
  
  memcpy(ip, msg + s_char, s_int);
  memcpy(port, msg + s_char + s_int, s_short);

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
    char ip1[INET_ADDRSTRLEN];        // ip addr in string.
    ipBytes2String(&node.pre_IP, ip1);
    printf("IP   = %s\n", ip1);
  }
  else {
    printf("IP   = %d\n", node.pre_IP);
  }
  printf("Port = %d\n", node.pre_Port);

  // Print current node.

  printf("Current Node:\t%d\n", node.cur_ID);
  if (node.cur_IP != 0)
  {
    char ip2[INET_ADDRSTRLEN];        // ip addr in string.
    ipBytes2String(&node.cur_IP, ip2);
    printf("IP   = %s\n", ip2);
  }
  else {
    printf("IP   = %d\n", node.cur_IP);
  }
  printf("Port = %d\n", node.cur_Port);

  // Print successor.

  printf("Successor:\t%d\n", node.suc_ID);
  if (node.suc_IP != 0)
  {
    char ip3[INET_ADDRSTRLEN];         // ip addr in string.
    ipBytes2String(&node.suc_IP, ip3);
    printf("IP   = %s\n", ip3);
  }
  else {
    printf("IP   = %d\n", node.suc_IP);
  }
  printf("Port = %d\n", node.suc_Port);
  
}
