#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <sys/syslog.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <fcntl.h>
#include "queue.h"

#define MYPORT "9000"
#define BACKLOG 10
#define MAX_DATA_SIZE 1024

struct conn_thread_args_s
{
    int clientfd;
    bool status_flag;
};

struct thread_list_s
{
    pthread_t thread_id;
    struct conn_thread_args_s conn_thread_args;
    SLIST_ENTRY(thread_list_s) entries;
};

typedef struct thread_list_s thread_list_t;

int sockfd = 0;
FILE* fptr_w = NULL;
pthread_mutex_t data_mutex;
SLIST_HEAD(slisthead, thread_list_s) head;
pthread_t ts_buffer_thread;
time_t last_timestamp = 0;
bool do_process_ts = true;
struct addrinfo *res;


void sighandler(int sig)
{
    syslog(LOG_DEBUG, "Exiting gracefully...");
    syslog(LOG_DEBUG, "Caught signal, exiting");

    if (sockfd != 0)
        close(sockfd);
   
    // Wait for all threads to join
    struct thread_list_s *thread, *tp;
    SLIST_FOREACH_SAFE(thread, &head, entries, tp) {
        printf("Joining thread %d\n", (int)(thread->thread_id));
        // Join
        while (true) {
            if (thread->conn_thread_args.status_flag == true)
            {
                if (pthread_join(thread->thread_id, NULL) != 0) {
                    syslog(LOG_DEBUG, "CRASH PANIC");
                    perror("pthread_join");
                    exit(__LINE__);
                }
                break;
            }
        }

        // Remove 
        SLIST_REMOVE(&head, thread, thread_list_s, entries);
        free(thread);
    }

    if (fptr_w) {
        fclose(fptr_w);
        remove("/var/tmp/aesdsocketdata");
    }

    pthread_mutex_lock(&data_mutex);
    pthread_mutex_unlock(&data_mutex);
    pthread_mutex_destroy(&data_mutex);

    do_process_ts = false;
    if (!pthread_join(ts_buffer_thread, NULL)) {
        perror("pthread_join");
        exit(__LINE__);
    }

    freeaddrinfo(res);  // Add this (make res global or pass it)
    closelog();         // Add this before exit

    exit(EXIT_SUCCESS);
}

void* process_ts_buffer(void* param) {
    while (do_process_ts) {
        if (last_timestamp == 0) { usleep(1000); continue; }

        char rfc2822time[128], s[128+14];
        pthread_mutex_lock(&data_mutex);

        time_t now = last_timestamp;
        struct tm *tm_info = localtime(&now);

        strftime(rfc2822time, sizeof(rfc2822time), "%a, %d %b %Y %H:%M:%S %z", tm_info);

        sprintf(s, "timestamp:%s\n", rfc2822time);

        fwrite(s, sizeof(char), strlen(s), fptr_w);
        fflush(fptr_w);

        pthread_mutex_unlock(&data_mutex);
        last_timestamp = 0;
    }

    return NULL;
}

void timer_handler(int signo) {
    if (signo != SIGALRM) {
        perror("signo");
        exit(__LINE__);
    }

    last_timestamp = time(NULL);
    syslog(LOG_DEBUG, "Writing timestamp: %d\n", last_timestamp);
    //fflush(stdout);

    /*char rfc2822time[128], s[128+14];
    pthread_mutex_lock(&data_mutex);

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);

    strftime(rfc2822time, sizeof(rfc2822time), "%a, %d %b %Y %H:%M:%S %z", tm_info);

    sprintf(s, "timestamp:%s\n", rfc2822time);

    fwrite(s, sizeof(char), strlen(s), fptr_w);
    fflush(fptr_w);

    pthread_mutex_unlock(&data_mutex);*/
}

void* process_connection(void* args)
{
    struct conn_thread_args_s *conn_thread_args = (struct conn_thread_args_s*)args;
    char *packet = NULL;
    FILE *fptr_r = fopen("/var/tmp/aesdsocketdata", "rb");
    if (fptr_r == NULL) {
        perror("fopen");
        exit(__LINE__);
    }
    printf("Entering new thread with clientfd: %d\n", conn_thread_args->clientfd);
    for (;;)
    {
        // Recieve data
        char buf[MAX_DATA_SIZE];
        int numbytes, packet_size = 0;
        bool packet_recieved = false;

        while (!packet_recieved) {
            numbytes = recv(conn_thread_args->clientfd, buf, MAX_DATA_SIZE - 1, 0);
            if (numbytes == -1) {
                perror("recv");
                exit(__LINE__);
            }

            if (numbytes == 0) {
                //pthread_mutex_lock(&data_mutex);
                syslog(LOG_DEBUG, "Connection closed by remote");
                //pthread_mutex_unlock(&data_mutex);
                break;
            }

            packet = realloc(packet, packet_size + numbytes + 1);
            if (packet == NULL) {
                perror("realloc");
                exit(__LINE__);
            }

            memcpy(packet + packet_size, buf, numbytes);

            packet_size += numbytes;

            // Check if packet is complete
            for (char *c = packet; c != packet + packet_size; c++)
            {
                if (*c == '\n') packet_recieved = true;
            }
        }

        pthread_mutex_lock(&data_mutex);

        if (numbytes > 0) {
            // Packet is recieved
            // So dump
            size_t bwritten = 0;
            bwritten = fwrite(packet, sizeof(char), packet_size, fptr_w);
            if (bwritten != packet_size) {
                perror("fwrite");
                exit(__LINE__);
            }
            fflush(fptr_w);
        }

        fseek(fptr_r, 0, SEEK_SET);
        // Transmit back
        for (;numbytes > 0;)
        {
            syslog(LOG_DEBUG, "Transmitting back...");
            size_t bread = fread(buf, sizeof(char), MAX_DATA_SIZE, fptr_r);

            // Transmit
            if ((bread > 0) && (send(conn_thread_args->clientfd, buf, bread, 0) == -1)) {
                perror("send");
                exit(__LINE__);
            }

            if (bread != MAX_DATA_SIZE) {
                syslog(LOG_DEBUG, "Transmission complete");
                break; // Tx complete
            }
        }

        pthread_mutex_unlock(&data_mutex);
        if (numbytes == 0) {
            free(packet);
            //syslog(LOG_DEBUG, "Closed connection from %s", their_addr_str);
            break;
        }
    }
    close(conn_thread_args->clientfd);
    fclose(fptr_r);
    conn_thread_args->status_flag = true; // signal completion to parent

    return NULL;
}

int main(int argc, char **argv)
{
    if (argc > 1) {
        syslog(LOG_DEBUG, "Running fork()...");
        int p = fork();
        if (p != 0) {
            syslog(LOG_DEBUG, "Goodbye!");
            exit(EXIT_SUCCESS);
        }
    }
    struct sockaddr their_addr;
    socklen_t addr_size;
    struct addrinfo hints;
    int rv;
    char their_addr_str[INET_ADDRSTRLEN];

    SLIST_INIT(&head);

    pthread_mutex_init(&data_mutex, NULL);

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
    if (signal(SIGALRM, timer_handler) == SIG_ERR)
    {
        perror("sigalrm");
        exit(__LINE__);
    }

    syslog(LOG_DEBUG, "Opening file /var/tmp/aesdsocketdata...");
    
    fptr_w = fopen("/var/tmp/aesdsocketdata", "ab+");
    if (fptr_w == NULL) {
        perror("fopen");
        exit(__LINE__);
    }

    struct itimerval tv;
    tv.it_value.tv_sec = 10;
    tv.it_value.tv_usec = 0;
    tv.it_interval.tv_sec = 10;
    tv.it_interval.tv_usec = 0;

    setitimer(ITIMER_REAL, &tv, NULL);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    syslog(LOG_DEBUG, "Running getaddrinfo()...");
    rv = getaddrinfo(NULL, MYPORT, &hints, &res);
    if (rv != 0) {
        printf("[ERROR] getaddrinfo returned error code: %d", rv);
        exit(__LINE__);
    }

    syslog(LOG_DEBUG, "Running socket()...");
    if ((sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1)
    {
        perror("socket");
        exit(__LINE__);
    }

    int yes = 1;

    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    syslog(LOG_DEBUG, "Running bind()...");
    if ((bind(sockfd, res->ai_addr, res->ai_addrlen)) == -1)
    {
        close(sockfd);
        perror("bind");
        exit(__LINE__);
    }

    

    syslog(LOG_DEBUG, "Running listen()");
    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(__LINE__);
    }
    syslog(LOG_DEBUG, "Listening sucessful.");

    // Timer
    if (pthread_create(&ts_buffer_thread, NULL, &process_ts_buffer, NULL) != 0) {
        printf("Error! panikkk");
        perror("pthread");
        exit(__LINE__);
    }

    addr_size = sizeof(their_addr);
    // Connect to client
    // syslog(LOG_DEBUG, "Connecting to client");
    // if (connect(sockfd, &their_addr, INET_ADDRSTRLEN) == -1) {
    //     perror("connect");
    //     exit(1);
    // }

    int opt = 1;
    int flags;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct thread_list_s *new_thread;
    int b = 1;
    for (;;) {
        //syslog(LOG_DEBUG, "Attempting to accept %d...", b++);
        
        flags = fcntl(sockfd, F_GETFL);
        fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

        int remote_fd = accept(sockfd, &their_addr, &addr_size);

        flags = fcntl(sockfd, F_GETFL);
        fcntl(sockfd, F_SETFL, flags & ~O_NONBLOCK);

        if (remote_fd == -1) {
            //perror("accept");
            // If accept fails, it's usually a recoverable error (e.g., EINTR), so continue to accept.
            // However, a persistent error might indicate a bigger problem.
            //sleep(1);
            continue; // Go back and try to accept again
        }

        printf("Recieved fd: %d\n", remote_fd);
        inet_ntop(AF_INET, &(((struct sockaddr_in*)&their_addr)->sin_addr), their_addr_str, INET_ADDRSTRLEN);
        syslog(LOG_DEBUG, "Accepting connection from %s", their_addr_str);

        new_thread = malloc(sizeof(*new_thread));
        if (new_thread == NULL) {
            perror("malloc");
            exit(__LINE__);
        }

        new_thread->conn_thread_args.clientfd = remote_fd;
        new_thread->conn_thread_args.status_flag = false;

        // Create a new thread
        if (pthread_create(&(new_thread->thread_id), NULL, process_connection, &(new_thread->conn_thread_args)) != 0) {
            perror("pthread_create");
            exit(__LINE__);
        }

        SLIST_INSERT_HEAD(&head, new_thread, entries);
        // Join threads that are completed
        struct thread_list_s *tp;
        SLIST_FOREACH_SAFE(new_thread, &head, entries, tp) {
            // Join
            if (new_thread->conn_thread_args.status_flag == true)
            {
                printf("Joining thread %d\n", (int)(new_thread->thread_id));
                if (pthread_join(new_thread->thread_id, NULL) != 0) {
                    perror("pthread_join");
                    exit(__LINE__);
                }
                // Remove 
                SLIST_REMOVE(&head, new_thread, thread_list_s, entries);
                free(new_thread);
            }
        }
    }

    freeaddrinfo(res);  // Add this (make res global or pass it)
    closelog();         // Add this before exit
    return 0;
}
