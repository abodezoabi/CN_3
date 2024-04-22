#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/time.h>
// Constants for packet flags
#define SYN_FLAG 100
#define ACK_FLAG 200
#define FIN_FLAG 400
#define SYN_ACK_FLAG 300
#define FIN_ACK_FLAG 600
#define seq_no 1111

// Structure for RUDP header
typedef struct {
    int length;  // Length of data
    unsigned short int  checksum;  // Checksum for data integrity
    int flags;  // Flags for packet type (SYN, ACK, FIN, etc.)
} RUDP_Header;

// A struct that represents RUDP Socket
typedef struct _rudp_socket
{
    int socket_fd;  // UDP socket file descriptor
    bool isServer;  // True if the RUDP socket acts like a server, false for client.
    bool isConnected;  // True if there is an active connection, false otherwise.
    struct sockaddr_in dest_addr;  // Destination address. Client fills it when it connects via rudp_connect(), server fills it when it accepts a connection via rudp_accept().
} RUDP_Socket;



// Helper function to send control packets
ssize_t send_control_packet(RUDP_Socket *sockfd, int flags) {
    // Prepare the header with the appropriate flags
    RUDP_Header header;
    header.flags = flags;


    // Send the packet over the socket
    ssize_t bytes_sent = sendto(sockfd->socket_fd, &header, sizeof(RUDP_Header), 0,
                                (struct sockaddr *)&(sockfd->dest_addr), sizeof(struct sockaddr_in));
    if (bytes_sent < 0) {
        perror("Error sending control packet");
        return -1; // Error
    }

    return bytes_sent; // Number of bytes sent
}

int receive_control_packet(RUDP_Socket *sockfd,RUDP_Header *stam, struct sockaddr_in *recvr_addr,socklen_t *addr_len) {
    if (sockfd == NULL) {
        printf("error:in receive conrol packet 1");
        return -1; // Invalid socket
    }
    // Receive the control packet
    ssize_t bytes_received = recvfrom(sockfd->socket_fd, stam, sizeof(RUDP_Header), 0, (struct sockaddr *) recvr_addr,addr_len);
    if (bytes_received < 0) {
        printf("error:in receive conrol packet 2\n");
        perror("Error receiving control packet\n");
        return -1; // Error receiving data
    } else if (bytes_received == 0) {
        printf("error:in receive conrol packet 3\n");
        return -1; // No data received
    }

    return 1; // Return the received packet header
}



// Function to calculate checksum
unsigned short int calculate_checksum(void *data, unsigned int bytes) {
    unsigned short int *data_pointer = (unsigned short int *)data;
    unsigned int total_sum = 0;

    // Main summing loop
    while (bytes > 1) {
        total_sum += *data_pointer++;
        bytes -= 2;
    }

    // Add left-over byte, if any
    if (bytes > 0)
        total_sum += *((unsigned char *)data_pointer);

    // Fold 32-bit sum to 16 bits
    while (total_sum >> 16)
        total_sum = (total_sum & 0xFFFF) + (total_sum >> 16);

    return (~((unsigned short int)total_sum));
}

RUDP_Socket* rudp_socket(bool isServer, unsigned short int listen_port) {
    RUDP_Socket* sockfd = (RUDP_Socket*)malloc(sizeof(RUDP_Socket));
    if (sockfd == NULL) {
        perror("Failed to allocate memory for socket structure");
        return NULL;
    }

    // Create UDP socket

    sockfd->socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd->socket_fd < 0) {
        perror("Failed to create UDP socket");
        free(sockfd);
        return NULL;
    }

    // Fill in socket address structure
    memset(&sockfd->dest_addr, 0, sizeof(sockfd->dest_addr));
    sockfd->dest_addr.sin_family = AF_INET;
    // sockfd->dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    sockfd->dest_addr.sin_port = htons(listen_port);
    sockfd->dest_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // Bind the socket if it's a server
    if (isServer) {
        if (bind(sockfd->socket_fd, (struct sockaddr *)&(sockfd->dest_addr), sizeof(struct sockaddr_in)) < 0) {
            perror("Failed to bind socket");
            close(sockfd->socket_fd);
            free(sockfd);
            return NULL;
        }
        printf("server bind success\n");
    }

    sockfd->isServer = isServer;
    sockfd->isConnected = false;

    return sockfd;
}

 int rudp_connect(RUDP_Socket *sockfd, struct sockaddr_in *rcvr_addr, socklen_t rcvr_len, char *receiver_ip, unsigned short receiver_port) {
    if (sockfd == NULL || sockfd->isConnected || sockfd->isServer) {
        return 0; // Failure
    }

    memset(&(sockfd->dest_addr), 0, sizeof(struct sockaddr_in));
    sockfd->dest_addr.sin_family = AF_INET;
    sockfd->dest_addr.sin_addr.s_addr = inet_addr(receiver_ip);
    sockfd->dest_addr.sin_port = htons(receiver_port);

    // Set a timeout period for receiving the SYN-ACK packet
    struct timeval timeout;
    timeout.tv_sec = 5; // 5 seconds timeout
    timeout.tv_usec = 0;

    // Send SYN packet to the receiver
    if (send_control_packet(sockfd, SYN_FLAG) < 0) {
        printf("Failed to send SYN packet\n");
        close(sockfd->socket_fd);
        return 0; // Failure
    }
    printf("syn-sent\n");

    while (true) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(sockfd->socket_fd, &read_fds);

        int select_result = select(sockfd->socket_fd + 1, &read_fds, NULL, NULL, &timeout);
        if (select_result < 0) {
            perror("select");
            return 0; // Error in select function
        } else if (select_result == 0) {
            // Timeout occurred, retransmit SYN packet
            printf("Timeout occurred, retransmitting SYN packet\n");
            if (send_control_packet(sockfd, SYN_FLAG) < 0) {
                printf("Failed to retransmit SYN packet\n");
                close(sockfd->socket_fd);
                return 0; // Failure
            }
        } else {
            // Received a packet
            RUDP_Header syn_ack_header;
            ssize_t syn_ack_received = recvfrom(sockfd->socket_fd, &syn_ack_header, sizeof(syn_ack_header), 0, NULL, 0);
            if (syn_ack_received < 0) {
                printf("Failed to receive SYN-ACK packet\n");
                close(sockfd->socket_fd);
                return 0; // Failure
            }
            if (syn_ack_header.flags == SYN_ACK_FLAG) {
                printf("syn-ack-received\n");
                // Send ACK packet to the receiver
                if (send_control_packet(sockfd, ACK_FLAG) < 0) {
                    printf("Failed to send ACK packet\n");
                    close(sockfd->socket_fd);
                    return 0; // Failure
                }
                printf("ack sent successfully\n");
                break; // Handshake successful, exit loop
            }
        }
    }

    return 1; // Success
}

int rudp_accept(RUDP_Socket *receiver_socket, struct sockaddr_in *sndr_addr, socklen_t sndr_len, char *sender_ip, unsigned short sender_port) {
    if (receiver_socket == NULL || receiver_socket->isConnected || !receiver_socket->isServer) {
        printf("accept 1 ");
        return 0; // Failure
    }

    // Create a sender socket
    RUDP_Socket *sender_socket = rudp_socket(false, 0); // Create a client socket
    if (sender_socket == NULL) {
        fprintf(stderr, "Error: Failed to create sender socket\n");
        return 0; // Failure to create sender socket
    }

    // Fill in destination address structure for sender
    memset(&(sender_socket->dest_addr), 0, sizeof(struct sockaddr_in));
    sender_socket->dest_addr.sin_family = AF_INET;
    sender_socket->dest_addr.sin_addr.s_addr = inet_addr(sender_ip);
    sender_socket->dest_addr.sin_port = htons(sender_port);

    // Set a timeout period for receiving the SYN packet
    // timer starts as soon the receiver file started 
    struct timeval timeout;
    timeout.tv_sec = 30; // 30 seconds timeout
    timeout.tv_usec = 0;

    while (true) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(receiver_socket->socket_fd, &read_fds);

        int select_result = select(receiver_socket->socket_fd + 1, &read_fds, NULL, NULL, &timeout);
        if (select_result < 0) {
            perror("select");
            printf("accept 2 \n");
            return 0; // Error in select function
        } else if (select_result == 0) {
            // Timeout occurred, retransmit SYN-ACK packet
            printf("Timeout occurred, retransmitting SYN-ACK packet\n");
            RUDP_Header syn_ack_header;
            syn_ack_header.flags = SYN_ACK_FLAG;
            if (sendto(receiver_socket->socket_fd, &syn_ack_header, sizeof(syn_ack_header), 0, sndr_addr, sndr_len) < 0) {
                printf("cannot send syn ack\n");
                return 0;
            }
            printf("syn-ack sent\n");
        } else {
            // Received a packet
            RUDP_Header syn_header;
            if (receive_control_packet(receiver_socket, &syn_header, sndr_addr, &sndr_len) < 0) {
                printf("accept 4444 \n");
                return 0; // Failed to receive SYN packet
            }
            if (syn_header.flags == SYN_FLAG) {
                printf("syn-received\n");

                RUDP_Header syn_ack_header;
                syn_ack_header.flags = SYN_ACK_FLAG;

                // Send SYN-ACK packet to sender
                if (sendto(receiver_socket->socket_fd, &syn_ack_header, sizeof(syn_ack_header), 0, sndr_addr, sndr_len) < 0) {
                    printf("cannot send syn ack\n");
                    return 0;
                }
                printf("syn-ack sent\n");

                // Set a timeout period for receiving the ACK packet
                struct timeval ack_timeout;
                ack_timeout.tv_sec = 5; // 5 seconds timeout
                ack_timeout.tv_usec = 0;

                // Receive ACK packet from sender
                fd_set ack_read_fds;
                FD_ZERO(&ack_read_fds);
                FD_SET(receiver_socket->socket_fd, &ack_read_fds);

                int ack_select_result = select(receiver_socket->socket_fd + 1, &ack_read_fds, NULL, NULL, &ack_timeout);
                if (ack_select_result < 0) {
                    perror("select");
                    printf("accept 6 \n");
                    return 0; // Error in select function
                } else if (ack_select_result == 0) {
                    // Timeout occurred, retransmit SYN-ACK packet
                    printf("Timeout occurred, retransmitting SYN-ACK packet\n");
                   if (sendto(receiver_socket->socket_fd, &syn_ack_header, sizeof(syn_ack_header), 0, sndr_addr, sndr_len) < 0) {
                       printf("cannot send syn ack\n");
                       return 0;
                   }
                   printf("syn-ack sent\n");
                } else {
                    // Received ACK packet
                    RUDP_Header ack_header;
                    if (receive_control_packet(receiver_socket, &ack_header, sndr_addr, &sndr_len) < 0) {
                        printf("accept 6 \n");
                        return 0; // Failed to receive ACK packet
                    }
                    if (ack_header.flags == ACK_FLAG) {
                        printf("ack received\n");
                        // Handshake successful, set isConnected flag
                        receiver_socket->isConnected = true;
                        printf("HandShake successfully\n");

                        break; // Handshake successful, exit loop
                    }
                }
            }
        }
    }

    

    

    return 1; // Success
}




// Helper function to receive acknowledgment packet
int receive_acknowledgment(RUDP_Socket *sockfd) {
    uint8_t ack_packet; // Variable to store received acknowledgment packet
    int bytes_received = recv(sockfd->socket_fd, &ack_packet, sizeof(ack_packet), 0);
    if (bytes_received < 0) {
        perror("Error receiving acknowledgment");
        return -1; // Error
    }

    return ack_packet; // Return received acknowledgment packet
}




#define MAX_RETRIES 3

int rudp_close(RUDP_Socket *sockfd) {
   // if(sockfd->isServer){ printf(" is server\n");return -1;}

    // Step 1: Send FIN packet
    RUDP_Header fin_header;
    fin_header.flags = FIN_FLAG;

    int retries = 0;
    bool fin_ack_received = false;

    while (retries < MAX_RETRIES && !fin_ack_received) {
        ssize_t bytes_sent_fin = send_control_packet(sockfd, fin_header.flags);
        if (bytes_sent_fin < 0) {
            perror("send");
            return -1; // Error in sending FIN packet
        }
        printf("fin sent\n");

        // Wait for SYN/ACK packet with a timeout
        struct timeval timeout;
        timeout.tv_sec = 30; // 30 seconds timeout
        timeout.tv_usec = 0;

        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(sockfd->socket_fd, &read_fds);

        int select_result = select(sockfd->socket_fd + 1, &read_fds, NULL, NULL, &timeout);
        if (select_result < 0) {
            perror("select");
            return -1; // Error in select function
        } else if (select_result == 0) {
            // Timeout occurred
            printf("Timeout occurred while waiting for FIN/ACK packet, retrying...\n");
            retries++;
        } else {
            // Data is available to read, check if it's a FIN/ACK packet
            RUDP_Header fin_ack_header;
            struct sockaddr_in recvr_addr;
            socklen_t addr_len = sizeof(recvr_addr);
            ssize_t bytes_received = recvfrom(sockfd->socket_fd, &fin_ack_header, sizeof(fin_ack_header), 0, (struct sockaddr *) &recvr_addr, &addr_len);
            if (bytes_received < 0) {
                perror("recvfrom");
                return -1; // Error in receiving SYN/ACK packet
            }
            if (fin_ack_header.flags == FIN_ACK_FLAG) {
                fin_ack_received = true;
                printf("fin-ack received\n");
            } else {
                printf("Received unexpected packet while waiting for FIN/ACK, retrying...\n");
                retries++;
            }
        }
    }

    // If fin/ACK received, send ACK packet
    if (fin_ack_received) {
        RUDP_Header ack_header;
        ack_header.flags = ACK_FLAG;
        ssize_t bytes_sent_ack = send_control_packet(sockfd, ACK_FLAG);
        if (bytes_sent_ack < 0) {
            perror("send");
            return -1; // Error in sending ACK packet
        }
        return 1;
    } else {
        printf("Maximum retries reached, closing connection failed.\n");
        return -1; // Maximum retries reached, closing connection failed
    }

    return -1; // Connection closed successfully
}

#define MAX_RETRIES 3


#define MAX_RETRIES 3


int rudp_recv_close(RUDP_Socket *sockfd, struct sockaddr_in *sndr_addr, socklen_t sndr_len,bool fin_recvd) {
    bool fin_received = fin_recvd;

    // Step 1: Receive FIN packet
    while (!fin_received) {
        RUDP_Header fin_header;
        ssize_t bytes_received_fin = recvfrom(sockfd->socket_fd, &fin_header, sizeof(fin_header), 0, (struct sockaddr *)sndr_addr, &sndr_len);
        if (bytes_received_fin < 0) {
            perror("recvfrom");
            return -1; // Error in receiving FIN packet
        } else if (bytes_received_fin == 0) {
            printf("Connection closed by peer\n");
            return -1; // Connection closed by peer
        } else {
            fin_received = true;
            printf("Received closing FIN\n");
        }
    }

    // Step 2: Send FIN-ACK packet
    RUDP_Header fin_ack_header;
    fin_ack_header.flags = FIN_ACK_FLAG;
    ssize_t bytes_sent_fin_ack = sendto(sockfd->socket_fd, &fin_ack_header, sizeof(fin_ack_header), 0, (struct sockaddr *)sndr_addr, sndr_len);
    if (bytes_sent_fin_ack < 0) {
        perror("sendto");
        return -1; // Error in sending FIN-ACK packet
    }

    // Step 3: Wait for ACK packet
    bool ack_received = false;
    int retries = 0;

    while (retries < MAX_RETRIES && !ack_received) {
        // Wait for ACK packet with a timeout
        struct timeval timeout;
        timeout.tv_sec = 30; // 30 seconds timeout
        timeout.tv_usec = 0;

        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(sockfd->socket_fd, &read_fds);

        int select_result = select(sockfd->socket_fd + 1, &read_fds, NULL, NULL, &timeout);
        if (select_result < 0) {
            perror("select");
            return -1; // Error in select function
        } else if (select_result == 0) {
            // Timeout occurred
            printf("Timeout occurred while waiting for ACK packet, retrying...\n");

            // Retransmit FIN-ACK packet
            ssize_t bytes_sent_fin_ack_retry = sendto(sockfd->socket_fd, &fin_ack_header, sizeof(fin_ack_header), 0, (struct sockaddr *)sndr_addr, sndr_len);
            if (bytes_sent_fin_ack_retry < 0) {
                perror("sendto");
                return -1; // Error in retransmitting FIN-ACK packet
            }

            retries++;
        } else {
            // Data is available to read, check if it's an ACK packet
            RUDP_Header ack_header;
            ssize_t bytes_received_ack = recvfrom(sockfd->socket_fd, &ack_header, sizeof(ack_header), 0, NULL, 0);
            if (bytes_received_ack < 0) {
                perror("recvfrom");
                return -1; // Error in receiving ACK packet
            }
            if (ack_header.flags == ACK_FLAG) {
                ack_received = true;
                sockfd->isConnected = false;
                printf("Received Closing ack after fin-ack\n");
                return 1;
            } else {
                printf("Received unexpected packet while waiting for ACK, retrying...\n");
                retries++;
            }
        }
    }

    if (!ack_received) {
        printf("Maximum retries reached, closing connection failed.\n");
        return -1; // Maximum retries reached, closing connection failed
    }

    return 0; // Connection closed successfully
}




int send_data_packet(RUDP_Socket *sockfd, char *data, size_t data_size, unsigned short int checksum) {

    // Prepare data packet with flags and checksum
    RUDP_Header header;
    header.flags = seq_no; //check if same data
    header.checksum = checksum;
    printf("checksum sent:%d",checksum);

    

    // Send the header first
    ssize_t bytes_sent = sendto(sockfd->socket_fd, &header, sizeof(header),
     0,(struct sockaddr *)&(sockfd->dest_addr),sizeof(struct sockaddr_in));
    
    
    if (bytes_sent < 0) {
        printf("send 1\n");
        return -1; // Error in sending header
    }
    printf("data packet header sent \n");

    // Then send the actual data
      bytes_sent = sendto(sockfd->socket_fd, &data,data_size,
     0,(struct sockaddr *)&(sockfd->dest_addr),sizeof(struct sockaddr_in));
    if (bytes_sent < 0) {
        printf("send 2\n");
        return -1; // Error in sending data
    }
    printf("data packet sent \n");

    return bytes_sent; // Return number of bytes sent
}


int receive_data_packet(RUDP_Socket *sockfd, void *buffer, unsigned int buffer_size, unsigned short int *checksum) {
    // Receive the header first
    

    RUDP_Header header;
    ssize_t header_received = recv(sockfd->socket_fd, &header, sizeof(header), 0);
    if (header_received < 0) {
        perror("recv");
        return -1; // Error in receiving header
    }

    // Check if it's a data packet
    if (header.flags != seq_no) {
        printf("Received packet is not a data packet\n");
        return -1; // Not a data packet, discard
    }

    // Receive the actual data
    ssize_t data_received = recv(sockfd->socket_fd, buffer, buffer_size, 0);
    if (data_received < 0) {
        perror("recv");
        return -1; // Error in receiving data
    }

    // Update checksum
    *checksum = header.checksum;

    return data_received; // Return number of bytes received
}






int rudp_send_file_1(RUDP_Socket *sockfd, char *buffer, size_t buffer_size,char *receiver_ip, unsigned short receiver_port) {
    int bytes_sent = -1;
    bool ack1_received = false;
    bool ack2_received = false;
    unsigned short int checksum = calculate_checksum(buffer, sizeof(buffer));

    memset(&(sockfd->dest_addr), 0, sizeof(struct sockaddr_in));
    sockfd->dest_addr.sin_family = AF_INET;
    sockfd->dest_addr.sin_addr.s_addr = inet_addr(receiver_ip);
    sockfd->dest_addr.sin_port = htons(receiver_port);

    while (!(ack1_received )) {
        // Prepare data packet with flags and checksum
        RUDP_Header header;
        header.flags = seq_no; // Assuming seqnum for the whole file
        header.checksum = checksum;

        // Send the header
        ssize_t bytes_sent_header = sendto(sockfd->socket_fd,&header,sizeof(RUDP_Header),0
                ,(struct sockaddr *)&(sockfd->dest_addr),sizeof (struct sockaddr_in));
        if (bytes_sent_header < 0) {
            perror("send");
            return -1; // Error in sending header
        }

        // Wait for acknowledgment packet 1 with a timeout of 30 seconds
        struct timeval timeout1;
        timeout1.tv_sec = 2; // 30 seconds timeout
        timeout1.tv_usec = 0;

        fd_set read_fds1;
        FD_ZERO(&read_fds1);
        FD_SET(sockfd->socket_fd, &read_fds1);

        int select_result1 = select(sockfd->socket_fd + 1, &read_fds1, NULL, NULL, &timeout1);
        if (select_result1 < 0) {
            perror("select");
            return -1; // Error in select function
        } else if (select_result1 == 0) {
            // Timeout occurred
            printf("Timeout occurred for acknowledgment 1\n");
        } else {
            // Acknowledgment 1 is available to read
            RUDP_Header ack1_header;
            struct sockaddr_in recvr_addr1;
            socklen_t addr_len1 = sizeof(recvr_addr1);
            int ack1_result = receive_control_packet(sockfd, &ack1_header, &recvr_addr1, &addr_len1);
            if (ack1_result == -1) {
                printf("Failed to receive acknowledgment 1 packet\n");
                return -1; // Error in receiving acknowledgment packet
            }
            if (ack1_header.flags == ACK_FLAG) {
                // Acknowledgment 1 received
                ack1_received = true;
              //  printf("Acknowledgment 1 received\n");
            }
        }

        if (ack1_received) {
            while(!ack2_received){
            // Send the data
                ssize_t bytes_sent_data = sendto(sockfd->socket_fd, buffer, buffer_size, 0,(struct sockaddr *)&(sockfd->dest_addr),sizeof (struct sockaddr_in));
            if (bytes_sent_data < 0) {
                perror("send");
                return -1; // Error in sending data
            }
              //  printf("files sent\n");

            // Wait for acknowledgment packet 2 with a timeout of 30 seconds
            struct timeval timeout2;
            timeout2.tv_sec = 2; // 30 seconds timeout
            timeout2.tv_usec = 0;

            fd_set read_fds2;
            FD_ZERO(&read_fds2);
            FD_SET(sockfd->socket_fd, &read_fds2);

            int select_result2 = select(sockfd->socket_fd + 1, &read_fds2, NULL, NULL, &timeout2);
            if (select_result2 < 0) {
                perror("select");
                return -1; // Error in select function
            } else if (select_result2 == 0) {
                // Timeout occurred
                printf("Timeout occurred for acknowledgment 2\n");
            } else {
                // Acknowledgment 2 is available to read
                RUDP_Header ack2_header;
                struct sockaddr_in recvr_addr2;
                socklen_t addr_len2 = sizeof(recvr_addr2);
                int ack2_result = receive_control_packet(sockfd, &ack2_header, &recvr_addr2, &addr_len2);
                if (ack2_result == -1) {
                    printf("Failed to receive acknowledgment 2 packet\n");
                    return -1; // Error in receiving acknowledgment packet
                }
                if (ack2_header.flags == ACK_FLAG) {
                    // Acknowledgment 2 received
                    ack2_received = true;
                   // printf("Acknowledgment 2 received, file transfer successful\n");
                    return 1;
                   // break;
                }
              }
            }
        }
    }

    return bytes_sent; // Return number of bytes sent
}


int rudp_rcv_file_1(RUDP_Socket *sockfd, char *buffer, size_t buffer_size,struct sockaddr_in *sndr_addr,
        socklen_t sndr_len) {
    int bytes_received = -1;
    bool ack1_sent = false;
    bool ack2_sent = false;
    bool checksum_received = false;
    unsigned short int sent_checksum ;
    unsigned short int recvd_checksum;

    while (!ack2_sent) {
        // Receive the header for checksum
        RUDP_Header header;
        ssize_t bytes_received_header = receive_control_packet(sockfd, &header, sndr_addr, &sndr_len);
        if (bytes_received_header < 0) {
            perror("recv");
            return -1; // Error in receiving header
        } else if (bytes_received_header == 0) {
            printf("Connection closed by peer\n");
            return -1;
        }


        // Check if header contains checksum
        if (header.flags == seq_no) {
            checksum_received = true;
            sent_checksum = header.checksum;
         //   printf("checksum received\n");
        }



        // Send acknowledgment 1 if checksum received
        if (checksum_received && !ack1_sent) {
            RUDP_Header ack1_header;
            ack1_header.flags = ACK_FLAG;
            ssize_t bytes_sent_ack1 = sendto(sockfd->socket_fd, &ack1_header, sizeof(ack1_header), 0, sndr_addr,
                                             sndr_len);
            if (bytes_sent_ack1 < 0) {
                perror("send");
                return -1; // Error in sending acknowledgment 1
            }
            ack1_sent = true;
        }

        // Receive the data if checksum received and acknowledgment 1 sent
        if (checksum_received && ack1_sent) {
            ssize_t bytes_received_data = recvfrom(sockfd->socket_fd, buffer, buffer_size, 0, sndr_addr, &sndr_len);
            if (bytes_received_data < 0) {
                perror("recv");
                return -1; // Error in receiving data
            }
//             if( sent_checksum != calculate_checksum(buffer, sizeof(buffer)) ){ printf("checksum mismatch");return 0;}
//            if (recvd_checksum != sent_checksum) {
//                printf("checksum mismatched in receive file 1 \n ");
//                return EXIT_FAILURE;
//            }
//            printf("file received\n");
        }




        // Send acknowledgment 2
        RUDP_Header ack2_header;
        ack2_header.flags = ACK_FLAG;
        ssize_t bytes_sent_ack2 = sendto(sockfd->socket_fd, &ack2_header, sizeof(ack2_header), 0, sndr_addr,
                                         sndr_len);
        if (bytes_sent_ack2 < 0) {
            perror("send");
            return -1; // Error in sending acknowledgment 2
        }
      // CHECK FOR CHECKSUM EQUALITY in every file part
        if( sent_checksum != calculate_checksum(buffer, sizeof(buffer)) ){ return 0;// 2 checksums unequal
        }else{
//            printf("File Status: Received\n");
//            printf("2 CheckSums : EQUAL\n");
            ack2_sent = true;
            return 1;
        }
    }
    return bytes_received; // Return number of bytes received
}



