#include <stdio.h>     
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>   
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#define MAXPENDING 8

pthread_mutex_t mutex;
int* sectors;
int found = 0;
int printed = 0;
unsigned short* clients;
int clients_count;
int ptr = 0;

typedef struct threadArgs {
    int socket;
    int sectorsCount;
    struct sockaddr_in client_addr;
} threadArgs;

void DieWithError(char *errorMessage)
{
    perror(errorMessage);
    exit(1);
}

void *clientThread(void* args) {
    int socket;
    struct sockaddr_in client_addr;
    struct sockaddr_in my_client;
    pthread_detach(pthread_self());
    socket = ((threadArgs*)args)->socket;
    int sectorsCount = ((threadArgs*)args)->sectorsCount;
    free(args);
    unsigned int client_length = sizeof(client_addr);
    int sendData[2];
    int recvData[2];
    int hasTreasure = 0;
    int selected_sector = -1;

    recvfrom(socket, recvData, sizeof(recvData), 0, (struct sockaddr*) &client_addr, &client_length);
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < clients_count; i++) {
        if (clients[i] == client_addr.sin_port) {
            pthread_mutex_unlock(&mutex);
            goto snd;
        } else {
            clients[ptr++] = client_addr.sin_port;
            pthread_mutex_unlock(&mutex);
            break;
        }
    }
    printf("New connection from %s\n", inet_ntoa(client_addr.sin_addr));
    my_client = client_addr;
    for (;;) {
        pthread_mutex_lock(&mutex);
        for (int i = 0; i < sectorsCount; i++) {
            if (sectors[i] != 1) {
                selected_sector = i;
		        hasTreasure = sectors[i] == 2;
		        sectors[i] = 1;
                break;
            }
        }
	    pthread_mutex_unlock(&mutex);
	    sendData[0] = selected_sector;
	    sendData[1] = hasTreasure;
        printf("SENDTO %s:%d\n", inet_ntoa(my_client.sin_addr), my_client.sin_port);
        sendto(socket, sendData, sizeof(sendData), 0, (struct sockaddr*) &my_client, sizeof(my_client));
	    if (selected_sector == -1) break;
        recvfrom(socket, recvData, sizeof(recvData), 0, (struct sockaddr*) &client_addr, &client_length);
snd:
        pthread_mutex_lock(&mutex);
        if (recvData[0] == 1) {
            if (!printed) {
                printf("Pirates found trasure!\n");
                printed = 1;
            }
	        for (int i = 0; i < sectorsCount; i++) {
	       	  sectors[i] = 1;
	        }
        }
        pthread_mutex_unlock(&mutex);
	    selected_sector = -1;
    }
}

int main(int argc, char *argv[])
{
    unsigned short port;
    int server_socket;
    int client_socket;
    unsigned int client_length;
    struct sockaddr_in client_addr;
    struct sockaddr_in server_addr;
    pthread_t threadId;
    pthread_mutex_init(&mutex, NULL);
    
    if (argc != 5)
    {
        fprintf(stderr, "Usage:  %s <Client port> <Number of sectors> <Sector with trasure> <Number of clients>\n", argv[0]);
        exit(1);
    }

    port = atoi(argv[1]);
    int sectorsCount = atoi(argv[2]);
    int trasureSector = atoi(argv[3]);
    clients_count = atoi(argv[4]);
    trasureSector -= 1;

    sectors = (int*) malloc(sizeof(int) * sectorsCount);
    clients = (unsigned short*) malloc(sizeof(unsigned short) * clients_count);
    for (int i = 0; i < sectorsCount; i++) {
        sectors[i] = 0;
    }
    for (int i = 0; i < clients_count; i++) {
        clients[i] = -1;
    }
    sectors[trasureSector] = 2;

    if ((server_socket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) DieWithError("socket() failed");
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;              
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); 
    server_addr.sin_port = htons(port);

    if (bind(server_socket, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) DieWithError("bind() failed");
    printf("Open socket on %s:%d\n", inet_ntoa(server_addr.sin_addr), port);

    for (int i = 0; i < clients_count; i++) {
        threadArgs *args = (threadArgs*) malloc(sizeof(threadArgs));
        args->socket = server_socket;
        args->sectorsCount = sectorsCount;
        if (pthread_create(&threadId, NULL, clientThread, (void*) args) != 0) DieWithError("pthread_create() failed");
    }
    while(1) {
        sleep(1);
    }
    free(sectors);
    free(clients);
    pthread_mutex_destroy(&mutex);
    return 0;
}
