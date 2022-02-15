// 
// created by asaad sleman - tel aviv university
//

#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <netinet/in.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

/***** Macros *****/
#define N_BYTES              (4)
#define MAX_CHAR           (126)
#define MIN_CHAR            (32)
#define GET_FROM_A(c)      (c-MIN_CHAR)
#define GET_FROM_Z(c) (c+MIN_CHAR)
#define L_BACK         (10)
#define NOC      (95) //NUM OF CHARACTERS

/***** GLOBALS *****/
static int sigint = 0;

static int busy = 0;

static uint32_t pccBufferArr[NOC] = { 0 };

static uint32_t pccArr[NOC] = { 0 };


/***** PROTOTYPES *****/

// create new socket, return socket FD
int initServer();

// wrapper function for execution loop
int mainFrame(int server);

void reportBack();

// count printable chars in buffer, return count
uint32_t viableChars(char* message);

// pcc_server main program flow
int main(int argc, char* argv[]);

// update pccBufferArr, restore on failure
void updatePCC(int mode);

// SIGINT handler
void sigint_handler(int signal);

/***** IMPLEMENTATION  *****/
int initServer() {
    int fd;
    // initialize sockets
    if (0 == (fd = socket(AF_INET, SOCK_STREAM, 0))) {
        perror("failed to initialize socket\n");
        exit(1);
    }
    return fd;
}

uint32_t viableChars(char* message) {
    uint32_t counter = 0;
    int i;

    for (int j = 0; j < strlen(message); j++) {
        int byte_value = (int)message[j];
        // if printable char, add to count
        if ((byte_value >= MIN_CHAR) && (byte_value <= MAX_CHAR)) { 
            i = GET_FROM_A(byte_value);
            pccBufferArr[i]++;
            counter++;
        }
    }

    return counter;
}

void updatePCC(int mode) {
    for (int j = 0; j < NOC; j++) {
        if (mode == 0) {
            // restore older version / before connection failure
            pccBufferArr[j] = pccArr[j];
        }
        else if (mode == 1) {
            // update by connection results
            pccArr[j] = pccBufferArr[j];
        }
    }
}

int mainFrame(int server) {
    int sock_conn;
    struct sockaddr_in client;
    char* msg_to_send;
    int clen = sizeof(client);

    // loop is interrupted by signit 
    while (!sigint) {
        busy = 0;
        sigset_t newsig;
        sigset_t oldsig;

        //Connect
        sock_conn = accept(server, (struct sockaddr*)&client, (socklen_t*)&clen);

        //block sigint signals, handle client
        sigemptyset(&newsig);
        sigaddset(&newsig, SIGINT);
        sigprocmask(SIG_BLOCK, &newsig, &oldsig);

        //if sigint, exit loop
        if (sigint) {
            break; 
        }

        busy = 1;

        //check connect, DO NOT EXIT ON FAILURE
        if (sock_conn < 0) {
            perror("Couldn't connect to client\n");
            busy = 0;
            sigprocmask(SIG_UNBLOCK, &newsig, &oldsig);
            return 1;
        }

        int netNum, hostNum;
        int recFlag = recv(sock_conn, &netNum, N_BYTES, 0);

        //check recieve msg, DO NOT EXIT ON FAILURE
        if (recFlag < 0) {
            if ((errno == EPIPE) || (errno == ETIMEDOUT) || (errno == ECONNRESET)) {
                perror("error recieving message\n");
                close(sock_conn);
                busy = 0;
                sigprocmask(SIG_UNBLOCK, &newsig, &oldsig);
                continue;
            }
            // we deal with this elsewhere
            else if ((errno == SIGINT) || (errno == EINTR)) {}
            // any other error recieved
            else {
                perror("Error receiving message length\n");
                busy = 0;
                sigprocmask(SIG_UNBLOCK, &newsig, &oldsig);
                return 1;
            }
        }

        // network buffer of size N
        hostNum = ntohl(netNum);
        msg_to_send = malloc(sizeof(char) * hostNum);
        memset(msg_to_send, '\0', sizeof(char) * hostNum);

        int msg_recieved = recv(sock_conn, msg_to_send, sizeof(char) * hostNum, 0);

        //check recieve
        if (msg_recieved < 0) {
            if ((errno == ETIMEDOUT) || (errno == ECONNRESET) || (errno == EPIPE)) {
                perror("Non-fatal error occurred");
                close(sock_conn);
                busy = 0;
                sigprocmask(SIG_UNBLOCK, &newsig, &oldsig);
                continue;
            }
            else if ((errno == SIGINT) || (errno == EINTR)) {
                //Do nothing, part of the flow
            }
            else {
                perror("Error receiving message");
                busy = 0;
                sigprocmask(SIG_UNBLOCK, &newsig, &oldsig);
                return 1;
            }
        }

        //Sending the number of printable characters to client
        uint32_t tmpCounter = viableChars(msg_to_send);
        if (send(sock_conn, &tmpCounter, sizeof(tmpCounter), 0) < 0) {
            if ((errno == ETIMEDOUT) || (errno == ECONNRESET) || (errno == EPIPE)) {
                perror("error recieving message\n");
                close(sock_conn);
                updatePCC(0);
                busy = 0;
                sigprocmask(SIG_UNBLOCK, &newsig, &oldsig);
                continue;
            }
            else if ((errno == SIGINT) || (errno == EINTR)) {
            }
            else {
                perror("error sending message\n");
                busy = 0;
                sigprocmask(SIG_UNBLOCK, &newsig, &oldsig);
                return 1;
            }
        }
        // no need for it
        free(msg_to_send);
        //Close current connection
        close(sock_conn);
        //Update pcc array
        updatePCC(1);
        // sigint unblocked, if recived more, break 
        busy = 0;
        sigprocmask(SIG_UNBLOCK, &newsig, &oldsig);
    }

    return 0;
}

void sigint_handler(int sig) {
    if (!busy) {
        sig = 1;
    }
}

void reportBack() {
    for (int i = 0; i < NOC; i++) {
        printf("char '%c' : %u times\n", GET_FROM_Z(i), pccArr[i]);
    }
}

int main(int argc, char* argv[]) {
    int port;
    int server;
    struct sockaddr_in address;
    
    //check input args
    if (argc != 2) {
        fprintf(stderr, "Argument Error! Please Check!\n");
        exit(1);
    }
    //initialize socket
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    server = initServer();
    // attatch to port, EXIT ON FAILURE
    if (1 == setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &(int) {1}, sizeof(int))) {
        perror("socket options config. error\n");
        exit(1);
    }
    port = atoi(argv[1]);
    address.sin_port = port;

    if (0 != bind(server, (struct sockaddr*)&address, sizeof(address))) {
        perror("server binding error!\n");
        exit(1);
    }

    if (0 != listen(server, L_BACK)) {
        perror("server listening error!\n");
        exit(1);
    }

    //init sigint handler
    struct sigaction handleSigint = {
            .sa_handler = sigint_handler
    };
    if ((-1) == sigaction(SIGINT, &handleSigint, NULL)) {
        fprintf(stderr, "error handling SIGINT!\n");
        if (!busy) {
            sigint = 1;
        }
    }
    //main message loop
    if (0 != mainFrame(server)) {
        perror("Error receiving Message\n");
        exit(1);
    }
    // on disconnecting
    reportBack();
    close(server);
    return 0;
}