#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <math.h>
#include <errno.h>
#include <stdint.h>
#include <calcLib.h>
#include "protocol.h"

// Function to send data over UDP
int send_data(int socket_fd, struct sockaddr_in *server, socklen_t server_size, void *data, size_t data_size) {
    int sent_length = sendto(socket_fd, data, data_size, 0, (struct sockaddr *)server, server_size);
    if (sent_length < 0) {
        perror("sendto failed");
        exit(EXIT_FAILURE);
    }
    return sent_length;
}

// Function to receive data over UDP
int receive_data(int socket_fd, struct sockaddr_in *server, socklen_t *server_size, void *buffer, size_t buffer_size) {
    int received_length = recvfrom(socket_fd, buffer, buffer_size, 0, (struct sockaddr *)server, server_size);
    if (received_length < 0 && errno != EAGAIN) {
        perror("recvfrom failed");
        exit(EXIT_FAILURE);
    }
    return received_length;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <server_ip:port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int udp_socket;
    struct calcMessage request_msg;
    struct sockaddr_in server_address;
    struct calcProtocol calc_request;
    struct calcMessage response_msg;

    char *server_ip = strtok(argv[1], ":");
    char *server_port_str = strtok(NULL, ":");
    int server_port = atoi(server_port_str);

    printf("Connecting to server %s on port %d\n", server_ip, server_port);

    udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udp_socket < 0) {
        perror("Socket creation failed");
        return EXIT_FAILURE;
    }

    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(server_port);
    server_address.sin_addr.s_addr = inet_addr(server_ip);

    // Initialize the request message
    request_msg.type = htons(22);  // Client-to-server binary protocol
    request_msg.message = htonl(0);  // NA
    request_msg.protocol = htons(17);  // UDP
    request_msg.major_version = htons(1);
    request_msg.minor_version = htons(0);

    // Send the initial message to the server
    send_data(udp_socket, &server_address, sizeof(server_address), &request_msg, sizeof(request_msg));

    // Set timeout for receiving the server's response
    alarm(2);

    socklen_t server_address_size = sizeof(server_address);
    int recv_length = receive_data(udp_socket, &server_address, &server_address_size, &calc_request, sizeof(calc_request));

    // Retry logic for timeout
    int retransmission_count = 0;
    while (recv_length < 0 && errno == EAGAIN && retransmission_count < 2) {
        printf("Timeout occurred. Retrying...\n");
        send_data(udp_socket, &server_address, sizeof(server_address), &request_msg, sizeof(request_msg));
        alarm(2);
        recv_length = receive_data(udp_socket, &server_address, &server_address_size, &calc_request, sizeof(calc_request));
        retransmission_count++;
    }

    if (recv_length < 0) {
        fprintf(stderr, "No response from server after retransmissions.\n");
        close(udp_socket);
        return EXIT_FAILURE;
    }

    printf("Server assigned task:\n");
    printf("Type: %d, Version: %d.%d, ID: %d\n",
           ntohs(calc_request.type),
           ntohs(calc_request.major_version),
           ntohs(calc_request.minor_version),
           ntohl(calc_request.id));
    printf("Operation: %d, Values: %d, %d, %g, %g\n",
           ntohl(calc_request.arith),
           ntohl(calc_request.inValue1),
           ntohl(calc_request.inValue2),
           calc_request.flValue1,
           calc_request.flValue2);

    // Perform the calculation
    switch (ntohl(calc_request.arith)) {
        case 1:  // Add integers
            calc_request.inResult = htonl(ntohl(calc_request.inValue1) + ntohl(calc_request.inValue2));
            break;
        case 2:  // Subtract integers
            calc_request.inResult = htonl(ntohl(calc_request.inValue1) - ntohl(calc_request.inValue2));
            break;
        case 3:  // Multiply integers
            calc_request.inResult = htonl(ntohl(calc_request.inValue1) * ntohl(calc_request.inValue2));
            break;
        case 4:  // Divide integers
            calc_request.inResult = htonl(ntohl(calc_request.inValue1) / ntohl(calc_request.inValue2));
            break;
        case 5:  // Add floats
            calc_request.flResult = calc_request.flValue1 + calc_request.flValue2;
            break;
        case 6:  // Subtract floats
            calc_request.flResult = calc_request.flValue1 - calc_request.flValue2;
            break;
        case 7:  // Multiply floats
            calc_request.flResult = calc_request.flValue1 * calc_request.flValue2;
            break;
        case 8:  // Divide floats
            calc_request.flResult = calc_request.flValue1 / calc_request.flValue2;
            break;
        default:
            fprintf(stderr, "Unknown operation requested.\n");
            close(udp_socket);
            return EXIT_FAILURE;
    }

    // Send the result back to the server
    send_data(udp_socket, &server_address, sizeof(server_address), &calc_request, sizeof(calc_request));

    // Receive the server's acknowledgment
    recv_length = receive_data(udp_socket, &server_address, &server_address_size, &response_msg, sizeof(response_msg));
    if (recv_length < 0) {
        fprintf(stderr, "Failed to receive acknowledgment from server.\n");
        close(udp_socket);
        return EXIT_FAILURE;
    }

    printf("Server response:\n");
    printf("Type: %d, Message: %d, Protocol: %d\n",
           ntohs(response_msg.type),
           ntohl(response_msg.message),
           ntohs(response_msg.protocol));

    if (ntohs(response_msg.type) == 2 && ntohl(response_msg.message) == 1) {
        printf("Status: OK\n");
    } else {
        printf("Status: NOT OK\n");
    }

    close(udp_socket);
    return EXIT_SUCCESS;
}
