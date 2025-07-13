#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <sys/syslog.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>

#define MYPORT "9000"
#define BACKLOG 10
#define MAX_DATA_SIZE 1024

int sockfd = 0, remote_fd = 0;
FILE *fptr = NULL, *fptr2 = NULL;

void sighandler(int sig)
{
    puts("Exiting gracefully...");
    syslog(LOG_DEBUG, "Caught signal, exiting");

    if (sockfd != 0)
        close(sockfd);
    if (remote_fd != 0)
        close(remote_fd);
    if (fptr) {
        fclose(fptr);
        remove("/var/tmp/aesdsocketdata");
    }
    if (fptr2)
        fclose(fptr2);

    exit(EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
    struct sockaddr their_addr;
    socklen_t addr_size;
    struct addrinfo hints, *res;
    int rv;
    char their_addr_str[INET_ADDRSTRLEN];

    if (signal(SIGINT, sighandler) == SIG_ERR)
    {
        perror("sigint");
        exit(__LINE__);
    }
    if (signal(SIGTERM, sighandler) == SIG_ERR)
    {
        perror("sigterm");
        exit(__LINE__);
    }

    puts("Opening file /var/tmp/aesdsocketdata...");
    fptr = fopen("/var/tmp/aesdsocketdata", "ab+");
    fptr2 = fopen("/var/tmp/aesdsocketdata", "rb");

    if (fptr == NULL) {
        perror("fopen");
        exit(__LINE__);
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    puts("Running getaddrinfo()...");
    rv = getaddrinfo(NULL, MYPORT, &hints, &res);
    if (rv != 0) {
        printf("[ERROR] getaddrinfo returned error code: %d", rv);
        exit(__LINE__);
    }

    puts("Running socket()...");
    if ((sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1)
    {
        perror("socket");
        exit(__LINE__);
    }

    int yes = 1;

    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    puts("Running bind()...");
    if ((bind(sockfd, res->ai_addr, res->ai_addrlen)) == -1)
    {
        close(sockfd);
        perror("bind");
        exit(__LINE__);
    }

    if (argc > 1) {
        puts("Running fork()...");
        int p = fork();
        if (p != 0) {
            puts("Goodbye!");
            exit(EXIT_SUCCESS);
        }
    }

    puts("Running listen()");
    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(__LINE__);
    }

    addr_size = sizeof(their_addr);
    // Connect to client
    // puts("Connecting to client");
    // if (connect(sockfd, &their_addr, INET_ADDRSTRLEN) == -1) {
    //     perror("connect");
    //     exit(1);
    // }

    

    for (;;) {
        remote_fd = accept(sockfd, &their_addr, &addr_size);

        inet_ntop(AF_INET, &(((struct sockaddr_in*)&their_addr)->sin_addr), their_addr_str, INET_ADDRSTRLEN);
        syslog(LOG_DEBUG, "Accepting connection from %s", their_addr_str);

        for (;;)
        {
            // Recieve data
            char buf[MAX_DATA_SIZE], *packet = malloc(MAX_DATA_SIZE);
            int numbytes, packet_size = 0;
            bool packet_recieved = false;

            while (!packet_recieved) {
                numbytes = recv(remote_fd, buf, MAX_DATA_SIZE - 1, 0);
                if (numbytes == -1) {
                    perror("recv");
                    exit(__LINE__);
                }
                else {
                    printf("Recieved %d bytes from %s\n", numbytes, their_addr_str);
                }

                if (numbytes == 0) {
                    puts("Connection closed by remote");
                    break;
                }

                buf[numbytes + 1] = '\0';

                packet = realloc(packet, packet_size + numbytes);
                if (packet == NULL) {
            fprintf(stderr, "realloc failed with errno: %d\n", errno); // Add this line
                    perror("realloc");
                    exit(__LINE__);
                }

                strcpy(packet + packet_size, buf);

                packet_size += numbytes;

                // Check if packet is complete
                for (char *c = packet; *c; c++)
                {
                    if (*c == '\n') packet_recieved = true;
                }
            }

            if (numbytes > 0) {
                // Packet is recieved
                // So dump
                size_t bwritten = 0;
                bwritten = fwrite(packet, sizeof(char), packet_size, fptr);
                if (bwritten != packet_size) {
                    perror("fwrite");
                    exit(__LINE__);
                }
                fflush(fptr);
            }

            fseek(fptr2, 0, SEEK_SET);
            // Transmit back
            for (;;)
            {
                size_t bread = fread(buf, sizeof(char), MAX_DATA_SIZE, fptr2);

                // Transmit
                if (send(remote_fd, buf, bread, 0) == -1) {
                    perror("send");
                    exit(__LINE__);
                } else {
                    printf("Sent %d data bytes to %s", bread, their_addr_str);
                }

                if (bread != MAX_DATA_SIZE) break; // Tx complete
            }

            if (numbytes == 0) {
                free(packet);
                syslog(LOG_DEBUG, "Closed connection from %s", their_addr_str);
                break;
            }
        }
    }
    close(remote_fd);
    close(sockfd);
    fclose(fptr);
    fclose(fptr2);

    return 0;
}
