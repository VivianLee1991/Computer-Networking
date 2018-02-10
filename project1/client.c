// This file is a simple TCP client.
// Written by Wenwen Li (NEU ID: 001830563)
// Date completed: 09-28-2017

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

void UserErrorMessage(const char *msg, const char *err_info);
void SystemErrorMessage(const char *msg);
char* BuildMsg(const char *type, char *msg);
int SendMsg(int sock, char *msg);
int RecvMsg(int sock, char *buffer, int buf_size);

int main(int argc, char const *argv[]) {
  // Parse the command line:

  // Test for correct number of arguments
  // If incorrect, throw error message and exit.
  if ((argc != 3) && (argc != 5)) {
    UserErrorMessage("Command Error",
                     "Input format is : ./client <-p port> [hostname] [NEU ID]");
  }

  // -p port is the TCP port of the server
  // This parameter is optional.
  // If it is not supplied on the command line, set port = 27993
  int serv_port = 27993;
  if (argc == 5) {
    serv_port = atoi(argv[2]);
  }
  // Test:
  //printf("Port is %d\n", serv_port);

  // server is the DNS name of the server
  const char* SERVER_5700 = "cs5700f17.ccs.neu.edu";
  char *server = argv[argc-2];
  if ( strcmp(server, SERVER_5700) != 0) {
    UserErrorMessage("Command Error",
                     "Server name is cs5700f17.ccs.neu.edu");
  }

  // NEU ID for li.wenw is 001830563
  char *my_id = argv[argc-1];
  // Test:
  //printf("%s\n", my_id);

  // DNS lookup
  struct hostent *host_name = NULL;
  host_name = gethostbyname(server);
  if (host_name == NULL) {
    herror("Can't get IP address!"); //Error message for gethostbyname()
    exit(1);
  }
  // Test:
  //printf("%s\n", host_name->h_name);

  // Create a TCP socket
  int tcpSock = socket(PF_INET, SOCK_STREAM, 0);
  if (tcpSock < 0) {
    SystemErrorMessage("Socket creation failed!");
  }

  // Construct the server address structure
  struct sockaddr_in servAddr;
  memset(&servAddr, 0, sizeof(servAddr));
  servAddr.sin_family = AF_INET;
  // Port (in Network Byte Order)
  servAddr.sin_port = htons(serv_port);
  // 4-byte IP address (in Network Byte Order)
  servAddr.sin_addr = *((struct in_addr*)host_name->h_addr_list[0]);
  //servAddr.sin_addr.s_addr = *(host_name->h_addr_list[0]);

  // Establish a connection to the server
  int conn_status = connect(tcpSock, (struct sockaddr *) &servAddr,
                              sizeof(servAddr));
  //printf("%s\n", "...");
  if (conn_status < 0) {
    SystemErrorMessage("Connection failed!");
  }
  //else { printf("%s\n", "Connection success"); }

  // define the type of message
  const char *HELLO = "cs5700fall2017 HELLO ";
  const char *SOLUTION = "cs5700fall2017 ";

  // Send a HELLO message to the server
  // HELLO format:
  // cs5700fall2017 HELLO [NEU ID]\n
  char *hello_msg = BuildMsg(HELLO, my_id);
  SendMsg(tcpSock, hello_msg);
  // printf("hello_msg: %s", hello_msg);

  // Communicate with the server
  // define the maximum buffer size
  const int BUFSIZE = 256;
  //int message_count = 0;

  while (1) {
    // Receive a message from the server
    char recv_buffer[BUFSIZE]; // Store the reveived message
    RecvMsg(tcpSock, recv_buffer, BUFSIZE);

    // Parse the received message
    int msg_num = 0;
    char *str[10]; // store the substrings of the pasrsed message

    char *recv_msg;
    recv_msg = strtok(recv_buffer, " \n");
    str[msg_num] = recv_msg;
    while (recv_msg != NULL) {
      msg_num++;
      recv_msg = strtok(NULL, " \n");
      str[msg_num] = recv_msg;
      //printf("%s\n", str[msg_num]);
    }

    // Receive a BYE message from the server
    // BYE format:
    // cs5700fall2017 [a 64 byte secret flag] BYE\n
    if (strcmp(str[2], "BYE") == 0) {
      // print out exactly one line of output: [secret flag]
      printf("The secret flag is: %s\n", str[1]);
      break;
    }

    // Receive the STATUS message from the server
    // STATUS format:
    // cs5700fall2017 STATUS [a number] [a math operator] [another number]\n
    // [a math operator] could be "+", "-", "*", "/"
    // [number] = 1 ~ 1000
    int math_result = 0;
    if (strcmp(str[1], "STATUS") == 0) {
      int num1 = atoi(str[2]);
      int num2 = atoi(str[4]);
      switch (*str[3]) {
        // Solve the Math expression
        // (do not send floating point numbers to the server)
        case '+': math_result = num1 + num2; break;
        case '-': math_result = num1 - num2; break;
        case '*': math_result = num1 * num2; break;
        case '/': math_result = (int)(num1 / num2 + 0.5); break;// Return the nearest integer
        default: { UserErrorMessage("Calculation failed",
                                    "Illegal operator received");
                   break; }
      }
    }

    // Return the answer to the server in a SOLUTION message
    // Build the SOLUTION message string
    // SOLUTION format:
    // cs5700fall2017 [the solution]\n
    char math_result_str[100];
    //printf("math_result_str: %s\n", math_result_str);
    sprintf(math_result_str, "%d", math_result); // Turn int into string
    char *solution_msg = BuildMsg(SOLUTION, math_result_str);
    SendMsg(tcpSock, solution_msg);

  }

  // close connection
  //close(tcpSock);
  shutdown(tcpSock, 2);

  return 0;
}

/************************     Helper Functions      ***************************/

// Throw an error message and terminate the program.
// Output the error message caused by user
void UserErrorMessage(const char *msg, const char *err_info) {
  fputs(msg, stderr);
  fputs("! ", stderr);
  fputs(err_info, stderr);
  fputc('\n', stderr);
  exit(1);
}

// Output the error message of the previous function
void SystemErrorMessage(const char *msg) {
  perror(msg);
  exit(1);
}

// Build a message string of the given format
char* BuildMsg(const char *type, char *msg){
  const char *ending = "\n";
  char *full_msg = malloc( strlen(type)+strlen(msg)+strlen(ending)+1 );

  strcpy(full_msg, type);
  strcat(full_msg, msg);
  strcat(full_msg, ending);

  return full_msg;
}

// Send a message to the server
int SendMsg(int sock, char *msg){
  int msg_len = strlen(msg);

  int sentBytes = send(sock, msg, msg_len, 0);
  if (sentBytes < 0) {
    SystemErrorMessage("Send failed");
  }
  if (sentBytes!= msg_len) {
    UserErrorMessage("Send failed", "Unexpected sending bytes.");
  }
  //else {
  //  printf("Successfully send: %s", msg);
  //};
  return 0;
}

int RecvMsg(int sock, char *buffer, int buf_size){
  // Receive a message from the server
  int recv_bytes = recv(sock, buffer, buf_size, 0);
  //printf("No.: %d    bytes: %d\n", message_count++, recv_bytes);

  if (recv_bytes == -1) {
    SystemErrorMessage("Receive failed");
  }

  if (recv_bytes == 0) {
    // server terminates connection
    UserErrorMessage("Connection closed",
                     "Incorrect solution sent to the sever.");
  }
  //printf("Received message: %s", buffer);
  return 0;
}
