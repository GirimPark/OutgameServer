#include "pch.h"
#include "ServerCore.h"

ServerCore::ServerCore(const char* port)
    : m_bEndServer(false)
    , m_listeningPort(port)
{
    SYSTEM_INFO systemInfo;
    GetSystemInfo(&systemInfo);
    m_nThread = systemInfo.dwNumberOfProcessors * 2;
    //m_nThread = 1;
    m_threads.resize(m_nThread);
}

ServerCore::~ServerCore()
{
    m_sessionMap.clear();
}

bool ServerCore::Run()
{
    InitializeCriticalSection(&m_criticalSection);

    if((m_hCleanupEvent[0] = WSACreateEvent()) == WSA_INVALID_EVENT)
    {
        printf("WSACreateEvent() failed: %d\n", WSAGetLastError());
        return false;
    }

    WSADATA wsaData;
    int rt = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (rt != 0)
    {
        printf("WSAStartup() faild: %d\n", rt);
        return false;
    }

    m_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    if (m_hIOCP == NULL)
    {
        printf("CreateIoCompletionPort() failed to create I/O completion port: %d\n",
            GetLastError());
        return false;
    }

    // ���� ���� �� ���ؽ�Ʈ ���� + acceptEx �Խ�
    if(!CreateListenContext())
    {
        printf("Create Listen Socket Context failed\n");
        return false;
    }

    // ��Ŀ ������ ����/���� + ���
    for(int i=0; i<m_nThread; ++i)
    {
        m_threads[i] = new std::thread(&ServerCore::ProcessThread, this);
    }

    WSAWaitForMultipleEvents(1, m_hCleanupEvent, true, WSA_INFINITE, false);

    // ����
    Finalize();
    return true;
}

void ServerCore::Finalize()
{
    m_bEndServer = true;

    // ���� �۾� �Ϸ� ��� �� ���� ����
    for(auto iter : m_sessionMap)
    {
        while (!HasOverlappedIoCompleted(&iter.second->recvOverlapped))
            Sleep(0);

        delete iter.second;
    }
    m_sessionMap.clear();

    // ���� ���� ���ؽ�Ʈ, ������, �̺�Ʈ, ���� ��, ũ��Ƽ�� ����, iocp �ڵ�
	// ���� ���� ���ؽ�Ʈ ����
    delete m_pListenSocketCtxt;

    if(m_hIOCP)
    {
        // ������ ����
	    for(int i=0; i<m_nThread; ++i)
	    {
            PostQueuedCompletionStatus(m_hIOCP, 0, 0, NULL);
	    }

        // IOCP �ڵ� ����
        CloseHandle(m_hIOCP);
        m_hIOCP = NULL;
    }

    // ������ ����
    for (int i = 0; i < m_nThread; ++i)
    {
        if (m_threads[i] && m_threads[i]->joinable())
        {
            m_threads[i]->join();
        }
    }

    // �̺�Ʈ ����
    if(m_hCleanupEvent[0] != WSA_INVALID_EVENT)
    {
        WSACloseEvent(m_hCleanupEvent[0]);
        m_hCleanupEvent[0] = WSA_INVALID_EVENT;
    }

    // ũ��Ƽ�� ���� ����
    DeleteCriticalSection(&m_criticalSection);

    WSACleanup();
}

SOCKET ServerCore::CreateSocket()
{
    SOCKET newSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED);
    if (newSocket == INVALID_SOCKET)
    {
        printf("WSASocket(sdSocket) failed: %d\n", WSAGetLastError());
        return newSocket;
    }

    return newSocket;
}

bool ServerCore::CreateListenContext()
{
    m_pListenSocketCtxt = new ListenContext;

    if(m_pListenSocketCtxt)
    {
        m_pListenSocketCtxt->listenSocket = CreateListenSocket();
        if(!m_pListenSocketCtxt->listenSocket)
        {
            closesocket(m_pListenSocketCtxt->listenSocket);
            printf("create Listen Socket failed: %d\n", GetLastError());
            return false;
        }

        m_pListenSocketCtxt->acceptedSocket = CreateSocket();
        if (!m_pListenSocketCtxt->acceptedSocket)
        {
            closesocket(m_pListenSocketCtxt->acceptedSocket);
            printf("create Accepted Socket failed: %d\n", GetLastError());
            return false;
        }

        m_pListenSocketCtxt->acceptOverlapped.Internal = 0;
        m_pListenSocketCtxt->acceptOverlapped.InternalHigh = 0;
        m_pListenSocketCtxt->acceptOverlapped.Offset = 0;
        m_pListenSocketCtxt->acceptOverlapped.OffsetHigh = 0;
        m_pListenSocketCtxt->acceptOverlapped.hEvent = nullptr;
    }
    else
    {
        delete m_pListenSocketCtxt;
        printf("new ListenContext failed: %d\n", GetLastError());
        return false;
    }

    m_hIOCP = CreateIoCompletionPort((HANDLE)m_pListenSocketCtxt->listenSocket, m_hIOCP, (ULONG_PTR)m_pListenSocketCtxt, 0);
    if (!m_hIOCP)
    {
        printf("CreateIoCompletionPort failed to associate socket with error: %d\n", GetLastError());
        return false;
    }

    // Accept �Խ�
    if (!StartAccept())
    {
        return false;
    }

    return true;
}

SOCKET ServerCore::CreateListenSocket()
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

    rt = listen(listenSocket, 5);
    if (rt == SOCKET_ERROR) {
        printf("listen() failed: %d\n", WSAGetLastError());
        freeaddrinfo(addrlocal);
        return NULL;
    }

    freeaddrinfo(addrlocal);

    return listenSocket;
}

Session* ServerCore::CreateSession()
{
    static unsigned int nextSessionId = 1;
    Session* session = new Session();
    session->sessionId = nextSessionId++;
    ZeroMemory(&session->sendOverlapped, sizeof(OVERLAPPED));
    ZeroMemory(&session->recvOverlapped, sizeof(OVERLAPPED));

    return session;
}

void ServerCore::ProcessThread()
{
    bool bSuccess = false;
    int rt = 0;

    OVERLAPPED* overlapped = nullptr;
    OVERLAPPED_STRUCT* overlappedStruct = nullptr;
    ULONG_PTR completionKey = NULL;
    Session* session = nullptr;
    ListenContext* listenCtxt = nullptr;
    DWORD flags = 0;
    DWORD nTransferredByte = 0;

    while(true)
    {
        bSuccess = GetQueuedCompletionStatus(m_hIOCP, &nTransferredByte, &completionKey, &overlapped, INFINITE);
        if(!bSuccess)
        {
            printf("GetQueuedCompletionStatus() failed: %d\n", GetLastError());
        }
        if(!m_pListenSocketCtxt)
        {
            return;
        }
        if(m_bEndServer)
        {
            return;
        }

        listenCtxt = reinterpret_cast<ListenContext*>(completionKey);
        session = reinterpret_cast<Session*>(completionKey);
        overlappedStruct = reinterpret_cast<OVERLAPPED_STRUCT*>(overlapped);
        if(listenCtxt && listenCtxt->type == eCompletionKeyType::LISTEN_CONTEXT)
        {
            HandleAcceptCompletion();
        }
        else if(session && session->type == eCompletionKeyType::SESSION)
        {
            // READ or WRITE
            switch (overlappedStruct->IOOperation)
            {
            case OVERLAPPED_STRUCT::eIOType::READ:
            {
                HandleReadCompletion(session, nTransferredByte);
                //HandleReadCompletion(session, MAX_BUF_SIZE);
                break;
            }
            case OVERLAPPED_STRUCT::eIOType::WRITE:
            {
                HandleWriteCompletion(session);
                break;
            }
            default:
            {
                printf("���ǵ��� ���� IO Type\n");
                return;
            }
            }
        }
        else
        {
            printf("Completion Key Ÿ�� ĳ���� ����\n");
        }
        
    }
}

void ServerCore::HandleAcceptCompletion()
{
    sockaddr_in* localAddr = nullptr;
    sockaddr_in* remoteAddr = nullptr;
    int localAddrLen = 0, remoteAddrLen = 0;

    GetAcceptExSockaddrs(
        m_pListenSocketCtxt->acceptBuffer,
        INIT_DATA_SIZE,  // �ʱ� ������ ����
        sizeof(SOCKADDR_IN) + IP_SIZE,  // ���� �ּ� ���� ����
        sizeof(SOCKADDR_IN) + IP_SIZE,  // ���� �ּ� ���� ����
        (sockaddr**)&localAddr,
        &localAddrLen,
        (sockaddr**)&remoteAddr,
        &remoteAddrLen
    );

    // ���� Ŭ���̾�Ʈ ���� �ɼ� ����
    int rt = setsockopt(m_pListenSocketCtxt->acceptedSocket,
        SOL_SOCKET,
        SO_UPDATE_ACCEPT_CONTEXT,
        (char*)&m_pListenSocketCtxt->listenSocket,
        sizeof(m_pListenSocketCtxt->listenSocket));
    if (rt == SOCKET_ERROR)
    {
        printf("setsockopt(SO_UPDATE_ACCEPT_CONTEXT) failed to update accept socket : %d\n", GetLastError());
        WSASetEvent(m_hCleanupEvent[0]);
        return;
    }

    // ���� ����, iocp ���
    Session* session = CreateSession();
    session->clientSocket = m_pListenSocketCtxt->acceptedSocket;
    session->clientIP = *remoteAddr;

    m_hIOCP = CreateIoCompletionPort((HANDLE)session->clientSocket, m_hIOCP, (ULONG_PTR)session, 0);
    if (!m_hIOCP)
    {
        printf("CreateIoCompletionPort failed to associate socket with error: %d\n", GetLastError());
        return;
    }

    char addr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &localAddr->sin_addr, addr, INET_ADDRSTRLEN);
    printf("Local Address: %s\n", addr);
    inet_ntop(AF_INET, &remoteAddr->sin_addr, addr, INET_ADDRSTRLEN);
    printf("Remote Address: %s\n", addr);

    // �ʱ� ������ ó��(����, ���� ����, ����)
    ProcessInitialData(session, m_pListenSocketCtxt->acceptBuffer, INIT_DATA_SIZE);

    // �ٸ� ����, ���� �۾� �Խ�
    StartAccept();

    // ���� ����
    ZeroMemory(session->recvOverlapped.buffer, MAX_BUF_SIZE);
    memcpy(session->recvOverlapped.buffer, m_pListenSocketCtxt->acceptBuffer, INIT_DATA_SIZE);
    StartSend(session, session->recvOverlapped.buffer, INIT_DATA_SIZE);
}

void ServerCore::HandleReadCompletion(Session* session, DWORD bytesTransferred)
{
    if (bytesTransferred > 0) {
        printf("Data received: %.*s\n", bytesTransferred, session->recvOverlapped.wsaBuffer.buf);
        StartSend(session, session->recvOverlapped.buffer, bytesTransferred);
    } else {
        // No data received, handle accordingly
        printf("No data received\n");
    }
}

void ServerCore::HandleWriteCompletion(Session* session)
{
    printf("Data send: %s\n", session->sendOverlapped.wsaBuffer.buf);
    StartReceive(session);
}

bool ServerCore::StartAccept()
{
    if (m_pListenSocketCtxt->acceptedSocket == INVALID_SOCKET)
    {
        printf("failed to create new accept socket\n");
        return false;
    }

    DWORD nRecvByte = 0;
    int rt = AcceptEx(
        m_pListenSocketCtxt->listenSocket,
        m_pListenSocketCtxt->acceptedSocket,
        m_pListenSocketCtxt->acceptBuffer,
        INIT_DATA_SIZE,
        sizeof(SOCKADDR_IN) + IP_SIZE,
        sizeof(SOCKADDR_IN) + IP_SIZE,
        &nRecvByte,
        &m_pListenSocketCtxt->acceptOverlapped);
    if (rt == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING))
    {
        printf("AcceptEx() failed: %d\n", WSAGetLastError());
        return false;
    }

    return true;
}

bool ServerCore::StartReceive(Session* session)
{
    DWORD flags = 0;
    DWORD bytesReceived = 0;
    session->recvOverlapped.IOOperation = OVERLAPPED_STRUCT::eIOType::READ;
    session->recvOverlapped.wsaBuffer.len = sizeof(session->recvOverlapped.buffer);

    int rt = WSARecv(session->clientSocket, &session->recvOverlapped.wsaBuffer, 1, &bytesReceived, &flags, &session->recvOverlapped, NULL);
    if(rt == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
    {
        printf("WSARecv() failed: %d\n", WSAGetLastError());
        return false;
    }

    return true;
}

bool ServerCore::StartSend(Session* session, const char* data, int length)
{
    session->sendOverlapped.IOOperation = OVERLAPPED_STRUCT::eIOType::WRITE;
    ZeroMemory(session->sendOverlapped.buffer, MAX_BUF_SIZE);
    memcpy(session->sendOverlapped.buffer, data, length);
    session->sendOverlapped.wsaBuffer.len = length;

    int rt = WSASend(session->clientSocket, &session->sendOverlapped.wsaBuffer, 1, NULL, 0, &session->sendOverlapped, NULL);
    if (rt == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) 
    {
        printf("WSASend() failed: %d\n", WSAGetLastError());
        return false;
    }

    return true;
}

void ServerCore::ProcessInitialData(Session* session, char* data, int length)
{
    const char* responseData;

    std::string username(data, data + 50);
    std::string password(data + 50, data + 100);
    if (AuthenticateUser(username, password))
    {
        // ���� ��Ͽ� �߰�
        m_sessionMap.emplace(session->sessionId, session);
        responseData = "login success";
    }
    else
    {
        // Accept�� ���� ó���ؾ� ��. ���� �� �α��� ���� ���� -> ���� ����
        responseData = "login failed";
    }

    // �۽� �۾� �Խ�
    //StartSend(session, responseData, 20);
}

bool ServerCore::AuthenticateUser(const std::string_view& username, const std::string_view& password)
{
    return true;
}
