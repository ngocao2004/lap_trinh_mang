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
    char userName[100];
} Session;

#define MAX_SESSIONS 100
Session* sessionTable[MAX_SESSIONS];
int sessionCount = 0;
pthread_mutex_t sessionTableMutex = PTHREAD_MUTEX_INITIALIZER;

extern Node* root;
const char* prefix[5] = {"REGISTER ","LOGOUT", "LOGIN ","GET_READY_LIST","CHALLENGE "
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


bool addSessionToTable(Session* session) {

    pthread_mutex_lock(&sessionTableMutex);
    if (sessionCount >= MAX_SESSIONS) {
        pthread_mutex_unlock(&sessionTableMutex);
        return false; 
    }
    sessionTable[sessionCount++] = session;
    pthread_mutex_unlock(&sessionTableMutex);
    return true;
}

bool removeSessionFromTable(Session* session) {
    pthread_mutex_lock(&sessionTableMutex);
    int found = -1;
    for (int i = 0; i < sessionCount; i++) {
        if (sessionTable[i] == session) {
            found = i;
            break;
        }
    }
    if (found == -1) {
        pthread_mutex_unlock(&sessionTableMutex);
        return false; // not found
    }
    // Shift remaining sessions
    for (int i = found; i < sessionCount - 1; i++) {
        sessionTable[i] = sessionTable[i + 1];
    }
    sessionTable[sessionCount - 1] = NULL;
    sessionCount--;
    pthread_mutex_unlock(&sessionTableMutex);

    return true;
}

int getSocketByUsername(const char* username) {
    int sock = -1;
    pthread_mutex_lock(&sessionTableMutex);
    for (int i = 0; i < sessionCount; i++) {
        if (strcmp(sessionTable[i]->userName, username) == 0) {
            sock = sessionTable[i]->socket;
            break;
        }
    }
    pthread_mutex_unlock(&sessionTableMutex);
    return sock;
}

bool sendChallengeNotification(const char* opponent, const char* fromUser) {
    int sock = getSocketByUsername(opponent);
    if (sock == -1) {
        printf("[sendChallengeNotification] opponent '%s' not found in sessionTable\n", opponent);
        return false;
    }

    char msg[256];
    snprintf(msg, sizeof(msg), "140 %s\r\n", fromUser);
    ssize_t sent = send(sock, msg, strlen(msg), 0);
    if (sent < 0) {
        perror("send() error in sendChallengeNotification");
        return false;
    }
    printf("[sendChallengeNotification] forwarded challenge from '%s' to '%s' (sock=%d), bytes=%zd\n", fromUser, opponent, sock, sent);
    return true;
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
                    // store username in session and add to session table
                    strncpy(currentSession->userName, username, sizeof(currentSession->userName) - 1);
                    currentSession->userName[sizeof(currentSession->userName) - 1] = '\0';
                    addSessionToTable(currentSession);
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
    // remove from session table when logging out
    removeSessionFromTable(currentSession);
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


int getReadyList(Session* currentSession)
{
    if (!currentSession->currentAccount) {
        send(currentSession->socket, "214\r\n", 5, 0);
        return 214;
    }

    pthread_mutex_lock(&mutexVar.lock);

    char listBuffer[4096] = {0};
    int count = collectReadyUsers(root,
                                  listBuffer,
                                  sizeof(listBuffer),
                                  currentSession->currentAccount);

    pthread_mutex_unlock(&mutexVar.lock);

    char response[4096+100];
    int headerLen = snprintf(response, sizeof(response), "120 %d\r\n", count);

    if (headerLen < 0 || headerLen >= sizeof(response)) {
        send(currentSession->socket, "500 Internal error\r\n", 20, 0);
        return 500;
    }

    size_t totalLen = headerLen;

    if (count > 0) {
        int n = snprintf(response + headerLen, 
                         sizeof(response) - headerLen,
                         "%s", 
                         listBuffer);
        
        if (n > 0 && n < (int)(sizeof(response) - headerLen)) {
            totalLen += n;
        } else {
            printf("Warning: Response truncated\n");
            totalLen = sizeof(response) - 1;
            response[totalLen] = '\0';
        }
    }

    ssize_t sent = send(currentSession->socket, response, totalLen, 0);
    
    if (sent < 0) {
        perror("send failed");
        return -1;
    } else if (sent < (ssize_t)totalLen) {
        printf("Warning: Only sent %zd/%zu bytes\n", sent, totalLen);
    }

    return 120;
}



int handleChallenge(Session* currentSession, char* opponentName) {
    if (!currentSession->currentAccount || !currentSession->currentAccount->isLoggedIn) {
        send(currentSession->socket, "214\r\n", 5, 0);
        return 214; 
    }
    if (strcmp(currentSession->currentAccount->userName, opponentName) == 0) {
        send(currentSession->socket, "231\r\n", 5, 0); 
        return 231;
    }

    pthread_mutex_lock(&mutexVar.lock);

    Node* targetNode = find(root, opponentName);
    if (!targetNode || !targetNode->account.isLoggedIn) {
        pthread_mutex_unlock(&mutexVar.lock);
        send(currentSession->socket, "230\r\n", 5, 0);
        return 230;
    }

    if (targetNode->account.isWaiting) {
        pthread_mutex_unlock(&mutexVar.lock);
        send(currentSession->socket, "231\r\n", 5, 0);
        return 231;
    }

    if (abs(currentSession->currentAccount->score - targetNode->account.score) > 10) {
        pthread_mutex_unlock(&mutexVar.lock);
        send(currentSession->socket, "232\r\n", 5, 0);
        return 232;
    }

    targetNode->account.isWaiting = true;
    targetNode->account.challengedBy = currentSession->currentAccount;
    currentSession->currentAccount->challenging = &targetNode->account;

    pthread_mutex_unlock(&mutexVar.lock);

    send(currentSession->socket, "130\r\n", 5, 0);
    sendChallengeNotification(opponentName,
                            currentSession->currentAccount->userName);

    return 130;

}


void handle_challenge_resp(Session* session, char* message) {
    char cmd[32], opponentName[32], action[16];

    int count = sscanf(message, "%s %s %s", cmd, opponentName, action);
    if (count != 3 || strcmp(cmd, "CHALLENGE_RESP") != 0) {
        send(session->socket, "300\r\n", 5, 0);
        return;
    }

    Account *me = session->currentAccount;
    if (!me || !me->isLoggedIn) {
        send(session->socket, "214\r\n", 5, 0);
        return;
    }

    pthread_mutex_lock(&mutexVar.lock);

    Node *oppNode = find(root, opponentName);
    if (!oppNode) {
        pthread_mutex_unlock(&mutexVar.lock);
        send(session->socket, "233\r\n", 5, 0);
        return;
    }

    Account *opp = &oppNode->account;

    if (me->challengedBy != opp) {
        pthread_mutex_unlock(&mutexVar.lock);
        send(session->socket, "233\r\n", 5, 0);
        return;
    }

    int oppSock = getSocketByUsername(opp->userName);

    // ===== ACCEPT =====
    if (strcmp(action, "ACCEPT") == 0) {
        me->challengedBy = NULL;
        opp->challenging = NULL;
        me->isWaiting = false;

        pthread_mutex_unlock(&mutexVar.lock);

        send(session->socket, "131\r\n", 5, 0);
        if (oppSock != -1)
            send(oppSock, "131\r\n", 5, 0);

        return;
    }

    // ===== REJECT =====
    if (strcmp(action, "REJECT") == 0) {
        me->challengedBy = NULL;
        opp->challenging = NULL;
        me->isWaiting = false;

        pthread_mutex_unlock(&mutexVar.lock);

        send(session->socket, "132\r\n", 5, 0);
        if (oppSock != -1)
            send(oppSock, "132\r\n", 5, 0);

        return;
    }

    pthread_mutex_unlock(&mutexVar.lock);
    send(session->socket, "300\r\n", 5, 0);
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
    } else if (strcmp(cmd, "GET_READY_LIST") == 0) {
        res = getReadyList(currentSession);
        putLog(res, process_buffer, currentSession->client_addr);
    } else if (strcmp(cmd, "CHALLENGE") == 0) {
        char *opponentName = strtok_r(NULL, " ", &tmp);
        if (!opponentName) {
            const char *err = "300\r\n";
            send(currentSession->socket, err, strlen(err), 0);
            putLog(300, process_buffer, currentSession->client_addr);
            res = 300;
        } else {
            res = handleChallenge(currentSession, opponentName);
            putLog(res, process_buffer, currentSession->client_addr);
        }
    } else if (strcmp(cmd, "CHALLENGE_RESP") == 0) {
        handle_challenge_resp(currentSession, process_buffer);
        return 0;   
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
    Session *currentSession = (Session *)arg;
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
            received_bytes = recv(currentSession->socket, msg_buff, sizeof(msg_buff) - 1, 0);
            if (received_bytes < 0) {
                if (errno == EINTR) continue;
                perror("recv() error");
                // cleanup on error
                if (currentSession->currentAccount) {
                    currentSession->currentAccount->isLoggedIn = false;
                    removeSessionFromTable(currentSession);
                }
                close(currentSession->socket);
                free(currentSession);
                return NULL;
            } else if (received_bytes == 0) {
                // connection closed by client
                if (currentSession->currentAccount) {
                    currentSession->currentAccount->isLoggedIn = false;
                    removeSessionFromTable(currentSession);
                }
                printf("Connection closed by client.\n");
                close(currentSession->socket);
                free(currentSession);
                return NULL;
            }

            msg_buff[received_bytes] = '\0';
            strcat(process_buffer, msg_buff);

        } while ((delimiter = strstr(process_buffer, "\r\n")) == NULL);
        
        while ((delimiter = strstr(process_buffer, "\r\n")) != NULL) {
            *delimiter = '\0';  
            process_request(process_buffer, currentSession);
            strcpy(temp_buffer, delimiter + 2);
            strcpy(process_buffer, temp_buffer);
        }
       
        memset(msg_buff, 0, sizeof(msg_buff));
        memset(process_buffer, 0, sizeof(process_buffer));
        memset(temp_buffer, 0, sizeof(temp_buffer));
    }
    
    // should not reach here, but clean up just in case
    if (currentSession->currentAccount) {
        currentSession->currentAccount->isLoggedIn = false;
        removeSessionFromTable(currentSession);
    }
    close(currentSession->socket);
    free(currentSession);
    return NULL;
}