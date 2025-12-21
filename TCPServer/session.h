#ifndef SESSION_H
#define SESSION_H

#include "account.h"
#include <netinet/in.h>

#define MAX_SESSIONS 100

struct Match;
typedef struct Match Match;

typedef struct Session {
    int socket;
    struct sockaddr_in client_addr;
    Account* currentAccount;
    Account opponentAccount;
    Match *match;
} Session;


Session *sessionTable[MAX_SESSIONS];
int sessionCount = 0;
MutexVar sessionTableMutex = {PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, true};
int addSessionToTable(Session *session) {

    pthread_mutex_lock(&sessionTableMutex.lock);
    while (sessionTableMutex.ready == false) {
        pthread_cond_wait(&sessionTableMutex.cond, &sessionTableMutex.lock);
    }
    sessionTableMutex.ready = false;
    if (sessionCount >= MAX_SESSIONS) {
        pthread_mutex_unlock(&sessionTableMutex.lock);
        return false; 
    }
    sessionTable[sessionCount++] = session;
    int i = sessionCount;
    sessionTableMutex.ready = true;
    pthread_cond_signal(&sessionTableMutex.cond);
    pthread_mutex_unlock(&sessionTableMutex.lock);
    return i;
}

bool removeSessionFromTable(Session *session)
{
    pthread_mutex_lock(&sessionTableMutex.lock);

    while (!sessionTableMutex.ready) {
        pthread_cond_wait(&sessionTableMutex.cond,
                          &sessionTableMutex.lock);
    }
    sessionTableMutex.ready = false;

    int found = -1;

    for (int i = 0; i < sessionCount; i++) {
        if (sessionTable[i] == session) {
            found = i;
            break;
        }
    }

    if (found == -1) {
        sessionTableMutex.ready = true;
        pthread_cond_signal(&sessionTableMutex.cond);
        pthread_mutex_unlock(&sessionTableMutex.lock);
        return false;
    }

    // Shift remaining sessions
    for (int i = found; i < sessionCount - 1; i++) {
        sessionTable[i] = sessionTable[i + 1];
    }

    // Clear last entry safely (struct, not pointer)
    sessionTable[sessionCount - 1] = NULL;
    sessionCount--;

    sessionTableMutex.ready = true;
    pthread_cond_signal(&sessionTableMutex.cond);
    pthread_mutex_unlock(&sessionTableMutex.lock);
    return true;
}



int getSessionByUsername(char username[])
{   
    if (!username) return -1;
    int res = -1;
    pthread_mutex_lock(&sessionTableMutex.lock);
    while (sessionTableMutex.ready == false) {
        pthread_cond_wait(&sessionTableMutex.cond, &sessionTableMutex.lock);
    }
    sessionTableMutex.ready = false;

    for (int i = 0; i < sessionCount; i++) {
       
        if (
            sessionTable[i] &&
            sessionTable[i]->currentAccount &&
            sessionTable[i]->currentAccount->userName &&
            strcmp(sessionTable[i]->currentAccount->userName, username) == 0) {
            res = i;
            break;
        }
    }

    sessionTableMutex.ready = true;
    pthread_cond_signal(&sessionTableMutex.cond);
    pthread_mutex_unlock(&sessionTableMutex.lock);

    printf("Get session by username: %s\n", username);
    return res;   // NULL if not found (SAFE)
}

#endif
