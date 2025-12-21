

#ifndef ACCOUNT_H
#define ACCOUNT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>

enum Status {
    ONLINE,
    MATCHING,
    IN_GAME, 
    OFFLINE
};
typedef struct Account {
    char userName[30];
    char password[30];
    int score;
    enum Status status;
} Account;





typedef struct Node {
    Account account;
    struct Node* left;
    struct Node* right;     
} Node;

typedef struct MutexVar {
    pthread_mutex_t lock;
    pthread_cond_t cond;
    bool ready;
} MutexVar;

MutexVar mutexVar = {PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, true};
Node *root;


/**
 * @brief Create a new BST node with account data.
 *
 * @param userName A pointer to the username string.
 * @param status   Account status ( 1 = active, 0 = banned).
 *
 * @return A pointer to the newly created Node.
 */
Node* createNode(char* userName, char *password, int score) {
    Node* newNode = (Node*)malloc(sizeof(Node));
    strcpy(newNode->account.userName, userName);
    strcpy(newNode->account.password, password);
    newNode->account.score = score;
    newNode->account.status = OFFLINE;
    newNode->left = NULL;
    newNode->right = NULL;   
    return newNode;
}


/**
 * @brief Insert an account into the binary search tree.
 *
 * If the username already exists, no new node is inserted.
 *
 * @param r        Root of the BST.
 * @param userName A pointer to the username string.
 * @param status   Account status.
 *
 * @return A pointer to the root node of the updated BST.
 */
Node* insert(Node* r, char* userName, char* password, int score) {
    if (r == NULL){ 
        Node* newNode = createNode(userName, password, score);
        return newNode;
    }
    int cmp = strcmp(r->account.userName, userName);
    if (cmp == 0) { 
        return r;  // account existed
    } else if (cmp < 0) {
        r->right = insert(r->right, userName, password, score);
        return r;
    } else {
        r->left = insert(r->left, userName, password, score);
        return r;
    }
}



/**
 * @brief Find an account node in the BST by username.
 *
 * @param r        Root of the BST.
 * @param userName A pointer to the username string.
 *
 * @return A pointer to the Node if found, NULL otherwise.
 */
Node* find(Node* r, char* userName) {
    if (r == NULL) return NULL;
    int cmp = strcmp(r->account.userName, userName);
    if (cmp == 0) return r;
    else if (cmp < 0) return find(r->right, userName);
    return find(r->left, userName);
}


/**
 * @brief Load account data from file into BST, call only ONCE when program starts.
 *
 * File format: `<username> <status>` per line.
 *
 * @param fileName A pointer to the file name string.
 */
int initList() {
    printf("\nLoading data... ");
    FILE* file = fopen("account.txt", "r");
    if (file == NULL) {
        printf("\nFile not found!");
        return -1;
    }
    root = NULL;
    while (!feof(file)) {
        char name[30]; 
        char password[30];
        int score;
        fscanf(file, "%s %s %d", name, password, &score);
        root = insert(root, name, password, score);         
    }
    printf("Done!");
    fclose(file);
    return 0;
}



int collectReadyUsers(Node* root,
                      char list [][40],
                      int index,
                      Account* currentAccount)
{
    if (root == NULL || index >= 4096) return 0;

    int count = 0;

    if (root->account.status == ONLINE &&
        strcmp(root->account.userName, currentAccount->userName) != 0) {
        strcpy(list[index], root->account.userName);
        strcat(list[index], " ");
        char scoreStr[10];
        snprintf(scoreStr, sizeof(scoreStr), "%d", root->account.score);
        strcat(list[index], scoreStr);
        index++;
        count++;
    }

    count += collectReadyUsers(root->left, list, index, currentAccount);
    count += collectReadyUsers(root->right, list, index, currentAccount);

    return count;
}



/**
 * @brief Recursively free memory allocated for the BST.
 *
 * @param r Root of the BST.
 */

void freeTree(Node* r) {
    if (r == NULL) return;
    freeTree(r->left);
    freeTree(r->right);
    free(r); 
    r = NULL;
}

#endif