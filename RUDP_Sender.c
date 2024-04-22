#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>

#include "RUDP_API.h" // Include the RUDP API header file
#define DEFAULT_IP "127.0.0.1" // Receiver's IP address
#define DEFAULT_PORT 4567 // Port number of the receiver

/*
* @brief A random data generator function based on srand() and rand().
* @param size The size of the data to generate (up to 2^32 bytes).
* @return A pointer to the buffer.
*/
char *util_generate_random_data(unsigned int size) {
    char *buffer = NULL;
    // Argument check.
    if (size == 0)
        return NULL;
    buffer = (char *) calloc(size, sizeof(char));
    // Error checking.
    if (buffer == NULL)
        return NULL;
    // Randomize the seed of the random number generator.
    srand(time(NULL));
    for (unsigned int i = 0; i < size; i++)
        *(buffer + i) = ((unsigned int) rand() % 256);

    return buffer;
}

int main(int argc, char *argv[]) {

    // Check the number of command-line arguments
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <IP> <PORT>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *receiver_ip = DEFAULT_IP;
    unsigned short receiver_port = DEFAULT_PORT;

    // Parse command-line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-ip") == 0 && i + 1 < argc) {
            receiver_ip = argv[i + 1];
            i++;
        } else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            receiver_port = atoi(argv[i + 1]);
            i++;
        } else {
            fprintf(stderr, "Usage: %s -ip <IP> -p <port>\n", argv[0]);
            return EXIT_FAILURE;
        }
    }


    // Validate that both IP and port are provided
    if (receiver_ip == NULL || receiver_port == 0) {
        fprintf(stderr, "Error: IP address and port number are required\n");
        return EXIT_FAILURE;
    }

    // Convert receiver IP address to binary format
    struct sockaddr_in receiver_addr;
    memset(&receiver_addr, 0, sizeof(receiver_addr));
    receiver_addr.sin_family = AF_INET;
    if (inet_pton(AF_INET, receiver_ip, &receiver_addr.sin_addr) <= 0) {
        perror("inet_pton");
        return EXIT_FAILURE;
    }
    receiver_addr.sin_port = htons(receiver_port);


    // Read the created file
    char *file_data = util_generate_random_data(2097152); // Generate random data for a 2MB file


    // Create a UDP socket between the Sender and the Receiver
    RUDP_Socket *sender_socket = rudp_socket(false, receiver_port); // Create a client socket
    if (sender_socket == NULL) {
        fprintf(stderr, "Error: Failed to create UDP socket\n");
        return EXIT_FAILURE;
    }
    printf("sender socket created\n");

    // Connect to the Receiver
    int connect_status = rudp_connect(sender_socket, &receiver_addr, sizeof(receiver_addr), receiver_ip , receiver_port);
    if (connect_status == 0) {
        fprintf(stderr, "Error: Failed to connect to the receiver\n");
        rudp_close(sender_socket);
        return EXIT_FAILURE;
    } else {
        printf("Connected to the Receiver\n");
    }

    // Send the file via the RUDP protocol
    int chunk_size = 2097152 / 50;
    for (int i = 0; i < 50; ++i) {
        int offset = i * chunk_size;
        int bytes_sent = rudp_send_file_1(sender_socket, file_data + offset, chunk_size, receiver_ip , receiver_port);
        if (bytes_sent < 0) {
            fprintf(stderr, "Error: Failed to send file\n");
            free(file_data);
            rudp_close(sender_socket);
            return EXIT_FAILURE;
        }
    }



    // User decision: Send the file again?
    char choice;
    do {
        printf("Do you want to send the file again? (y/n): ");
        scanf(" %c", &choice);
        if (choice == 'n') {
            break;
        } else if (choice == 'y') {
            // Resend the file
            for (int i = 0; i < 50; ++i) {
                int offset = i * chunk_size;
                int bytes_sent = rudp_send_file_1(sender_socket, file_data + offset, chunk_size, receiver_ip , receiver_port);
                if (bytes_sent < 0) {
                    fprintf(stderr, "Error: Failed to send file\n");
                    free(file_data);
                    rudp_close(sender_socket);
                    return EXIT_FAILURE;
                }
            }
        }
    } while (choice == 'y');

    // Send an exit message to the receiver
    if (rudp_send_file_1(sender_socket, "EXIT", strlen("EXIT"), receiver_ip , receiver_port) < 0) {
        perror("Failed sending EXIT\n");
        return EXIT_FAILURE;
    }


    printf("exit sent\n");

    // Close the connection
    if (rudp_close(sender_socket) < 0) {
        perror("close failed in sender file\n");
        return EXIT_FAILURE;
    }

    // Free the allocated memory for file data
    free(file_data);

    printf("Sender program finished\n");
    return EXIT_SUCCESS;
}
