#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>  // Windows套接字
#include <windows.h>   // Windows线程管理
#include <time.h>
#include "defs.h"

#define MAX_BUFFER_SIZE 2048
#define MAX_CLIENTS 10

// 用 CRITICAL_SECTION 替换 pthread_mutex_t
CRITICAL_SECTION mutex;

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
            packet->length = strlen(packet->data);
            send(sockfd, packet, sizeof(Packet), 0);
            printf("Time sent to client %d\n", sockfd);
            break;
        }
        case CMD_NAME:
        {
            strcpy(packet->data, "Server");
            packet->length = strlen(packet->data);
            send(sockfd, packet, sizeof(Packet), 0);
            printf("Name sent to client %d\n", sockfd);
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
            packet->length = strlen(packet->data);
            send(sockfd, packet, sizeof(Packet), 0);
            printf("Client list sent to client %d\n", sockfd);
            break;
        }
        case CMD_SEND_MESSAGE:
        {
            client *current = clients->next;
            while (current != NULL)
            {
                if (current->sockfd == packet->client_id)
                {
                    packet->length = strlen(packet->data);
                    packet->client_id = this_client->sockfd;
                    send(current->sockfd, packet, sizeof(Packet), 0);
                    printf("Message sent to client %d\n", current->sockfd);
                    break;
                }
                current = current->next;
            }
            if (current == NULL)
            {
                strcpy(packet->data, "Client not found.");
                packet->length = strlen(packet->data);
                packet->client_id = 0;
                send(sockfd, packet, sizeof(Packet), 0);
                printf("Client not found.\n");
            }
            break;
        }
        default:
            break;
        }
    }

    EnterCriticalSection(&mutex);  // 使用 Windows 的 CRITICAL_SECTION
    printf("Client disconnected: %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
    this_client->prev->next = this_client->next;
    if (this_client->next != NULL)
        this_client->next->prev = this_client->prev;
    free(this_client);
    LeaveCriticalSection(&mutex);  // 释放锁

    closesocket(sockfd); // 使用closesocket替代close
    return 0;
}

int main()
{
    WSADATA wsaData;
    int server_sockfd;
    struct sockaddr_in server_addr;
    clients = (client *)malloc(sizeof(client));
    clients->next = clients->prev = NULL;

    // 初始化 Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        printf("WSAStartup failed.\n");
        exit(1);
    }

    server_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sockfd < 0)
    {
        printf("Socket creation failed.\n");
        WSACleanup(); // 清理Winsock
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERVER_PORT);

    if (bind(server_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        printf("Bind failed.\n");
        closesocket(server_sockfd); // 使用closesocket
        WSACleanup();
        exit(1);
    }

    if (listen(server_sockfd, MAX_CLIENTS) < 0)
    {
        printf("Listen failed.\n");
        closesocket(server_sockfd); // 使用closesocket
        WSACleanup();
        exit(1);
    }

    printf("Server started. Listening on port %d.\n", SERVER_PORT);

    // 初始化 CRITICAL_SECTION
    InitializeCriticalSection(&mutex);

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

        EnterCriticalSection(&mutex);  // 加锁

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

        LeaveCriticalSection(&mutex);  // 释放锁

        // 创建线程
        HANDLE thread_id;
        thread_id = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)client_handler, (void *)new_client, 0, NULL);
        if (thread_id == NULL)
        {
            printf("Thread creation failed.\n");
            closesocket(client_sockfd);
            free(new_client);
        }
    }

    closesocket(server_sockfd); // 使用closesocket
    WSACleanup();  // 清理 Winsock
    DeleteCriticalSection(&mutex);  // 删除 CRITICAL_SECTION
    return 0;
}
