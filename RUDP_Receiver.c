#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/time.h>

#include "RUDP_API.h" // Include the RUDP API header file

#define DEFAULT_PORT 4567 // Port number to listen on
#define DEFAULT_IP "127.0.0.1"

int main(int argc, char *argv[]) {
    // Check the number of command-line arguments
    if (argc != 3) {
        fprintf(stderr, "Usage: %s -p <PORT>\n", argv[0]);
        return EXIT_FAILURE;
    }

    unsigned short port = DEFAULT_PORT;

    // Parse command-line arguments
    if (strcmp(argv[1], "-p") == 0 && argc == 3) {
        port = atoi(argv[2]);
    } else {
        fprintf(stderr, "Usage: %s -p <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    // Create a UDP connection between the Receiver and the Sender
    RUDP_Socket *sockfd = rudp_socket(true, port); // Create a server socket
    if (sockfd == NULL) {
        fprintf(stderr, "Error: Failed to create UDP socket\n");
        return EXIT_FAILURE;
    }
    struct sockaddr_in sndr_addr;
    socklen_t addr_len = sizeof(sndr_addr);
    memset(&sndr_addr, 0, addr_len);

    printf("Starting Receiver...\n");

    // Wait for RUDP connection
    printf("Waiting for RUDP connection....\n");
    int accept_status = rudp_accept(sockfd, &sndr_addr, addr_len, DEFAULT_IP, port);
    if (accept_status == 0) {
        fprintf(stderr, "Error: Failed to accept connection in accept\n");
        return EXIT_FAILURE;
    } else {
        printf("Connection request received.\n");
    }

    printf("Sender connected, beginning to receive file...\n");
    printf("--------------------------------------\n");
    printf("- * Statistics For  Each Run * -\n");


    // Receive the file
    char buffer[2097152]; // Buffer to store received data

    FILE *output_file = fopen("received_file.txt", "wb"); // Open file for writing
    if (output_file == NULL) {
        fprintf(stderr, "Error: Failed to open output file\n");
        rudp_close(sockfd);
        return EXIT_FAILURE;
    }

    struct timeval start, end;
    double total_time = 0.0;
    int run_counter = 0;
    double total_bandwidth = 0.0;

    while (true) {
        int bytes_received;
        gettimeofday(&start, NULL); // Start measuring time
        for(int i=0;i<50;i++) {// 2MB/50000==42

            bytes_received = rudp_rcv_file_1(sockfd, buffer, sizeof (buffer), &sndr_addr, addr_len);
            // Check for "EXIT" message
            if (strncmp(buffer, "EXIT", 4) == 0) {
                printf("Received EXIT message from sender. Exiting...\n");
                break;
            }}
        if (bytes_received < 0) {
            fprintf(stderr, "Error: Failed to receive data\n");
            fclose(output_file);
            rudp_close(sockfd);
            return EXIT_FAILURE;
        } else if (bytes_received == 0) {
            break; // End of file or connection closed
        }

        // Check for "EXIT" message
        if (strncmp(buffer, "EXIT", 4) == 0) {
            printf("Received EXIT message from sender. Exiting...\n");
            break;
        }

        // Write received data to file
        fwrite(buffer, 1, bytes_received, output_file);

        gettimeofday(&end, NULL); // Stop measuring time
        double elapsed_time = (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_usec - start.tv_usec) / 1000.0;
        double bandwidth = ((2097152) / elapsed_time) * 1000.0 / (1024 * 1024); // MB/s
        total_time += elapsed_time;
        total_bandwidth += bandwidth;
        run_counter++;

        printf("Run #%d: Time=%.1fms; Bandwidth=%.2fMB/s\n", run_counter, elapsed_time, bandwidth);

    }

    // Close the connection
    if (rudp_recv_close(sockfd, &sndr_addr, addr_len, false) < 0) {
        perror("close failed");
        return EXIT_FAILURE;
    }

    fclose(output_file); // Close the output file

    // Calculate average time and total average bandwidth
    double average_time = total_time / run_counter;
    double average_bandwidth = total_bandwidth / run_counter;

    printf("----------------------------------\n");
    printf("- * Statistics For All Runs * -\n");
    printf("-\n");
    printf("- Average time: %.1fms\n", average_time);
    printf("- Average bandwidth: %.2fMB/s\n", average_bandwidth);
    printf("----------------------------------\n");

    printf("Receiver program finished\n");
    return EXIT_SUCCESS;
}
