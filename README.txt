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

	If we can't connect to the sever should we just terminate the program, or try to reconnect?
	When we get and error through communication with the server, do we need to send and error message to the client?
	Can we have other things before "www" besides "http://"?
	How do we determine whether this is a correctly formatted http request or not if there is no http?
	Help us set up testing?
	
