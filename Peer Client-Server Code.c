#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>
#include <errno.h>
#include <fcntl.h>

#define BUFLEN 100
#define FILEBUF 1000 // The project document didnt specify the size of the file buffer, so i made it 200

struct pdu
{
    char type;
    char data[100];
};

struct contentPDU
{
    char type;
    char data[500];
};

int main(int argc, char **argv)
{
    char *host = "localhost";
    int port = 3000;
    struct hostent *phe;
    struct sockaddr_in sin;
    struct sockaddr_in contentServer;
    struct sockaddr_in client;
    int s, n, clientFile, myPort;
    int contentNum = 0;
    char name[10];
    char contentBuf[FILEBUF];
    char contentList[10][10];

    switch (argc)
    {
    case 1:
        fprintf(stderr, "Error: args not valid.\n");
        exit(1);
    case 2:
        host = argv[1];
        break;
    case 3:
        host = argv[1];
        port = atoi(argv[2]);
        break;
    default:
        fprintf(stderr, "Error: args not valid.\n");
        exit(1);
    }

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);

    /* Resolve host */
    phe = gethostbyname(host);
    if (phe == NULL)
    {
        fprintf(stderr, "Unable to resolve host: %s\n", host);
        exit(1);
    }
    memcpy(&sin.sin_addr, phe->h_addr, phe->h_length);

    // UDP Socket
    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0)
    {
        perror("Can't create socket");
        exit(1);
    }

    // Connect to the index server
    if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) < 0)
    {
        perror("Can't connect to server");
        exit(1);
    }

    // Get username from client
    printf("Enter your client name:\n");
    n = read(0, name, 10);
    name[strcspn(name, "\n")] = 0;

    char ch;
    printf("Choose an option from the menu: \n");
    printf("1. Register content\n");
    printf("2. Download content\n");
    printf("3. Acquire list of available content\n");
    printf("4. Deregister content\n");
    printf("5. Quit\n");

    fd_set rfds, afds;
    int s2 = socket(AF_INET, SOCK_STREAM, 0);
    FD_ZERO(&afds);
    FD_SET(0, &afds); // stdin

    while (1)
    {

        struct pdu peerData;
        struct pdu indexData;
        struct contentPDU peerRequest;

        // Copy new socket to rfds
        memcpy(&rfds, &afds, sizeof(rfds));
        FD_SET(s2, &afds);

        // Select stdin or socket that needs service
        if (select(FD_SETSIZE, &rfds, NULL, NULL, NULL) < 0)
            perror("select error");

        // Handle input from STDIN
        if (FD_ISSET(0, &rfds))
        {

            ch = getchar();
            switch (ch)
            {

            // User wants to register content
            case '1':
            {
                char content[10];

                // Open a TCP socket to be used for download
                struct sockaddr_in reg_addr;
                s2 = socket(AF_INET, SOCK_STREAM, 0);
                reg_addr.sin_family = AF_INET;
                reg_addr.sin_port = htons(0);
                reg_addr.sin_addr.s_addr = htonl(INADDR_ANY);

                // Bind the TCP socket
                if (bind(s2, (struct sockaddr *)&reg_addr, sizeof(reg_addr)) < 0)
                {
                    perror("Bind failed");
                    break;
                }

                // Retrieve port number
                int alen = sizeof(struct sockaddr_in);
                getsockname(s2, (struct sockaddr *)&reg_addr, &alen);
                myPort = ntohs(reg_addr.sin_port);

                // Socket can now listen for incoming connections
                if (listen(s2, 5) < 0)
                {
                    perror("Listen error");
                    break;
                }

                // Send R pdu to index server
                peerData.type = 'R';

                // Get content name from client
                printf("Enter the name of the content to register:\n");
                n = read(0, content, 10);
                content[strcspn(content, "\n")] = 0;

                // Format string to send to index server
                snprintf(peerData.data, 100, "%s %s %d", name, content, myPort);

                // Send the PDU
                if (send(s, &peerData, sizeof(peerData), 0) < 0)
                    perror("send error");

                if (recv(s, &indexData, sizeof(indexData), 0) < 0)
                    perror("recv error");

                // print message from index server
                printf("%s\n", indexData.data);

                // Add registered content to peers list of content
                strncpy(contentList[contentNum], content, 10);

                // Increment number of contents peer has registered
                contentNum++;

                break;
            }

            // User wants to download content
            case '2':
            {
                char content[10];
                
                // Send S type pdu
                peerData.type = 'S';

                // Name of content to download
                printf("Enter the name of the content to download:\n");
                n = read(0, content, 10);
                content[strcspn(content, "\n")] = 0;

                // Format string to send to index server
                snprintf(peerData.data, 100, "%s %s", name, content);

                /// Send S type PDU with data
                if (send(s, &peerData, sizeof(peerData), 0) < 0)
                {
                    perror("send error");
                }

                struct pdu indexData;

                if (recv(s, &indexData, sizeof(indexData), 0) < 0)
                    perror("recv error");

                // We need to tokenize the string sent back by the index server to setup
                // TCP connection with port # and IP address
                char *token = strtok(indexData.data, " ");
                char contentServAddr[20]; // Hold content server IP addr
                int contentServPort;      // Hold content server port #

                // First token is IP address of content server
                if (token)
                    strncpy(contentServAddr, token, sizeof(contentServAddr) - 1);

                // Next token is port number of content server
                token = strtok(NULL, " ");

                contentServPort = atoi(token);

                // Etract address and port for content server peer
                printf("Address: %s Port: %d\n", contentServAddr, contentServPort);

                // Server address structure
                memset(&contentServer, 0, sizeof(struct sockaddr_in));
                contentServer.sin_family = AF_INET;
                contentServer.sin_port = htons(contentServPort);

                // IP address is a string, convert to binary
                inet_pton(AF_INET, contentServAddr, &contentServer.sin_addr);

                int clientSock = socket(AF_INET, SOCK_STREAM, 0);
                // Set up TCP conection
                if (connect(clientSock, (struct sockaddr *)&contentServer, sizeof(contentServer)) == -1) // Fails
                {
                    // Debugging info
                    fprintf(stderr, "Can't connect \n");
                    printf("Error: %d\n", errno);
                    printf("%s\n", strerror(errno));
                    break;
                }

                // Succeeds
                else
                {
                    printf("Connection established!\n");
                    // Send D type PDU with filename to request file
                    peerData.type = 'D';

                    char sbuf[BUFLEN];
                    int checkFail = 0;
                    int bytesRead;

                    snprintf(sbuf, 100, "%c %s", peerData.type, content);
                    printf("%s\n", sbuf);

                    // Write the PDU to the socket
                    if (write(clientSock, sbuf, sizeof(sbuf)) < 0)
                    {
                        printf("Write failed: %d.", errno);
                        printf("%s\n", strerror(errno));
                        close(clientSock);
                    }

                    // open a file for storing downloaded content
                    clientFile = open(content, O_WRONLY | O_CREAT | O_TRUNC, 0644);

                    // Read the incoming bytes
                    while ((bytesRead = read(clientSock, contentBuf, FILEBUF)) > 0)
                    {   
                        // Invalid PDU type, should be C
                        if (contentBuf[0] != 'C')
                        {
                            printf("Invalid PDU type: %c\n", contentBuf[0]);
                            checkFail = 1;
                            break;
                        }
                        // Write to the file
                        write(clientFile, contentBuf + 1, bytesRead - 1);
                    }

                    // Close file when no more content to read
                    close(clientFile);

                    // Download fails
                    if (checkFail) {
                        printf("Download failed. Please try again.\n");
                    }

                    // Download succeeds
                    else {

                        // Inform user download has completed
                        printf("You have successfully downloaded: %s\n", content);

                        // Register as content server with index server
                        peerData.type = 'R';

                        // Format string to send to index server
                        snprintf(peerData.data, 100, "%s %s %d", name, content, myPort);

                        /* Send the PDU */
                        if (send(s, &peerData, sizeof(peerData), 0) < 0)
                            perror("send error");

                        if (recv(s, &indexData, sizeof(indexData), 0) < 0)
                            perror("recv error");

                        printf("%s\n", indexData.data);

                        // Increment number of contents peer has registered
                        contentNum++;
                    }

                }

                // Close
                close(clientSock);
                break;
            }

            // User asks for list of content
            case '3':
            {
                // Send O type PDU
                peerData.type = 'O';
                sprintf(peerData.data, "Requesting list\n");

                if (send(s, &peerData, sizeof(peerData), 0) < 0)
                {
                    perror("send error");
                }

                printf("List of available content:\n");
                while (strcmp(indexData.data, "------------") != 0)
                {
                    if (recv(s, &indexData, sizeof(indexData), 0) < 0)
                        perror("recv error");
                    printf("%s\n", indexData.data);
                }

                break;
            }

            // Deregister
            case '4':
            {
                peerData.type = 'T';
                char content[10];
                // Send T PDU to deregsiter
                printf("Enter the content name you wish to deregister:\n");
                n = read(0, content, 10);
                content[strcspn(content, "\n")] = 0;

                snprintf(peerData.data, 100, "%s %s", name, content);

                if (send(s, &peerData, sizeof(peerData), 0) < 0)
                    perror("send error");

                if (recv(s, &indexData, sizeof(indexData), 0) < 0)
                    perror("recv error");

                // Reorganize content list for peer
                if (indexData.type == 'A') {

                    for (int i = 0; i < contentNum; i++) {
                        // Search for deregistered content
                        if (strcmp(content, contentList[i]) == 0) {
                            for (int j = i; j < contentNum-1; j++)
                                strncpy(contentList[j], contentList[j+1], 10);
                        }
                    }
                    // Decrement number of contents registered
                    contentNum--;

                    for (int i = 0; i < contentNum; i++) {
                        printf("Content: %s\n", contentList[i]);
                    }
                }
                printf("%s\n", indexData.data);
               
                break;
            }

            // User quits and deregisters all content
            case '5':
            {
                // All content is deregisterd
                peerData.type = 'T';

                // Iterate through list of registered content
                for (int i = 0; i < contentNum; i++)
                {

                    snprintf(peerData.data, 100, "%s %s", name, contentList[i]);

                    /* Send the PDU */
                    if (send(s, &peerData, sizeof(peerData), 0) < 0)
                        perror("send error");

                    if (recv(s, &indexData, sizeof(indexData), 0) < 0)
                        perror("recv error");

                    printf("%s \n", indexData.data);
                }
                printf("Successfully logged off.\n");

                exit(0);
            }
            }
        }

        if (FD_ISSET(s2, &rfds))
        {
            char buf1[FILEBUF];
            int fd;
            int i;
            int len = sizeof(client);

            // Accept the peer trying to connect
            int new_sd = accept(s2, (struct sockaddr *)&client, &len);

            // Receive the PDU from the client
            int bytes = read(new_sd, buf1, BUFLEN);

            if (bytes < 0)
            {
                perror("Recv error");
                printf("%s\n", strerror(errno));
            }

            // Check PDU type
            peerRequest.type = buf1[0];

            strncpy(peerRequest.data, buf1 + 2, strlen(buf1) - 2);
            peerRequest.data[strlen(peerRequest.data)] = '\0';

            printf("%s\n", peerRequest.data);

            // Check to ensure PDU type is D
            if (peerRequest.type == 'D')
            {
                // Begin sending content to peer
                printf("Type: %c. Data: %s\n", peerRequest.type, peerRequest.data);

                // Open the file requested by the user
                fd = open(peerRequest.data, O_RDONLY);

                // File doesnt exist, inform peer with E PDU
                if (fd == -1)
                {
                    printf("%d\n", errno);
                    printf("%s\n", strerror(errno));
                    buf1[0] = 'E';
                    write(new_sd, buf1, 1);
                    close(new_sd);
                }

                // File exists, mark the first byte as 'C' and send it
                else
                {
                    while ((i = read(fd, buf1 + 1, FILEBUF - 1)) > 0)
                    {   
                        // Mark as C type PDU and write to requesting peer
                        buf1[0] = 'C';
                        write(new_sd, buf1, i + 1);

                    }
                }
                close(fd);
                // End of download
            }

            // Error
            else
            {
                send(new_sd, "Error: unrecognized PDU type", 29, 0);
            }

            // clear the buffer
            memset(buf1, 0, 200);
            // Close the socket
            close(new_sd);
        }
        // Clear data
        memset(peerData.data, 0, 100);
        memset(indexData.data, 0, 100);
        memset(peerRequest.data, 0, 100);
        
    }

    // End of main function
    close(s);
    return 0;
}