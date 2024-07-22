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

    /// Input
    cout << "User Name: ";
    cin >> m_username;
    cout << "Password: ";
    cin >> m_password;

    m_socket = CreateConnectedSocket();

    std::thread sendThread(&LoginClient::SendThread, this);
    std::thread receiveThread(&LoginClient::ReceiveThread, this);

    if (sendThread.joinable())
        sendThread.join();
    if (receiveThread.joinable())
        receiveThread.join();
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


    /// 초기 송신
    Protocol::C2S_LoginRequest loginRequest;
    loginRequest.set_username(m_username);
    loginRequest.set_password(m_password);

    char* sendBuf = PacketBuilder::Instance().Serialize(EPacketType::C2S_LOGIN_REQUEST, loginRequest);
    int sendByte = 0;
    int totalSendByte = PacketHeader::Size() + loginRequest.ByteSizeLong();

    while (sendByte < totalSendByte)
    {
        int nSend = send(socket, sendBuf + sendByte, totalSendByte - sendByte, 0);
        if (nSend == SOCKET_ERROR)
        {
            printf("send() failed: %d\n", WSAGetLastError());
            break;
        }
        if (nSend == 0)
        {
            printf("connection closed\n");
            break;
        }

        sendByte += nSend;
    }


    return socket;
}

void LoginClient::SendThread()
{
    int sendByte = 0;

	std::shared_ptr<ClientStruct> test;
	while (true)
    {
        if (!m_sendQueue.try_pop(test))
            continue;

        char* serializedPacket = PacketBuilder::Instance().Serialize(test->header->type, *test->data);

        while(sendByte < test->header->length)
        {
	        int nSend = send(m_socket, serializedPacket + sendByte, test->header->length - sendByte, 0);
		        
	        if (nSend == SOCKET_ERROR)
	        {
	            printf("send() failed: %d\n", WSAGetLastError());
	            return;
	        }
	        if (nSend == 0)
	        {
	            printf("connection closed\n");
                return;
	        }

	        sendByte += nSend;
        }

        cout << "Validation Response Send" << endl;
        delete[] serializedPacket;
        sendByte = 0;
    }
}

void LoginClient::ReceiveThread()
{
    while (true)
    {
        char recvBuf[256];
        int recvByte = 0;

        int nRecv = recv(m_socket, recvBuf + recvByte, sizeof(recvBuf) - recvByte, 0);
        if (nRecv == SOCKET_ERROR)
        {
            printf("recv() failed: %d\n", WSAGetLastError());
            break;
        }
        if (nRecv == 0)
        {
            printf("connection closed\n");
            break;
        }

        recvByte += nRecv;

        if (recvByte >= PacketHeader::Size())
        {
            PacketHeader header;
            PacketBuilder::Instance().DeserializeHeader(recvBuf, sizeof(recvBuf), header);
            if (header.length > recvByte)
                continue;

            switch (header.type)
            {
            case EPacketType::S2C_LOGIN_RESPONSE:
            {
                Protocol::S2C_LoginResponse loginResponse;
                PacketBuilder::Instance().DeserializeData(recvBuf, sizeof(recvBuf), header, loginResponse);
                cout << loginResponse.sucess().value() << endl;
                printf("connected\n");
                break;
            }
            case EPacketType::S2C_VALIDATION_REQUEST:
            {
                std::shared_ptr<ClientStruct> test = std::make_shared<ClientStruct>();
                test->header = std::make_shared<PacketHeader>(PacketBuilder::Instance().CreateHeader(EPacketType::C2S_VALIDATION_RESPONSE, 0));
                test->header->type = EPacketType::C2S_VALIDATION_RESPONSE;
                test->header->length = PacketHeader::Size();
                test->data = std::make_shared<Protocol::C2S_ValidationResponse>();
                m_sendQueue.push(test);

                break;
            }
            case EPacketType::S2C_SESSION_EXPIRED_NOTIFICATION:
            {
                cout << "세션이 만료되었습니다" << endl;

                break;
            }
            default:
	        {
                cout << "패킷 헤더가 이상해용..." << endl;
                break;
	        }
            }
        }
    }
}
