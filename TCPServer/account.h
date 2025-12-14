#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>

typedef struct Account {
    char userName[10000];
    char password[10000];
    int score;
    bool isLoggedIn; 
    bool isWaiting;
    struct Account* challengedBy;   
    struct Account* challenging; 
} Account;





typedef struct Node {
    Account account;
    struct Node* left;
    struct Node* right;     
} Node;

struct {
    pthread_mutex_t lock;
    pthread_cond_t cond;
    bool ready;
} mutexVar = {PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, true};

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
        FILE* file = fopen("account.txt", "a");
        fprintf(file, "%s %s %d\n", userName, password, score);
        fclose(file);
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


int collectReadyUsers(Node* root,
                      char* buffer,
                      size_t bufferSize,
                      Account* currentAccount)
{
    if (root == NULL) return 0;

    int count = 0;

    count += collectReadyUsers(root->left, buffer, bufferSize, currentAccount);

    if (strcmp(root->account.userName,currentAccount->userName) != 0 &&
        root->account.isLoggedIn &&
        !root->account.isWaiting)
    {
        size_t len = strlen(buffer);
        if (len < bufferSize - 3) {
            snprintf(buffer + len,
                    bufferSize - len,
                    "%s\r\n",
                    root->account.userName);
            count++;
        }
    }


    count += collectReadyUsers(root->right, buffer, bufferSize, currentAccount);

    return count;
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
        char name[10000]; 
        char password[10000];
        int score;
        fscanf(file, "%s %s %d", name, password, &score);
        root = insert(root, name, password, score);         
    }
    printf("Done!");
    fclose(file);
    return 0;
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
