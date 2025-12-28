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

typedef struct Match {
    int game_id;
    Session *black;
    Session *white;
    char board[BOARD_SIZE][BOARD_SIZE];
    char turn;
    int finished;
} Match;

extern Match *matchList[MAX_MATCH];
extern int matchCount;

Match* createMatch(Session *player1, Session *player2);
void initBoard(Match *m);
void sendMatchStart(Match *m);

#endif

#ifdef MATCH_IMPLEMENTATION

Match *matchList[MAX_MATCH];
int matchCount = 0;
static int nextGameId = 1;

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
    m->turn = BLACK;
    m->finished = 0;

    initBoard(m);

    matchList[matchCount++] = m;

    player1->match = m;
    player2->match = m;

    return m;
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

#endif
