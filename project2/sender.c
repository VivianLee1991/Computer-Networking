// Reliable File transfer.
// Written by W. Li 
// Date completed: 10-21-2017

// This file is a UDP Sender program.
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
#include <unistd.h>     // for close() and alarm()
#include <errno.h>      // for errno and EINTR
#include <signal.h>     // for sigaction()

#define TIMEOUT_SECS 2  // period before resend
#define MAX_TRIES 5     // trials before giving up

// Definitions of three types of packets.
const char INIT = '1';
const char ACK = '2';
const char DATA = '3';

const int max_filename_size = 30; // Maxium size of file name in bytes.
int tries = 0;   // number of tries.

void UserErrorMessage(const char *msg, const char *err_info);
void SystemErrorMessage(const char *msg);
void print_message(const char* buf, int buf_len);
char* BuildInitPacket(char* res, const char *filename,long filesize, int mode);
char* BuildDataPacket(char* res, int num, const char *data, int data_size);
void SendAPacket(int sock, const char *msg, int len, struct addrinfo *receiver);
char* ReceiveAck(char *buffer, int sock, int size);
int min(int a, int b);
void CatchAlarm(int ignored);

int main(int argc, char const *argv[]) {
/*----------------------------------------------------------------------------*/
  /* Step 1: Test for correct number of arguments in command line. */
  // If incorrect, throw error message and exit.
  if (argc != 9) {
    UserErrorMessage("Command Error",
                     "Input format is : ./sender <-m mode> <-p port> <-h hostname> <-f filename>");
  }
/*----------------------------------------------------------------------------*/
  /* Step 2: Parse the command line. */
  int mode = 0;
  const char *port;
  const char *receiver;
  const char *filename;
  for (int i = 1; i < argc; i++) {
    // -m mode: mode = 1 use the Stop-and-Wait;
    //          mode > 1 use the Go-Back-N, and N = mode.
    if (strcmp(argv[i], "-m") == 0) {
      mode = atoi(argv[i+1]);
      continue;
    }
    // -p port is the port of the receiver
    if (strcmp(argv[i], "-p") == 0) {
      port = argv[i+1];
      continue;
    }
    // -h hostname is the DNS name of the receiver
    if (strcmp(argv[i], "-h") == 0) {
      receiver = argv[i+1];
      continue;
    }
    // -f filename is the name of the file to be sent
    if (strcmp(argv[i], "-f") == 0) {
      filename = argv[i+1];
      continue;
    }
  }
  // Test for Step 1 and Step 2.
  printf("Mode is %d\n", mode);
  printf("Port is %s\n", port);
  printf("Receiver is %s\n", receiver);
  printf("Filename is %s\n", filename);

/*----------------------------------------------------------------------------*/
  /* Step 3.1: Construct the receiver's address. */
  int status;
  struct addrinfo hints;
  struct addrinfo *recv_info; // point to the result.

  memset(&hints, 0, sizeof(hints)); // zero out the structure
  hints.ai_family = AF_UNSPEC;      // any address family, IPv4 or IPv6
  hints.ai_socktype = SOCK_DGRAM;   // UDP datagram sockets
  hints.ai_protocol = IPPROTO_UDP;  // only UDP protocol

  status = getaddrinfo(receiver, port, &hints, &recv_info);
  if (status != 0) {
    UserErrorMessage("receiver getaddrinfo() failed", gai_strerror(status));
  }
  else { printf("%s\n", "receiver getaddrinfo() success"); }  // test.

  /* Step 3.2: Construct the local address. */
  int local_status;
  struct addrinfo addr_criteria;
  struct addrinfo *local_info; // point to the result.

  memset(&addr_criteria, 0, sizeof(addr_criteria)); // zero out the structure
  addr_criteria.ai_family = AF_UNSPEC;      // any address family, IPv4 or IPv6
  addr_criteria.ai_socktype = SOCK_DGRAM;   // UDP datagram sockets
  addr_criteria.ai_protocol = IPPROTO_UDP;  // only UDP protocol
  addr_criteria.ai_flags = AI_PASSIVE;      // Use IP of the local host

  local_status = getaddrinfo(NULL, port, &addr_criteria, &local_info);
  if (local_status != 0) {
    UserErrorMessage("local getaddrinfo() failed", gai_strerror(local_status));
  }
  else { printf("%s\n", "local getaddrinfo() success"); }

/*----------------------------------------------------------------------------*/
  /* Step 4.1: Create a UDP socket to receive packets. */
  int sock_recv = socket(local_info->ai_family,
                         local_info->ai_socktype, local_info->ai_protocol);
  if (sock_recv < 0) {
    SystemErrorMessage("Socket creation failed!");
  }
  else { printf("%s\n", "Socket to receive creation success"); }  // test.

  /* Step 4.2: Create a UDP socket to send packets. */
  int sock_send = socket(recv_info->ai_family,
                         recv_info->ai_socktype, recv_info->ai_protocol);
    if (sock_send < 0) {
      SystemErrorMessage("Socket creation failed!");
    }
    else { printf("%s\n", "Socket to send creation success"); }  // test.

  /* Step 4.3: Bind the socket for receiving to the local IP and port. */
  if (bind(sock_recv, local_info->ai_addr, local_info->ai_addrlen) < 0) {
    SystemErrorMessage("bind() failed");
  }
  else { printf("%s\n", "bind() success"); }

  freeaddrinfo(local_info);    // Free address list allocated by getaddrinfo().

/*----------------------------------------------------------------------------*/
  /* Step 5: Go-Back-N protocol. */
  const int window_size = mode;  // size of the window in packets.
  int ack_num = -1;              // sequence number in received ACK packet.
  int last_ack_num = -1;         // sequence number of the last acknowledged packet.
  int largest_sent_num = 0;      // largest sequence number that could be sent in a window.
  int max_seq_num = 0;           // the maxium sequence number that shoud be sent
  const int data_buff_size = 16; // size of file data contained in each DATA packet.
  const int win_buff_size = window_size * data_buff_size;

  char type;                     // type of packet
  //size of packets.
  const int init_packet_size = 1 + sizeof(long) + max_filename_size + sizeof(int);
  const int data_packet_size = 1 + sizeof(int) +data_buff_size;
  const int ack_packet_size = 1 + sizeof(int);
  //printf("size of INIT packet is %d\n", init_packet_size);

  // packet definitions.
  char init_packet[init_packet_size];
  memset(init_packet, 0, init_packet_size);
  char data_packet[data_packet_size];
  memset(data_packet, 0, data_packet_size);
  char ack_packet[ack_packet_size];        // buffer to store the received message.
  memset(ack_packet, 0, ack_packet_size);

  // Set signal handler for alarm signal.
  struct sigaction handler;   //signal handler.
  handler.sa_handler = CatchAlarm;

  if (sigfillset(&handler.sa_mask) < 0) {  // block everything in handler.
    SystemErrorMessage("sigfillset() failed!");
  }
  handler.sa_flags = 0;

  if (sigaction(SIGALRM, &handler, 0) < 0) {
    SystemErrorMessage("sigaction() failed for SIGALARM!");
  }

  // Open the given file.
  FILE *fh = NULL;
  fh = fopen(filename, "r");
  if (fh == NULL ) {
    SystemErrorMessage("File open error!");
  }
  else { printf("%s\n", "File open success"); }

  // Get the file size.
  long file_size = 0;            // size of the file in bytes
  fseek(fh, 0, SEEK_END);        // seek to the end of file
  file_size = ftell(fh);
  fseek(fh, 0, SEEK_SET);        // set back to the start of the file
  printf("The size of %s is %ld bytes.\n", filename, file_size); // test.

  max_seq_num = file_size / data_buff_size; // maxium seq number of DATA packets.
  if (file_size % data_buff_size) {
    max_seq_num++;        // add a buff to store the last few bytes of the file.
  }
  printf("The maxium seq number is %d\n", max_seq_num);

  // Build a INIT packet
  BuildInitPacket(init_packet, filename, file_size, mode);
  //printf("init_packet after built is:\n");
  //print_message(init_packet, init_packet_size);

  // Send an INIT packet to the receiver.
  SendAPacket(sock_send, init_packet, init_packet_size, recv_info);

  // Receive an ACK packet from the receiver.
  ReceiveAck(ack_packet, sock_recv, ack_packet_size);
  memcpy(&type, ack_packet, sizeof(char));
  memcpy(&ack_num, ack_packet+sizeof(char), sizeof(int));
  // ACK 0 indicates a successful match with the receiver.
  if (type != ACK) {
    UserErrorMessage("Connection failure", "No ACK received");
  }
  else if (ack_num != 0) {
    UserErrorMessage("Connection failure", "Cannot pair with the receiver.");
  }
  else {
    last_ack_num = ack_num;
    printf("%s\n", "Connected with the receiver, ready to send...");}

  // Send DATA packets.
  // buffers to process file data.
  char data_buff[data_buff_size];           // buffer to store a packet of file data.
  memset(&data_buff, 0, sizeof(data_buff)); // zero out the buffer.
  char win_buff[win_buff_size];             // buffer to store a window of file data.
  memset(&win_buff, 0, sizeof(win_buff));   // zero out the buffer.
  char read_buff[win_buff_size];            // buffer to read data from file.

  int read_count = 0;      // number of bytes read from file each time.
  int sum_bytes = 0;       // total number of bytes read from the file.
  int num = 0;
  int recv_bytes = 0;      // number of bytes received.

  struct sockaddr_storage from_addr;   // source address of the receiver.
  socklen_t from_addr_len = sizeof(from_addr);

  while (1) {
    // Stop sending when reaches the End of the file.
    if (feof(fh) != 0) {
      break;
    }

    // read new data sets from the file.
    memset(&read_buff, 0, sizeof(read_buff)); // zero out the buffer.
    read_count = fread(read_buff, sizeof(char),
                       (window_size - (largest_sent_num - last_ack_num))*data_buff_size, fh);
    sum_bytes += read_count;
    // Test
    printf("Read count: %d\n", read_count);
    printf("Buff content: %s\n", read_buff);

    // put new data sets into window buffer.
    num = read_count / data_buff_size;
    if (read_count % data_buff_size) {
      num++;
    }
    memcpy(win_buff, read_buff, num*data_buff_size);

    // updata largest_sent_num.
    largest_sent_num = min(max_seq_num, last_ack_num + window_size);

    // send data in the window buffer.
    for (int i = last_ack_num+1; i <= largest_sent_num; i++) {
      // build a DATA packet
      memcpy(data_buff, win_buff+((i-1)%window_size)*data_buff_size, data_buff_size);
      BuildDataPacket(data_packet, i, data_buff, data_buff_size);

      // send a DATA packet.
      SendAPacket(sock_send, data_packet, data_packet_size, recv_info);
    }

    // Receive ACKs.
    while (last_ack_num < largest_sent_num ) {
      alarm(TIMEOUT_SECS);  // set the timeout.

      // Wait to receive ACK.
      while((recv_bytes = recvfrom(sock_recv, ack_packet, ack_packet_size,
             0, (struct sockaddr *)&from_addr, &from_addr_len)) < 0)
      {
        if (errno == EINTR) {  // alarm went off.
          break;
        }
        else {
          SystemErrorMessage("recvfrom() failed!");
        }
      }

      if (errno == EINTR) {
         if (tries < MAX_TRIES) {
           printf("Time out, %d more times to retry...\n", MAX_TRIES - tries);
           break;
         }
         else {
           UserErrorMessage("No response", "unable to communicate with the receiver.");
         }
      }

      if (recv_bytes) {
        alarm(0);
        memcpy(&type, ack_packet, sizeof(char));
        memcpy(&ack_num, ack_packet+sizeof(char), sizeof(int));
        if (type == ACK) {
          if ((ack_num - 1) > last_ack_num) {
            last_ack_num = ack_num - 1;
            printf("Receive ACK %d\n", ack_num);
          }
          else if ((ack_num - 1) == last_ack_num) {
            printf("Packet lost from %d. Begin to resend.\n", ack_num);
            break;
          }
        }
      }
    }

    if (last_ack_num == max_seq_num) { // All the data in file has been sent.
      break;
    }
  }

  printf("Total read count: %d\n", sum_bytes);
  // Close the file.
  fclose(fh);
  fh = NULL;
  // Close sockets.
  close(sock_recv);
  close(sock_send);

  return 0;
}

//************************     Helper Functions      ***************************

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

// build the INIT packet.
// INIT which is the first message sent by the sender
// and specifies the name and length of the file, and
// the protocol used (1 means Stop-and-Wait, > 1 size
// of window in Go-Back-N)
char* BuildInitPacket(char* res, const char *filename,long filesize, int mode){
  char *packet = res;
  memcpy(packet, &INIT, sizeof(char));
  memcpy(packet+sizeof(char), filename, max_filename_size);
  memcpy(packet+sizeof(char)+max_filename_size, &filesize, sizeof(filesize));
  memcpy(packet+sizeof(char)+max_filename_size+sizeof(filesize), &mode, sizeof(mode));

  return res;
}

// Build DATA packet
char* BuildDataPacket(char* res, int num, const char *data, int data_size)
{
  memcpy(res, &DATA, sizeof(char));
  memcpy(res+sizeof(char), &num, sizeof(num));
  memcpy(res+sizeof(char)+sizeof(num), data, data_size);

  return res;
}

// Send a packet to receiver
void SendAPacket(int sock, const char *msg, int len, struct addrinfo *receiver) {
  int sent_bytes = sendto(sock, msg, len,
                          0, receiver->ai_addr, receiver->ai_addrlen);
  if (sent_bytes < 0) {
    SystemErrorMessage("sendto() failed!");
  }
  else if (sent_bytes != len) {
    UserErrorMessage("sendto() error", "sent unexpected number of bytes");
  }
  else { printf("%s\n", "sendto() success!");}
}

char *ReceiveAck(char *buffer, int sock, int size){
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

// return the smaller number.
int min(int a, int b){
  return a < b? a:b;
}

// Handler for SIGALARM.
void CatchAlarm(int ignored) {
  tries++;
}

///*
void print_message(const char* buf, int buf_len)
{
    int i = 0, written = 0, j =0, last = 0;
    for (i=0; i<buf_len; i++) {
        unsigned char uc = buf[i];
        if (i%16 == 0)
            printf("0x%04x:   ", i);
        written += printf("%02x", uc);
        if ((i+1)%2 == 0)
            written += printf(" ");
        if ((i+1)%16 == 0 || i == buf_len-1) {
            if (i == buf_len-1)
                 last = i+1 - (i+1)%16;
            else
                last = i - 15;
            for (j=written; j<40; j++)
                printf(" ");
            written = 0;
            printf("\t");
            for (j=last; j<=i; j++) {
                if (buf[j] < 32 || buf[j] > 127)
                    printf(".");
                else
                    printf("%c", buf[j]);
            }
            printf("\n");
        }
    }
    printf("\n\n");
}
//*/
