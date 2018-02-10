// Reliable File transfer.
// Written by W. Li
// Date completed: 10-21-2017

// This file is a UDP Receiver program.
// Sender name: toque.ccs.neu.edu
// Receiver name: beret.ccs.neu.edu
// Port: 15070

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>    // for getpid().

// Definitions of three types of packets.
const char INIT = '1';
const char ACK = '2';
const char DATA = '3';

const int max_filename_size = 30;  // Maxium size of file name in bytes.

void UserErrorMessage(const char *msg, const char *err_info);
void SystemErrorMessage(const char *msg);
char* BuildAckPacket(char* res, int seq_num);
char* ReceiveAPacket(char *buffer, int sock, int size);
void SendACK(int sock, const char *msg, int len, struct addrinfo *sender);

int main(int argc, char const *argv[]) {
/*----------------------------------------------------------------------------*/
  /* Step 1: Test for correct number of arguments in command line. */
  // If incorrect, throw error message and exit.
  if (argc != 7) {
    UserErrorMessage("Command Error",
                     "Input format is : ./receiver <-m mode> <-p port> <-h hostname>");
  }

/*----------------------------------------------------------------------------*/
  /* Step 2: Parse the command line. */
  int mode = 0;
  const char *port;
  const char *sender;
  for (int i = 1; i < argc; i++) {
    // -m mode: mode = 1 use the Stop-and-Wait;
    //          mode > 1 use the Go-Back-N, and N = mode.
    if (strcmp(argv[i], "-m") == 0) {
      mode = atoi(argv[i+1]);
      continue;
    }
    // -p port is the port of the sender
    if (strcmp(argv[i], "-p") == 0) {
      port = argv[i+1];
      continue;
    }
    // -h hostname is the DNS name of the sender
    if (strcmp(argv[i], "-h") == 0) {
      sender = argv[i+1];
      continue;
    }
  }
  // Test for Step 1 and Step 2.
  printf("Mode is %d\n", mode);
  printf("Port is %s\n", port);
  printf("Sender is %s\n", sender);

/*----------------------------------------------------------------------------*/
  /* Step 3.1: Construct the receiver's address. */
  int status;
  struct addrinfo hints;
  struct addrinfo *recv_info;      // point to the result.

  memset(&hints, 0, sizeof(hints)); // zero out the structure
  hints.ai_family = AF_UNSPEC;      // any address family, IPv4 or IPv6
  hints.ai_socktype = SOCK_DGRAM;   // UDP datagram sockets
  hints.ai_protocol = IPPROTO_UDP;  // only UDP protocol
  hints.ai_flags = AI_PASSIVE;      // Use IP of the local host

  status = getaddrinfo(NULL, port, &hints, &recv_info);
  if (status != 0) {
    UserErrorMessage("local getaddrinfo() failed", gai_strerror(status));
  }
  else { printf("%s\n", "local getaddrinfo() success"); }

  /* Step 3.2: Construct the sender's address. */
  int send_status;
  struct addrinfo addr_criteria;
  struct addrinfo *send_info; // point to the result.

  memset(&addr_criteria, 0, sizeof(addr_criteria)); // zero out the structure
  addr_criteria.ai_family = AF_UNSPEC;      // any address family, IPv4 or IPv6
  addr_criteria.ai_socktype = SOCK_DGRAM;   // UDP datagram sockets
  addr_criteria.ai_protocol = IPPROTO_UDP;  // only UDP protocol

  send_status = getaddrinfo(sender, port, &addr_criteria, &send_info);
  if (send_status != 0) {
    UserErrorMessage("sender getaddrinfo() failed", gai_strerror(send_status));
  }
  else { printf("%s\n", "sender getaddrinfo() success"); }

/*----------------------------------------------------------------------------*/
  /* Step 4.1: Create a UDP socket to receive pacets*/
  int sock_recv = socket(recv_info->ai_family,
                         recv_info->ai_socktype, recv_info->ai_protocol);
  if (sock_recv < 0) {
    SystemErrorMessage("Socket creation failed!");
  }
  else { printf("%s\n", "Socket creation success"); }

  /* Step 4.2: Create a UDP socket to send packets. */
  int sock_send = socket(recv_info->ai_family,
                    recv_info->ai_socktype, recv_info->ai_protocol);
  if (sock_recv < 0) {
    SystemErrorMessage("Socket creation failed!");
  }
  else { printf("%s\n", "Socket creation success"); }

/*----------------------------------------------------------------------------*/
  /* Step 5: Bind the socket for receving to the local IP and port. */
  if (bind(sock_recv, recv_info->ai_addr, recv_info->ai_addrlen) < 0) {
    SystemErrorMessage("bind() failed");
  }
  else { printf("%s\n", "bind() success"); }

  freeaddrinfo(recv_info);  // Free address list allocated by getaddrinfo()

/*----------------------------------------------------------------------------*/
  /* Step 6: Go-Back-N protocol. */
  int recv_mode = 0;                 // protocol mode received from the sender
  char filename[max_filename_size];  // name of the file received.
  long file_size = 0;               // size of the file in bytes

  int recv_seq_num = 0;         // sequence number of current data packet.
  int expected_seq_num = 0;     // expected sequence number of next packet.
  int max_seq_num = 0;              // maxium sequence number that shoud be received.

  const int data_buff_size = 16;    // size of file data contained in each DATA packet,
                                    // should match with the sender's.
  char type = '0';   // packet type, '0' indicates no msg received.
  //size of packets.
  const int init_packet_size = sizeof(char) + max_filename_size + sizeof(long) + sizeof(int);
  const int data_packet_size = sizeof(char) + sizeof(int) + data_buff_size;
  const int ack_packet_size = sizeof(char) + sizeof(int);

  // packet definitions.
  char init_packet[init_packet_size];       // buffer to store an INIT packet.
  memset(init_packet, 0, init_packet_size); // Zero out the buffer.
  char ack_packet[ack_packet_size];         // buffer to store an ACK packet.
  memset(ack_packet, 0, ack_packet_size);
  char data_packet[data_packet_size];       // buffer to store an DATA packet.
  memset(data_packet, 0, data_packet_size);

  // receive an INIT message from the sender
  printf("%s\n", "Waiting for an INIT packet......");
  while (type != INIT) {
    ReceiveAPacket(init_packet, sock_recv, init_packet_size);
    // check type of packet.
    memcpy(&type, init_packet, sizeof(char));
  }
  // check mode match or not.
  memcpy(&recv_mode, init_packet+sizeof(char)+max_filename_size+sizeof(file_size),  sizeof(mode));
  if (recv_mode != mode) {
    BuildAckPacket(ack_packet, -1);   // send ACK -1 to notify sender the connection failure.
    SendACK(sock_send, ack_packet, ack_packet_size, send_info);
    UserErrorMessage("Connection failure", "protocol modes don't match");
  }
  else { printf("Successful match. Mode is: %d\n", recv_mode);}
  // Get file name.
  memcpy(&filename, init_packet+sizeof(char), max_filename_size);
  printf("File to receive: %s\n", filename);
  // Get file size.
  memcpy(&file_size, init_packet+sizeof(char)+max_filename_size, sizeof(file_size));
  printf("Size of the file is %ld bytes.\n", file_size);

  // Send ACK 0 for INIT to the sender
  BuildAckPacket(ack_packet, expected_seq_num);
  SendACK(sock_send, ack_packet, ack_packet_size, send_info);
  expected_seq_num++;

  // maxium sequence number that will receive
  max_seq_num = file_size / data_buff_size;
  if (file_size % data_buff_size) {
    max_seq_num++; // add a buff to store the last few bytes of the file.
  }
  printf("The maxium seq number is %d.\n", max_seq_num);

  // Reveive DATA packets from sender.
  char data_buff[data_buff_size];     // to retrieve the file data from DATA packet.
  FILE *fh = NULL;
  char new_filename[50]; // name of the new file.
  char id[20];           // process id in string format.

  // Create a file name with the process id.
  pid_t p_id = getpid();
  //printf("Process id is: %d\n", p_id);
  sprintf(id, "%d", p_id);
  // cantenate received file name with the process id.
  strcpy(new_filename, id);
  strcat(new_filename, filename);

  // Create a new file.
  fh = fopen(new_filename, "w");
  if (fh == NULL ) {
    SystemErrorMessage("File creation error!");
  }
  else { printf("%s\n", "File creation success!"); }

  printf("%s\n", "Ready to receive data packets from the sender......");

  // start receiving packets.
  int write_count = 0; // number of bytes write to the file.
  int sum_count = 0;   // total number of bytes write to the file.
  while (1) {
    // receive a packet.
    ReceiveAPacket(data_packet, sock_recv, data_packet_size);

    // check type of received packet.
    memcpy(&type, data_packet, sizeof(char));
    if (type != DATA) {
      printf("receive unexpected packet type");
      continue;
    }

    // check sequence number of received packet.
    memcpy(&recv_seq_num, data_packet+sizeof(char), sizeof(int));
    if (recv_seq_num == expected_seq_num) {
      // send an ACK packet containing the expected sequence number
      expected_seq_num++;
      BuildAckPacket(ack_packet, expected_seq_num);
      SendACK(sock_send, ack_packet, ack_packet_size, send_info);
      printf("Send ACK %d\n", expected_seq_num);

      // write data to the new file.
      memset(data_buff, 0, data_buff_size);
      memcpy(data_buff, data_packet+sizeof(char)+sizeof(int), data_buff_size);
      write_count = fwrite(data_buff, sizeof(char), data_buff_size, fh);
      sum_count += write_count;
      //printf("Write count: %d\n", write_count);
    }
    else{
      // resend last ack packet.
      BuildAckPacket(ack_packet, expected_seq_num);
      SendACK(sock_send, ack_packet, ack_packet_size, send_info);
    }

    // Receive all the DATA packets for the file
    if (expected_seq_num > max_seq_num) {
      printf("All packets received. Total bytes received is %d\n", sum_count);
      break;
    }
  }

  // close file
  fclose(fh);
  fh = NULL;
  printf("%s\n", "File received success");
  printf("New file name is: %s\n", new_filename);

  close(sock_recv);
  close(sock_send);
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

char* BuildAckPacket(char* res, int seq_num){
  char *packet = res;
  memcpy(packet, &ACK, sizeof(char));
  memcpy(packet+sizeof(char), &seq_num, sizeof(seq_num));

  return res;
}

char* ReceiveAPacket(char *buffer, int sock, int size){
  // source address of the server/ receiver.
  struct sockaddr_storage from_addr;
  socklen_t from_addr_len = sizeof(from_addr);

  int recv_bytes = recvfrom(sock, buffer, size, 0, (struct sockaddr *)&from_addr, &from_addr_len);
  if (recv_bytes < 0) {
    SystemErrorMessage("recvfrom() failed!");
  }
  else if (recv_bytes != size) {
    UserErrorMessage("recvfrom() error", "received unexpected number of bytes.");
  }
  else { printf("%s\n", "recvfrom() success!");}

  return buffer;
}

// Send an ACK packet to the sender.
void SendACK(int sock, const char *msg, int len, struct addrinfo *sender){
  int sent_bytes = sendto(sock, msg, len,
                          0, sender->ai_addr, sender->ai_addrlen);
  if (sent_bytes < 0) {
    SystemErrorMessage("sendto() failed!");
  }
  else if (sent_bytes != len) {
    UserErrorMessage("sendto() error", "sent unexpected number of bytes");
  }
  else { printf("%s\n", "send ACK success!");}
}
