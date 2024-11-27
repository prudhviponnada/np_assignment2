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
#include <netdb.h>

// Function to sending  data over UDP
int send_data(int socket_fd, struct sockaddr *server, socklen_t server_size, void *data, size_t data_size) {
    int sent_length = sendto(socket_fd, data, data_size, 0, server, server_size);
    if (sent_length < 0) {
        perror("sendto failed");
        exit(EXIT_FAILURE);
    }
    return sent_length;
}

// Function to receiving data over UDP
int receive_data(int socket_fd, struct sockaddr *server, socklen_t *server_size, void *buffer, size_t buffer_size) {
    int received_length = recvfrom(socket_fd, buffer, buffer_size, 0, server, server_size);
    if (received_length < 0 && errno != EAGAIN) {
        perror("recvfrom failed");
        exit(EXIT_FAILURE);
    }
    return received_length;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <hostname:port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int udp_socket;
    struct calcMessage request_msg;
    struct calcProtocol calc_request;
    struct calcMessage response_msg;

    char *hostname = strtok(argv[1], ":");
    char *port_str = strtok(NULL, ":");
    int port = atoi(port_str);

    printf("Resolving hostname: %s, port: %d\n", hostname, port);

    // Resolve hostname using getaddrinfo()
    struct addrinfo hints, *server_info, *addr;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;      // Support both IPv4 and IPv6
    hints.ai_socktype = SOCK_DGRAM;  // UDP

    if (getaddrinfo(hostname, port_str, &hints, &server_info) != 0) {
        perror("getaddrinfo failed");
        return EXIT_FAILURE;
    }

    // Create socket using the first valid address from getaddrinfo()
    addr = server_info;
    while (addr != NULL) {
        udp_socket = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
        if (udp_socket >= 0) {
            break;
        }
        addr = addr->ai_next;
    }

    if (addr == NULL) {
        fprintf(stderr, "Failed to create socket for any address.\n");
        freeaddrinfo(server_info);
        return EXIT_FAILURE;
    }

    // Let's start transmission part from here
    request_msg.type = htons(22);  
    request_msg.message = htonl(0);  
    request_msg.protocol = htons(17);  
    request_msg.major_version = htons(1);
    request_msg.minor_version = htons(0);

    // Send the initial message to the server
    send_data(udp_socket, addr->ai_addr, addr->ai_addrlen, &request_msg, sizeof(request_msg));

    // Set timeout for receiving the server's response
    alarm(2);

    socklen_t server_addr_len = addr->ai_addrlen;
    int recv_length = receive_data(udp_socket, addr->ai_addr, &server_addr_len, &calc_request, sizeof(calc_request));

    // Retry logic for timeout
    int retransmission_count = 0;
    while (recv_length < 0 && errno == EAGAIN && retransmission_count < 3) {
        printf("Timeout occurred. Retrying...\n");
        send_data(udp_socket, addr->ai_addr, addr->ai_addrlen, &request_msg, sizeof(request_msg));
        alarm(2);
        recv_length = receive_data(udp_socket, addr->ai_addr, &server_addr_len, &calc_request, sizeof(calc_request));
        retransmission_count++;
    }
// checking whether the receiver part is ok or not
    if (recv_length < 0) {
        fprintf(stderr, "No response from server after retransmissions.\n");
        close(udp_socket);
        freeaddrinfo(server_info);
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
        case 1:  
            calc_request.inResult = htonl(ntohl(calc_request.inValue1) + ntohl(calc_request.inValue2));
            break;
        case 2:  
            calc_request.inResult = htonl(ntohl(calc_request.inValue1) - ntohl(calc_request.inValue2));
            break;
        case 3:  
            calc_request.inResult = htonl(ntohl(calc_request.inValue1) * ntohl(calc_request.inValue2));
            break;
        case 4:  
            calc_request.inResult = htonl(ntohl(calc_request.inValue1) / ntohl(calc_request.inValue2));
            break;
        case 5:  
            calc_request.flResult = calc_request.flValue1 + calc_request.flValue2;
            break;
        case 6:  
            calc_request.flResult = calc_request.flValue1 - calc_request.flValue2;
            break;
        case 7:  
            calc_request.flResult = calc_request.flValue1 * calc_request.flValue2;
            break;
        case 8:  
            calc_request.flResult = calc_request.flValue1 / calc_request.flValue2;
            break;
        default:
            fprintf(stderr, "Unknown operation requested.\n");
            close(udp_socket);
            freeaddrinfo(server_info);
            return EXIT_FAILURE;
    }

    // Send the result back to the server
    send_data(udp_socket, addr->ai_addr, addr->ai_addrlen, &calc_request, sizeof(calc_request));

    // Receive the server's acknowledgment
    recv_length = receive_data(udp_socket, addr->ai_addr, &server_addr_len, &response_msg, sizeof(response_msg));
    if (recv_length < 0) {
        fprintf(stderr, "Failed to receive acknowledgment from server.\n");
        close(udp_socket);
        freeaddrinfo(server_info);
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
    freeaddrinfo(server_info);
    return EXIT_SUCCESS;
}
