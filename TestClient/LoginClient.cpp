#include "pch.h"
#include "LoginClient.h"

#include "../PacketLibrary/Protocol.pb.h"

#include <iostream>

using namespace std;

LoginClient::LoginClient(const char* ip, const char* port)
    : m_connectIP(ip)
    , m_port(port)
{
}

void LoginClient::Run()
{
    WSADATA WSAData;
    int rt;
    if ((rt = WSAStartup(MAKEWORD(2, 2), &WSAData)) != 0)
    {
        printf("WSAStartup() failed: %d", rt);
        return;
    }

    socket = CreateConnectedSocket();

    /// Input
    string username, password;
    cout << "User Name: ";
    cin >> username;
    cout << "Password: ";
    cin >> password;


    /// Send
    Protocol::C2S_Login_Request loginRequest;
    loginRequest.set_username(username);
    loginRequest.set_password(password);

    char* sendBuf = PacketBuilder::Instance().Serialize(EPacketType::C2S_LOGIN_REQUEST, loginRequest);
    int sendByte = 0;
    int totalSendByte = PacketHeader::Size() + loginRequest.ByteSizeLong();

    while(sendByte < totalSendByte)
    {
        int nSend = send(socket, sendBuf + sendByte, totalSendByte - sendByte, 0);
        if(nSend == SOCKET_ERROR)
        {
            printf("send() failed: %d\n", WSAGetLastError());
            return;
        }
        if(nSend == 0)
        {
            printf("connection closed\n");
            return;
        }

        sendByte += nSend;
    }

    /// Recv
    char recvBuf[256];

    int recvByte = 0;

    while(true)
    {
        int nRecv = recv(socket, recvBuf + recvByte, sizeof(recvBuf) - recvByte, 0);
        if(nRecv == SOCKET_ERROR)
        {
            printf("recv() failed: %d\n", WSAGetLastError());
            break;
        }
        if(nRecv == 0)
        {
            printf("connection closed\n");
            break;
        }

        recvByte += nRecv;

        if(recvByte > PacketHeader::Size())
        {
            PacketHeader header;
            PacketBuilder::Instance().DeserializeHeader(recvBuf, sizeof(recvBuf), header);
            if (header.length > recvByte)
                continue;

            Protocol::S2C_Login_Response loginResponse;
            PacketBuilder::Instance().DeserializeData(recvBuf, sizeof(recvBuf), header, loginResponse);
            cout << loginResponse.sucess().value() << endl;
            break;
        }
    }

    while(true)
    {
        int a = 0;
    }
}

SOCKET LoginClient::CreateConnectedSocket()
{
    addrinfo hints = { 0 };
    addrinfo* addr_srv = nullptr;

    hints.ai_flags = 0;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    if (getaddrinfo(m_connectIP, m_port, &hints, &addr_srv) != 0)
    {
        printf("getaddrinfo() failed with error %d\n", WSAGetLastError());
        return INVALID_SOCKET;
    }
    if (!addr_srv)
    {
        printf("getaddrinfo() failed to resolve/convert the interface\n");
        return INVALID_SOCKET;
    }

    SOCKET socket = ::socket(addr_srv->ai_family, addr_srv->ai_socktype, addr_srv->ai_protocol);
    if (socket == INVALID_SOCKET)
    {
        printf("socket() failed: %d\n", WSAGetLastError());
        freeaddrinfo(addr_srv);
        return INVALID_SOCKET;
    }

    int rt = connect(socket, addr_srv->ai_addr, (int)addr_srv->ai_addrlen);
    if (rt == SOCKET_ERROR)
    {
        printf("connect() failed: %d\n", WSAGetLastError());
        closesocket(socket);
        freeaddrinfo(addr_srv);
        return INVALID_SOCKET;
    }

    freeaddrinfo(addr_srv);

    printf("connected\n");
    return socket;
}
