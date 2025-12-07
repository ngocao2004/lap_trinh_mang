
#include "process.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <netdb.h>
#include <sys/wait.h>
#include <errno.h>
#include <pthread.h>

#define BACKLOG 20

extern Node* root;
/*
 * Receive and echo message to client
 * [IN] sockfd: socket descriptor that connects to client
 */


int main(int argc, char** argv) {
    if(argc != 2) {
        printf("Usage: %s <port number>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    const int PORT = atoi(argv[1]);
    if(initList("account.txt") == -1) {
        perror("Data loading error: ");
        exit(EXIT_FAILURE);
    }
    int listen_sock, conn_sock; /* file descriptors */
    struct sockaddr_in server_addr; /* server's address information */
    struct sockaddr_in client_addr; /* client's address information */
    pthread_t tid;
    int sin_size;

    if ((listen_sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket() error: ");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); /* INADDR_ANY puts your IP address automatically */

    if (bind(listen_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind() error: ");
        exit(EXIT_FAILURE);
    }

    if (listen(listen_sock, BACKLOG) == -1) {
        perror("listen() error: ");
        exit(EXIT_FAILURE);
    }

  

    printf("\nServer started at port number %d!\n", PORT);

    while (1) {
        sin_size = sizeof(struct sockaddr_in);
        if ((conn_sock = accept(listen_sock, (struct sockaddr*)&client_addr, &sin_size)) == -1) {
            if (errno == EINTR)
                continue;
            else {
                perror("accept() error: ");
                exit(EXIT_FAILURE);
            }
        }

        Session* args = malloc(sizeof(Session));
        args->socket = conn_sock;
        args->client_addr = client_addr;
        args->currentAccount = NULL;
        printf("New connection from %s:%d\n", inet_ntoa(args->client_addr.sin_addr), ntohs(args->client_addr.sin_port));
        send(args->socket, "100\r\n", strlen("100\r\n"), 0);
        pthread_create(&tid, NULL, receive_request, args);
        pthread_detach(tid);



    }

    close(listen_sock);
    return 0;
}



