#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>  // 用于套接字和网络功能
#include <windows.h>   // 用于线程和睡眠函数
#include "defs.h"

#define MAX_BUFFER_SIZE 2048
#define RETRY_LIMIT 5

// 在程序开始时初始化 Winsock 库
int sockfd;
int connection_status = 0;
HANDLE rcv_thread; // 使用 Windows 线程类型

void *receive(void *arg)
{
    int sockfd = *(int *)arg;
    char buffer[MAX_BUFFER_SIZE];
    int n;
    while ((n = recv(sockfd, buffer, MAX_BUFFER_SIZE, 0)) > 0)
    {
        // printf("\033[s");
        Packet *packet = (Packet *)buffer;
        // printf("\033[15;0H");
        // printf("\033[K\n\033[K\n\033[K\n\033[K\n\033[K\n");
        // printf("\033[15;0H");
        switch (packet->type)
        {
        case CMD_TIME:
            printf("Server time: %s\n", packet->data);
            break;
        case CMD_NAME:
            printf("Server name: %s\n", packet->data);
            break;
        case CMD_LIST:
            printf("Client list:\n%s", packet->data);
            break;
        case CMD_SEND_MESSAGE:
            if(packet->client_id == 0)
                printf("Message from server: %s\n", packet->data);
            else
                printf("Message from client %d: %s\n", packet->client_id, packet->data);
            break;
        default:
            printf("Unknown packet type.\n");
            break;
        }
        // printf("\033[u");
    }
    connection_status = 0;
    // printf("\033[s");
    Packet *packet = (Packet *)buffer;
    // printf("\033[15;0H");
    // printf("\033[K\n\033[K\n\033[K\n\033[K\n\033[K\n");
    // printf("\033[15;0H");
    printf("Server disconnected.\n");
    // printf("\033[u");
    return NULL;
}

void show_menu()
{
    // printf("\033[0;0H");
    // printf("\033[K\n\033[K\n\033[K\n\033[K\n\033[K\n");
    // printf("\033[K\n\033[K\n\033[K\n\033[K\n");
    // printf("\033[0;0H");
    if (connection_status == 0)
        printf("Server Not Connected\n");
    else
        printf("Server Connected\n");

    printf("Menu:\n");
    printf("1. Connect to server\n");
    printf("2. Disconnect from server\n");
    printf("3. Get time\n");
    printf("4. Get name\n");
    printf("5. Get client list\n");
    printf("6. Send message\n");
    printf("7. Exit\n");
}

int main()
{
    WSADATA wsaData;
    struct sockaddr_in server_addr;
    int choice;
    int retry;

    // 初始化 Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        printf("WSAStartup failed.\n");
        return 1;
    }

    server_addr.sin_family = AF_INET;
    // system("cls");  // Windows 环境下使用 cls 清屏

    while (1)
    {
        show_menu();
        // printf("\033[10;0H");
        printf("Enter your choice: ");
        // printf("\033[K");
        scanf("%d", &choice);
        // printf("\033[K\n\033[K\n\033[K\n\033[K\n");
        // printf("\033[11;0H");
        Packet packet;
        memset(&packet, 0, sizeof(packet));
        switch (choice)
        {
        case CMD_CONNECT: // connect to server
            if (connection_status == 0)
            {
                sockfd = socket(AF_INET, SOCK_STREAM, 0);
                if (sockfd < 0)
                {
                    // printf("\033[K");
                    printf("Socket creation failed.\n");
                    exit(1);
                }
                char ip[20];
                // printf("\033[K");
                printf("Enter server IP: ");
                scanf("%s", ip);
                server_addr.sin_addr.s_addr = inet_addr(ip);
                server_addr.sin_port = htons(SERVER_PORT);
                retry = 0;
                while (retry < RETRY_LIMIT && connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
                {
                    // printf("\033[K");
                    retry++;
                    Sleep(100);  // 使用 Sleep 代替 usleep，单位是毫秒
                }
                if (retry >= RETRY_LIMIT){
                    printf("Server cannot be reached.\n");
                    }
                else
                {
                    connection_status = 1;
                    printf("Connected to server.\n");
                    rcv_thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)receive, (void *)&sockfd, 0, NULL);
                }
            }
            else
                printf("Already connected.\n");
            break;
        case CMD_DISCONNECT: // disconnect from server
            if (connection_status == 1)
            {
                closesocket(sockfd);  // 使用 closesocket 关闭套接字
                connection_status = 0;
                printf("Disconnected from server.\n");
            }
            else
                printf("Not connected yet.\n");
            break;
        case CMD_TIME: // get time from server
            
            if (connection_status == 1)
            {
                packet.length = 0;
                packet.type = CMD_TIME;
                packet.client_id = 0;
                send(sockfd, &packet, sizeof(packet), 0);
            }
            else
                printf("Not connected yet.\n");
            
            break;
        case CMD_NAME: // get name from server
            if (connection_status == 1)
            {
                packet.length = 0;
                packet.type = CMD_NAME;
                packet.client_id = 0;
                send(sockfd, &packet, sizeof(packet), 0);
            }
            else
                printf("Not connected yet.\n");
            break;
        case CMD_LIST: // get client list from server
            if (connection_status == 1)
            {
                packet.length = 0;
                packet.type = CMD_LIST;
                packet.client_id = 0;
                send(sockfd, &packet, sizeof(packet), 0);
            }
            else
                printf("Not connected yet.\n");
            break;
        case CMD_SEND_MESSAGE: // send message to server
            if (connection_status == 1)
            {
                packet.type = CMD_SEND_MESSAGE;
                printf("Enter client ID: ");
                scanf("%d", &packet.client_id);
                printf("Enter message in a line: ");
                char ch = getchar(), *data = packet.data;
                while ((ch = getchar()) != '\n' && ch != EOF)
                {
                    *data = ch;
                    data++;
                }
                *data = '\0';
                packet.length = strlen(packet.data);
                send(sockfd, &packet, sizeof(packet), 0);
                printf("Message sent to server.\n");
            }
            else
                printf("Not connected yet.\n");
            break;
        case CMD_EXIT: // exit
            // system("cls");  // Windows 环境下使用 cls 清屏
            if (connection_status == 1)
            {
                closesocket(sockfd);
                connection_status = 0;
                printf("Disconnected from server.\n");
            }
            printf("Exiting...\n");
            Sleep(500); // 使用 Sleep 代替 usleep
            WSACleanup();  // 清理 Winsock
            exit(0);
            break;
        default:
            printf("Invalid choice.\n");
            break;
        }
        Sleep(1000); // 使用 Sleep 代替 usleep
        printf("\n\n\n");
    }

    return 0;
}
