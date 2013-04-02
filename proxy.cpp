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
void * worker(void * ptr);
void send_bad_request(int sock);
void send_no_impl(int sock);

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
		connection_sock = accept(listen_sock, NULL, NULL);
		if (connection_sock < 0) {
			error("Accepting failed");
			continue;
		}		
		
		// now the client is connected to the proxy
		
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
	int * cur_sock = new int(sock);
	
	
	if (pthread_create(&worker_pthread, &attr, worker, (void*) cur_sock) != 0) {
		// close the connection socket
		close(sock);
		perror("Cannot create the thread.");
	}
}

void * worker(void * ptr)
{	
	int *client_sock_ptr = (int*) ptr;
	int client_sock = *client_sock_ptr;
	
	//const char * no_impl = "HTTP/1.0 501 NOT IMPLEMENTED \r\n";
	//const char * bad_request = "HTTP/1.0 400 BAD REQUEST \r\n";
	
	// get request from the client
	char buffer[BUFFER_SIZE];
	bzero(buffer, sizeof(buffer));
	if (read(client_sock, buffer, BUFFER_SIZE) < 0) {
		perror("Reading from socket failed");
		// TODO: do we need to send error msg back to client?
		
		return 0;
	}
	
	//cout << "### " << buffer << endl;
	
	// parsing the request message
	stringstream ss(buffer);
	string method, url, version, host, path, header, value;
	int portno = 80;
	bool isRelative = false;
	
	ss >> method >> url >> version;
	
	// check the method and make sure it is GET
	if (method != "GET") {
		perror("Method is not GET");
		send_no_impl(client_sock);
		close(client_sock);
		delete client_sock_ptr;
		return 0;
	}
	
	//cout << "### method is GET\n";
	
	// check this is a HTTP request
	if (version.substr(0, 4) != "HTTP") {
		perror("This is not a HTTP request");
		send_bad_request(client_sock);
		close(client_sock);
		delete client_sock_ptr;
		return 0;
	}
	
	//cout << "### is a HTTP request\n";
	
	// check whether relative or absolute
	if (url[0] == '/') {
		isRelative = true;
		path = url;
	}
	
	// check other header format, and store all <header_name, header_value> pairs in map
	map<string, string> header_map;
	while(ss >> header) {
		if(*(header.end() - 1) != ':') {
			perror("Header name format is wrong");
			send_bad_request(client_sock);
			close(client_sock);
			delete client_sock_ptr;
			return 0;
		}
		
		if(getline(ss, value)) {
			if(*(value.end() - 1) == ':') { //TODO: each line terminate with \r\n
				perror("Header value format is wrong");
				send_bad_request(client_sock);
				close(client_sock);
				delete client_sock_ptr;
				return 0;
			}
		}
		else {
			perror("Header name and value pair does not match");
			send_bad_request(client_sock);
			close(client_sock);
			delete client_sock_ptr;
			return 0;
		}

		if(header == "Host:")
			host = value;
		
		header_map[header] = value;
	}
	
	// error if the url is relative but no host header is found
	if(isRelative && header_map.find("Host:") == header_map.end()) {
		perror("No host header is found for the relative url");
		send_bad_request(client_sock);
		close(client_sock);
		delete client_sock_ptr;
		return 0;
	}
	
	// parse the url, only for absolute 
	if (!isRelative) {
		
		// get the starting index of host
		int pos_head = url.find("www");
		if (pos_head == string::npos) {
			perror("No www is found in url");
			send_bad_request(client_sock);
			close(client_sock);
			delete client_sock_ptr;
			return 0;
		}
		
		// get the last index of host which always ends with '/'
		int pos_tail = url.find_first_of("/", pos_head);
		if (pos_tail == string::npos) {
			perror("No / is found in url");
			send_bad_request(client_sock);
			close(client_sock);
			delete client_sock_ptr;
			return 0;
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
	request += "\r\n";
	
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
		// TODO: do we need to send error msg back to client?
		send_bad_request(client_sock);
		close(client_sock);
		delete client_sock_ptr;
		return 0;
	}
	
	// get the server's DNS entry
	struct hostent * server = gethostbyname(host.c_str());
	if (!server) {
		perror("No such host");
		// TODO: do we need to send error msg back to client?
		send_bad_request(client_sock);
		close(client_sock);
		delete client_sock_ptr;
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
		// TODO: do we need to send error msg back to client?
		send_bad_request(client_sock);
		close(client_sock);
		delete client_sock_ptr;
		return 0;
	}
	
	// now connection to server is set up
	//cout << "### connection to server is set up\n";
	
	
	// send message to the server
	if (write(server_sock, request.c_str(), strlen(request.c_str())) < 0) {
		perror("Writing to socket failed");
		// TODO: do we need to send error msg back to client?
		send_bad_request(client_sock);
		close(client_sock);
		delete client_sock_ptr;
		close(server_sock);
		return 0;
	}
	
	//cout << "### wrote to sever\n";
	
	// receive response from server byte by byte, and send to client immediately
	char byte;
	int byte_read = -1;
	char last_four_bytes[4] = {' ', ' ', ' ', ' '};
	int line_terminator_cnt = 0;
	
	
	while (true) {
		
		// read one byte from the server
		byte_read = read(server_sock, (void*) &byte, 1);
		if (byte_read < 0) {
			perror("Reading from server failed");
			// TODO: do we need to send error msg back to client?
			send_bad_request(client_sock);
			close(client_sock);
			delete client_sock_ptr;
			close(server_sock);
			return 0;
		}
		else if (byte_read == 0) {
			perror("Server close the connection");
			send_bad_request(client_sock);
			close(client_sock);
			delete client_sock_ptr;
			close(server_sock);
			return 0;
		}
		
		// send one byte to client
		if (write(client_sock, (void*) &byte, 1) < 0) {
			perror("Writing to socket failed");
			// TODO: do we need to send error msg back to client?
			send_bad_request(client_sock);
			close(client_sock);
			delete client_sock_ptr;
			close(server_sock);
			return 0;
		}
		
		// check for "\r\n\r\n"
		for (int i = 0; i < 3; ++i)
			last_four_bytes[i] = last_four_bytes[i+1];
		last_four_bytes[3] = byte;
		if (last_four_bytes[0] == '\r' && last_four_bytes[1] == '\n' && last_four_bytes[2] == '\r' && last_four_bytes[3] == '\n') {
			if (line_terminator_cnt == 0)
				line_terminator_cnt++;
			else
				break;
		}
	}
	 
	// close connection to remote server	
	close(server_sock);
	
	// close connection to client
	close(client_sock);
	delete client_sock_ptr;
}

void send_bad_request(int sock)
{
	const char * bad_request = "HTTP/1.0 400 BAD REQUEST \r\n";
	int tmp = -1;
	while (tmp < 0) {
		tmp = write(sock, bad_request, strlen(bad_request));
	}
}

void send_no_impl(int sock)
{
	const char * no_impl = "HTTP/1.0 501 NOT IMPLEMENTED \r\n";
	int tmp = -1;
	while (tmp < 0) {
		tmp = write(sock, no_impl, strlen(no_impl));
	}
}
