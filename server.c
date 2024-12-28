#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include "defs.h"

#define MAX_BUFFER_SIZE 2048
#define MAX_CLIENTS 10

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
typedef struct client_struct
{
    int sockfd;
    struct sockaddr_in addr;
    struct client_struct *next;
    struct client_struct *prev;
} client;
client *clients;

void *client_handler(void *arg)
{
    client *this_client = (client *)arg;
    int sockfd = this_client->sockfd;
    struct sockaddr_in client_addr = this_client->addr;

    char buffer[MAX_BUFFER_SIZE];
    int n;
    while ((n = recv(sockfd, buffer, MAX_BUFFER_SIZE, 0)) > 0)
    {
        Packet *packet = (Packet *)buffer;
        switch (packet->type)
        {
        case CMD_TIME:
        {
            time_t now;
            time(&now);
            struct tm *local = localtime(&now);
            strftime(packet->data, MAX_DATA_LEN, "%Y-%m-%d %H:%M:%S", local);
            packet->length = sizeof(Packet);
            send(sockfd, packet, sizeof(Packet), 0);
            break;
        }
        case CMD_NAME:
        {
            strcpy(packet->data, "Server");
            packet->length = sizeof(Packet);
            send(sockfd, packet, sizeof(Packet), 0);
            break;
        }
        case CMD_LIST:
        {
            client *current = clients->next;
            char *data = packet->data;
            while (current != NULL)
            {
                data += sprintf(data, "%d\t%s:%d\n", current->sockfd, inet_ntoa(current->addr.sin_addr), ntohs(current->addr.sin_port));
                current = current->next;
            }
            packet->length = sizeof(Packet);
            send(sockfd, packet, sizeof(Packet), 0);
            break;
        }
        case CMD_SEND_MESSAGE:
        {
            client *current = clients->next;
            while (current != NULL)
            {
                if (current->sockfd == packet->client_id)
                {
                    packet->length = sizeof(Packet);
                    packet->client_id = this_client->sockfd;
                    send(current->sockfd, packet, sizeof(Packet), 0);
                    break;
                }
                current = current->next;
            }
            break;
        }
        default:
            break;
        }
    }

    pthread_mutex_lock(&mutex);
    printf("Client disconnected: %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
    this_client->prev->next = this_client->next;
    if (this_client->next != NULL)
        this_client->next->prev = this_client->prev;
    free(this_client);
    pthread_mutex_unlock(&mutex);

    close(sockfd);
    pthread_exit(NULL);
}

int main()
{
    int server_sockfd;
    struct sockaddr_in server_addr;
    clients = (client *)malloc(sizeof(client));
    clients->next = clients->prev = NULL;

    server_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sockfd < 0)
    {
        printf("Socket creation failed.\n");
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERVER_PORT);

    if (bind(server_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        printf("Bind failed.\n");
        exit(1);
    }

    if (listen(server_sockfd, MAX_CLIENTS) < 0)
    {
        printf("Listen failed.\n");
        exit(1);
    }

    printf("Server started. Listening on port %d.\n", SERVER_PORT);

    while (1)
    {
        int client_sockfd;
        struct sockaddr_in client_addr;
        int client_len = sizeof(client_addr);

        client_sockfd = accept(server_sockfd, (struct sockaddr *)&client_addr, &client_len);
        if (client_sockfd < 0)
        {
            printf("Accept failed.\n");
            continue;
        }

        pthread_mutex_lock(&mutex);

        client *new_client = (client *)malloc(sizeof(client)), *last = clients;
        new_client->sockfd = client_sockfd;
        new_client->addr = client_addr;
        new_client->next = clients->next;

        while (last->next != NULL)
            last = last->next;
        last->next = new_client;
        new_client->prev = last;
        new_client->next = NULL;

        printf("Client connected: %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        pthread_mutex_unlock(&mutex);

        pthread_t thread_id;
        pthread_create(&thread_id, NULL, client_handler, (void *)new_client);
    }
}
