#ifndef DEFS_H
#define DEFS_H

#include <stdint.h>

#define CMD_CONNECT 1
#define CMD_DISCONNECT 2
#define CMD_TIME 3
#define CMD_NAME 4
#define CMD_LIST 5
#define CMD_SEND_MESSAGE 6
#define CMD_EXIT 7
#define SERVER_PORT 6034

#define MAX_DATA_LEN 1024

typedef struct
{
    uint32_t length;         // 数据包总长度
    int type;                // 请求类型
    uint32_t client_id;      // 客户端编号，发送消息时使用
    char data[MAX_DATA_LEN]; // 数据区，用于传输时间、名字、列表或消息内容
} Packet;

#endif