#include "account.h"
#include <time.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <netdb.h>
#include <sys/wait.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>

#define BUFF_SIZE 4096
#define MAXLEN 10000
#define PREFIX_LEN 10
#define CODE_LEN 3


typedef struct Session {
    int socket;
    struct sockaddr_in client_addr;
    Account* currentAccount;
} Session;

extern Node* root;
const char* prefix[3] = {"REGISTER ","LOGOUT", "LOGIN "
                        };


void putLog(int result, char input[], struct sockaddr_in client_addr) {
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    char buffer[100];
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, sizeof(client_ip));
    int client_port = ntohs(client_addr.sin_port);
    strftime(buffer, sizeof(buffer), "[%d/%m/%Y %H:%M:%S]", t);
    printf("%s $ %s:%d $ %s $ %d\n", buffer, client_ip, client_port, input, result);
}

/**
 * @brief Log in to an account by username.
 *
 * @return 0 if login succeeds,
 *         1 if already logged in,
 *         2 if account is banned,
 *         3 if account not found.
 */
int logIn(char *username, char *password, Session* currentSession) {
    int bytes_sent;
    if (currentSession->currentAccount != NULL) {
        bytes_sent = send(currentSession->socket, "213\r\n", strlen("213\r\n"), 0);
        if (bytes_sent < 0) {
            perror("send() error: ");
            return -1;
        }
        return 213;
    }
    
    pthread_mutex_lock(&mutexVar.lock);
    while (mutexVar.ready == false) {
        pthread_cond_wait(&mutexVar.cond, &mutexVar.lock);
    }
    mutexVar.ready = false;
    Node* result = find(root, username);



    int res;

    if (result != NULL) {
          
            if(result->account.isLoggedIn == false){
                if(strcmp(result->account.password, password) == 0){
                    result->account.isLoggedIn = true;
                    currentSession->currentAccount = &result->account;
                    bytes_sent = send(currentSession->socket, "111\r\n", strlen("111\r\n"), 0);
                    res = 111;
    
                }
                else{      
                    bytes_sent = send(currentSession->socket, "215\r\n", strlen("215\r\n"), 0);
                    res = 215;
    
                }
            }
            else {
                bytes_sent = send(currentSession->socket, "213\r\n", strlen("213\r\n"), 0);
                res = 213;
            }

         
        
    }
    else{
        bytes_sent = send(currentSession->socket, "215\r\n", strlen("215\r\n"), 0);
                
        res = 215;
    }
    mutexVar.ready = true;
    pthread_cond_signal(&mutexVar.cond);
    pthread_mutex_unlock(&mutexVar.lock);
    if (bytes_sent < 0) {
                    perror("send() error: ");
                    res = -1;
                }
    
    
    return res;

}


/**
 * @brief Log out of the current account.
 *
 * @return 0 if logout succeeds,
 *         1 if no user is logged in.
 */
int logOut(Session* currentSession) {
    int bytes_sent;
    if (currentSession->currentAccount == NULL) {
        bytes_sent = send(currentSession->socket, "221\r\n", strlen("221\r\n"), 0);
            if (bytes_sent < 0) {
                perror("send() error: ");
                return -1;
            }
        return 214;
    }
    pthread_mutex_lock(&mutexVar.lock);
    while (mutexVar.ready == false) {
        pthread_cond_wait(&mutexVar.cond, &mutexVar.lock);
    }
    mutexVar.ready = false;

    currentSession->currentAccount->isLoggedIn = false;
    currentSession->currentAccount = NULL;
    mutexVar.ready = true;
    pthread_cond_signal(&mutexVar.cond);
    pthread_mutex_unlock(&mutexVar.lock);


    bytes_sent = send(currentSession->socket, "112\r\n", strlen("112\r\n"), 0);
            if (bytes_sent < 0) {
                perror("send() error: ");
                return -1;
            }
    
    return 112;
}


int signUp(char *username, char *password, Session *currentSession) {
    int bytes_sent, res;
    if (currentSession->currentAccount != NULL) {
        bytes_sent = send(currentSession->socket, "213\r\n", strlen("213\r\n"), 0);
        if (bytes_sent < 0) {
            perror("send() error: ");
            return -1;
        }
        return 213;
    }

    pthread_mutex_lock(&mutexVar.lock);
    while (mutexVar.ready == false) {
        pthread_cond_wait(&mutexVar.cond, &mutexVar.lock);
    }
    mutexVar.ready = false;

    Node* result = find(root, username);
    if (result != NULL) {
        bytes_sent = send(currentSession->socket, "212\r\n", strlen("212\r\n"), 0);
        res = 212;
    }

    root = insert(root, username, password, 100); // Default status is active
    bytes_sent = send(currentSession->socket, "110\r\n", strlen("110\r\n"), 0);
    res = 110;

    mutexVar.ready = true;
    pthread_cond_signal(&mutexVar.cond);
    pthread_mutex_unlock(&mutexVar.lock);
    if (bytes_sent < 0) {
        perror("send() error: ");
        res = -1;
    }
    return res;
}




int process_request(char process_buffer[], Session *currentSession) {
    int res = 300;
    process_buffer[strcspn(process_buffer, "\r\n")] = '\0';
    char *tmp;
    char *cmd = strtok_r(process_buffer, " ", &tmp);
    if(strcmp(cmd, "LOGIN") == 0) {
        char *username = strtok_r(NULL, " ", &tmp);
        char *password = strtok_r(NULL, " ", &tmp);
        res = logIn(username, password, currentSession);
        putLog(res, process_buffer, currentSession->client_addr);
    } else if (strcmp(cmd, "LOGOUT") == 0) {
        res = logOut(currentSession);
        putLog(res, process_buffer, currentSession->client_addr);
    } else if (strcmp(cmd, "REGISTER") == 0) {
        char *username = strtok_r(NULL, " ", &tmp);
        char *password = strtok_r(NULL, " ", &tmp);
        res = signUp(username, password, currentSession);
        putLog(res, process_buffer, currentSession->client_addr);
    } else {
        // Unknown command
        int bytes_sent = send(currentSession->socket, "300\r\n", strlen("300\r\n"), 0);
        if (bytes_sent < 0) {
            perror("send() error: ");
            return -1;
        }
        putLog(300, process_buffer, currentSession->client_addr);
    }
    return 0;
}

void *receive_request(void *arg) {
    Session currentSession = *(Session *)arg;
    free(arg); // free allocated memory from main thread
    char process_buffer[PREFIX_LEN + MAXLEN + 2];
    char temp_buffer[PREFIX_LEN + MAXLEN + 2];
    ssize_t received_bytes;
    char msg_buff[BUFF_SIZE];
    char *delimiter;

    memset(msg_buff, 0, sizeof(msg_buff));
    memset(process_buffer, 0, sizeof(process_buffer));
    memset(temp_buffer, 0, sizeof(temp_buffer));
    while (1) {
        do {
            received_bytes = recv(currentSession.socket, msg_buff, sizeof(msg_buff) - 1, 0);
            if (received_bytes < 0) {
                if (errno == EINTR) continue;
                perror("recv() error");
                close(currentSession.socket);
                return NULL;
            } else if (received_bytes == 0) {
                printf("Connection closed by client.\n");
                close(currentSession.socket);
                return NULL;
            }

            msg_buff[received_bytes] = '\0';
            strncat(process_buffer, msg_buff,
                    sizeof(process_buffer) - strlen(process_buffer) - 1);

        } while ((delimiter = strstr(process_buffer, "\r\n")) == NULL);
        
        while ((delimiter = strstr(process_buffer, "\r\n")) != NULL) {
            *delimiter = '\0';  
            process_request(process_buffer, &currentSession);
            strcpy(temp_buffer, delimiter + 2);
            strcpy(process_buffer, temp_buffer);
        }
       
        memset(msg_buff, 0, sizeof(msg_buff));
        memset(process_buffer, 0, sizeof(process_buffer));
        memset(temp_buffer, 0, sizeof(temp_buffer));
    }
    
    close(currentSession.socket);
   

    
}