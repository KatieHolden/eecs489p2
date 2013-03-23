/******************************************
 *  EECS 489 PA 2
 *  
 *  proxy.c
 *  	usage: ./proxy <port>
 *
 *****************************************/

#include <iostream>
#include <string>
#include <sstream>
#include <map>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include "utility.h"

void work_dispatcher(int sock);
void* worker(void * ptr);

using namespace std;

int main(int argc, char * argv[]) 
{
	int listen_portno;
	int listen_sock;
	int conn_sock;
	struct sockaddr_in proxy_addr;
	//char buffer[BUFFER_SIZE];
	
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
	proxy_addr.sin_port = htons(listen_portno);
	
	/* Bind */
	if (bind(listen_sock, (struct sockaddr *) &proxy_addr, sizeof(proxy_addr)) < 0)
		error("Binding failed");
	
	// TODO: 20?
	/* Listen */
	if (listen(listen_sock, MAX_CONCURRENT) < 0)
		error("Listening failed");
	
	while(true){
		int connection_sock = accept(listen_sock, NULL, NULL);
		if (connection_sock < 0)
			error("Accepting failed");
		
		work_dispatcher(connection_sock);
	}
	
	return 0;
}

void work_dispatcher(int sock)
{
	// create a pthread
	pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setstacksize(&attr, 1024 * 1024);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	
  pthread_t worker_pthread;
	if (pthread_create(&worker_pthread, &attr, worker, (void*) sock)) {
		// close the connection socket
		close(sock);
		error("Cannot create the thread.");
	}
	
	close(sock);
}

void* worker(void * ptr)
{
	int *connection_socket_ptr = (int*) ptr;
	int connection_socket = *connection_socket_ptr;
	
	char buffer[BUFFER_SIZE];
	
	bzero(buffer, sizeof(buffer));
	if (read(connection_socket, buffer, BUFFER_SIZE) < 0)
		error("Reading from socket failed");
	printf("%s", buffer);
	
	// parsing the msg...
	stringstream ss(buffer);
	string method, url, version, host_header, path, header, value;
	int portno = 80;
	bool isRelative = false;
	
	ss >> method >> url >> version;
	// check the method
	if (method != "GET") {
		// TODO: 501
	}
	
	// check whether relative or absolute
	if (url[0] == '/') {
		isRelative = true;
		path = url;
		
		// find the host header
		string tmp;
		ss >> tmp;
		if (tmp == "Host:") {
			ss >> host_header;
		}
		else {
			// TODO: 400
		}
	}
	
	// check other header format
	map<string, string> header_map;
	while(ss >> header){
		
		if(*header.end() != ':'){
				//bad request error
		}
		
		if(ss >> value){
			if(*value.end() == ':'){
					//bad request error
			}
		}
		else{
			//bad request
		}
		
		header_map[header] = value;
	}
	
	// parse the url, only for absolute 
	if (!isRelative) {
		if (url.substr(0, 6) != "http:/") {
			// bad request
		}
		
		int pos_head = url.find("www");
		if (pos_head == string::npos) {
			// bad request
		}
		
		int pos_tail = url.find_first_of("/", pos_head);
		if (pos_tail == string::npos) {
			// bad request
		}
			
		host_header = url.substr(pos_head, pos_tail - pos_head + 1);
		path = url.substr(pos_tail);
		
	}
	
	// form the request
	string request = "GET " + path + " HTTP/1.0\r\n" + "Host: " + host_header + "\r\n";
	for(map<string, string>::iterator it = header_map.begin(); it != header_map.end(); ++it) {
		string header_line = it->first + " " + it->second + "\r\n";
		request += header_line;
	}
	
	// send the request to server
	int sock;
	struct hostent * server;
	struct sockaddr_in server_addr;
	
	// create the socket
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0)
		error("Creating socket failed");
	
	// Get the server's DNS entry
	server = gethostbyname(host_header.c_str());
	if (!server)
		error("No such host");
	
	// Build the server's internet address
	server_addr.sin_family = AF_INET;
	bcopy((char *) server->h_addr, (char *) &server_addr.sin_addr.s_addr, server->h_length);
	server_addr.sin_port = htons(portno);
	
	// Create a connection with the server
	if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
		error("Connection failed");
	
	// Send message to the server
	if (write(sock, request.c_str(), strlen(request.c_str())) < 0)
		error("Writing to socket failed");
	
	// Receive response from server
	char response_buffer[BUFFER_SIZE];
	if (read(sock, response_buffer, BUFFER_SIZE) < 0)
		error("Reading from socket failed");
	
	// send the response back to client
	if (write(connection_socket, response_buffer, strlen(response_buffer)) < 0)
		error("Writing to socket failed");
	
	// Close the connection
	close(sock);
}
