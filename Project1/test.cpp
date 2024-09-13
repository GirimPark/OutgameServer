#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#pragma comment(lib, "Ws2_32.lib")


void ErrorCK(const char* msg) {
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


    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        ErrorCK("Socket library initialization error!");

    // ���� ����
    Socket = socket(PF_INET, SOCK_STREAM, 0);
    if (Socket == INVALID_SOCKET)
        ErrorCK("Socket creation error!");

    memset(&servAdr, 0, sizeof(servAdr));
    servAdr.sin_family = AF_INET;
    servAdr.sin_addr.s_addr = inet_addr("127.0.0.1");
    servAdr.sin_port = htons(5001);

    // ������ ���� ������ ���� ��û
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

        // ������ �ۼ���
        send(Socket, msg, strlen(msg), 0);
        strLength = recv(Socket, msg, 1023, 0);
        msg[strLength] = 0;
        printf("Message from server: %s", msg);
    }

    closesocket(Socket);
    WSACleanup();
    return 0;
}