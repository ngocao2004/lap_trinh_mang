#include "menu.h"

#define MAXLINE 4096
extern bool quitFlag;
extern char buffer[];
struct in_addr ipv4addr;
pthread_mutex_t menuMutex;
pthread_cond_t menuCond;
int lastResponse;

int main(int argc, char** argv) {
    char *temp = malloc(sizeof(argv[1]) * strlen(argv[1]));
	strcpy(temp, argv[1]);
	
	// Validate command line arguments
	if(argc == 2) {
		printf("Missing server port number!\n");
		exit(0);
	}
	if(argc < 2) {
		printf("Missing server port number and ip address!\n");
		exit(0);
	}
	
	// Validate IP address format
	if(inet_pton(AF_INET, argv[1], &ipv4addr) != 1) {
		printf("Invalid Ip Address!\n");
		exit(0);
	}
	
	// Verify IP address is reachable
	if(gethostbyaddr(&ipv4addr, sizeof(ipv4addr), AF_INET) == NULL ) {
		printf("Not found information Of IP Address [%s]\n", temp);
		exit(0);
	}

	// Variable declarations
	int client_sock;                       /* Socket file descriptor */
	struct sockaddr_in server_addr;        /* Server's address information */
	int bytes_received;        /* Track bytes in send/recieve operations */


	//Step 1: Build socket
	client_sock = socket(AF_INET,SOCK_STREAM,0);
	int ser_port = atoi(argv[2]);
	
	//Step 2: Specify server address
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(ser_port);
	server_addr.sin_addr.s_addr = inet_addr(temp);

	//Step 3: Request connect server
	if(connect(client_sock, (struct sockaddr*)&server_addr, sizeof(struct sockaddr)) < 0){
		printf("\nError!Can not connect to sever! Client exit imediately!\n");
		return 0;
	}

	// Receive welcome message from server
	bytes_received = recv(client_sock, buffer, MAXLINE, 0);
    if(bytes_received <= 0){
        printf("\nError!Cannot receive data from sever!\n");
        return 1;
    }
    buffer[bytes_received] = '\0';
    if (strcmp(buffer, "100\r\n") == 0) {
		printf("Connected to server successfully! Have a nice day!\n");
	} else {
		printf("Failed to connect to server. Server response: %s\n", buffer);
		return 1;
	}
	// start background listener thread to receive unsolicited server messages
	pthread_t recv_tid;
	int *psock = malloc(sizeof(int));
	*psock = client_sock;
	if (pthread_create(&recv_tid, NULL, recv_server, psock) != 0) {
		perror("pthread_create() failed");
		free(psock);
	} else {
		pthread_detach(recv_tid);
	}
    while (!quitFlag) {
			switch (menu)
			{
			case MAIN:
				showMenu(client_sock);
				break;
			
			default:
				break;
			} 
		
	}
    close(client_sock);
    return 0;
}