#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <stdio.h>
#include <arpa/inet.h>

#define MaxPeers 20

// Protocol Data Unit for index server and peer
struct pdu
{

    char type;
    char data[100];
};

// Struct for registered peers
struct registered
{

    char peerName[10];
    char peerContent[10];
    char peerAddress[20];
    int peerPort;
    int used; // If a
};

int main(int argc, char *argv[])
{

    struct sockaddr_in fsin; /* the from address of a client	*/
    char buf[100];           /* "input" buffer; any size > 0	*/
    char *pts;
    int sock;               /* server socket		*/
    int alen;               /* from-address length		*/
    struct sockaddr_in sin; /* an Internet endpoint address         */
    int s, type;            /* socket descriptor and socket type    */
    int port = 3000;
    int numPeers = 0;                     // Number of peers registered
    struct registered peerList[MaxPeers]; // Array to hold the peer data

    // Process command-line arguments
    switch (argc)
    {

    case 1:
        break;
    case 2:
        port = atoi(argv[1]);
        break;
    default:
        fprintf(stderr, "Usage: %s [port]\n", argv[0]);
        exit(1);
    }

    memset(&sin, 0, sizeof(sin)); // Socket address
    sin.sin_family = AF_INET;     // IPv4
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(port);

    /* Allocate a socket */
    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0)
        fprintf(stderr, "Can't create socket.\n");

    /* Bind the socket */
    if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0)
        fprintf(stderr, "Can't bind to port %d.\n", port);

    alen = sizeof(fsin);

    // Index server responds to PDUs sent by peers
    while (1)
    {

        struct pdu peerData;
        struct pdu indexData;

        // Recv incoming PDU
        int n = recvfrom(s, &peerData, sizeof(peerData), 0, (struct sockaddr *)&fsin, &alen);

        if (n < 0)
            fprintf(stderr, "recvfrom error\n");

        peerData.data[n] = '\0';

        printf("%c\n", peerData.type);
        printf("%s\n", peerData.data);

        // Index server takes action based on PDU type
        switch (peerData.type)
        {

        // Peer registers content
        case 'R':
        {

            int i;

            char *token = strtok(peerData.data, " ");
            char Rname[10] = {0}; // Initialize to zero to ensure proper null-termination
            char Rcontent[10] = {0};
            char Raddress[20] = {0};
            int port;

            // Get the name of the peer
            if (token)
                strncpy(Rname, token, sizeof(Rname) - 1);

            // Get the content name from the peer
            token = strtok(NULL, " ");
            if (token)
                strncpy(Rcontent, token, sizeof(Rcontent) - 1);

            // Get the IP address of the peer requesting registration
            inet_ntop(AF_INET, &fsin.sin_addr, Raddress, sizeof(Raddress));

            token = strtok(NULL, " ");

            // Port number
            port = atoi(token);

            // Displaying this info for debugging
            printf("Name: %s\n", Rname);
            printf("Content Name: %s\n", Rcontent);
            printf("Address: %s\n", Raddress);
            printf("Port: %d\n", port);

            char dest[100];

            int Rflag = 0;

            for (int i = 0; i < numPeers; i++)
            {
                // Error, peer with name and content already registered!
                if (strcmp(Rcontent, peerList[i].peerContent) == 0 && strcmp(Rname, peerList[i].peerName) == 0)
                {
                    // Set flag to 1
                    Rflag = 1;
                    // Error returned to peer
                    indexData.type = 'E';
                    i = sprintf(dest, "Peer with name %s and content %s already registered. Choose another name.\n", Rname, Rcontent);
                    dest[i - 1] = '\0';
                    strncpy(indexData.data, dest, sizeof(dest));
                    sendto(s, &indexData, sizeof(indexData), 0, (struct sockaddr *)&fsin, alen);
                    break;
                }
            }

            // If flag is not set, then proceed to register the peer
            if (!Rflag)
            {
                // Send ack to peer
                indexData.type = 'A';
                i = sprintf(dest, "Success. Peer %s with content %s registered.\n", Rname, Rcontent);
                dest[i - 1] = '\0';
                strncpy(indexData.data, dest, sizeof(dest));
                sendto(s, &indexData, sizeof(indexData), 0, (struct sockaddr *)&fsin, alen);

                // Add the registered peer to the list of peers
                if (numPeers < MaxPeers)
                {
                    // Add name to list of peers
                    strncpy(peerList[numPeers].peerName, Rname, sizeof(Rname) - 1);
                    peerList[numPeers].peerName[sizeof(Rname) - 1] = '\0';

                    // Add content name to list
                    strncpy(peerList[numPeers].peerContent, Rcontent, sizeof(Rcontent) - 1);
                    peerList[numPeers].peerContent[sizeof(Rcontent) - 1] = '\0';

                    // Add content IP address to list
                    strncpy(peerList[numPeers].peerAddress, Raddress, sizeof(Raddress) - 1);
                    peerList[numPeers].peerAddress[sizeof(Raddress) - 1] = '\0';

                    // Store peer address
                    peerList[numPeers].peerPort = port;
                    peerList[numPeers].used = 0;

                    numPeers++;
                }

                else
                {
                    indexData.type = 'E';
                    strcpy(indexData.data, "Peer list is full.\n");
                    sendto(s, &indexData, sizeof(indexData), 0, (struct sockaddr *)&fsin, alen);
                }
            }

            break;
        }

        // Peer sends request to search for specific content
        case 'S':
        {

            int Sflag = 0;
            int min;
            int peer;
            char *token = strtok(peerData.data, " ");
            char Sname[10] = {0};
            char Scontent[10] = {0};
            char Sdest[100];

            // Tokenizing string to get peer name and content name
            if (token)
                strncpy(Sname, token, sizeof(Sname) - 1);

            token = strtok(NULL, " ");

            if (token)
                strncpy(Scontent, token, sizeof(Scontent) - 1);

            // Dont know if the name is necessary but its in the project manual
            printf("Peer: %s requesting content: %s \n", Sname, Scontent);

            // Check if this content exists
            for (int i = 0; i < numPeers; i++)
            {
                // Find content with matching name
                if ((strcmp(Scontent, peerList[i].peerContent) == 0))
                {   
                    // Set it as the min
                    min = peerList[i].used;
                    peer = i; // In case this is the min or only occurence of the content
                    Sflag = 1;
                    break;
                }
            }

            // Iterate through list again
            for (int i = 0; i < numPeers; i++)
            {
                // Find content with matching name and check if its had fewer downloads
                if ((strcmp(Scontent, peerList[i].peerContent) == 0) && peerList[i].used < min)
                {   
                    // Set it as the min
                    min = peerList[i].used;
                    peer = i;
                    // For testing
                    printf("%s: %s downloaded  %d times.\n", peerList[i].peerName, peerList[i].peerContent, peerList[i].used);

                }
            }

            // Check to ensure the content servers not downloading from itself
            if (strcmp(Sname, peerList[peer].peerName) == 0){
                Sflag = 0;
            }

            // Flag set, content found
            if (Sflag) 
            {   // Send the IP address and port of content server to requesting peer
                indexData.type = 'S';
                snprintf(Sdest, 100, "%s %d", peerList[peer].peerAddress, peerList[peer].peerPort);
                strncpy(indexData.data, Sdest, sizeof(indexData.data) - 1);
                indexData.data[sizeof(indexData.data) - 1] = '\0';
                sendto(s, &indexData, sizeof(indexData), 0, (struct sockaddr *)&fsin, alen);
                // This content has been downloaded, so increment the number of downloads
                printf("%s downloading %s from content server %s.\n", Sname, Scontent, peerList[peer].peerName);
                peerList[peer].used++;
            }

            else
            { // Content not found, send out Error PDU
                indexData.type = 'E';
                sprintf(Sdest, "Content %s not found or trying to download from your own content server.\n", Scontent);
                strncpy(indexData.data, Sdest, sizeof(Sdest));
                Sdest[sizeof(Sdest) - 1] = '\0';
                sendto(s, &indexData, sizeof(indexData), 0, (struct sockaddr *)&fsin, alen);
            }
            break;
        }

        // Peer asks for list of all registered content
        case 'O':
        {

            if (numPeers <= 0)
            {
                // List is empty, send an Error
                indexData.type = 'E';
                strcpy(indexData.data, "No registered clients.\n");
                sendto(s, &indexData, sizeof(indexData), 0, (struct sockaddr *)&fsin, alen);
                strncpy(indexData.data, "------------", sizeof(indexData.data) - 1);
                indexData.data[sizeof(indexData.data) - 1] = '\0';
                sendto(s, &indexData, sizeof(indexData), 0, (struct sockaddr *)&fsin, alen);
            }

            else
            {
                // Sending back O type PDU

                indexData.type = 'O';

                for (int i = 0; i < numPeers; i++)
                {
                    char entry[100] = {0}; // Initialize the buffer to zero
                    snprintf(entry, sizeof(entry), "%d. %s from %s", i + 1, peerList[i].peerContent, peerList[i].peerName);

                    // Copy to indexData with null-termination
                    strncpy(indexData.data, entry, sizeof(indexData.data) - 1);
                    indexData.data[sizeof(indexData.data) - 1] = '\0';
                    printf("%s\n", indexData.data);
                    // Send to peer
                    sendto(s, &indexData, sizeof(indexData), 0, (struct sockaddr *)&fsin, alen);
                }

                // Distinct string to mark the end of the list
                strncpy(indexData.data, "------------", sizeof(indexData.data) - 1);
                indexData.data[sizeof(indexData.data) - 1] = '\0';
                sendto(s, &indexData, sizeof(indexData), 0, (struct sockaddr *)&fsin, alen);
            }
            break;
        }

        // Peer deregisters its content
        case 'T':
        {
            // Tokenize packet sent by peer and then deregister it
            char Tname[10];
            char Tcontent[10];
            char *token = strtok(peerData.data, " ");
            int flag = 0;

            if (token)
                strncpy(Tname, token, sizeof(Tname) - 1);

            token = strtok(NULL, " ");

            if (token)
                strncpy(Tcontent, token, sizeof(Tcontent) - 1);

            // Iterate through content list, check if name and content match, then remove
            for (int i = 0; i < numPeers; i++)
            {
                // Check for name and content
                if ((strcmp(peerList[i].peerContent, Tcontent) == 0) && (strcmp(peerList[i].peerName, Tname))==0)
                {   
                    // Raise the flag
                    flag = 1;
                    // Shift the elements to remove the peer
                    for (int j = i; j < numPeers - 1; j++)
                    {
                        peerList[j] = peerList[j + 1];
                    }
                    // Decrement num peers
                    numPeers--;
                }
            }

            // Flag set, peer is found so send back ack
            if (flag) {
                indexData.type = 'A';
                sprintf(indexData.data, "Content %s from %s deregistered successfully.\n", Tcontent, Tname);
            }

            // Flag not set, peer with content not found, send error
            else {
                indexData.type = 'E';
                strcpy(indexData.data, "Error.\n");
            }

            sendto(s, &indexData, sizeof(indexData), 0, (struct sockaddr *)&fsin, alen);
            break;
        }
        // PDU type is not recognized by index server, send out an error
        default:
        {
            fprintf(stderr, "Unrecognized PDU type.");
            indexData.type = 'E';
            strcpy(indexData.data, "Unrecognized PDU type.");
            sendto(s, &indexData, sizeof(indexData), 0, (struct sockaddr *)&fsin, alen);
            break;
        }
        }
        // Zero out the data sent and received to prevent previous data from being used
        memset(indexData.data, 0, 100);
        memset(peerData.data, 0, 100);
    }

    return 0;
}