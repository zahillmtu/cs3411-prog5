// Scott Kuhl
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <dirent.h>

#include <arpa/inet.h>

char* password = "notyourserver";
char* dir = "./files";

/**
 * Method to create ./files directory and move into it
 */
void createSubdir(void) {

    struct stat st = {0};

    // create the directory if it does not exist
    if (stat(dir, &st) == -1) {
        printf("Creating files subdirectory\n");
        if (mkdir(dir, 0700) == -1) {
            perror("mkdir");
            exit(1); // May not want to do this
        }
    }
    // change to the directory
    chdir(dir);
}

/* We will use getaddrinfo() to get an addrinfo struct which will
 * contain information the information we need to create a TCP
 * stream socket. The caller should use
 * freeaddrinfo(returnedValue) when the returend addrinfo struct
 * is no longer needed. */
struct addrinfo* example_getaddrinfo(const char* hostname, const char* port)
{
	/* First we need to create a struct that tells getaddrinfo() what
	 * we are interested in: */
	struct addrinfo hints; // a struct
	memset(&hints, 0, sizeof(hints)); // Set bytes in hints struct to 0
	hints.ai_family = AF_INET;    // AF_INET for IPv4, AF_INET6 for IPv6, AF_UNSPEC for either
	hints.ai_socktype = SOCK_STREAM;  // Use TCP

	// hints.ai_flags is ignored if hostname is not NULL. If hostname
	// is NULL, this indicates to getaddrinfo that we want to create a
	// server.
	hints.ai_flags = AI_PASSIVE;
	struct addrinfo *result;  // A place to store info getaddrinfo() provides to us.

	/* Now, call getaddrinfo() and check for error: */
	int gai_ret = getaddrinfo(hostname, port, &hints, &result);
	if(gai_ret != 0)
	{
		// Use gai_strerror() instead of perror():
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(gai_ret));
		exit(EXIT_FAILURE);
	}
	return result;
}

/* Create a socket. Returns a file descriptor. */
int example_socket(struct addrinfo *ai)
{
	int sock = socket(ai->ai_family,
	                  ai->ai_socktype,
	                  ai->ai_protocol);
	if(sock == -1)
	{
		perror("socket() error:");
		exit(EXIT_FAILURE);
	}
	return sock;
}

void example_setsockopt(int sock)
{
	/* This section of code is not strictly necessary for a server.
	   It eliminates a problem where you run your server, kill it, and
	   run it again on the same port but the call to bind() produces
	   an error. The solution is to add this line of code or to wait
	   a couple of minutes before running the server again. The
	   problem is that the server's socket gets stuck in the TIME_WAIT
	   state even though the server has been killed. When the server
	   restarts, it is unable to acquire that socket again unless you
	   wait for a long enough time.

	   If you comment out this code and want to see the socket reach
	   the TIME_WAIT state, run your server and run "netstat -atnp |
	   grep 8080" to see the server listening on the port. Next,
	   connect to the server with a client and re-run the netstat
	   command. You will see that the socket remains in the TIME_WAIT
	   state for a while (a couple minutes).
	*/
	int yes = 1;
#if 1
	if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) != 0)
	{
		perror("server: setsockopt() failed: ");
		close(sock);
		exit(EXIT_FAILURE);
	}
#endif


#if 0
	/* Keepalive periodically sends a packet after a connection has
	 * been idle for a long time to ensure that our connection is
	 * still OK. It typically is not needed and defaults to OFF.

	 http://www.tldp.org/HOWTO/html_single/TCP-Keepalive-HOWTO/
	 http://tools.ietf.org/html/rfc1122
	*/

	/* Check the status for the keepalive option */
	int optval;
	socklen_t optlen=sizeof(int);
	if(getsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &optval, &optlen) < 0) {
		perror("getsockopt()");
		close(sock);
		exit(EXIT_FAILURE);
	}
	printf("SO_KEEPALIVE is %s\n", (optval ? "ON" : "OFF"));

	/* Set the option active */
	yes = 1;
		if(setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &yes, sizeof(int)) < 0) {
			perror("setsockopt()");
			close(sock);
			exit(EXIT_FAILURE);
		}
	printf("SO_KEEPALIVE set on socket\n");
#endif

}

void example_bind(int sock, struct addrinfo *ai)
{
	if(bind(sock, ai->ai_addr, ai->ai_addrlen) == -1)
	{
		perror("server: bind() failed: ");
		printf("\n");
		printf("If bind failed because the address is already in use,\n");
		printf("it is likely because a server is already running on\n");
		printf("the same port. Only one server can listen on each port\n");
		printf("at any given time.\n");
		close(sock);
		exit(EXIT_FAILURE);
	}
}

void example_listen(int sock, int queueSize)
{
	/* Listen for incoming connections on the socket */
	if(listen(sock, queueSize)==-1)
	{
		printf("server: listen() failed: ");
		close(sock);
		exit(EXIT_FAILURE);
	}
}

/**
 * Wrapper method for closing the socket
 */
void closeSock(int sock) {
    if(close(sock) == -1)
    {
	perror("close");
	exit(EXIT_FAILURE);
    }
}

int recvBytes(int sock, char * recvbuf, int bufSize) {

    ssize_t recvdBytes = 0;

    recvdBytes = recv(sock, recvbuf, bufSize, 0);
    if(recvdBytes < 0) {
    	perror("recv:");
    	close(sock);
    	exit(EXIT_FAILURE);
    }
    return recvdBytes;
}

void sendBytes(int sock, char * bytes, size_t size) {

	if(send(sock, bytes, size, 0) == -1)
	{
		perror("send");
		closeSock(sock);
		exit(EXIT_FAILURE);
	}
}

/**
 * Method to validate the password sent to server from the client
 */
int checkPass(int sock) {

    char recvbuf[1024];
    memset(recvbuf, 0, 1024);

    recvBytes(sock, recvbuf, strlen(password));
    printf("Password received: %s\n", recvbuf);
    // check if it is equal to the password
    if(strcmp(recvbuf, password) == 0) {
        return 1;
    } else {
        return -1;
    }
}

/**
 * Method to send the file data to the client
 */
void sendList(int sock) {

    int numOfFiles;
    struct dirent **namelist;
    char buffer[1024];

    // gather all the file names
    numOfFiles = scandir(".", &namelist, NULL, alphasort);
    if (numOfFiles == -1) {
        printf("There was an error reading the directory - Exiting\n");
        exit(EXIT_FAILURE);
    }

    struct stat fileInfo;

    for (int i = 0; i < numOfFiles; i++) {

        // gather the file info
        if (stat(namelist[i]->d_name, &fileInfo) == -1) {
            perror("stat");
            exit(EXIT_FAILURE);
        }

        // put the information into a buffer
        snprintf(buffer, 1024, "%s\n", namelist[i]->d_name);

        // send the info to the client
	sendBytes(sock, buffer, strlen(buffer));

        //printf("%s %lld\n", namelist[i]->d_name, (long long) fileInfo.st_size);
        free(namelist[i]);
    }
    free(namelist);

    closeSock(sock);
}

/**
 * Method to send the bytes of a file to the client
 */
void getFile(int sock, char * filename) {

    printf("Looking for file %s\n", filename);

    FILE * fp = fopen(filename, "r");
    if (fp == NULL) { // the file did not exist
        // close the socket
        perror("fopen");
        closeSock(sock);
        return;
    }

    printf("Found the file %s, sending Found\n", filename);
    // Send bytes to say we found it
    sendBytes(sock, "Found", strlen("Found"));

    // Send the bytes of the file
    char buf[1024];
    memset(buf, 0, 1024);
    ssize_t retVal = 1024;

    while (retVal == 1024) {
        retVal = fread(buf, 1, 1024, fp);
        sendBytes(sock, buf, strlen(buf));
    }
    closeSock(sock);
    fclose(fp);
}

void putFile(int sock, char * filename) {

    printf("Putting in file %s\n", filename);

    char recvbuf[1024];
    memset(recvbuf, 0, 1024);

    // create a file to write the contents into
    FILE * fp = fopen(filename, "w");
    if (fp == NULL) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    // continually read the bytes and write the data
    while(1) {

        ssize_t ret = 0;
        memset(recvbuf, 0, 1024);

        ret = recvBytes(sock, recvbuf, 1024);
        if (ret == 0) {
            // server closed connection, close the file
            fclose(fp);
            return;
        }
        fwrite(recvbuf, 1, ret, fp);
    }

}

void getLine(int sock, char line[1024]) {

    ssize_t recvdBytes = 0;
    char byte[2];
    memset(line, 0, 1024);


    while (1) {
        memset(byte, 0, 2);
        recvdBytes = recv(sock, byte, 1, 0);
            if(recvdBytes < 0) {
            	perror("recv:");
            	close(sock);
            	exit(EXIT_FAILURE);
            }
        if (byte[0] == '\n') {
            break;
        }
        strcat(line, byte);
    }

}

int main(int argc, char* argv[])
{

        // args should only be the port number and the name
        // of the program running
        if (argc != 2) {
            printf("Usage Error - provide port number\n");
            exit(1);
        }

	char *port = argv[1];

        // create the ./files dir
        createSubdir();


	// getaddrinfo(): network address and service translation:
	struct addrinfo *socketaddrinfo = example_getaddrinfo(NULL, port);

	// socket(): create an endpoint for communication
	int sock = example_socket(socketaddrinfo);

	example_setsockopt(sock);
	example_bind(sock, socketaddrinfo);
	example_listen(sock, 128);
	freeaddrinfo(socketaddrinfo); // done with socketaddrinfo

	//printf("Press Ctrl+C to exit server.\n");
	//printf("Run a web browser on the same computer as you run the server and point it to:\n");
	//printf("http://127.0.0.1:%s\n", port);

	while(1)
	{
                printf("Waiting for connections...\n");
		// Get a socket for a particular incoming connection.
		int newsock = accept(sock, NULL, NULL);
		if(newsock == -1)
		{
			perror("accept(): ");
			continue; // try again if accept failed.
		}
                printf("Client connected\n");

                // Check the password of the client
                if (checkPass(newsock) == -1) {
                    printf("Incorrect password - Connection aborted\n");
                    closeSock(newsock);
                    continue;
                } else {
                    printf("Password accepted - Continuing connection\n");
                }

	        sendBytes(newsock, "You connected!\n", strlen("You connected!\n"));

                char cmd[1024];
                memset(cmd, 0, 1024);
                recvBytes(newsock, cmd, 1024);

                printf("Received command: %s\n", cmd);

                if (strcmp(cmd, "list") == 0) {
                    sendBytes(newsock, "File listing:\n", strlen("File listing:\n"));

                    // Scan the dirextory and send the file list
                    sendList(newsock);
                    continue;
                }

                if (strcmp(cmd, "get") == 0) {
                    char retBuf[1024];
                    memset(retBuf, 0, 1024);

                    // Read the name of the file
                    recvBytes(newsock, retBuf, 1024);

                    // Check if the file exists and send it
                    getFile(newsock, retBuf);
                    continue;
                }

                if (strcmp(cmd, "put") == 0) {
                    char retBuf[1024];
                    memset(retBuf, 0, 1024);

                    // Read the name of the file
                    getLine(newsock, retBuf);

                    // Check if the file exists and send it
                    putFile(newsock, retBuf);
                    continue;
                }

//		char http_body[] = "hello world";
//		char http_headers[1024];
//		snprintf(http_headers, 1024,
//		         "HTTP/1.0 200 OK\r\n"
//		         "Content-Length: %zu\r\n"
//		         "Content-Type: text/html\r\n\r\n",
//		         strlen(http_body)); // should check return value


		/* Note: send() does not guarantee that it will send all of
		 * the data that we ask it too. To reliably send data, we
		 * should inspect the return value from send() and then call
		 * send() again on any bytes did not get sent. */

		// Send the HTTP headers
		//if(send(newsock, http_headers, strlen(http_headers), 0) == -1)
		//{
		//	perror("send");
		//	close(newsock);
		//	exit(EXIT_FAILURE);
		//}

		//// Send the HTTP body
		//if(send(newsock, http_body, strlen(http_body), 0) == -1)
		//{
		//	perror("send");
		//	close(newsock);
		//	exit(EXIT_FAILURE);
		//}

		/* shutdown() (see "man 2 shutdown") can be used before
		   close(). It allows you to partially close a socket (i.e.,
		   indicate that we are done sending data but still could
		   receive). This could allow us to signal to a peer that we
		   are done sending data while still allowing us to receive
		   data. SHUT_RD will disallow reads from the socket, SHUT_WR
		   will disallow writes, and SHUT_RDWR will disallow read and
		   write. Also, unlike close(), shutdown() will affect all
		   processes that are sharing the socket---while close() only
		   affects the process that calls it. */
#if 0
		if(shutdown(newsock, SHUT_RDWR) == -1)
		{
			perror("shutdown");
			close(newsock);
			exit(EXIT_FAILURE);
		}
#endif

		closeSock(newsock);
	}
}

/**
 * References
 * ----------
 *  code from Dr. Kuhl
 *      https://github.com/skuhl/sys-prog-examples/blob/master/simple-examples/internet-stream-client.c
 *  Guide by Beej
 *      http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html
 */
