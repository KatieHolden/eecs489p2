(check this logic in office hours)
inside while loop:

	- create thread
	- check whether request is formatted correctly
		maybe fix small things
		if it's too incorrect, return error
	- check that request is GET method
		if not, return Error message
	- if relative path, check that header is included
	- parse request
	- create sending socket
	- send request to server
	- wait for response
	- send response to client. 


Other Quesitons:

- how long should the queue be on the proxy server's listening socket
- if something is invalid, do we send and HTTP response, or our own type of message? And what should that response look like?
- How many "small errors" are we supposed to accept? And what types?
- Is the Host header guarenteed to come after the first line? Optionally followed by other header?
- are we only responsible for http requests?
- What will it look like, when the port is specified?
- How do you type CRLF \r\n
- HTTP/1.0 do we 'fix this' or just type it in? 