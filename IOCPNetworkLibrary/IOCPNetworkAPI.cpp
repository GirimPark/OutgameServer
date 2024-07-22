#include "pch.h"
#include "IOCPNetworkAPI.h"

bool IOCPNetworkAPI::InitializeIOCP(HANDLE& hIOCP)
{
    WSADATA wsaData;
    int rt = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (rt != 0)
    {
        printf("WSAStartup() faild: %d\n", rt);
        FinalizeIOCP(hIOCP);
        return false;
    }

    hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    if (hIOCP == NULL)
    {
        printf("CreateIoCompletionPort() failed to create I/O completion port: %d\n",
            GetLastError());
        FinalizeIOCP(hIOCP);
        return false;
    }
}

void IOCPNetworkAPI::FinalizeIOCP(HANDLE& hIOCP)
{
    CloseHandle(hIOCP);
    hIOCP = nullptr;

    WSACleanup();
}

SOCKET IOCPNetworkAPI::CreateSocket()
{
    SOCKET newSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED);
    if (newSocket == INVALID_SOCKET)
    {
        printf("WSASocket(sdSocket) failed: %d\n", WSAGetLastError());
        return newSocket;
    }

    int zero = 0;
    int rt = setsockopt(newSocket, SOL_SOCKET, SO_SNDBUF, (char*)&zero, sizeof(zero));
    if (rt == SOCKET_ERROR)
    {
        printf("setsockopt(SNDBUF) failed: %d\n", WSAGetLastError());
        return newSocket;
    }

    if (newSocket == NULL)
    {
        closesocket(newSocket);
        printf("create Socket failed: %d\n", GetLastError());
        return newSocket;
    }

    return newSocket;
}

SOCKET IOCPNetworkAPI::CreateListenSocket()
{
    int rt = 0;
    addrinfo hints = { 0 };
    addrinfo* addrlocal = nullptr;

    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_IP;

    if (getaddrinfo(NULL, m_listeningPort, &hints, &addrlocal) != 0) {
        printf("getaddrinfo() failed with error %d\n", WSAGetLastError());
        return NULL;
    }

    if (addrlocal == NULL) {
        printf("getaddrinfo() failed to resolve/convert the interface\n");
        return NULL;
    }

    SOCKET listenSocket = CreateSocket();
    if (listenSocket == INVALID_SOCKET) {
        freeaddrinfo(addrlocal);
        return NULL;
    }

    rt = bind(listenSocket, addrlocal->ai_addr, (int)addrlocal->ai_addrlen);
    if (rt == SOCKET_ERROR) {
        printf("bind() failed: %d\n", WSAGetLastError());
        freeaddrinfo(addrlocal);
        return NULL;
    }

    rt = listen(listenSocket, m_backlog);
    if (rt == SOCKET_ERROR) {
        printf("listen() failed: %d\n", WSAGetLastError());
        freeaddrinfo(addrlocal);
        return NULL;
    }

    freeaddrinfo(addrlocal);

    return listenSocket;
}

bool IOCPNetworkAPI::CreateListenContext(ListenContext*& pListenContext, HANDLE& hIOCP)
{
    pListenContext = new ListenContext;

    if (pListenContext)
    {
        pListenContext->listenSocket = IOCPNetworkAPI::Instance().CreateListenSocket();
        if (!pListenContext->listenSocket)
        {
            closesocket(pListenContext->listenSocket);
            printf("create Listen Socket failed: %d\n", GetLastError());
            return false;
        }
        pListenContext->acceptedSocket = INVALID_SOCKET;
        pListenContext->acceptOverlapped.Internal = 0;
        pListenContext->acceptOverlapped.InternalHigh = 0;
        pListenContext->acceptOverlapped.Offset = 0;
        pListenContext->acceptOverlapped.OffsetHigh = 0;
        pListenContext->acceptOverlapped.hEvent = nullptr;
    }
    else
    {
        delete pListenContext;
        printf("new ListenContext failed: %d\n", GetLastError());
        return false;
    }

    hIOCP = CreateIoCompletionPort((HANDLE)pListenContext->listenSocket, hIOCP, (ULONG_PTR)pListenContext, 0);
    if (!hIOCP)
    {
        printf("CreateIoCompletionPort failed to associate socket with error: %d\n", GetLastError());
        return false;
    }

    // Accept 게시
    if (!IOCPNetworkAPI::Instance().StartAccept(pListenContext))
    {
        return false;
    }

    return true;
}

bool IOCPNetworkAPI::ConfigureAcceptedSocket(ListenContext*& pListenContext, sockaddr_in*& remoteAddr)
{
    sockaddr_in* localAddr = nullptr;
    int localAddrLen = 0, remoteAddrLen = 0;

    GetAcceptExSockaddrs(
        pListenContext->acceptBuffer,
        INIT_DATA_SIZE,
        sizeof(SOCKADDR_IN) + IP_SIZE,
        sizeof(SOCKADDR_IN) + IP_SIZE,
        (sockaddr**)&localAddr,
        &localAddrLen,
        (sockaddr**)&remoteAddr,
        &remoteAddrLen
    );

    // 수락 클라이언트 소켓 옵션 설정
    int rt = setsockopt(pListenContext->acceptedSocket,
        SOL_SOCKET,
        SO_UPDATE_ACCEPT_CONTEXT,
        (char*)&pListenContext->listenSocket,
        sizeof(pListenContext->listenSocket));
    if (rt == SOCKET_ERROR)
    {
        printf("setsockopt(SO_UPDATE_ACCEPT_CONTEXT) failed to update accept socket : %d\n", GetLastError());
        return false;
    }

    char addr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &localAddr->sin_addr, addr, INET_ADDRSTRLEN);
    printf("Local Address: %s\n", addr);
    inet_ntop(AF_INET, &remoteAddr->sin_addr, addr, INET_ADDRSTRLEN);
    printf("Remote Address: %s\n", addr);
}

bool IOCPNetworkAPI::StartAccept(ListenContext*& listenSocketCtxt)
{
    // 새로운 acceptedSocket 생성
    listenSocketCtxt->acceptedSocket = CreateSocket();
    if (listenSocketCtxt->acceptedSocket == INVALID_SOCKET)
    {
        printf("failed to create new accept socket\n");
        return false;
    }

    DWORD nRecvByte = 0;
    int rt = AcceptEx(
        listenSocketCtxt->listenSocket,
        listenSocketCtxt->acceptedSocket,
        listenSocketCtxt->acceptBuffer,
        INIT_DATA_SIZE,
        sizeof(SOCKADDR_IN) + IP_SIZE,
        sizeof(SOCKADDR_IN) + IP_SIZE,
        &nRecvByte,
        &listenSocketCtxt->acceptOverlapped);
    if (rt == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING))
    {
        printf("AcceptEx() failed: %d\n", WSAGetLastError());
        closesocket(listenSocketCtxt->acceptedSocket);
        listenSocketCtxt->acceptedSocket = INVALID_SOCKET; // 소켓 상태 초기화
        return false;
    }

    return true;
}

bool IOCPNetworkAPI::StartReceive(SOCKET& socket, OVERLAPPED_STRUCT& overlapped)
{
    DWORD flags = 0;
    DWORD bytesReceived = 0;
    overlapped.IOOperation = OVERLAPPED_STRUCT::eIOType::READ;
    overlapped.wsaBuffer.len = sizeof(overlapped.buffer);

    int rt = WSARecv(socket, &overlapped.wsaBuffer, 1, &bytesReceived, &flags, &overlapped, NULL);
    if (rt == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
    {
        printf("WSARecv() failed: %d\n", WSAGetLastError());
        return false;
    }

    return true;
}

bool IOCPNetworkAPI::StartSend(SOCKET& socket, OVERLAPPED_STRUCT& overlapped, const char* data, int length)
{
    overlapped.IOOperation = OVERLAPPED_STRUCT::eIOType::WRITE;
    ZeroMemory(overlapped.buffer, MAX_BUF_SIZE);
    memcpy(overlapped.buffer, data, length);
    overlapped.wsaBuffer.len = length;

    int rt = WSASend(socket, &overlapped.wsaBuffer, 1, NULL, 0, &overlapped, NULL);
    if (rt == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
    {
        printf("WSASend() failed: %d\n", WSAGetLastError());
        return false;
    }

    return true;
}