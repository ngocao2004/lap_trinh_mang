#define MATCH_IMPLEMENTATION
#include "../TCPServer/match.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
#include <ctype.h>

#define BOARD_SIZE 10
#define BLACK 'X'
#define WHITE 'O'
#define EMPTY '.'


typedef struct {
    int game_id;
    char board[BOARD_SIZE][BOARD_SIZE];
    char turn;         
    int finished;
    char blackName[32]; 
    char whiteName[32]; 
} ClientMatch;

void printClientBoard(ClientMatch *m) {
    printf("  ");
    for (int j = 0; j < BOARD_SIZE; j++)
        printf("%d ", j);
    printf("\n");

    for (int i = 0; i < BOARD_SIZE; i++) {
        printf("%d ", i);
        for (int j = 0; j < BOARD_SIZE; j++) {
            printf("%c ", m->board[i][j]);
        }
        printf("\n");
    }
}



ClientMatch *currentMatch = NULL;

#define BUFF_SIZE 4096
#define CODE_LEN 3

volatile bool challengePending = false; 
char challenger[32];  

pthread_mutex_t resp_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  resp_cond  = PTHREAD_COND_INITIALIZER;
int lastResponse = -1;

const char* responseCode[2] = {"+OK", "-ERR"};
const char* commandPrefix[6] = {"LOGIN ","LOGOUT", "REGISTER ","GET_READY_LIST","CHALLENGE ","CHALLENGE_RESP "};
char buffer[BUFF_SIZE];

enum Menu{
	MAIN,
	CHALLENGE,
	LOGIN,
	LOGOUT,
	REGISTER,
	LIST,
	INVITED,
    GAME
};

enum Menu menu = MAIN;

int sendMessage(int socketFd, char buffer[], const char *input, int type) {
    const char *prefix_str = commandPrefix[type];
    const char *end_marker = "\r\n";


    if (send(socketFd, prefix_str, strlen(prefix_str), 0) < 0) {
        perror("send() prefix error");
        return -1;
    }

  
    if (send(socketFd, input, strlen(input), 0) < 0) {
        perror("send() input error");
        return -1;
    }


    if (send(socketFd, end_marker, 2, 0) < 0) {
        perror("send() CRLF error");
        return -1;
    }

    return strlen(prefix_str) + strlen(input);
}

int waitResponse() {
    pthread_mutex_lock(&resp_mutex);
    while (lastResponse == -1)
        pthread_cond_wait(&resp_cond, &resp_mutex);

    int res = lastResponse;
    lastResponse = -1;
    pthread_mutex_unlock(&resp_mutex);
    return res;
}




void* recv_server(void* arg) {
    int sock = *(int*)arg;
    char buf[BUFF_SIZE];
    char cache[BUFF_SIZE] = {0};

    while (1) {
        int n = recv(sock, buf, sizeof(buf)-1, 0);
        if (n <= 0) break;
        buf[n] = '\0';
        strcat(cache, buf);

        char* p;
        while ((p = strstr(cache, "\r\n")) != NULL) {
            *p = '\0';
            dispatch_message(cache, sock);
            memmove(cache, p + 2, strlen(p + 2) + 1);
        }
    }
    return NULL;
}

void dispatch_message(char* msg, int sock)
{
    static int waitingReadyList = 0;
    static int readyCount = 0;
    static int readyReceived = 0;

    if (waitingReadyList) {
        printf("- %s\n", msg);
        readyReceived++;

        if (readyReceived >= readyCount) {
            waitingReadyList = 0;
            readyReceived = 0;
            readyCount = 0;

            printf("=====================\n");
            fflush(stdout);
        }
        return;
    }

    int code = atoi(msg);

    if (code == 111) {
        printf("\nLogin successful\n");
        pthread_mutex_lock(&resp_mutex);
        lastResponse = 111;
        pthread_cond_signal(&resp_cond);
        pthread_mutex_unlock(&resp_mutex);
    }
    else if (code == 110) { // Register success
        pthread_mutex_lock(&resp_mutex);
        lastResponse = 110;
        pthread_cond_signal(&resp_cond);
        pthread_mutex_unlock(&resp_mutex);
    }
    else if (code == 211) {  // Username exists
        pthread_mutex_lock(&resp_mutex);
        lastResponse = 211;
        pthread_cond_signal(&resp_cond);
        pthread_mutex_unlock(&resp_mutex);
    }
    else if (code == 213) {  // Already logged in
        pthread_mutex_lock(&resp_mutex);
        lastResponse = 213;
        pthread_cond_signal(&resp_cond);
        pthread_mutex_unlock(&resp_mutex);
    }
    else if (code == 215) {  // Login failed
        pthread_mutex_lock(&resp_mutex);
        lastResponse = 215;
        pthread_cond_signal(&resp_cond);
        pthread_mutex_unlock(&resp_mutex);
    }
    else if (code == 130) {
        printf("\nChallenge sent successfully\nWaiting for opponent...\n");
    }
    else if (code == 131) {
        printf("\nChallenge accepted! Game starting ...\n");
        menu = GAME;
    }
    else if (code == 132) {
        printf("\nChallenge rejected\n");
        menu = MAIN;
    }
    else if (code == 233) {
        printf("\nNo challenge exists\n");
        menu = MAIN;
    }
    else if (code == 140) {  
        if (challengePending){
            char msg[256];
            snprintf(msg, sizeof(msg),
                 "CHALLENGE_RESP %s REJECT\r\n", challenger);
            send(sock, msg, strlen(msg), 0);
            return;
        }
        char opponent[64];
        if (sscanf(msg, "140 %s", opponent) == 1) {
            strncpy(challenger, opponent, sizeof(challenger)-1);
            challenger[sizeof(challenger)-1] = '\0';
            challengePending = true;
        }
        if( menu == MAIN ) menu = INVITED;
    }

    /* ===== READY LIST HEADER ===== */
    else if (code == 120) {
        sscanf(msg, "120 %d", &readyCount);

        printf("\n=== READY PLAYERS (%d) ===\n", readyCount);

        if (readyCount == 0) {
            printf("(No player online)\n");
            printf("=====================\n");
            fflush(stdout);
        }
        else{
            waitingReadyList = 1;
            readyReceived = 0;
        }
        menu = MAIN;
    }

    else if (code == 155) {
        int gameId;
        char black[32], white[32];
        sscanf(msg, "155 %d %s %s", &gameId, black, white);

        if (currentMatch != NULL) {
            free(currentMatch);
            currentMatch = NULL;
        }

        currentMatch = malloc(sizeof(ClientMatch));
        memset(currentMatch, 0, sizeof(ClientMatch));
        currentMatch->game_id = gameId;
        printf("\n=== Game Started! Game ID: %d ===\n", gameId);
        strncpy(currentMatch->blackName, black, sizeof(currentMatch->blackName)-1);
        strncpy(currentMatch->whiteName, white, sizeof(currentMatch->whiteName)-1);
        currentMatch->turn = BLACK;
        currentMatch->finished = 0;
        printf("Game started: %s (X) vs %s (O)\n", black, white);
        menu = GAME;
        fflush(stdout);
    }

    else if (code == 150) {
        printf("\nYour move accepted!\n");
        fflush(stdout);
    }
    
    else if (code == 151) {
        char playerName[32];
        int x, y;

        if (sscanf(msg, "151 %s %d %d", playerName, &x, &y) == 3) {
            char symbol = (strcmp(playerName, currentMatch->blackName) == 0) ? 'X' : 'O';

            currentMatch->board[y][x] = symbol;

            printf("\n>>> %s (%c) played at [%d, %d]\n", playerName, symbol, x, y);

            currentMatch->turn = (currentMatch->turn == BLACK) ? WHITE : BLACK;

            char* currentPlayer = (currentMatch->turn == BLACK)
                                    ? currentMatch->blackName
                                    : currentMatch->whiteName;

            printf("\nCurrent turn: %s (%c)\n", currentPlayer,
                (currentMatch->turn == BLACK) ? 'X' : 'O');
            fflush(stdout);
        }
    }


    
    else if (code == 250) {
        printf("\nNot your turn!\n");
        fflush(stdout);
    }
    
    else if (code == 251) {
        printf("\nPosition already occupied!\n");
        fflush(stdout);
    }
    
    else if (code == 252) {
        printf("\nInvalid position (out of bounds)!\n");
        fflush(stdout);
    }

    else if (code == 170) {
        printf("\nYou surrendered. You lose!\n");

        if (currentMatch) {
            currentMatch->finished = 1;
        }

        menu = MAIN;
        fflush(stdout);
    }
    else if (code == 171) {
        char loser[32];
        if (sscanf(msg, "171 %s", loser) == 1) {
            printf("\n%s surrendered. You win!\n", loser);
        } else {
            printf("\nOpponent surrendered. You win!\n");
        }

        if (currentMatch) {
            currentMatch->finished = 1;
        }

        menu = MAIN;
        fflush(stdout);
    }

    
    else if (code == 300) {
        printf("\nsai kiểu thông điệp\n");
        if (currentMatch) {
            free(currentMatch);
            currentMatch = NULL;
        }
        menu = MAIN;
        fflush(stdout);
    }




    else if (strncmp(msg, "CHALLENGE ", 10) == 0) {
        sscanf(msg, "CHALLENGE %s", challenger);
    }
    else {
        printf("\n[Server] %s\n", msg);
    }
    fflush(stdout);
}


void showChallengeMenu(int sock) {
    int choice;
    char msg[256];
    if (challenger == NULL) return;
    printf("\nYou are challenged by %s\n", challenger);
    printf("1. Accept challenge\n");
    printf("2. Reject challenge\n");
    printf("Your choice: ");
    scanf("%d", &choice);
    getchar();

    if (choice == 1) {
        snprintf(msg, sizeof(msg),
                 "CHALLENGE_RESP %s ACCEPT\r\n", challenger);
        send(sock, msg, strlen(msg), 0);
        while (menu != GAME) {
            usleep(50000); // sleep 50ms
        }
    }
    else if (choice == 2) {
        snprintf(msg, sizeof(msg),
                 "CHALLENGE_RESP %s REJECT\r\n", challenger);
        send(sock, msg, strlen(msg), 0);
    }
    else {
        printf("Invalid choice.\n");
    }
}
/**
 * @brief Sends login request to server and handles response.
 * @param socketFd Socket file descriptor.
 * @return Server response code, -1 on error.
 */
int logIn(int socketFd) {
    if (menu != MAIN) return -1;
    char userName[100];
    char password[100];
    menu = LOGIN;
    printf("\nUsername: ");
    fgets(userName, sizeof(userName), stdin);
    printf("Password: ");
    fgets(password, sizeof(password), stdin);

    userName[strcspn(userName, "\n")] = 0;
    password[strcspn(password, "\n")] = 0;

    char input[256];
    snprintf(input, sizeof(input), "%s %s", userName, password);

    sendMessage(socketFd, buffer, input, 0);
    printf("Login request sent. Waiting response...\n");

    /*ĐỢI RESPONSE từ listen_server */
    pthread_mutex_lock(&resp_mutex);
    while (lastResponse == -1)
        pthread_cond_wait(&resp_cond, &resp_mutex);

    int res = lastResponse;
    lastResponse = -1;
    pthread_mutex_unlock(&resp_mutex);

    switch (res) {
        case 111:
            printf("Hello %s!\n", userName);
            break;
        case 213:
            printf("Already logged in.\n");
            break;
        case 215:
            printf("Login failed.\n");
            break;
        default:
            printf("Server response: %d\n", res);
    }
    menu = MAIN;
    return res;
}




/**
 * @brief Sends logout request to server and handles response.
 * @param socketFd Socket file descriptor.
 * @return Server response code, -1 on error.
 */
int logOut(int socketFd) {
    if (menu != MAIN) return -1;
    menu = LOGOUT;
    if (sendMessage(socketFd, buffer, "", 1) == -1) {
        printf("Send LOGOUT failed\n");
        return -1;
    }

    printf("Logout request sent. Waiting for server response...\n");
    return 0;   
}


int getReadyList(int socketFd)
{   
    if (menu != MAIN) return -1;
    menu = LIST;
    if (sendMessage(socketFd, buffer, "", 3) == -1) {
        printf("Failed to send GET_READY_LIST\n");
        return -1;
    }

    printf("GET_READY_LIST sent. Waiting for server response...\n");
    
    sleep(1);
    
    return 0;
}


int challengePlayer(int socketFd) {
    if (menu != MAIN) return -1;
    char opponent[256];

    printf("\nEnter username to challenge: ");
    fgets(opponent, sizeof(opponent), stdin);
    opponent[strcspn(opponent, "\n")] = 0;
    if (sendMessage(socketFd, buffer, opponent, 4) == -1) {
        printf("Send challenge failed\n");
        return -1;
    }

    printf("Challenge request sent to %s!\n", opponent);
    return 0;
}




int signUp(int socketFd) {
    char user[1000], pass[1000];

    printf("Username: ");
    fgets(user, sizeof(user), stdin);
    printf("Password: ");
    fgets(pass, sizeof(pass), stdin);

    user[strcspn(user, "\n")] = 0;
    pass[strcspn(pass, "\n")] = 0;

    char msg[2000];
    snprintf(msg, sizeof(msg), "%s %s", user, pass);

    sendMessage(socketFd, buffer, msg, 2);

    int res = waitResponse();

    if (res == 110) printf("Register success\n");
    else if (res == 211) printf("Username exists\n");
    else printf("Server code: %d\n", res);

    return res;
}

bool quitFlag = false;


/**
 * @brief Sets quit flag to true to terminate program loop.
 */
void quit() {
    quitFlag = true;
}





/**
 * @brief Displays menu options and executes selected user action.
 * @param socketFd Socket file descriptor.
 */

void showMenu(int socketFd) {

    switch (menu) {
        case MAIN: {
            printf("\nMenu:\n"
                   "1. Log in\n"
                   "2. Log out\n"
                   "3. Register \n"
                   "4. Get ready list\n"
                   "5. Challenge a player\n"
                   "6. See challenge(%d)\n"
                   "7. Quit\n", challengePending);
            int choice;
            printf("Your choice: ");
            scanf("%d", &choice);
            getchar(); // clear newline
            switch (choice) {
                case 1: logIn(socketFd); break;
                case 2: logOut(socketFd); break;
                case 3: signUp(socketFd); break;
                case 4: getReadyList(socketFd); break;
                case 5: challengePlayer(socketFd); break;
                case 6: showChallengeMenu(socketFd);
                break;
                case 7: quit(); break;
                default: printf("\nPlease choose from 1 to 7!"); break;
            }
            break;
        }

        case CHALLENGE:
            printf("Waiting for challenge response...\n");
            break;

        case GAME:
            if (currentMatch == NULL || currentMatch->finished) {
                menu = MAIN;
                break;
            }

            printClientBoard(currentMatch);

            printf("\nYour move (row col) or 'surrender': ");
            fflush(stdout);

            char input[128];
            if (fgets(input, sizeof(input), stdin) == NULL) break;

            input[strcspn(input, "\n")] = 0;
            if (strlen(input) == 0) break;

            if (strcasecmp(input, "surrender") == 0) {
                char msg[64];
                snprintf(msg, sizeof(msg), "REQUEST_STOP %d\r\n", currentMatch->game_id);
                send(socketFd, msg, strlen(msg), 0);
                printf("\nSent REQUEST_STOP\n");
                break;
            }


            int x, y;
            if (sscanf(input, "%d %d", &x, &y) == 2) {
                char msg[128];
                snprintf(msg, sizeof(msg), "MOVE %d %d %d\r\n",
                        currentMatch->game_id, x, y);
                send(socketFd, msg, strlen(msg), 0);
                printf("Move sent: [%d, %d]\n", x, y);
            } else {
                printf("Invalid format! Use: row col (e.g., 7 7)\n");
            }
            break;
    }
}
