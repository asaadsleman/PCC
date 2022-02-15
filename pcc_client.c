// 
// created by asaad sleman - tel aviv university
//

#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>


/***** PROTOTYPES *****/
// new client socket
int initClient();

// main program flow
int main(int argc, char* argv[]);

/***** IMPLEMENTATION *****/
int initClient() {
    int fd;
    // similar to pcc_server function initServer
    if (0 == (fd = socket(AF_INET, SOCK_STREAM, 0))) {
        perror("error initializing socket\n");
        exit(1);
    }
    return fd;
}

int main(int argc, char* argv[]) {
    struct sockaddr_in server;
    struct in_addr ip;
    char* msg_read;
    char* filepath;
    FILE* file;
    int port;
    int client;

    //Argument check
    if (argc != 4) {
        fprintf(stderr, "Arguments Error! please fix!\n");
        exit(1);
    }

    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_family = AF_INET;
    // conert ip address
    inet_aton(argv[1], &ip); 
    filepath = argv[3];
    client = initClient();
    port = atoi(argv[2]);
    server.sin_port = port;
    file = fopen(filepath, "rw");
    if (file == NULL) {
        perror("Error opening file");
        exit(1);
    }

    //server connect
    if (connect(client, (struct sockaddr*)&server, sizeof(server)) < 0) {
        perror("error in client connection!\n");
        exit(1);
    }

    fseek(file, 0, SEEK_END);
    // obtain file size
    uint32_t size = ftell(file);
    uint32_t size_network = htonl(size);

    if (send(client, &size_network, sizeof(htonl(size)), 0) < 0) {
        if ((errno == ETIMEDOUT) || (errno == EPIPE) || (errno == ECONNRESET)) {
            perror("Error sending message!\n");
            close(client);
            exit(0);
        }
        else if ((errno != SIGINT) && (errno != EINTR)) {
            perror("Error sending message!\n");
            exit(1);
        }
    }
    // return to file beginning
    rewind(file);
    msg_read = malloc(size);
    memset(msg_read, '\0', size);

    int temp;
    unsigned int index = 0U;
    // read file contents
    while ((temp = fgetc(file))) {

        if (temp == EOF) {
            msg_read[index] = '\0';
            break;
        }
        else if (temp == '\n') {
            msg_read[index] = '\0';
            index = 0U;
            continue;
        }
        else
            msg_read[index++] = (char)temp;
    }
    //send content to server
    if (send(client, msg_read, strlen(msg_read), 0) < 0) {
        if ((errno == ETIMEDOUT) || (errno == EPIPE) || (errno == ECONNRESET)) {
            perror("Error sending message!\n");
            close(client);
            exit(0);
        }
        else if ((errno != SIGINT) && (errno != EINTR)) {
            perror("Error sending message!\n");
            return 1;
        }
    }
    fclose(file);

    uint32_t counter;
    if (recv(client, &counter, sizeof(counter), 0) < 0) {
        if ((errno == ETIMEDOUT) || (errno == EPIPE) || (errno == ECONNRESET)) {
            perror("Error sending message!\n");
            close(client);
            exit(0);
        }
        else if((errno != SIGINT) && (errno != EINTR)) {
            perror("Error sending message!\n");
            exit(1);
        }
    }
    free(msg_read);
    close(client);
    // LINE OF DESIRED OUTPUT
    printf("# of printable characters: %u\n", counter);
    
    return 0;
}