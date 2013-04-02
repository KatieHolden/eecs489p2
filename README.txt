Katie Holden - kaholden
Amanda Le - xinning
EECS 489 Project 2 

Description:
This is code to run a multithread web-proxy. It will listen for incoming requests from a client, parse the request and send it to the appropriate remote server. If there are errors while parsing the client request, the appropriate HTTP response message will be sent to the client. For small errors like incorrect HTTP version number or incorrect line terminators, the proxy will still accept the client request, and fix the errors before sending the request on to the remote server. The proxy will wait for a response from the remote server and send the response, as is, back to the client.

Design Decisions:
- We decided to make one thread per HTTP request. We thought that this would allow the proxy to service multiple requests without loosing performance, and would scale well when working with a browser.

- When we read the request from the client, we store it in a large buffer. We chose to do this, because we needed to parse the client request, rather than send it as-is. We stop reading from the client when we have read the \r\n\r\n sequence.

- When we read the HTTP response from the remote server, we chose to read the message byte-by-byte. We did this because we just have to send the message as-is back to the client. Also, the response message could be very large, since it contains the HTML for the page. It is unlikely that this would fit in a large buffer. So, in a loop, we read a byte and immediately send it to the client.

- Lastly, when parsing the client request, if we come across a "Connection: " header, we do not send this HTTP header/value pair to the remote server. The value of the attribute is most likely "keep-alive". Keeping TCP connections alive slows down the response time significantly.
 