#include "pch.h"
#include "MemoryPoolTestClient.h"

#include <iostream>
using namespace std;

MemoryPoolTestClient::MemoryPoolTestClient(const char* ip, const char* port, int clientCnt, int cycleCnt)
	: m_connectIP(ip)
	, m_port(port)
	, m_clientCnt(clientCnt)
	, m_cycleCnt(cycleCnt)
{

}

void MemoryPoolTestClient::Run()
{
	WSADATA WSAData;
	int rt;
	if ((rt = WSAStartup(MAKEWORD(2, 2), &WSAData)) != 0)
	{
		printf("WSAStartup() failed: %d", rt);
		return;
	}

    InitializeCriticalSection(&m_criticalSection);

	for(int i=0; i<m_clientCnt; ++i)
	{
		TestClient* client = new TestClient;
		client->username = "test" + std::to_string(i);
		client->password = "1234";
		client->thread = new thread(&MemoryPoolTestClient::TestThread, this, client);

        m_clients.emplace_back(client);
	}

    for(auto& client : m_clients)
    {
	    if(client->thread->joinable())
	    {
            client->thread->join();
            delete client->thread;
	    }
    }

    // 전체 응답 평균 시간
    long long avgResponseTime = 0;
    for(const auto& client : m_clients)
    {
        avgResponseTime += client->avgResponseTime;
    }
    avgResponseTime /= m_clientCnt;
    cout << "전체 평균 응답 시간 : " << avgResponseTime << "ms" << endl;

    for(auto& client : m_clients)
    {
        delete client;
    }
    m_clients.clear();

    DeleteCriticalSection(&m_criticalSection);

    WSACleanup();
}

SOCKET MemoryPoolTestClient::CreateConnectedSocket()
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

    return socket;
}

void MemoryPoolTestClient::TestThread(TestClient* client)
{
    client->avgResponseTime = 0;
    while(client->curCycle < m_cycleCnt)
    {
        auto start = std::chrono::high_resolution_clock::now();
        client->socket = CreateConnectedSocket();
        if (client->socket == INVALID_SOCKET)
            continue;

        auto temp1 = std::chrono::high_resolution_clock::now();
        auto tempInterval1 = std::chrono::duration_cast<std::chrono::milliseconds>(temp1 - start).count();

        if (!Login(client))
            continue;

        auto temp2 = std::chrono::high_resolution_clock::now();
        auto tempInterval2 = std::chrono::duration_cast<std::chrono::milliseconds>(temp2 - temp1).count(); 

        if (!Logout(client))
            continue;

        linger lingerStruct;
        lingerStruct.l_onoff = 1;
        lingerStruct.l_linger = 0;
        setsockopt(client->socket, SOL_SOCKET, SO_LINGER, (char*)&lingerStruct, sizeof(lingerStruct));
        int result = closesocket(client->socket);
        if (result == SOCKET_ERROR) {
            int error = WSAGetLastError();
            printf("closesocket() failed with error: %d\n", error);
        }
        client->socket = INVALID_SOCKET;

        auto temp3 = std::chrono::high_resolution_clock::now();
        auto tempInterval3 = std::chrono::duration_cast<std::chrono::milliseconds>(temp3 - temp2).count();

        auto end = std::chrono::high_resolution_clock::now();

        auto responseTime = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        client->avgResponseTime += responseTime;

        client->curCycle++;

        this_thread::sleep_for(1s);
    }

    client->avgResponseTime /= m_cycleCnt;
}

bool MemoryPoolTestClient::Login(TestClient* client)
{
    /// Send Login Request
    Protocol::C2S_LoginRequest loginRequest;
    loginRequest.set_username(client->username);
    loginRequest.set_password(client->password);

    char* sendBuf = PacketBuilder::Instance().Serialize(PacketID::C2S_LOGIN_REQUEST, loginRequest);
    int sendByte = 0;
    int totalSendByte = PacketHeader::Size() + loginRequest.ByteSizeLong();

    while (sendByte < totalSendByte)
    {
        int nSend = send(client->socket, sendBuf + sendByte, totalSendByte - sendByte, 0);
        if (nSend == SOCKET_ERROR)
        {
            printf("send() failed: %d\n", WSAGetLastError());
            closesocket(client->socket);
            client->socket = INVALID_SOCKET;
            return false;
        }
        if (nSend == 0)
        {
            printf("connection closed\n");
            closesocket(client->socket);
            client->socket = INVALID_SOCKET;
            return false;
        }

        sendByte += nSend;
    }

    delete[] sendBuf;

    /// Receive Login Response
    char recvBuf[256];
    //char* recvBufPtr = recvBuf;
    int recvByte = 0;

    PacketHeader header = PacketHeader();

    while(true)
    {
        int nRecv = recv(client->socket, recvBuf + recvByte, sizeof(recvBuf), 0);
        if (nRecv == SOCKET_ERROR)
        {
            printf("recv() failed: %d\n", WSAGetLastError());
            closesocket(client->socket);
            client->socket = INVALID_SOCKET;
            return false;
        }
        if (nRecv == 0)
        {
            printf("connection closed\n");
            closesocket(client->socket);
            client->socket = INVALID_SOCKET;
            return false;
        }

        recvByte += nRecv;

        if (recvByte < PacketHeader::Size())
            continue;

        // header deserialize
        
        if(!PacketBuilder::Instance().DeserializeHeader(recvBuf, recvByte, header))
        {
            printf("Deserialize Header Failed\n");
            closesocket(client->socket);
            client->socket = INVALID_SOCKET;
            return false;
        }
        if (recvByte >= header.length)
            break;  // 수신 완료
    }

    // data deserialize
    if (header.packetId != PacketID::S2C_LOGIN_RESPONSE)
    {
        cout << "로그인 응답 패킷이 아님" << endl;
        closesocket(client->socket);
        client->socket = INVALID_SOCKET;
        return false;
    }
    Protocol::S2C_LoginResponse loginResponse;
    if (!PacketBuilder::Instance().DeserializeData(recvBuf, recvByte, header, loginResponse))
    {
        printf("Deserialize Data Failed\n");
        closesocket(client->socket);
        client->socket = INVALID_SOCKET;
        return false;
    }

    if (!loginResponse.success().value())
    {
        printf("login failed\n");
        closesocket(client->socket);
        client->socket = INVALID_SOCKET;
        return false;
    }

    return true;
}

bool MemoryPoolTestClient::Logout(TestClient* client)
{
    /// Send Logout Request
    Protocol::C2S_LogoutRequest logoutRequest;

    char* sendBuf = PacketBuilder::Instance().Serialize(PacketID::C2S_LOGOUT_REQUEST, logoutRequest);
    int sendByte = 0;
    int totalSendByte = PacketHeader::Size() + logoutRequest.ByteSizeLong();

    while (sendByte < totalSendByte) 
    {
        int nSend = send(client->socket, sendBuf + sendByte, totalSendByte - sendByte, 0);
        if (nSend == SOCKET_ERROR)
        {
            printf("send() failed: %d\n", WSAGetLastError());
            closesocket(client->socket);
            client->socket = INVALID_SOCKET;
            return false;
        }
        if (nSend == 0)
        {
            printf("connection closed\n");
            closesocket(client->socket);
            client->socket = INVALID_SOCKET;
            return false;
        }

        sendByte += nSend;
    }

    delete[] sendBuf;

    /// Receive Logout Response
    char recvBuf[256];
    //char* recvBufPtr = recvBuf;
    int recvByte = 0;

    PacketHeader header = PacketHeader();

    while (true)
    {
        int nRecv = recv(client->socket, recvBuf + recvByte, sizeof(recvBuf), 0);
        if (nRecv == SOCKET_ERROR)
        {
            printf("recv() failed: %d\n", WSAGetLastError());
            closesocket(client->socket);
            client->socket = INVALID_SOCKET;
            return false;
        }
        if (nRecv == 0)
        {
            printf("connection closed\n");
            closesocket(client->socket);
            client->socket = INVALID_SOCKET;
            return false;
        }

        recvByte += nRecv;

        if (recvByte < PacketHeader::Size())
            continue;

        // header deserialize

        if (!PacketBuilder::Instance().DeserializeHeader(recvBuf, recvByte, header))
        {
            printf("Deserialize Header Failed\n");
            closesocket(client->socket);
            client->socket = INVALID_SOCKET;
            return false;
        }
        if (recvByte >= header.length)
            break;  // 수신 완료
    }

    // data deserialize
    if (header.packetId != PacketID::S2C_LOGOUT_RESPONSE)
    {
        cout << "로그아웃 응답 패킷이 아님" << endl;
        closesocket(client->socket);
        client->socket = INVALID_SOCKET;
        return false;
    }
    Protocol::S2C_LogoutResponse logoutResponse;
    if (!PacketBuilder::Instance().DeserializeData(recvBuf, recvByte, header, logoutResponse))
    {
        printf("Deserialize Data Failed\n");
        closesocket(client->socket);
        client->socket = INVALID_SOCKET;
        return false;
    }

    return true;
}
