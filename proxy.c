/******************************************
 *  EECS 489 PA 2
 *  
 *  proxy.c
 *  	usage: ./proxy <port>
 *
 *****************************************/

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "utility.h"

int main(int argc, char * argv[]) 
{
	int listen_portno;
	int listen_sock;
	int conn_sock;
	struct sockaddr_in proxy_addr;
	char buffer[BUFFER_SIZE];
	
	if (argc < 2) {
		fprintf(stderr, "ERROR, no port provided\n");
		exit(1);
	}
	
	/* Get port number from command line */
	listen_portno = atoi(argv[1]);
	
	/* Create a socket to listen on requests */
	listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_sock < 0)
		error("Creating socket failed");
	
	/* Build the proxy's internet address */
	proxy_addr.sin_family = AF_INET;
	proxy_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	proxy_addr.sin_port = htons(portno);
	
	/* Bind */
	if (bind(listen_sock, (struct sockaddr *) &proxy_addr, sizeof(proxy_addr)) < 0)
		error("Binding failed");
	
	// TODO: 20?
	/* Listen */
	if (listen(listen_sock, 20) < 0)
		error("Listening failed");
	
	while(true){
		
		
		
		
	
	}
	
	return 0;
}
