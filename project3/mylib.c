#include "mylib.h"

// Throw an error message and terminate the program.
// Output the error message caused by user

void userErrorMessage(const char *msg, const char *err_info) {
  fputs(msg, stderr);
  fputs("! ", stderr);
  fputs(err_info, stderr);
  fputc('\n', stderr);
  exit(1);
}


// Output the error message of the previous function

void systemErrorMessage(const char *msg) {
  perror(msg);
  exit(1);
}


// Output SHA1 computing erroe and terminate.

void sha1ErrorMsg(const char *msg, int err) {
  fprintf(stderr, "%s %d.\n", msg, err);
  exit(1);
}


// GIVEN: two strings
// RETURNS: a new string which combines the two given strings.

char * combStr (char *s1, char *s2) {
  char *res = malloc(strlen(s1) + strlen(s2) + 1);
  if (res == NULL)
  {
    exit(1);
  }
  strcpy(res, s1);
  strcat(res, s2);
  return res;
} 


int initRecvSock(char *host, char *port, unsigned int *ip4) {

  // Construct the local address

  int status;
  struct addrinfo hints;
  struct addrinfo *info;

  memset(&hints, 0, sizeof(hints)); // zero out the structure
  hints.ai_family = AF_INET;        // IPv4
  //hints.ai_family = AF_UNSPEC;    // any address family, IPv4 or IPv6
  hints.ai_socktype = SOCK_STREAM;  // TCP stream sockets
  hints.ai_flags = AI_PASSIVE;      // Use IP of the local host

  status = getaddrinfo(host, port, &hints, &info);
  if (status != 0) {
    userErrorMessage("local getaddrinfo() failed", gai_strerror(status));
  } 
  //else { 
  //  printf("%s\n", "local getaddrinfo() success"); 
  //}

  // get my IP addr in Network Byte Order.

  if (info->ai_family == AF_INET) // IPv4
  {
    struct sockaddr_in *addr = (struct sockaddr_in *) (info->ai_addr);
    memcpy(ip4, & ((addr->sin_addr).s_addr), sizeof(unsigned int));
  }

  // Create a TCP socket

  int sock = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
  if (sock < 0) {
    systemErrorMessage("Socket creation failed!");
  } 
  //else { 
  //  printf("%s\n", "Socket creation success"); 
  //}

  // Bind to local port

  if (bind(sock, info->ai_addr, info->ai_addrlen) < 0) {
    systemErrorMessage("bind() failed");
  } 
  //else { 
  //	printf("%s\n", "bind() success"); 
  //}

  // Listen for incomming connection.

  if (listen(sock, MAX_REQUS) < 0)
  {
    systemErrorMessage("listen() failed!");
  }

  // Free address list allocated by getaddrinfo()

  freeaddrinfo(info);  

  return sock;
}


int initSendSock(char* server, char *port) {

  // Construct the server address

  int status;
  struct addrinfo hints;
  struct addrinfo *res;

  memset(&hints, 0, sizeof(hints)); // zero out the structure
  hints.ai_family = AF_INET;//AF_UNSPEC;      // any address family, IPv4 or IPv6
  hints.ai_socktype = SOCK_STREAM;  // TCP stream sockets

  status = getaddrinfo(server, port, &hints, &res);
  if (status != 0) {
    userErrorMessage("server getaddrinfo() failed", gai_strerror(status));
  } 
  //else { 
  //  printf("%s\n", "server getaddrinfo() success"); 
  //}

  // Create a TCP socket
  int sock = socket(res->ai_family,
                    res->ai_socktype, res->ai_protocol);
  if (sock < 0) {
    systemErrorMessage("Socket creation failed!");
  } 
  //else { 
  //  printf("%s\n", "Socket creation success"); 
  //}

  // connect the server.

  int conn_status = connect(sock, res->ai_addr, res->ai_addrlen); 
  if (conn_status < 0) {
    systemErrorMessage("Connection failed!");
  } 
  //else { 
  //  printf("%s\n", "Connection success"); 
  //}

  return sock;
}


// try to connect a peer node
// return value < 0 => connect failure
// return value > 0 => connect success

int tryConnect(char* server, char *port) {
  
  // Construct the server address
  int status;
  struct addrinfo hints;
  struct addrinfo *res;

  memset(&hints, 0, sizeof(hints)); // zero out the structure
  hints.ai_family = AF_INET;//AF_UNSPEC;      // any address family, IPv4 or IPv6
  hints.ai_socktype = SOCK_STREAM;  // TCP stream sockets

  status = getaddrinfo(server, port, &hints, &res);
  if (status != 0) {
    userErrorMessage("server getaddrinfo() failed", gai_strerror(status));
  } 
  //else { 
  //  printf("%s\n", "server getaddrinfo() success"); 
  //}

  // Create a TCP socket
  int sock = socket(res->ai_family,
                    res->ai_socktype, res->ai_protocol);
  if (sock < 0) {
    systemErrorMessage("Socket creation failed!");
  } 
  //else { 
  //  printf("%s\n", "Socket creation success"); 
  //}

  // connect the server.

  int conn_status = connect(sock, res->ai_addr, res->ai_addrlen); 
  //if (conn_status < 0) {
  //  systemErrorMessage("Connection failed!");
  //} 
  //else { 
  //  printf("%s\n", "Connection success"); 
  //}

  close(sock);

  return conn_status;
}


// Accept an incoming connection.
// returns a sock used for the current connection.

int acceptConnect(int sock) {

  struct sockaddr_storage cur_addr;  // to store incoming address
  socklen_t addr_size;               // length of the incoming address
  addr_size = sizeof(cur_addr);
    
  int cur_sock = accept(sock, (struct sockaddr *) &cur_addr, &addr_size);
  if (cur_sock < 0)
  {
    systemErrorMessage("accept() failed");
  } 
  else {
    //printf("%s\n", "Accepted a new connection"); 
  }

  return cur_sock;
}


// convert IP address from bytes (ip4) to string (ip4_str).

void ipBytes2String(unsigned int *ip4, char *ip4_str) {

  if (inet_ntop(AF_INET, ip4, ip4_str, INET_ADDRSTRLEN) != NULL)
  {
    //printf("IP = %s\n", ip4_str);
  }
  else {
    printf("unable to get host address");
  }
}


// receive message from the client and parse it.

void RecvMsg(int sock, char *recv_buffer, int buff_size) {

  int numBytesRcvd = recv(sock, recv_buffer, buff_size, 0);
  if (numBytesRcvd < 0)
  {
    systemErrorMessage("recv() failed");
  }
}


// receive message from the client and parse it.

void SendMsg(int sock, char *msg, int msg_len) {

  int numBytesSent = send(sock, msg, msg_len, 0);
  //printf("numBytesSent: %d\n", numBytesSent);
  if (numBytesSent < 0)
  {
    systemErrorMessage("send() failed");
  } 
  else if (numBytesSent != msg_len)
  {
    userErrorMessage("send() failed", "sent unexpected number of bytes");
  }
}
