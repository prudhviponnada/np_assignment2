#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <math.h>
#include <netinet/in.h>
#include <cerrno>
/* You will to add includes here */

// Included to get the support library
#include <calcLib.h>
#include "protocol.h"

int main(int argc, char *argv[])
{

    /* Do magic */
    int sockfd;
    struct calcMessage msg; //?
    struct sockaddr_in server_addr; //?
    struct calcProtocol protocol; //?

    char *host = strtok(argv[1], ":");   //? strtok
    char *port_no = strtok(NULL, ":");   
    int port = atoi(port_no);

    printf("Host %s, and port %d.\n", host, port);

    sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd < 0)
    {
        printf("\nError creating socket\n");
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(host);

    // Message to the server including the Type, Protocol and version
    msg.type = htons(22);
    msg.message = htonl(0);
    msg.protocol = htons(17);
    msg.major_version = htons(1);
    msg.minor_version = htons(0);

    int send_len = sendto(sockfd, &msg, sizeof(msg), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (send_len < 0)
    {
        printf("\nError sendto\n");
        exit(1);
    }

    alarm(2);  //? time out for 2 seconds.

    socklen_t addr_size;   //? socklen_t
    addr_size = sizeof(server_addr);

    int recv_len = recvfrom(sockfd, &protocol, sizeof(protocol), 0, (struct sockaddr *)&server_addr, &addr_size);  //? recvfrom
    if (recv_len < 0)
    {
        int resend = 1;
        // Resend message to the server if the message is lost
        // For 2 Retransmissions
        while (errno == EAGAIN && resend < 3)
        {
            printf("\nerrno : %d\n", errno);
            if (sendto(sockfd, &msg, sizeof(msg), 0, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
            {
                printf("\nError\n");
                exit(1);
            }
            alarm(2);

            if (recvfrom(sockfd, &protocol, sizeof(protocol), 0, (struct sockaddr *)&server_addr, &addr_size) < 0)
                continue;
            else
                break;
            resend++;
        }
    }

    printf("\nServer : type : %d\tversion : %d.%d\tid : %d\n\nJob TO DO : arith : %d\tinValue1 : %d\tinValue2 : %d\tflValue1 : %8.8g\tflValue2 : %8.8g\n", ntohs(protocol.type), ntohs(protocol.major_version), ntohs(protocol.minor_version), ntohl(protocol.id), ntohl(protocol.arith), ntohl(protocol.inValue1), ntohl(protocol.inValue2), protocol.flValue1, protocol.flValue2);

    // Operation is done based on the Server's response
    if (ntohs(msg.type) == 2 && ntohs(msg.message) == 2)
    {
        printf("\n Protocol not supported \n\n Status: NOT OK \n");
        exit(1);
    }
    else if (ntohl(protocol.arith) == 1)   //? protocol.arith
    {
        int inResult1 = ntohl(protocol.inValue1) + ntohl(protocol.inValue2);   //? ntohl , protocol.inValue 
        protocol.inResult = htonl(inResult1); 
        printf("\nClient : %d\n", inResult1);
    }

    else if (ntohl(protocol.arith) == 2) 
    {
        int inResult1 = ntohl(protocol.inValue1) - ntohl(protocol.inValue2);
        protocol.inResult = htonl(inResult1);
        printf("\nClient : %d\n", inResult1);
    }

    else if (ntohl(protocol.arith) == 3)
    {
        int inResult1 = ntohl(protocol.inValue1) * ntohl(protocol.inValue2);
        protocol.inResult = htonl(inResult1);
        printf("\nClient : %d\n", inResult1);
    }

    else if (ntohl(protocol.arith) == 4)
    {
        int inResult1 = ntohl(protocol.inValue1) / ntohl(protocol.inValue2);
        protocol.inResult = htonl(inResult1);
        printf("\nClient : %d\n", inResult1);
    }

    else if (ntohl(protocol.arith) == 5)
    {
        protocol.flResult = protocol.flValue1 + protocol.flValue2;
        printf("\nClient : %8.8g\n", protocol.flResult);
    }

    else if (ntohl(protocol.arith) == 6)
    {
        protocol.flResult = protocol.flValue1 - protocol.flValue2;
        printf("\nClient : %8.8g\n", protocol.flResult);
    }

    else if (ntohl(protocol.arith) == 7)
    {
        protocol.flResult = protocol.flValue1 * protocol.flValue2;
        printf("\nClient : %8.8g\n", protocol.flResult);
    }

    else if (ntohl(protocol.arith) == 8)
    {
        protocol.flResult = protocol.flValue1 / protocol.flValue2;
        printf("\nClient : %8.8g\n", protocol.flResult);
    }

    // The obtained result is sent to the server.
    send_len = sendto(sockfd, &protocol, sizeof(protocol), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (send_len < 0)
    {
        printf("\nError sendto\n");
        exit(1);
    }

    // Response from the server based on the result we sent.
    struct calcMessage response;
    recv_len = recvfrom(sockfd, &response, sizeof(response), 0, (struct sockaddr *)&server_addr, &addr_size);
    if (recv_len < 0)
    {
        printf("\nError recvfrom\n");
        exit(1);
    }

    printf("\nServer : type : %d\t message : %d \t protocol : %d \n", ntohs(response.type), ntohl(response.message), ntohs(response.protocol));
    if (ntohs(response.type) == 2 && ntohl(response.message) == 1 && ntohs(response.protocol) == 17)
        printf("\nStatus : OK\n\n");
    else
        printf("\nStatus : NOT OK\n\n");
    close(sockfd);
    return 0;
}
