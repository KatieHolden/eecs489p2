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

void work_dispatcher(int sock);
void * worker(void * ptr);
void send_bad_request(int sock);
void send_no_impl(int sock);

// use this buffer size whenever you want to open a buffer to store data
const int BUFFER_SIZE = 1000000;
// the proxy should allow up to 10 concurrent connections
const int MAX_CONCURRENT = 10;

//print error
void error(const std::string& msg)
{
	perror(msg.c_str());
	exit(0);
}

using namespace std;

int main(int argc, char * argv[]) 
{
	
	if (argc < 2) {
		fprintf(stderr, "ERROR, no port provided\n");
		exit(1);
	}
	
	// get port number from command line
	int listen_portno = atoi(argv[1]);
	
	//this ignores broken pipe errors
	signal(SIGPIPE, SIG_IGN);
	 
	// create a socket to listen on requests
	int listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_sock < 0)
		error("Creating socket failed");
	
	int opVal = 1;
	setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opVal, sizeof(int));
	
	// build the proxy's internet address
	struct sockaddr_in proxy_addr;
	proxy_addr.sin_family = AF_INET;
	proxy_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	proxy_addr.sin_port = htons(listen_portno);
	
	// bind
	if (bind(listen_sock, (struct sockaddr *) &proxy_addr, (unsigned long) sizeof(proxy_addr)) < 0)
		error("Binding failed");
	
	// listen
	if (listen(listen_sock, MAX_CONCURRENT) < 0)
		error("Listening failed");
	
	int connection_sock;
	//proxy server accepts connections
	while(true) {
		connection_sock = accept(listen_sock, NULL, NULL);
		if (connection_sock < 0) {
			perror("Accepting failed");
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
		perror("Cannot create the thread.");
		send_bad_request(sock);
		// close the connection socket
		close(sock);
		delete cur_sock;
	}
}

void * worker(void * ptr)
{	
	int *client_sock_ptr = (int*) ptr;
	int client_sock = *client_sock_ptr;
	
	// get request from the client
	char buffer[BUFFER_SIZE];
	bzero(buffer, sizeof(buffer));
	int buffer_index = 0;
	int byte_remaining = BUFFER_SIZE;
	int byte_read = 0;
	
	//reading from client socket
	while (true) {
		byte_read = read(client_sock, (void *) &buffer[buffer_index], byte_remaining);
		buffer_index += byte_read;
		byte_remaining -= byte_read;
		if (buffer_index > 3 && buffer[buffer_index-4] == '\r' &&
				buffer[buffer_index-3] == '\n' && buffer[buffer_index-2] == '\r'
				&& buffer[buffer_index-1] == '\n') {
			break;
		}
	}
	
	// parsing the request message
	stringstream ss(buffer);
	string method, url, version, host, path, header, value;
	int portno = 80;
	bool isRelative = false;
	
	ss >> method >> url >> version;
	
	// check the method and make sure it is GET, if not, send 501 error
	if (method != "GET") {
		perror("Method is not GET");
		send_no_impl(client_sock);
		close(client_sock);
		delete client_sock_ptr;
		return 0;
	}
	
	// check this is a HTTP request, if not, send 400 error
	if (version.size() < 4 || version.substr(0, 4) != "HTTP") {
		perror("This is not a HTTP request");
		send_bad_request(client_sock);
		close(client_sock);
		delete client_sock_ptr;
		return 0;
	}
	
	// check whether url is relative or absolute
	if (url[0] == '/') {
		isRelative = true;
		path = url;
	}
	
	// check other header format, and store all <header_name, header_value> pairs in map
	map<string, string> header_map;
	while(ss >> header) {
		//header name must end with colon
		if(*(header.end() - 1) != ':') {
			perror("Header name format is wrong");
			send_bad_request(client_sock);
			close(client_sock);
			delete client_sock_ptr;
			return 0;
		}
		
		if(getline(ss, value)) {
			if(*(value.end() - 1) == ':') {
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
		
		//remove \r character from the header value string
		int value_size = value.size();
		value = value.substr(0, value_size-1);
		
		if(header == "Host:")
			host = value.substr(1);
		
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
		
		// find http:// and take it out
		if (url.length() >= 7 && url.substr(0, 7) == "http://") {
			url = url.substr(7);
		}
		
		// get the last index of host, host always ends with '/'
		size_t pos_tail = url.find_first_of("/", 0);
		if (pos_tail == string::npos) {
			perror("No / is found in url");
			send_bad_request(client_sock);
			close(client_sock);
			delete client_sock_ptr;
			return 0;
		}
		
		// path is everything after the slash, including the starting slash
		path = url.substr(pos_tail);
		
		// remove the path from url. now url may or may not have port number
		url = url.substr(0, pos_tail);
		
		// find port number, assign portno, and take it out of url
		size_t pos_port = url.find_first_of(":", 0);
		if(pos_port != string::npos && pos_port < pos_tail) {
			portno = atoi(url.substr(pos_port + 1, pos_tail - pos_port - 1).c_str());
			url = url.substr(0, pos_port);
		}
		
		host = url;
		
	}
		
	// form the request to server
	string request = "GET " + path + " HTTP/1.0\r\n" + "Host: " + host + "\r\n";
	for(map<string, string>::iterator it = header_map.begin(); it != header_map.end(); ++it) {
		//we have already included the Host name in the request and we don't want "Connection: keep-alive"
		//to be included in the request sent to the remote server
		if (it->first == "Host:" || it->first == "Connection:")
			continue;
		
		string header_line = it->first + it->second + "\r\n";
		request += header_line;
	}
	request += "\r\n";
		
	// start sending the request to server
	
	// create the socket to connect to server
	int server_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (server_sock < 0) {
		perror("Creating server_sock failed");
		send_bad_request(client_sock);
		close(client_sock);
		delete client_sock_ptr;
		return 0;
	}
	
	// get the server's DNS entry	
	struct hostent * server = gethostbyname(host.c_str());
	if (!server) {
		perror("No such host");
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
	if (connect(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
		perror("Connection failed");
		send_bad_request(client_sock);
		close(client_sock);
		delete client_sock_ptr;
		return 0;
	}
	
	// now connection to server is set up
		
	// send message to the server
	if (write(server_sock, request.c_str(), strlen(request.c_str())) < 0) {
		perror("Writing to socket failed1");
		send_bad_request(client_sock);
		close(server_sock);
		close(client_sock);
		delete client_sock_ptr;
		return 0;
	}
		
	// receive response from server byte by byte, and send to client immediately
	char byte;
	byte_read = -1;
	
	while (true) {
		// read one byte from the server
		byte_read = read(server_sock, (void*) &byte, 1);
		if (byte_read < 0) {
			perror("Reading from server failed");
			send_bad_request(client_sock);
			close(server_sock);
			close(client_sock);
			delete client_sock_ptr;
			return 0;
		}
		else if (byte_read == 0) { //if we are done reading response from server
			break;
		}
		
		// send one byte to client
		if (write(client_sock, (void*) &byte, 1) < 0) {
			perror("Writing to socket failed2");
			printf("errno is %d\n", errno);
			send_bad_request(client_sock);
			close(server_sock);
			close(client_sock);
			delete client_sock_ptr;
			return 0;
		}
	}
	
	// close connection to remote server	
	close(server_sock);
	
	// close connection to client
	close(client_sock);
	delete client_sock_ptr;
	return 0;
}

//this function sends a 400 - bad request HTTP response
void send_bad_request(int sock)
{
	const char * bad_request = "HTTP/1.0 400 BAD REQUEST\r\n";
	int tmp = -1;
	while (tmp < 0) {
		tmp = write(sock, bad_request, strlen(bad_request));
	}
}

//this function sends a 501 - not implemented HTTP response
void send_no_impl(int sock)
{
	const char * no_impl = "HTTP/1.0 501 NOT IMPLEMENTED\r\n";
	int tmp = -1;
	while (tmp < 0) {
		tmp = write(sock, no_impl, strlen(no_impl));
	}
}
