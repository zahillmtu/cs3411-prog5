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
#include <libgen.h>

#include <arpa/inet.h>

char* password = "notyourserver";


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

void example_connect(int sock, struct addrinfo *ai)
{
	if(connect(sock, ai->ai_addr, ai->ai_addrlen) == -1)
	{
		perror("connect() failure: ");
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

/**
 * Wrapper function to control recieving bytes from the socket
 * Set getVal to true when running the get command
 */
int recvBytes(int sock, char * recvbuf, int bufSize) {

    ssize_t recvdBytes = 0;

    recvdBytes = recv(sock, recvbuf, bufSize, 0);
    if(recvdBytes < 0) {
    	perror("recv:");
    	close(sock);
    	exit(EXIT_FAILURE);
    }
    else if(recvdBytes == 0) // server closed connection
    {
        fflush(stdout);
	close(sock);
	exit(EXIT_SUCCESS);
    }
    return recvdBytes;

}

/**
 * Wrapper function to control recieving bytes from the socket
 * Set waitVal to true when waiting for "Found"
 */
int getRecvBytes(int sock, char * recvbuf, int bufSize, int waitVal) {

    ssize_t recvdBytes = 0;

    recvdBytes = recv(sock, recvbuf, bufSize, 0);
    if(recvdBytes < 0) {
    	perror("recv:");
    	close(sock);
    	exit(EXIT_FAILURE);
    }
    else if(recvdBytes == 0) // server closed connection
    {
        if (waitVal) {
            printf("ERROR - Server did not find file. Exiting...\n");
            fflush(stdout);
	    close(sock);
	    exit(EXIT_FAILURE);
        }
    }
    return recvdBytes;

}

/**
 * Wrapper method to send bytes through the socket
 */
void sendBytes(int sock, char * bytes, size_t size) {

	if(send(sock, bytes, size, 0) == -1)
	{
		perror("send");
		closeSock(sock);
		exit(EXIT_FAILURE);
	}
}

/**
 * Method to interact with the server when the "list" command is sent
 */
void listCmd(int sock) {

        while(1) {

            char retBuf[1024];
            ssize_t ret = 0;
            memset(retBuf, 0, 1024);

            ret = recvBytes(sock, retBuf, 1024);

	    fwrite(retBuf, 1, ret, stdout);
        }
}

void getCmd(int sock, char * filename) {

    // Send the name of the file
    sendBytes(sock, filename, strlen(filename));

    // Recieve the found command
    char recvbuf[1024];
    memset(recvbuf, 0, 1024);
    getRecvBytes(sock, recvbuf, strlen("Found"), 1);
    if (strcmp(recvbuf, "Found") != 0) {
        printf("ERROR - Server could not find file.\n");
        exit(EXIT_FAILURE);
    }

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

        ret = getRecvBytes(sock, recvbuf, 1024, 0);
        if (ret == 0) {
            // server closed connection, close the file
            fclose(fp);
            exit(EXIT_SUCCESS);
        }
        fwrite(recvbuf, 1, ret, fp);
    }
}

void putCmd(int sock, char * filename) {

    printf("Sending file\n");

    // First send the name of the file
    char withnl[1024];
    memset(withnl, 0, 1024);
    strcat(withnl, filename);
    strcat(withnl, "\n");
    sendBytes(sock, withnl, strlen(withnl));

    FILE * fp = fopen(filename, "r");
    if (fp == NULL) { // the file did not exist
        // close the socket
        perror("fopen");
        closeSock(sock);
        exit(EXIT_FAILURE);
    }

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


int main(int argc, char* argv[])
{


        // args should have the hostname, the port, and commands
        if (argc < 4) {
            printf("Usage Error - must provide hostname, port, and command\n");
            exit(1);
        }
	char *hostname = argv[1];
	char *port = argv[2];

	// getaddrinfo(): network address and service translation:
	struct addrinfo *socketaddrinfo = example_getaddrinfo(hostname, port);

	// socket(): create an endpoint for communication
	int sock = example_socket(socketaddrinfo);

        printf("Connecting to server\n");
	// connect(): initiate a connection on a socket
	example_connect(sock, socketaddrinfo);
	freeaddrinfo(socketaddrinfo); // done with socketaddrinfo

        // send the password
        printf("Sending password\n");
        sendBytes(sock, password, strlen(password));

	char recvbuf[1024];
	ssize_t recvdBytes = 0;

        // Recieve connection confirmation
	recvdBytes = recvBytes(sock, recvbuf, 1024);
	fwrite(recvbuf, 1, recvdBytes, stdout);

        // send the command
        sendBytes(sock, argv[3], strlen(argv[3]));

        if (strcmp(argv[3], "list") == 0) {
            // Recieve the bytes and print them out
            if (argc > 4) {
                printf("USAGE ERROR - Too many arguments\n");
                exit(EXIT_FAILURE);
            }
            listCmd(sock);
        } else if (strcmp(argv[3], "get") == 0) {
            if (argc < 5 || argc > 5) {
                printf("ERROR - No filename specified. Exiting...\n");
                exit(EXIT_FAILURE);
            }
            getCmd(sock, argv[4]);
        } else if (strcmp(argv[3], "put") == 0) {
            if (argc < 5 || argc > 5) {
                printf("ERROR - No filename specified. Exiting...\n");
                exit(EXIT_FAILURE);
            }
            putCmd(sock, basename(argv[4]));
        }


        exit(EXIT_FAILURE);


	// Send an HTTP request:
	//char httpget[1024];
	//snprintf(httpget, 1024,
	//         "GET / HTTP/1.1\r\n"
	//         "Host: %s\r\n"
	//         "Connection: close\r\n\r\n",
	//         hostname);

	//ssize_t s = send(sock, httpget, strlen(httpget), 0);
	//if(s == -1)
	//{
	//	perror("send");
	//	exit(EXIT_FAILURE);
	//}
	//else if (s < (ssize_t)strlen(httpget))
	//{
	//	// If you want to ensure that the entire message gets sent,
	//	// you must loop and send the part of the message that didn't
	//	// get sent the first time.
	//	printf("Entire message did not get sent.");
	//}


	//while(1)
	//{
	//	char recvbuf[1024];
	//	ssize_t recvdBytes = 0;

	//	/* Read up to 1024 bytes from the socket. Even if we know that
	//	   there are far more than 1024 bytes sent to us on this
	//	   socket, recv() will only give us the bytes that the OS can
	//	   provide us. If there are no bytes for us to read (and the
	//	   connection is open and without error), recv() will block
	//	   until there are bytes available.

	//	   Note that it is possible to make recv() non-blocking with
	//	   fcntl(). */
	//	recvdBytes = recv(sock, recvbuf, 1024, 0);
	//	if(recvdBytes > 0) // print bytes we received to console
	//		fwrite(recvbuf, 1, recvdBytes, stdout);
	//	else if(recvdBytes == 0) // server closed connection
	//	{
	//		fflush(stdout);
	//		close(sock);
	//		exit(EXIT_SUCCESS);
	//	}
	//	else
	//	{   // error receiving bytes
	//		perror("recv:");
	//		close(sock);
	//		exit(EXIT_FAILURE);
	//	}
	//}
}

/**
 * References
 * ----------
 *  code from Dr. Kuhl
 *      https://github.com/skuhl/sys-prog-examples/blob/master/simple-examples/internet-stream-client.c
 *  guide by Beej
 *      http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html
 */
