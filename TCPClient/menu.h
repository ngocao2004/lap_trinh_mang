
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

#define BUFF_SIZE 4096
#define CODE_LEN 3

const char* responseCode[2] = {"+OK", "-ERR"};
const char* commandPrefix[3] = {"LOGIN ","LOGOUT", "REGISTER "};
char buffer[BUFF_SIZE];



/**
 * @brief Sends a message to the server with a specific command prefix.
 * @param socketFd Socket file descriptor.
 * @param buffer Data buffer.
 * @param input Message content.
 * @param type Message type (0: USER, 1: POST, 2: BYE).
 * @return Number of bytes sent on success, -1 on error.
 */
int sendMessage(int socketFd, char buffer[], const char *input, int type) {
    ssize_t bytes_sent;

    const char *prefix_str = commandPrefix[type];
    size_t prefix_len = strlen(prefix_str);

   
    bytes_sent = send(socketFd, prefix_str, prefix_len, 0);
    if (bytes_sent < 0) {
        perror("send() commandPrefix error");
        return -1;
    }

    
    size_t input_len = strlen(input);
    size_t sent_input = 0;

    while (sent_input < input_len) {
        size_t chunk_size = (input_len - sent_input > BUFF_SIZE)
                            ? BUFF_SIZE
                            : (input_len - sent_input);

        memcpy(buffer, input + sent_input, chunk_size);

        bytes_sent = send(socketFd, buffer, chunk_size, 0);
        if (bytes_sent < 0) {
            if (errno == EINTR) continue;
            perror("send() input error");
            return -1;
        }

        sent_input += bytes_sent;
    }

  
    const char *end_marker = "\r\n";
    bytes_sent = send(socketFd, end_marker, 2, 0);
    if (bytes_sent < 0) {
        perror("send() CRLF error");
        return -1;
    }

    
    return prefix_len + sent_input;
}





/**
 * @brief Receives a server response and extracts the response code.
 * @param serverSock Server socket file descriptor.
 * @param buffer Buffer to store received data.
 * @param size Size of the buffer.
 * @return Parsed response code, -1 on error.
 */
int receiveResult(int serverSock, char *buffer, size_t size) {
    memset(buffer, 0, size);
    char *delimiter;
    while (delimiter = strstr(buffer, "\r\n") == NULL) {
        ssize_t bytes = recv(serverSock, buffer, size - 1, 0);
        if (bytes < 0) {
            perror("recv() error");
            return -1;
        } else if (bytes == 0) {
            printf("Connection closed by server.\n");
            return 0;
        }
    }
    

    int response_code = atoi(buffer);
    return response_code;
}


    




/**
 * @brief Sends login request to server and handles response.
 * @param socketFd Socket file descriptor.
 * @return Server response code, -1 on error.
 */
int logIn(int socketFd) {
    char userName[10000];
    printf("\nUsername: ");
    fgets(userName, sizeof(userName), stdin);
    printf("\nPassword: ");
    char password[10000];
    fgets(password, sizeof(password), stdin);
    userName[strcspn(userName, "\n")] = 0; // remove newline
    password[strcspn(password, "\n")] = 0; // remove newline
    char input[20000];
    snprintf(input, sizeof(input), "%s %s", userName, password);
    

    // Send in multiple calls if needed
    int send = sendMessage(socketFd, buffer, input, 0);
    if (send == -1) {
        fprintf(stderr, "Failed to send message.\n");
        return -1;
    } 
        printf("Successfully sent login request. Waiting for client response...\n");
    int res = receiveResult(socketFd, buffer, BUFF_SIZE);
    switch (res)
    {
    case 111:
        printf("\nHello %s!\n", userName);
        break;
    case 213:
        printf("\nYou have already logged in\n");
        break;
    case 215:
        printf("\nLogin failed. Please check your username and password.\n");
        break;
    case 300:
        printf("\nBad request\n");
        break;
    default:
        printf("\nUnknown response from server. You may try again later.\n");
        break;
    }    
    return res;
    


}




/**
 * @brief Sends logout request to server and handles response.
 * @param socketFd Socket file descriptor.
 * @return Server response code, -1 on error.
 */
int logOut(int socketFd) {
    int send = sendMessage(socketFd, buffer, "", 1);
    if (send == -1) {
        fprintf(stderr, "Failed to send logout request.\n");
        return -1;
    }
    printf("Successfully sent logout request. Waiting for client response...\n");
    int res = receiveResult(socketFd, buffer, BUFF_SIZE);
    switch (res)
    {
    case 112:
        printf("\nLogged out successfully.\n");
        break;
    case 214:
        printf("\nYou are not logged in.\n");
        break;
    case 300:
        printf("\nBad request\n");
        break;
    default:
        printf("\nUnknown response from server. You may try again later.\n");
        break;
    }
    return res;
}


int signUp(int socketFd) {
    char userName[10000];
    printf("\nUsername: ");
    fgets(userName, sizeof(userName), stdin);
    printf("\nPassword: ");
    char password[10000];
    fgets(password, sizeof(password), stdin);
    userName[strcspn(userName, "\n")] = 0; // remove newline
    password[strcspn(password, "\n")] = 0; // remove newline
    char input[20000];
    snprintf(input, sizeof(input), "%s %s", userName, password);
  
    int send = sendMessage(socketFd, buffer, input, 2);
    if (send == -1) {
        fprintf(stderr, "Failed to send message.\n");
        return -1;
    } 
        printf("Successfully sent signUp request. Waiting for client response...\n");
    int res = receiveResult(socketFd, buffer, BUFF_SIZE);
    switch (res)
    {
    case 110:
        printf("\nRegister successful.\n");
        break;
    case 211:
        printf("\nUsername already exists.\n");
        break;
    case 300:
        printf("\nBad request\n");
        break;
    default:
        printf("\nUnknown response from server. You may try again later.\n");
        break;
    }    
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
    printf("\nMenu:\n1. Log in\n2. Log out\n3. Register \n4. Quit\nYour option: ");
    int choice;
    scanf("%d", &choice);
    getchar(); 
    switch (choice) {
        case 1: logIn(socketFd); break;
        case 2: logOut(socketFd); break;
        case 3: signUp(socketFd); break;
        case 4: quit(); break;      
        default: printf("\nPlease choose from 1 to 4!"); break;
    }
}
