#include <iostream>
#include <string>

//Use this buffer size whenever you want to open a buffer to store data
const int BUFFER_SIZE = 1000000;

//The proxy should allow up to 10 concurrent connections
const int MAX_CONCURRENT = 10;

//print error
void error(const std::string& msg)
{
    perror(msg.c_str());
    exit(0);
}

