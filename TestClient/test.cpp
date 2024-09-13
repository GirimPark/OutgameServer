#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#pragma comment(lib, "Ws2_32.lib")


void ErrorCK(char* msg) {
    fputs(msg, stderr);
    fputc('\n', stderr);
    exit(1);
}

int main() {
    WSADATA wsaData;
    SOCKET Socket;
    char msg[1024];
    int strLength;
    int port = -1;
    char addr[40];
    SOCKADDR_IN servAdr;

    printf("Connect to the server.\n");
    while (port < 0 || port > 65535) {
        printf("Enter address: ");
        scanf("%s", addr);
        printf("Enter port number: ");
        scanf("%d", &port);
        getchar();
    }


    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        ErrorCK("Socket library initialization error!");

    // 家南 积己
    Socket = socket(PF_INET, SOCK_STREAM, 0);
    if (Socket == INVALID_SOCKET)
        ErrorCK("Socket creation error!");

    memset(&servAdr, 0, sizeof(servAdr));
    servAdr.sin_family = AF_INET;
    servAdr.sin_addr.s_addr = inet_addr(addr);
    servAdr.sin_port = htons(port);

    // 家南阑 烹秦 辑滚俊 立加 夸没
    if (connect(Socket, (SOCKADDR*)&servAdr, sizeof(servAdr)) == SOCKET_ERROR)
        ErrorCK("Connection failure!\n");
    else
        printf("Connection success!\n");

    while (1) {
        fputs("Input msg(exit: Q): ", stdout);
        fgets(msg, 1024, stdin);

        if (!strcmp(msg, "q\n") || !strcmp(msg, "Q\n")) {
            printf("Connection is terminated.");
            break;
        }

        // 单捞磐 价荐脚
        send(Socket, msg, strlen(msg), 0);
        strLength = recv(Socket, msg, 1023, 0);
        msg[strLength] = 0;
        printf("Message from server: %s", msg);
    }

    closesocket(Socket);
    WSACleanup();
    return 0;
}