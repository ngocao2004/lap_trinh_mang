#ifndef MATCH_H
#define MATCH_H

#include "session.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#define BOARD_SIZE 10
#define MAX_MATCH 100
#define EMPTY '.'
#define BLACK 'X'
#define WHITE 'O'

enum Result{
    WIN,
    DRAW,
    FF,
    PLAYING
};

typedef struct Move
{
    bool isO;
    enum Result state;
    int x;
    int y;
} Move;

typedef struct Match {
    int game_id;
    Session *black;
    Session *white;
    char board[BOARD_SIZE][BOARD_SIZE];
    int turn;
    int finished;
    Move moves[BOARD_SIZE*BOARD_SIZE];
} Match;

extern Match *matchList[MAX_MATCH];
extern int matchCount;

Match* createMatch(Session *player1, Session *player2);
void initBoard(Match *m);
void sendMatchStart(Match *m);
void logMatch(Match *m);
#endif

#ifdef MATCH_IMPLEMENTATION

Match *matchList[MAX_MATCH];
int matchCount = 0;
static int nextGameId = 1;
MutexVar matchListMutex = {PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, true};
void initBoard(Match *m) {
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            m->board[i][j] = EMPTY;
        }
    }
}

void printBoard(Match *m) {
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


Match* createMatch(Session *player1, Session *player2) {
    if (matchCount >= MAX_MATCH) return NULL;

    Match *m = (Match *)malloc(sizeof(Match));
    if (!m) return NULL;
    

    m->game_id = nextGameId++;
    m->black = player1;
    m->white = player2;
    m->turn = 0;
    m->finished = 0;
      
    initBoard(m);
    for(int i = 0; i < BOARD_SIZE*BOARD_SIZE; ++i){
        m->moves[i].state = PLAYING;
    }
    pthread_mutex_lock(&matchListMutex.lock);
    while (matchListMutex.ready == false) {
        pthread_cond_wait(&matchListMutex.cond, &matchListMutex.lock);
    }
    matchListMutex.ready = false;
    matchList[matchCount++] = m;
    matchListMutex.ready = true;
    pthread_cond_signal(&matchListMutex.cond);
    pthread_mutex_unlock(&matchListMutex.lock);
    player1->match = m;
    player2->match = m;

    return m;
}

int removeMatch(Match *m){
    pthread_mutex_lock(&matchListMutex.lock);

    while (!matchListMutex.ready) {
        pthread_cond_wait(&matchListMutex.cond,
                          &matchListMutex.lock);
    }
    matchListMutex.ready = false;

    int found = -1;

    for (int i = 0; i < matchCount; i++) {
        if (matchList[i] == m) {
            found = i;
            break;
        }
    }

    if (found == -1) {
        matchListMutex.ready = true;
        pthread_cond_signal(&matchListMutex.cond);
        pthread_mutex_unlock(&matchListMutex.lock);
        return false;
    }

    // Shift remaining sessions
    for (int i = found; i < matchCount - 1; i++) {
        matchList[i] = matchList[i + 1];
    }

    // Clear last entry safely (struct, not pointer)
    matchList[matchCount - 1] = NULL;
    matchCount--;

    matchListMutex.ready = true;
    pthread_cond_signal(&matchListMutex.cond);
    pthread_mutex_unlock(&matchListMutex.lock);
    return true;
}
Match* findMatchById(int game_id) {
    for (int i = 0; i < matchCount; i++) {
        if (matchList[i]->game_id == game_id) {
            return matchList[i];
        }
    }
    return NULL;
}

void sendMatchStart(Match *m) {
    char msg[256];

    sprintf(msg, "155 %d %s %s\r\n",
            m->game_id,
            m->black->currentAccount->userName,
            m->white->currentAccount->userName);

    send(m->black->socket, msg, strlen(msg), 0);
    send(m->white->socket, msg, strlen(msg), 0);
}

void logMatch(Match *m){
    if (!m || !m->black || !m->white) return;

    char filename[200];
    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t);
    char buffer[100];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d_%H-%M-%S", &t);
    snprintf(filename,sizeof(filename), "%s-%svs%s.txt", buffer, m->black->currentAccount->userName, m->white->currentAccount->userName);
    FILE * f = fopen(filename, "w");
    if (!f) {
        perror("fopen");
        return;
    }
    fprintf(f, "%s %s(X) vs %s(O)\n", buffer, m->black->currentAccount->userName, m->white->currentAccount->userName);
    int i = 0;
    for(i = 0; i < BOARD_SIZE*BOARD_SIZE && m->moves[i].state == PLAYING; ++i){
        char c = (m->moves[i].isO)? 'O' : 'X';
        fprintf(f, "%c %d %d\n", c, m->moves[i].x, m->moves[i].y);
    }
    
    if (i < BOARD_SIZE*BOARD_SIZE){
        char c;
        switch (m->moves[i].state)
        {
            case WIN:
                 c = (m->moves[i].isO)? 'O' : 'X';
                fprintf(f, "%c %d %d\n%c WIN", c, m->moves[i].x, m->moves[i].y, c);
                break;
            case DRAW:
                c = (m->moves[i].isO)? 'O' : 'X';
                fprintf(f, "%c %d %d\nDRAW", c, m->moves[i].x, m->moves[i].y);
                break;
            case FF:
                c = (m->moves[i].isO)? 'O' : 'X';
                fprintf(f, "%c SURRENDERED", c);
                break;
            default:
            break;
    }
    fclose(f);
    f = fopen(filename, "r");
    if (!f) {
        perror("fopen");
        return;
    }
    char fileBuffer[1024];
    snprintf(fileBuffer, sizeof(fileBuffer), "180 %s ", filename);
    if (send(m->black->socket, fileBuffer, strlen(fileBuffer), 0) < 0 || send(m->white->socket, fileBuffer, strlen(fileBuffer), 0) < 0 ){
        fclose(f);
        perror("Error in sending file.");
        return;
    }
    memset(fileBuffer, 0, sizeof(fileBuffer));

    while(fgets(fileBuffer, 1024, f) != NULL){
        if (send(m->black->socket, fileBuffer, strlen(fileBuffer), 0) >= 0 && send(m->white->socket, fileBuffer, strlen(fileBuffer), 0) >= 0 ) {
            memset(fileBuffer, 0, sizeof(fileBuffer));
            continue;
        }
        fclose(f);
        perror("Error in sending file.");
        return;
    }

    if (send(m->black->socket, "\r\n", strlen("\r\n"), 0) < 0 || send(m->white->socket, "\r\n", strlen("\r\n"), 0) < 0 ){
        fclose(f);
        perror("Error in sending file.");
        return;
    }
    fclose(f);
}
}
#endif
