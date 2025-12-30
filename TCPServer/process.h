
#ifndef PROCESS_H
#define PROCESS_H

#include "session.h"
#include <time.h>
#include <stdbool.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <netdb.h>
#include <sys/wait.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include "match.h"


#define BUFF_SIZE 4096
#define MAXLEN 10000
#define PREFIX_LEN 10
#define CODE_LEN 3



extern Node* root;
extern MutexVar mutexVar;
extern Session *sessionTable[MAX_SESSIONS];

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

int sendMessage(int sockfd, const char *message)
{
    size_t total = strlen(message);
    size_t sent = 0;

    while (sent < total) {
        ssize_t n = send(sockfd,
                         message + sent,
                         total - sent,
                         0);

        if (n < 0) {
            if (errno == EINTR)
                continue; 

            perror("send");
            return -1;
        }

        if (n == 0) {
          
            return -1;
        }

        sent += n;
    }

    return (int)sent;
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
    int counter = 0;
    if (currentSession->currentAccount != NULL) {
        bytes_sent = sendMessage(currentSession->socket, "213\r\n");
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
          
            if(result->account.status == OFFLINE){
                if(strcmp(result->account.password, password) == 0){
                    result->account.status = ONLINE;
                    currentSession->currentAccount = &result->account;
                    bytes_sent = sendMessage(currentSession->socket, "111\r\n");
                    res = 111;
    
                }
                else{      
                    bytes_sent = sendMessage(currentSession->socket, "215\r\n");
                    res = 215;
    
                }
            }
            else {
                bytes_sent = sendMessage(currentSession->socket, "213\r\n");
                res = 213;
            }

         
        
    }
    else{
        bytes_sent = sendMessage(currentSession->socket, "215\r\n");
        res = 215;
    }
    mutexVar.ready = true;
    pthread_cond_signal(&mutexVar.cond);
    pthread_mutex_unlock(&mutexVar.lock);
    if (bytes_sent <= 0) {
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
        bytes_sent = sendMessage(currentSession->socket, "221\r\n");
            if (bytes_sent <= 0) {
                perror("send() error: ");
                return -1;
            }
        return 221;
    }
    pthread_mutex_lock(&mutexVar.lock);
    while (mutexVar.ready == false) {
        pthread_cond_wait(&mutexVar.cond, &mutexVar.lock);
    }
    mutexVar.ready = false;

    currentSession->currentAccount->status = OFFLINE;
    currentSession->currentAccount = NULL;
    mutexVar.ready = true;
    pthread_cond_signal(&mutexVar.cond);
    pthread_mutex_unlock(&mutexVar.lock);


    bytes_sent = sendMessage(currentSession->socket, "112\r\n");
            if (bytes_sent <= 0) {
                perror("send() error: ");
                return -1;
            }
    
    return 112;
}


int signUp(char *username, char *password, Session *currentSession) {
    int bytes_sent, res;
    if (currentSession->currentAccount != NULL) {
        bytes_sent = sendMessage(currentSession->socket, "213\r\n");
        if (bytes_sent <= 0) {
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
        return 212;
    }

    root = insert(root, username, password, 100); // Default status is active
    FILE* file = fopen("account.txt", "a");
    fprintf(file, "%s %s %d\n", username, password, 100);
    fclose(file);

    bytes_sent = sendMessage(currentSession->socket, "110\r\n");
    res = 110;

    mutexVar.ready = true;
    pthread_cond_signal(&mutexVar.cond);
    pthread_mutex_unlock(&mutexVar.lock);
    if (bytes_sent <= 0) {
        perror("send() error: ");
        res = -1;
    }
    return res;
}

int getReadyList(Session* currentSession)
{
    if (currentSession->currentAccount == NULL) {
        sendMessage(currentSession->socket, "221\r\n");
        return 221;
    }

    char list[4096][40];
    while (mutexVar.ready == false) {
        pthread_cond_wait(&mutexVar.cond, &mutexVar.lock);
    }
    int count = collectReadyUsers(root, list, 0, currentSession->currentAccount);
    char response[BUFF_SIZE*41 + strlen("120 \r\n")];
    snprintf(response, sizeof(response), "120 %d\r\n", count);
    for (int i = 0; i < count; i++) {
        strcat(response, list[i]);
        if (i < count - 1) {
            strcat(response, "\n");
        }
    }
    strcat(response, "\r\n");
    sendMessage(currentSession->socket, response);
    return 120;
}

int handleChallenge(Session* currentSession, char opponentName[]) {
    if (currentSession->currentAccount == NULL) {
        sendMessage(currentSession->socket, "221\r\n");
        return 221;
    }

    pthread_mutex_lock(&mutexVar.lock);
    int res = -1;

    // Send challenge request to opponent
    int opp = getSessionByUsername(opponentName);
    if (opp < 0) {
        sendMessage(currentSession->socket, "230\r\n");
        res = 230;
    }
    else {
        Session *opponentSession = sessionTable[opp];
        if (opponentSession->currentAccount == NULL || opponentSession->currentAccount->status == OFFLINE) {
            sendMessage(currentSession->socket, "230\r\n");
            res = 230;
        }

        else if (opponentSession->currentAccount->status != ONLINE) {
            sendMessage(currentSession->socket, "231\r\n");
            res = 231;
        }

        else if(abs(currentSession->currentAccount->score - opponentSession->currentAccount->score) > 10) {
            char msg[64];
            snprintf(msg, sizeof(msg), "232 %d %d\r\n", currentSession->currentAccount->score, opponentSession->currentAccount->score);
            sendMessage(currentSession->socket, msg);
            res = 232;
        }
        
        else {
            currentSession->currentAccount->status = MATCHING;
            char challengeMsg[BUFF_SIZE];
            snprintf(challengeMsg, sizeof(challengeMsg), "CHALLENGE %s\r\n", currentSession->currentAccount->userName);
            sendMessage(opponentSession->socket, challengeMsg);
            sendMessage(currentSession->socket, "130\r\n");
            currentSession->opponentAccount = *(opponentSession->currentAccount);
            res = 130;
        }
    }
    pthread_mutex_unlock(&mutexVar.lock);
    return res;
}

int handleChallengeResp(Session* currentSession, const char response[], const char challenger[]) {
    if (currentSession->currentAccount == NULL) {
        sendMessage(currentSession->socket, "221\r\n"); // Not logged in
        return 221;
    }

    int res = -1;

    pthread_mutex_lock(&mutexVar.lock); 
    int oppIndex = getSessionByUsername(challenger);
    if (oppIndex < 0) {
        sendMessage(currentSession->socket, "230\r\n"); // Opponent not found
        res = 230;
        pthread_mutex_unlock(&mutexVar.lock);
        return res;
    }

    Session* opponentSession = sessionTable[oppIndex];

    if (!opponentSession->currentAccount || opponentSession->currentAccount->status == OFFLINE) {
        sendMessage(currentSession->socket, "230\r\n"); // Opponent offline
        res = 230;
        pthread_mutex_unlock(&mutexVar.lock);
        return res;
    }
    if (opponentSession->currentAccount->status == IN_GAME) {
        sendMessage(currentSession->socket, "231\r\n"); // Opponent already in game
        res = 231;
        pthread_mutex_unlock(&mutexVar.lock);
        return res;
    }

    if (opponentSession->currentAccount->status != MATCHING ||
        strcmp(opponentSession->opponentAccount.userName, currentSession->currentAccount->userName) != 0) {
        sendMessage(currentSession->socket, "233\r\n"); // No matching challenge
        res = 233;
        pthread_mutex_unlock(&mutexVar.lock);
        return res;
    }

    // ===== ACCEPT =====
    if (strcasecmp(response, "ACCEPT") == 0) {
        currentSession->currentAccount->status = IN_GAME;
        opponentSession->currentAccount->status = IN_GAME;

        currentSession->opponentAccount = *(opponentSession->currentAccount);
        opponentSession->opponentAccount = *(currentSession->currentAccount);

        send(currentSession->socket, "131\r\n", strlen("131\r\n"), 0);
        if (opponentSession->socket != -1)
            send(opponentSession->socket, "131\r\n", strlen("131\r\n"), 0);

        Match* m = createMatch(opponentSession, currentSession);
        if (m != NULL) {
            sendMatchStart(m);
        }

        res = 131;
        pthread_mutex_unlock(&mutexVar.lock);
        return res;
    }

    // ===== REJECT =====
    if (strcasecmp(response, "REJECT") == 0) {
        memset(&currentSession->opponentAccount, 0, sizeof(Account));
        memset(&opponentSession->opponentAccount, 0, sizeof(Account));
        currentSession->currentAccount->status = ONLINE;
        opponentSession->currentAccount->status = ONLINE;

        send(currentSession->socket, "132\r\n", strlen("132\r\n"), 0);
        if (opponentSession->socket != -1)
            send(opponentSession->socket, "132\r\n", strlen("132\r\n"), 0);

        res = 132;
        pthread_mutex_unlock(&mutexVar.lock);
        return res;
    }

    send(currentSession->socket, "300\r\n", strlen("300\r\n"), 0);
    res = 300;
    pthread_mutex_unlock(&mutexVar.lock);
    return res;
}

int countDir(char board[BOARD_SIZE][BOARD_SIZE],
             int x, int y, int dx, int dy, int turn) {
    int count = 0;
    x += dx;
    y += dy;
    char player = (turn % 2 == 0)? 'X': 'O';
    while (x >= 0 && x < BOARD_SIZE &&
           y >= 0 && y < BOARD_SIZE &&
           board[x][y] == player) {
        count++;
        x += dx;
        y += dy;
    }
    return count;
}

int checkWin(char board[BOARD_SIZE][BOARD_SIZE],
             int x, int y, int turn) {
    int total;

    total = 1 + countDir(board, x, y, 0, 1, turn)
                + countDir(board, x, y, 0, -1, turn);
    if (total >= 5) return 1;

    total = 1 + countDir(board, x, y, 1, 0, turn)
                + countDir(board, x, y, -1, 0, turn);
    if (total >= 5) return 1;

    total = 1 + countDir(board, x, y, 1, 1, turn)
                + countDir(board, x, y, -1, -1, turn);
    if (total >= 5) return 1;

    total = 1 + countDir(board, x, y, 1, -1, turn)
                + countDir(board, x, y, -1, 1, turn);
    if (total >= 5) return 1;

    return 0;
}

int isBoardFull(char board[BOARD_SIZE][BOARD_SIZE]) {
    for (int i = 0; i < BOARD_SIZE; i++)
        for (int j = 0; j < BOARD_SIZE; j++)
            if (board[i][j] == EMPTY)
                return 0;
    return 1;
}




int handleMove(Session *currentSession, int game_id, int x, int y) {
    if (!currentSession) return 500;

    Match *match = findMatchById(game_id);
    if (!match || match->finished) {
        sendMessage(currentSession->socket, "300\r\n");
        return 300;
    }

    if ((match->turn%2 == 0 && currentSession != match->black) ||
        (match->turn%2 == 1 && currentSession != match->white)) {
        sendMessage(currentSession->socket, "250\r\n");
        return 250;
    }

    if (x < 0 || x >= BOARD_SIZE || y < 0 || y >= BOARD_SIZE) {
        sendMessage(currentSession->socket, "252\r\n");
        return 252;
    }

    if (match->board[x][y] != EMPTY) {
        sendMessage(currentSession->socket, "251\r\n");
        return 251;
    }
    match->moves[match->turn].x = x;
    match->moves[match->turn].y = y;
    match->moves[match->turn].isO = ((match->turn)%2 == 0)? false: true;
    match->moves[match->turn].state = PLAYING;
    match->board[x][y] = ((match->turn)%2 == 0)? 'X': 'O';

    Session *opponent = (currentSession == match->black) ? match->white : match->black;
    char msg[64];
    snprintf(msg, sizeof(msg), "151 %s %d %d\r\n",
             currentSession->currentAccount->userName, x, y);
    if (checkWin(match->board, x, y, match->turn)) {
        match->finished = 1;
        match->moves[match->turn].state = WIN;
        pthread_mutex_lock(&mutexVar.lock);
        while (!mutexVar.ready) {
            pthread_cond_wait(&mutexVar.cond, &mutexVar.lock);
        }
        mutexVar.ready = false;
        opponent->currentAccount->status = ONLINE;
        currentSession->currentAccount->status = ONLINE;
        opponent->currentAccount->score = (opponent->currentAccount->score - 3) * (opponent->currentAccount->score > 0);
        currentSession->currentAccount->score = currentSession->currentAccount->score + 3;
        mutexVar.ready = true;
        pthread_cond_signal(&mutexVar.cond);
        pthread_mutex_unlock(&mutexVar.lock);
        memset(msg, 0, sizeof(msg));
        snprintf(msg, sizeof(msg), "172 %s %d %d\r\n", currentSession->currentAccount->userName, x, y);
        sendMessage(currentSession->socket, msg);
        memset(msg, 0, sizeof(msg));
        snprintf(msg, sizeof(msg), "173 %s %d %d\r\n", currentSession->currentAccount->userName, x, y);
        sendMessage(opponent->socket, msg);
        logMatch(match);
        removeMatch(match);
        return 172;
    }

    if (isBoardFull(match->board)) {
        match->finished = 1;
        match->moves[match->turn].state = DRAW;
        memset(msg, 0, sizeof(msg));
        snprintf(msg, sizeof(msg), "174 %s %d %d\r\n", currentSession->currentAccount->userName, x, y);
        sendMessage(match->black->socket, msg);
        sendMessage(match->white->socket, msg);
        opponent->currentAccount->status = ONLINE;
        currentSession->currentAccount->status = ONLINE;
        logMatch(match);
        removeMatch(match);
        return 174;
    }
    match->turn++;
    sendMessage(opponent->socket, msg);
    sendMessage(currentSession->socket, msg);
    return 150;
}

int handleRequestStop(Session *currentSession, int gameId) {
    if (!currentSession || !currentSession->currentAccount) {
        sendMessage(currentSession->socket, "221\r\n");
        return 221;
    }

    Match *m = findMatchById(gameId);
    if (!m || m->finished) {
        sendMessage(currentSession->socket, "500\r\n");
        return 500;
    }

    Session *loser = NULL;
    Session *winner = NULL;

    if (m->black == currentSession) {
        loser = m->black;
        winner = m->white;
    } else if (m->white == currentSession) {
        loser = m->white;
        winner = m->black;
    } else {
        sendMessage(currentSession->socket, "500\r\n");
        return 500;
    }

    m->finished = 1;
    m->moves[m->turn].isO = ((m->turn)%2 == 0)? false: true;
    m->moves[m->turn].state = FF;
    pthread_mutex_lock(&mutexVar.lock);
    while (!mutexVar.ready) {
        pthread_cond_wait(&mutexVar.cond, &mutexVar.lock);
    }
    mutexVar.ready = false;
    loser->currentAccount->status = ONLINE;
    winner->currentAccount->status = ONLINE;
    loser->currentAccount->score = (loser->currentAccount->score - 3) * (loser->currentAccount->score > 0);
    winner->currentAccount->score = winner->currentAccount->score + 3;
    mutexVar.ready = true;
    pthread_cond_signal(&mutexVar.cond);
    pthread_mutex_unlock(&mutexVar.lock);
    sendMessage(loser->socket, "170\r\n");
    sendMessage(winner->socket, "171\r\n");
    logMatch(m);
    return 170;
}

int handleChallenge(Session* currentSession, char opponentName[]) {
    if (!currentSession->currentAccount) {
        sendMessage(currentSession->socket, "214\r\n");
        return 214;
    }

    pthread_mutex_lock(&mutexVar.lock);
    while (mutexVar.ready == false) {
        pthread_cond_wait(&mutexVar.cond, &mutexVar.lock);
    }
    mutexVar.ready = false;
    int res = -1;

    // Send challenge request to opponent
    printf("Challenging %s\n", opponentName);
    int opp = getSessionByUsername(opponentName);
    if (opp < 0) {
        sendMessage(currentSession->socket, "230\r\n");
        res = 230;
    }
    else {
        printf("HI");
        Session *opponentSession = sessionTable[opp];
        if (opponentSession->currentAccount == NULL || opponentSession->currentAccount->status == OFFLINE) {
            sendMessage(currentSession->socket, "230\r\n");
            res = 230;
        }

        else if (opponentSession->currentAccount->status != ONLINE) {
            sendMessage(currentSession->socket, "231\r\n");
            res = 231;
        }

        else if(abs(currentSession->currentAccount->score - opponentSession->currentAccount->score) > 10) {
            sendMessage(currentSession->socket, "232\r\n");
            res = 232;
        }
        
        else {
            currentSession->currentAccount->status = MATCHING;
            char challengeMsg[BUFF_SIZE];
            snprintf(challengeMsg, sizeof(challengeMsg), "CHALLENGE %s\r\n", currentSession->currentAccount->userName);
            sendMessage(opponentSession->socket, challengeMsg);
            sendMessage(currentSession->socket, "130\r\n");
            currentSession->opponentAccount = *(opponentSession->currentAccount);
            res = 130;
        }
    }
    mutexVar.ready = true;
    pthread_cond_signal(&mutexVar.cond);
    pthread_mutex_unlock(&mutexVar.lock);
    return res;
}

int handleChallengeResp(Session* currentSession, char response[], char challenger[] ) {
    
    if (!currentSession->currentAccount) {
        sendMessage(currentSession->socket, "214\r\n");
        return 214;
    }
    int res = -1;
    challenger[strcspn(challenger, "\n")] = 0;
    pthread_mutex_lock(&mutexVar.lock);
    while (mutexVar.ready == false) {
        pthread_cond_wait(&mutexVar.cond, &mutexVar.lock);
    }
    mutexVar.ready = false;
    int opp = getSessionByUsername(challenger);
    if (opp < 0) {
        sendMessage(currentSession->socket, "230\r\n");
        res = 230;
    }
    else {
        Session *opponentSession = sessionTable[opp];
        printf("HI!!!");
        if (opponentSession->currentAccount == NULL || opponentSession->currentAccount->status == OFFLINE) {       
            sendMessage(currentSession->socket, "230\r\n");
            res = 230;
        }
        else if(opponentSession->currentAccount->status == IN_GAME){
            sendMessage(currentSession->socket, "231\r\n");
            res = 231;
        }
      
        else if (strcmp(opponentSession->opponentAccount.userName, currentSession->currentAccount->userName) != 0) {
            sendMessage(currentSession->socket, "233\r\n");
            res = 233;
        }
        else if (opponentSession->currentAccount->status == MATCHING) {
            // ===== ACCEPT =====
            if (strcmp(response, "ACCEPT") == 0) {
                printf("HI1!!!");
                currentSession->currentAccount->status = IN_GAME;
                opponentSession->currentAccount->status = IN_GAME;
                currentSession->opponentAccount = *(opponentSession->currentAccount);
                opponentSession->opponentAccount = *(currentSession->currentAccount);
                

                send(currentSession->socket, "131\r\n", 5, 0);
                if (opponentSession->socket != -1)
                    send(opponentSession->socket, "131\r\n", 5, 0);

                res = 131;
            }

            // ===== REJECT =====
            if (strcmp(response, "REJECT") == 0) {
                printf("HI2!!!");
                memset(&currentSession->opponentAccount, 0, sizeof(Account));
                memset(&opponentSession->opponentAccount, 0, sizeof(Account));
                currentSession->currentAccount->status = ONLINE;
                opponentSession->currentAccount->status = ONLINE;
                send(currentSession->socket, "132\r\n", 5, 0);
                if (opponentSession->socket != -1)
                    send(opponentSession->socket, "132\r\n", 5, 0);

                res = 132;
            }
        }
        return res;
    }
        send(currentSession->socket, "300\r\n", 5, 0);
        res = 300;
        mutexVar.ready = true;
        pthread_cond_signal(&mutexVar.cond);
        pthread_mutex_unlock(&mutexVar.lock);
    
    
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
    } else if (strcmp(cmd, "GET_READY_LIST") == 0) {
        res = getReadyList(currentSession);
        putLog(res, process_buffer, currentSession->client_addr);
    }else if (strcmp(cmd, "CHALLENGE") == 0) {
        char *token;
        char challenger[32];
        token = strtok_r(NULL, " ", &tmp);
        if (token) {
            strncpy(challenger, token, sizeof(challenger) - 1);
        }
        if (!challenger) {
            sendMessage(currentSession->socket, "300\r\n");
            putLog(300, process_buffer, currentSession->client_addr);
            res = 300;
        } else {
            res = handleChallenge(currentSession, challenger);
            putLog(res, process_buffer, currentSession->client_addr);
        }
    } else if (strcmp(cmd, "CHALLENGE_RESP") == 0) {
        char challenger[32];
        char response[sizeof("ACCEPT")];
       
        char *token;
        token = strtok_r(NULL, " ", &tmp);
        if (token) {
            strncpy(challenger, token, sizeof(challenger) - 1);
        }

        token = strtok_r(NULL, " ", &tmp);
        if (token) {
            strncpy(response, token, sizeof(response) - 1);
        }
        res = handleChallengeResp(currentSession, response, challenger);
        putLog(res, process_buffer, currentSession->client_addr);
    } 
    else if (strcmp(cmd, "MOVE") == 0) {
        int game_id, x, y;
        char *token;

        token = strtok_r(NULL, " ", &tmp);
        if (!token || sscanf(token, "%d", &game_id) != 1) {
            sendMessage(currentSession->socket, "300\r\n");
            putLog(300, process_buffer, currentSession->client_addr);
            return 300;
        }

        token = strtok_r(NULL, " ", &tmp);
        if (!token || sscanf(token, "%d", &x) != 1) {
            sendMessage(currentSession->socket, "300\r\n");
            putLog(300, process_buffer, currentSession->client_addr);
            return 300;
        }

        token = strtok_r(NULL, " ", &tmp);
        if (!token || sscanf(token, "%d", &y) != 1) {
            sendMessage(currentSession->socket, "300\r\n");
            putLog(300, process_buffer, currentSession->client_addr);
            return 300;
        }

        res = handleMove(currentSession, game_id, x, y);
        putLog(res, process_buffer, currentSession->client_addr);
    } 
    else if (strcmp(cmd, "REQUEST_STOP") == 0) {
        int gameId;
        char *token;

        token = strtok_r(NULL, " ", &tmp);
        if (!token || sscanf(token, "%d", &gameId) != 1) {
            sendMessage(currentSession->socket, "300\r\n");
            putLog(300, process_buffer, currentSession->client_addr);
            return 300;
        }

        res = handleRequestStop(currentSession, gameId);
        putLog(res, process_buffer, currentSession->client_addr);
        return res;
    } else {
        int bytes_sent = sendMessage(currentSession->socket, "300\r\n");
        if (bytes_sent <= 0) {
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
    addSessionToTable(&currentSession);
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
                logOut(&currentSession);
                if(currentSession.match != NULL){
                    Session *opSession = (currentSession.match->black == &currentSession)? currentSession.match->white : currentSession.match->black;
                    sendMessage(opSession->socket, "175\r\n");
                    removeMatch(currentSession.match);
                    opSession->currentAccount->status = ONLINE;
                }
                close(currentSession.socket);
                removeSessionFromTable(&currentSession);
                return NULL;
            } else if (received_bytes == 0) {
                printf("Connection closed by client.\n");
                logOut(&currentSession);
                if(currentSession.match != NULL){
                    Session *opSession = (currentSession.match->black == &currentSession)? currentSession.match->white : currentSession.match->black;
                    sendMessage(opSession->socket, "175\r\n");
                    removeMatch(currentSession.match);
                    opSession->currentAccount->status = ONLINE;
                }
                close(currentSession.socket);
                removeSessionFromTable(&currentSession);
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
#endif