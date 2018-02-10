// Project 3: CHORD Distributed Hash Table.
// Written by W. Li 
// Date completed: 12-08-2017

// This file is a DHT Client program.

// Root host name: 
// Root Port: 15074

// Client host names:
// Client ports: 15070 ~ 15074

#include "dht_client.h"

static char *c_port;  // the client's own port
static char *c_host;  // the client's own hostname
static char *r_port;  // the root's port
static char *r_host;  // the root's hostname

static unsigned int   my_ip   = 0;  // client's ip addr in 4 bytes
static unsigned short my_port = 0;  // client's port in 2 bytes

//static int recv_sock = 0;           // socket for receving message at given local port

static int s_char  = sizeof(char);
static int s_int   = sizeof(unsigned int);
static int s_short = sizeof(unsigned short);


int main(int argc, char *argv[]) {

  printf("This is a client !\n\n");

  // Parse the command line.
  parseCommand(argc, argv);

  // Initialize my_port number.
  my_port = (unsigned short) strtoul(c_port, NULL, 0);

  // Initialize a receiving socket, and client_ip.
  int recv_sock = initRecvSock(c_host, c_port, &my_ip);

  while(1) {

    // Show a menu, and check user's input.

    char key = clientUserInterface();  

    // Exit when user inputs 'e'.

    if (key == EXIT) 
    { 
      break; 
    }
    
    // Enter keys other than the menu.

    if ((key != STORE) && (key != ITERATIVE) && (key != RECURSIVE)) 
    { 
      continue; 
    }

    // Prompt to enter object name.

    size_t buff_size = OBJ_NAME_SIZE + 1;
    char *input = (char *) malloc(buff_size);

    printf("Enter the object name:\n"); 
    int bytes_read = getline(&input, &buff_size, stdin);
    if (bytes_read == -1)
    {
      systemErrorMessage("getline() error!");
    }

    char *obj_name = strtok(input, "\n");  // delete the last '\n' in input string.
    printf("The object name is: %s\n", obj_name);

    // compute obj_ID by SHA1

    unsigned short obj_ID = computeSha1(obj_name);
    printf("obj_ID: %d\n", obj_ID);

    // Iteraction with keys.

    if (key == STORE)      // store an object.
    {
      sendStoreReq2Root (obj_ID);
      handleStoreReply (recv_sock, obj_name);
    }

    if (key == ITERATIVE)  // retrieve an object iteratively.
    {
      sendIteraRetrv2Root (obj_ID);
      handleIteraReply (recv_sock, obj_ID, obj_name);
    }

    if (key == RECURSIVE)  // retrieve an object recursively.
    {
      sendRecurRetrv2Root (obj_ID);
      handleRecurReply (recv_sock, obj_ID, obj_name);
    }  
  }

  close(recv_sock);

	return 0;
}

//************************     Helper Functions      ***************************


// GIVEN : the number of arguments on command line (argc), 
//         and the commands (argv).
// EFFECT: Designates the client's own port and hostname,
//         and the root's port and hostname.

void parseCommand(int argc, char *argv[]) {

  // Test for correct number of arguments in command line.

  if (argc != 9) {
    userErrorMessage("Command Error", 
      "Input format is : ./dht_client <-p client_port> <-h client_hostname>\
       <-r root_port> <-R root_hostname>");
  }
  
  // Retrieve information.

  for (int i = 1; i < argc; i++) {

    if (strcmp(argv[i], "-p") == 0) {
      c_port = argv[i+1];
      continue;
    }

    if (strcmp(argv[i], "-h") == 0) {
      c_host = argv[i+1];
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

  printf("Client port    : %s\n", c_port);
  printf("Client hostname: %s\n", c_host);
  printf("Root port      : %s\n", r_port);
  printf("Root hostname  : %s\n\n", r_host);
}


// user interface to control the peer

char clientUserInterface() {
  size_t size = 2;

  char *key;  // input from command line.
  key = (char *) malloc(size);

  printf("Menu\n");
  printf("Press 's': Store object\nPress 'i': Retrieve object iteratively\nPress 'r': Retrieve object recursively\nPress 'e': exit\n");

  int bytes_read = getline(&key, &size, stdin);
  if (bytes_read == -1)
  {
    systemErrorMessage("getline() error!");
  }
  return key[0];
}


// GIVEN  : an object name.
// RETURNS: the objID (16-bit integer) for the given object based on SHA1.

unsigned short computeSha1(char *obj_name) {

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


// Send a request to store an object to the root.
// Store request:
// STORE + obj_ID + client_IP + client_Port

void sendStoreReq2Root (unsigned short obj_id) {

  char msg_type = STORE;
  sendReq2Root (msg_type, obj_id);

  printf("%s\n", "STORE sent to root.");
}


// Send Recursive Retrieve request to the root.
// Recursive Retrieve:
// RECURSIVE + obj_ID + client_IP + client_Port

void sendRecurRetrv2Root (unsigned short obj_id) {

  char msg_type = RECURSIVE;
  sendReq2Root (msg_type, obj_id);

  printf("%s\n", "RECURSIVE Retrieve sent to root.");
}


// Send Iterative Retrieve request to the root.
// Iterative Retrieve:
// ITERATIVE + obj_ID + client_IP + client_Port

void sendIteraRetrv2Root (unsigned short obj_id) {
  
  char msg_type = ITERATIVE;
  sendReq2Root (msg_type, obj_id);

  printf("%s\n", "ITERATIVE Retrieve sent to root.");
}


// Send Iterative Retrieve request to next node.
// Iterative Retrieve:
// ITERATIVE + obj_ID + client_IP + client_Port

void sendIteraRetrv2Node (unsigned short obj_id, char *node_ip, char *node_port) {
  
  char msg_type = ITERATIVE;
  sendReq2Node (msg_type, obj_id, node_ip, node_port);

  printf("%s\n", "ITERATIVE Retrieve sent.");
}


// Send the given request to root.
// req_type + obj_ID + client_IP + client_Port

void sendReq2Root (char req_type, unsigned short obj_id) {

  sendReq2Node (req_type, obj_id, r_host, r_port); 
}


// Send the given request to any node (ip + port).
// req_type + obj_ID + client_IP + client_Port

void sendReq2Node (char req_type, unsigned short obj_id, char *ip, char *port) {

  // intitial a buffer for store request
  const int req_size = s_char + s_int + 2 * s_short;
  char request[req_size];

  memcpy(request, &req_type, s_char);
  memcpy(request + s_char, &obj_id, s_short);
  memcpy(request + s_char + s_short, &my_ip, s_int);
  memcpy(request + s_char + s_short + s_int , &my_port, s_short);

  // Initialize a sending socket to root and send request.

  int sock = initSendSock(ip, port);
  SendMsg(sock, request, req_size);
  close(sock);
}

// send object file to target node 
// Note: the file size fit into one TCP packet.
// DATA Message:
// DATA + client_IP + client_Port + filename + filesize + filedata 

void sendFileToTarget (int sock, char *filename, int filesize, char *filedata) {
  
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
  printf("%s\n", "File sent!");

}


// RETRIEVE Message:
// RETRIEVE + obj_ID + filename + client_IP + client_Port

void sendRetrv2Target (int sock, unsigned short obj_id, char *filename) {

  char msg_type = RETRIEVE;
  int  msg_size = s_char + s_short + OBJ_NAME_SIZE + s_int + s_short;
  char msg[msg_size];

  memset(msg, 0, msg_size);
  
  // construct data packet.

  memcpy(msg, &msg_type, s_char);
  memcpy(msg + s_char, &obj_id, s_short);
  memcpy(msg + s_char + s_short, filename, strlen(filename));
  memcpy(msg + s_char + s_short + OBJ_NAME_SIZE, &my_ip, s_int);
  memcpy(msg + s_char + s_short + OBJ_NAME_SIZE + s_int, &my_port, s_short);

  SendMsg(sock, msg, msg_size);
  printf("%s\n", "RETRIEVE sent to target!");
}


// handle STORE Reply sent from root.

void handleStoreReply(int recv_sock, char *obj_name) {

  // receive store reply from root.

  char recv_buffer[RECV_BUFSIZE];    
  memset(recv_buffer, 0, RECV_BUFSIZE);
  
  int cur_sock = acceptConnect(recv_sock);
  RecvMsg(cur_sock, recv_buffer, RECV_BUFSIZE);
  close(cur_sock);

  char msg_type;
  unsigned short target_ID   = 0;
  unsigned int   target_IP   = 0;
  unsigned short target_Port = 0;

  // parse received message.

  memcpy(&msg_type, recv_buffer, s_char);
  if (msg_type != STORE_REPLY)
  {
    userErrorMessage("Store Error", "can't receive reply from root");
  }

  memcpy(&target_ID, recv_buffer + s_char, s_short);
  memcpy(&target_IP, recv_buffer + s_char + s_short, s_int);
  memcpy(&target_Port, recv_buffer + s_char + s_short + s_int, s_short);

  // convert target IP and Port to strings.

  char target_ip_str[INET_ADDRSTRLEN];
  char target_port_str[PORT_SIZE];
  ipBytes2String (&target_IP, target_ip_str);
  portNum2String (target_Port, target_port_str);

  printf("Preparing to store object on Node %d: IP -- %s, Port -- %s\n", 
         target_ID, target_ip_str, target_port_str);


  // transimit object file to target node.

  char obj_buffer[PACKET_SIZE];  // buffer to store file data.
  int  obj_size = readObjData (obj_buffer, obj_name);
  //printf("obj_size is %d bytes.\n", obj_size);

  int target_sock = initSendSock(target_ip_str, target_port_str);
  sendFileToTarget (target_sock, obj_name, obj_size, obj_buffer);
  close(target_sock);

  // wait for ACK from target node.

  memset(recv_buffer, 0, RECV_BUFSIZE);

  int tar_sock = acceptConnect(recv_sock);
  RecvMsg(tar_sock, recv_buffer, RECV_BUFSIZE);
  close(tar_sock);

  char ack_type = 0;
  memcpy(&ack_type, recv_buffer, s_char);

  if (ack_type == ACK)
  {
    printf("%s\n", "File stored success.");
  }
  else {
    printf("%s\n", "File stored failure.");
  }
}



// handle RECURSIVE Reply sent from root.
// Recursive Rply:
// RECURSIVE + target_ID + target_IP + target_Port

void handleRecurReply (int recv_sock, unsigned short obj_id, char *obj_name) {

  // receive reply from root.

  char recv_buffer[RECV_BUFSIZE];    
  memset(recv_buffer, 0, RECV_BUFSIZE);
  
  int cur_sock = acceptConnect(recv_sock);
  RecvMsg(cur_sock, recv_buffer, RECV_BUFSIZE);
  close(cur_sock);

  char msg_type;
  unsigned short target_ID   = 0;
  unsigned int   target_IP   = 0;
  unsigned short target_Port = 0;

  // parse received message.

  memcpy(&msg_type, recv_buffer, s_char);
  if (msg_type != RECUR_REPLY)
  {
    userErrorMessage("Recursive Retrieve Error", "can't receive reply from root");
  }

  memcpy(&target_ID, recv_buffer + s_char, s_short);
  memcpy(&target_IP, recv_buffer + s_char + s_short, s_int);
  memcpy(&target_Port, recv_buffer + s_char + s_short + s_int, s_short);

  // convert target IP and Port to strings.

  char target_ip_str[INET_ADDRSTRLEN];
  char target_port_str[PORT_SIZE];
  ipBytes2String (&target_IP, target_ip_str);
  portNum2String (target_Port, target_port_str);

  printf("Preparing to retrieve object from Node %d: IP -- %s, Port -- %s\n", 
         target_ID, target_ip_str, target_port_str);

  // send RETRIEVE to target node

  int target_sock = initSendSock(target_ip_str, target_port_str);
  sendRetrv2Target (target_sock, obj_id, obj_name);
  close(target_sock);

  // wait to receive file data

  memset(recv_buffer, 0, RECV_BUFSIZE);  // clear out recv buffer. 

  int tar_sock = acceptConnect(recv_sock);
  RecvMsg(tar_sock, recv_buffer, RECV_BUFSIZE);
  close(tar_sock);

  memcpy(&msg_type, recv_buffer, s_char);

  if (msg_type == DATA)
  {
    handleData (recv_buffer);
    printf("%s\n", "File retrieve success.");
  }
  else {
    printf("%s\n", "File retrieve failure.");
  }

}

// handle RECURSIVE Reply or Next sent from root / peer.
// Recursive Rply:
// RECURSIVE + target_ID + target_IP + target_Port
// Recursive Next:
// ITERA_NEXT + next_ID + next_IP + next_Port

void handleIteraReply (int recv_sock, unsigned short obj_id, char *obj_name) {
  
  char recv_buffer[RECV_BUFSIZE];    
  char msg_type = 0;   // check reply type
  
  while (msg_type != ITERA_REPLY)  // continue lookup the target node.
  {
    memset(recv_buffer, 0, RECV_BUFSIZE);
    
    // receive reply from root or peer.
    int cur_sock = acceptConnect(recv_sock);
    RecvMsg(cur_sock, recv_buffer, RECV_BUFSIZE);
    close(cur_sock);

    memcpy(&msg_type, recv_buffer, s_char);

    if (msg_type == ITERA_NEXT)
    {
      unsigned short next_ID   = 0;
      unsigned int   next_IP   = 0;
      unsigned short next_Port = 0;

      memcpy(&next_ID, recv_buffer + s_char, s_short);
      memcpy(&next_IP, recv_buffer + s_char + s_short, s_int);
      memcpy(&next_Port, recv_buffer + s_char + s_short + s_int, s_short);

      // convert next node IP and Port to strings.

      char next_ip_str[INET_ADDRSTRLEN];
      char next_port_str[PORT_SIZE];
      ipBytes2String (&next_IP, next_ip_str);
      portNum2String (next_Port, next_port_str);

      printf("Continuing to contact Node %d: IP -- %s, Port -- %s\n", 
             next_ID, next_ip_str, next_port_str);

      // send ITERATIVE request to next node
      sendIteraRetrv2Node (obj_id, next_ip_str, next_port_str);
    }    
  }


  if (msg_type == ITERA_REPLY)  // locate the target node
  {
    unsigned short target_ID   = 0;
    unsigned int   target_IP   = 0;
    unsigned short target_Port = 0;

    memcpy(&target_ID, recv_buffer + s_char, s_short);
    memcpy(&target_IP, recv_buffer + s_char + s_short, s_int);
    memcpy(&target_Port, recv_buffer + s_char + s_short + s_int, s_short);

    // convert target IP and Port to strings.

    char target_ip_str[INET_ADDRSTRLEN];
    char target_port_str[PORT_SIZE];
    ipBytes2String (&target_IP, target_ip_str);
    portNum2String (target_Port, target_port_str);

    printf("Preparing to retrieve object from Node %d: IP -- %s, Port -- %s\n", 
           target_ID, target_ip_str, target_port_str);

    // send RETRIEVE to target node

    int target_sock = initSendSock(target_ip_str, target_port_str);
    sendRetrv2Target (target_sock, obj_id, obj_name);
    close(target_sock);

    // wait to receive file data

    memset(recv_buffer, 0, RECV_BUFSIZE);  // clear out recv buffer. 

    int tar_sock = acceptConnect(recv_sock);
    RecvMsg(tar_sock, recv_buffer, RECV_BUFSIZE);
    close(tar_sock);

    memcpy(&msg_type, recv_buffer, s_char);

    if (msg_type == DATA)
    {
      handleData (recv_buffer);
      printf("%s\n", "File retrieve success.");
    }
    else {
      printf("%s\n", "File retrieve failure.");
    }
  }
}


// Process data message sent from peer
// Data Message:
// DATA + peer_IP + peer_Port + filename + filesize + filedata 

void handleData (char *msg) {

  unsigned int   peer_IP   = 0;
  unsigned short peer_Port = 0;
  char filename[OBJ_NAME_SIZE];
  int  filesize = 0;
  char filedata[PACKET_SIZE];

  // retrieve info.
  memcpy(&peer_IP, msg + s_char, s_int);
  memcpy(&peer_Port, msg + s_char + s_int, s_short);

  memcpy(filename, msg + s_char + s_int + s_short, OBJ_NAME_SIZE);
  memcpy(&filesize, msg + s_char + s_int + s_short + OBJ_NAME_SIZE, s_int);
  memcpy(filedata, msg + s_char + s_int + s_short + OBJ_NAME_SIZE + s_int, filesize);

  // store object on node.
  writeObjData (filename, filesize, filedata);

  // send ACK to peer
  sendAck(peer_IP, peer_Port);

}


// when successfully store an object file on local node,
// send an ACK to the given client IP and Port.

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
