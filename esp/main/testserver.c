#include <stdio.h> 
#include <netdb.h> 
#include <netinet/in.h> 
#include <stdlib.h> 
#include <string.h> 
#include <sys/socket.h> 
#include <sys/types.h> 
#include <unistd.h>

#define PORT 9000
#define BUFSIZE (1 << 8)

void func(int connfd) { 
	char buf[BUFSIZE]; 

	int n; 
	while (1) {
		memset(buf, 0, BUFSIZE);
		read(connfd, buf, sizeof(buf)); 

		printf("From client: %s\t To client : ", buf); 

		// v TODO REMOVE

		memset(buf, 0, BUFSIZE);
		n = 0; 
		while ((buf[n++] = getchar()) != '\n'); // Copy server message to buffer 

		// and send that buffer to client 
		write(connfd, buf, sizeof(buf)); 

		// if msg contains "Exit" then server exit and chat ended. 
		if (strncmp("exit", buf, 4) == 0) { 
			printf("Server Exit...\n"); 
			break; 
		} 
	} 
} 

int main(void) { 
	const int sockfd = socket(AF_INET, SOCK_STREAM, 0); 
	if (sockfd == -1) { 
		printf("Failed to create socket!\n"); 
		return 1;
	} 

	const struct sockaddr_in addr = {
		.sin_family = AF_INET, 
		.sin_addr.s_addr = htonl(INADDR_ANY), 
		.sin_port = htons(PORT)
	};

	if (bind(sockfd, (struct sockaddr *) &addr, sizeof(addr))) { 
		printf("Failed to bind socket!\n");
		return 1;
	} 

	if (listen(sockfd, 5)) { 
		printf("Failed to listen!\n");
		return 1;
	} 

	struct sockaddr_in cli;
	int len = sizeof(struct sockaddr_in);
	const int connfd = accept(sockfd, (struct sockaddr *) &cli, &len); 
	if (connfd < 0) { 
		printf("Failed to accept!\n");
		return 1;
	} 

	func(connfd); 
	close(sockfd); 
}
