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
	if (argc < 2) {
		fprintf(stderr, "ERROR, no port provided\n");
		exit(1);
	}
	
	// get port number from command line
	int listen_portno = atoi(argv[1]);
	
	// create a socket to listen on requests
	int listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_sock < 0)
		error("Creating socket failed");
	
	// build the proxy's internet address
	struct sockaddr_in proxy_addr;
	proxy_addr.sin_family = AF_INET;
	proxy_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	proxy_addr.sin_port = htons(listen_portno);
	
	// bind
	if (bind(listen_sock, (struct sockaddr *) &proxy_addr, sizeof(proxy_addr)) < 0)
		error("Binding failed");
	
	// listen
	if (listen(listen_sock, MAX_CONCURRENT) < 0)
		error("Listening failed");
	
	int connection_sock;
	while(true) {
		//cout << "### should be here 1..." << endl;
		
		connection_sock = accept(listen_sock, NULL, NULL);
		if (connection_sock < 0) {
			error("Accepting failed");
			continue;
		}		
		
		// now the client is connected to the proxy
		
		cout<<"### sock is " << connection_sock << endl;
		
		//cout << "### should be here 2..." << endl;
		
		work_dispatcher(connection_sock);
	}
	
	return 0;
}

void work_dispatcher(int sock)
{
	//cout << "### into work_dispatcher" << endl;
	
	// create a pthread
	pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setstacksize(&attr, 1024 * 1024);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	
  pthread_t worker_pthread;
	int * cur_sock = new int(sock);
	
	
	if (pthread_create(&worker_pthread, &attr, worker, (void*) cur_sock) != 0) {
		// close the connection socket
		close(sock);
		perror("Cannot create the thread.");
	}
}

void* worker(void * ptr)
{	
	int *client_sock_ptr = (int*) ptr;
	int client_sock = *client_sock_ptr;
	
	char buffer[BUFFER_SIZE];
	const char * no_impl = "HTTP/1.0 501 NOT IMPLEMENTED \r\n";
	const char * bad_request = "HTTP/1.0 400 BAD REQUEST \r\n";
	
	//cout << "### into worker, with client_sock " << client_sock << endl;
	
	// get request from the client
	bzero(buffer, sizeof(buffer));
	
	cout << "### client_sock: " << client_sock << endl;
	if (read(client_sock, buffer, BUFFER_SIZE) < 0) {
		perror("Reading from socket failed");
		printf( "%s\n", strerror( errno ) );
		// TODO: do we need to send error msg back to client?
		
		return 0;
	}
	
	cout << "### " << buffer << endl;
	
	
	// parsing the request message
	stringstream ss(buffer);
	string method, url, version, host, path, header, value;
	int portno = 80;
	bool isRelative = false;
	
	ss >> method >> url >> version;
	
	// check the method and make sure it is GET
	if (method != "GET") {
		if (write(client_sock, no_impl, strlen(no_impl)) < 0) {
			perror("Writing to socket failed");
			close(client_sock);
			delete client_sock_ptr;
			return 0;
		}
	}
	
	cout << "### method is GET\n";
	
	// check this is a HTTP request
	if (version.substr(0, 4) != "HTTP") {
		if (write(client_sock, bad_request, strlen(bad_request)) < 0) {
			perror("This is not a HTTP request");
			close(client_sock);
			delete client_sock_ptr;
			return 0;
		}
	}
	
	cout << "### is a HTTP request\n";
	
	// check whether relative or absolute
	if (url[0] == '/') {
		isRelative = true;
		path = url;
	}
	
	// check other header format, and store all <header_name, header_value> pairs in map
	map<string, string> header_map;
	while(ss >> header) {
		
		if(*(header.end() - 1) != ':') {
			if (write(client_sock, bad_request, strlen(bad_request)) < 0) {
				perror("Writing to socket failed");
				close(client_sock);
				delete client_sock_ptr;
				return 0;
			}
		}
		
		if(ss >> value) {
			if(*(value.end() - 1) == ':') { //TODO: is it an error if header value ends with a ":" ? each line terminate with \r\n
				if (write(client_sock, bad_request, strlen(bad_request)) < 0) {
					perror("Writing to socket failed");
					close(client_sock);
					delete client_sock_ptr;
					return 0;
				}
			}
		}
		else {
			if (write(client_sock, bad_request, strlen(bad_request)) < 0) {
				perror("Writing to socket failed");
				close(client_sock);
				delete client_sock_ptr;
				return 0;
			}
		}

		if(header == "Host:")
			host = value;
		
		header_map[header] = value;
	}
	
	// error if the url is relative but no host header is found
	if(isRelative && header_map.find("Host:") == header_map.end()) {
		if (write(client_sock, bad_request, strlen(bad_request)) < 0) {
			perror("Writing to socket failed");
			close(client_sock);
			delete client_sock_ptr;
			return 0;
		}
	}
	
	// parse the url, only for absolute 
	if (!isRelative) {
		
		cout << "###\n";
		
		// get the starting index of host
		int pos_head = url.find("www");
		if (pos_head == string::npos) {
			if (write(client_sock, bad_request, strlen(bad_request)) < 0) {
				perror("Writing to socket failed");
				close(client_sock);
				delete client_sock_ptr;
				return 0;
			}
		}
		
		// get the last index of host which always ends with '/'
		int pos_tail = url.find_first_of("/", pos_head);
		if (pos_tail == string::npos) {
			if (write(client_sock, bad_request, strlen(bad_request)) < 0) {
				perror("Writing to socket failed");
				close(client_sock);
				delete client_sock_ptr;
				return 0;
			}
		}
		
		// get the portno if there is one. otherwise portno is set to 80
		int pos_port = url.find_first_of(":", pos_head);
		if(pos_port != string::npos && pos_port < pos_tail) {
			portno = atoi(url.substr(pos_port + 1, pos_tail - pos_port - 1).c_str());	
		}
		
		host = url.substr(pos_head, pos_tail - pos_head);
		path = url.substr(pos_tail);
	}
	
	// form the request to server
	string request = "GET " + path + " HTTP/1.0\r\n" + "Host: " + host + "\r\n";
	for(map<string, string>::iterator it = header_map.begin(); it != header_map.end(); ++it) {
		string header_line = it->first + " " + it->second + "\r\n";
		request += header_line;
	}
	
	cout << "### request to server is " << request << ", and portno is " << portno << endl;
	
	// send the request to server
	
	// create the socket to connect to server
	int server_sock = socket(AF_INET, SOCK_STREAM, 0);
	/*
	// if fail, try for two more times
	int count = 0;
	while (count < 2 && server_sock < 0) {
		server_sock = socket(AF_INET, SOCK_STREAM, 0);
		count++;
	}
	*/
	if (server_sock < 0) {
		perror("Creating server_sock failed");
		close(client_sock);
		delete client_sock_ptr;
		
		// TODO: do we need to send error msg back to client?
		
		return 0;
	}
	
	// get the server's DNS entry
	struct hostent * server = gethostbyname(host.c_str());
	if (!server) {
		perror("No such host");
		close(client_sock);
		delete client_sock_ptr;
		
		// TODO: do we need to send error msg back to client?
		
		return 0;
	}
	
	// build the server's internet address
	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	bcopy((char *) server->h_addr, (char *) &server_addr.sin_addr.s_addr, server->h_length);
	server_addr.sin_port = htons(portno);
	
	// create a connection with the server
	/*
	count = 0;
	int isConnected = connect(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
	while (cound < 2 && isConnected < 0) {
		isConnected = connect(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
		count++;
	}
	if (isConnected < 0) {
	*/
	if (connect(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
		perror("Connection failed");
		close(client_sock);
		delete client_sock_ptr;
		// TODO: do we need to send error msg back to client?
		
		return 0;
	}
	
	// now connection to server is set up
	cout << "### connection to server is set up\n";
	
	
	// send message to the server
	if (write(server_sock, request.c_str(), strlen(request.c_str())) < 0) {
		perror("Writing to socket failed");
		close(client_sock);
		delete client_sock_ptr;
		// TODO: do we need to send error msg back to client?
		
		close(server_sock);
		return 0;
	}
	
	cout << "### wrote to sever\n";
	
	// receive response from server and store in buffer
	char response_buffer[BUFFER_SIZE];
	if (read(server_sock, response_buffer, BUFFER_SIZE) < 0) {
		perror("Reading from socket failed");
		close(client_sock);
		delete client_sock_ptr;
		// TODO: do we need to send error msg back to client?
		
		close(server_sock);
		return 0;
	}
	
	cout << "### read from server\n";

	// close connection to remote server	
	close(server_sock);

	// send the response back to client
	if (write(client_sock, response_buffer, strlen(response_buffer)) < 0) {
		perror("Writing to socket failed");
		close(client_sock);
		delete client_sock_ptr;
		// TODO: do we need to send error msg back to client?
		
		return 0;
	}
	
	close(client_sock);
	delete client_sock_ptr;
}
