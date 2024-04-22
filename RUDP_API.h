#ifndef RUDP_SENDER_H
#define RUDP_SENDER_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

// Constants for packet flags
#define SYN_FLAG 0x01
#define ACK_FLAG 0x02
#define FIN_FLAG 0x04

// Structure for RUDP header
typedef struct {
    uint16_t length;    // Length of data
    uint16_t checksum;  // Checksum for data integrity
    uint8_t flags;      // Flags for packet type (SYN, ACK, FIN, etc.)
} RUDP_Header;

// A struct that represents RUDP Socket
typedef struct _rudp_socket {
    int socket_fd;              // UDP socket file descriptor
    bool isServer;              // True if the RUDP socket acts like a server, false for client.
    bool isConnected;           // True if there is an active connection, false otherwise.
    struct sockaddr_in dest_addr;  // Destination address. Client fills it when it connects via rudp_connect(), server fills it when it accepts a connection via rudp_accept().
} RUDP_Socket;

// Function prototypes
RUDP_Socket *rudp_socket(bool isServer, unsigned short int listen_port);
int rudp_connect(RUDP_Socket *sockfd, struct sockaddr_in *rcvr_addr, socklen_t rcvr_len,char *receiver_ip, unsigned short receiver_port);
int rudp_accept(RUDP_Socket *sockfd,struct sockaddr_in *sndr_addr, socklen_t sndr_len,char *sender_ip, unsigned short sender_port);
int rudp_send(RUDP_Socket *sockfd, void *buffer, unsigned int buffer_size);//need to add &receiver addr 
int rudp_recv(RUDP_Socket *sockfd, void *buffer, unsigned int buffer_size);//need to add sndr addr 
int rudp_recv_close(RUDP_Socket *sockfd,struct sockaddr_in *sndr_addr, socklen_t sndr_len,bool fin_recvd);
int rudp_close(RUDP_Socket *sockfd);//need to implement
int receive_acknowledgment(RUDP_Socket *sockfd);//need to delete 
int rudp_send_file(RUDP_Socket *sockfd, void *buffer, unsigned int buffer_size);//need to delete/update
int receive_data_packet(RUDP_Socket *sockfd, void *buffer, unsigned int buffer_size, unsigned short int *checksum);
int send_data_packet(RUDP_Socket *sockfd, char *data, size_t data_size, unsigned short int checksum);
//int rudp_send_file_1(RUDP_Socket *sockfd, void *buffer, unsigned int buffer_size,char *receiver_ip, unsigned short receiver_port);
int rudp_rcv_file_1(RUDP_Socket *sockfd, char *buffer, size_t buffer_size,struct sockaddr_in *sndr_addr,socklen_t sndr_len);
int rudp_send_file_1(RUDP_Socket *sockfd, void *buffer, size_t buffer_size,char *receiver_ip, unsigned short receiver_port);
#endif /* RUDP_SENDER_H */
