#ifdef _DEBUG
#pragma comment(lib, "libprotobufd.lib")
#else
#pragma comment(lib, "libprotobuf.lib")
#endif

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Mswsock.lib")

#include "pch.h"
#include "EchoClient.h"
#include "../PacketLibrary/Echo.pb.h"

EchoClient::EchoClient(const char* ip, const char* port)
    : m_bEndClient(false)
    , m_connectIP(ip)
    , m_port(port)
{
    m_clients.resize(MAX_CLIENT);
    for (auto& client : m_clients)
    {
        client.thread = nullptr;
        client.socket = INVALID_SOCKET;
    }

    m_hCleanupEvent[0] = WSA_INVALID_EVENT;
}

EchoClient::~EchoClient()
{
}

void EchoClient::Run()
{
    if ((m_hCleanupEvent[0] = WSACreateEvent()) == WSA_INVALID_EVENT)
    {
        printf("WSACreateEvent() failed: %d\n", WSAGetLastError());
        return;
    }

    WSADATA WSAData;
    int rt;
    if ((rt = WSAStartup(MAKEWORD(2, 2), &WSAData)) != 0)
    {
        printf("WSAStartup() failed: %d", rt);
        return;
    }

    for (int i = 0; i < MAX_CLIENT; ++i)
    {
        if (m_bEndClient)
            break;

        m_clients[i].socket = CreateConnectedSocket(i);
        if (m_clients[i].socket == INVALID_SOCKET)
        {
            break;
        }
        m_clients[i].thread = new std::thread(&EchoClient::EchoThread, this, i);
        if (!m_clients[i].thread)
        {
            printf("CreateThread(%d) failed: %d\n", i, GetLastError());
            return;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    WSAWaitForMultipleEvents(1, m_hCleanupEvent, true, WSA_INFINITE, false);

    // ÇØÁ¦
    for (auto& iter : m_clients)
    {
        if (iter.thread && iter.thread->joinable())
        {
            closesocket(iter.socket);
            iter.socket = INVALID_SOCKET;
            iter.thread->join();
            delete iter.thread;
        }
    }

    if (m_hCleanupEvent[0] != WSA_INVALID_EVENT)
    {
        WSACloseEvent(m_hCleanupEvent[0]);
        m_hCleanupEvent[0] = WSA_INVALID_EVENT;
    }

    WSACleanup();
}

SOCKET EchoClient::CreateConnectedSocket(int threadId)
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
        printf("connect(thread %d) failed: %d\n", threadId, WSAGetLastError());
        closesocket(socket);
        freeaddrinfo(addr_srv);
        return INVALID_SOCKET;
    }

    freeaddrinfo(addr_srv);

    printf("connected on thread %d\n", threadId);
    return socket;
}

void EchoClient::EchoThread(int threadId)
{
    char* inbuf = nullptr;
    char* outbuf = nullptr;

    inbuf = new char[MAX_BUF_SIZE];
    outbuf = new char[MAX_BUF_SIZE];

    if ((inbuf) && (outbuf))
    {
        ZeroMemory(outbuf, MAX_BUF_SIZE);

        while (true)
        {
            if (SendBuffer(threadId, outbuf) && RecvBuffer(threadId, inbuf))
            {
                if ((inbuf[0] != outbuf[0]) ||
                    (inbuf[MAX_BUF_SIZE - 1] != outbuf[MAX_BUF_SIZE - 1]))
                {
                    printf("nak(%d) in[0]=%d, out[0]=%d, in[%d]=%d out[%d]=%d\n",
                        threadId,
                        inbuf[0], outbuf[0],
                        MAX_BUF_SIZE - 1, inbuf[MAX_BUF_SIZE - 1],
                        MAX_BUF_SIZE - 1, outbuf[MAX_BUF_SIZE - 1]);
                    break;
                }
            }
            else
                break;
        }
    }

    if (inbuf)
        delete[] inbuf;
    if (outbuf)
        delete[] outbuf;
}

bool EchoClient::SendBuffer(int threadId, char* outbuf)
{
    Protocol::C2S_Echo echoMessage;
    echoMessage.set_data("Hello, this is a test message!");

    bool rt = true;
    char* bufp = PacketBuilder::Instance().Serialize(EPacketType::C2S_ECHO, echoMessage);
    int nTotalSend = 0;
    int nSend = 0;
    m_nSendByte = PacketHeader::Size() + echoMessage.ByteSizeLong();

    while (nTotalSend < m_nSendByte)
    {
        nSend = send(m_clients[threadId].socket, bufp + nTotalSend, m_nSendByte - nTotalSend, 0);
        if (nSend == SOCKET_ERROR)
        {
            printf("send(thread=%d) failed: %d\n", threadId, WSAGetLastError());
            rt = false;
            return rt;
        }
        if (nSend == 0)
        {
            printf("connection closed\n");
            rt = false;
            return rt;
        }

        nTotalSend += nSend;
    }

	memcpy(outbuf, echoMessage.data().c_str(), echoMessage.data().size());
	printf("Send: %s\n", outbuf);

    return rt;
}

bool EchoClient::RecvBuffer(int threadId, char* inbuf)
{
    ZeroMemory(inbuf, MAX_BUF_SIZE);
    char* bufp = inbuf;
    int nTotalRecv = 0;
    int nRecv = 0;

    while (nTotalRecv < m_nSendByte)
    {
        nRecv = recv(m_clients[threadId].socket, bufp + nTotalRecv, m_nSendByte - nTotalRecv, 0);
        if (nRecv == SOCKET_ERROR)
        {
            printf("recv(thread=%d) failed: %d\n", threadId, WSAGetLastError());
            return false;
        }
        if (nRecv == 0)
        {
            printf("connection closed\n");
            return false;
        }

        nTotalRecv += nRecv;
    }

    PacketHeader header;
    Protocol::S2C_Echo data;

    if (!PacketBuilder::Instance().Deserialize(inbuf, nTotalRecv, header, data))
    {
        printf("Packet Deserialize Failed\n");
        return false;
    }

    std::string receivedMessage = data.data();
    memcpy(inbuf, receivedMessage.c_str(), receivedMessage.size());
    printf("Received: %s\n", receivedMessage.c_str());

    return true;
}
