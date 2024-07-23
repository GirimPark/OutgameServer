#include "pch.h"
#include "ServerCore.h"

ServerCore::ServerCore(const char* port, int backlog)
    : m_bEndServer(false)
    , m_listeningPort(port)
	, m_backlog(backlog)
{
    SYSTEM_INFO systemInfo;
    GetSystemInfo(&systemInfo);
    m_nThread = 5;// systemInfo.dwNumberOfProcessors * 2;
    m_IOCPThreads.resize(m_nThread);
}

ServerCore::~ServerCore()
{
}

void ServerCore::Run()
{
    InitializeCriticalSection(&m_criticalSection);

    WSADATA wsaData;
    int rt = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (rt != 0)
    {
        printf("WSAStartup() faild: %d\n", rt);
        Finalize();
        return;
    }

    m_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    if (m_hIOCP == NULL)
    {
        printf("CreateIoCompletionPort() failed to create I/O completion port: %d\n",
            GetLastError());
        Finalize();
        return;
    }

    // 리슨 소켓 및 컨텍스트 생성 + acceptEx 게시
    if(!CreateListenContext())
    {
        printf("Create Listen Socket Context failed\n");
        Finalize();
        return;
    }

    // 워커 스레드 생성/실행 + 대기
    for(int i=0; i<m_nThread; ++i)
    {
        m_IOCPThreads[i] = new std::thread(&ServerCore::ProcessThread, this);
        if(!m_IOCPThreads[i])
        {
            printf("std::thread() failed to create process thread: %d\n", GetLastError());
            Finalize();
            return;
        }
    }

    for(int i=0; i<m_nThread; ++i)
    {
        if (m_IOCPThreads[i]->joinable())
            m_IOCPThreads[i]->join();
    }

    // 해제
    Finalize();
}

void ServerCore::RegisterCallback(ReceiveDataCallback callback)
{
    m_receiveCallbacks.emplace_back(callback);
}

void ServerCore::Finalize()
{
    m_bEndServer = true;

	// 리슨 소켓 컨텍스트 해제
    if(m_pListenSocketCtxt)
    {
        DWORD result = WaitForSingleObject(m_pListenSocketCtxt->acceptOverlapped.hEvent, INFINITE);
        if (result != WAIT_OBJECT_0)
        {
            std::cerr << "Error waiting for AcceptEx to complete: " << GetLastError() << std::endl;
        }

        delete m_pListenSocketCtxt;
    }
        
    // 세션 해제
    for(int i = 0; i<m_sessionMap.size(); ++i)
    {
        if(m_sessionMap[i])
        {
			UnregisterSession(m_sessionMap[i]->sessionId);
            --i;
        }
    }
    m_sessionMap.clear();

    if(m_hIOCP)
    {
        // IOCP 핸들 해제
        CloseHandle(m_hIOCP);
        m_hIOCP = NULL;
    }

    // 스레드 해제
    for (int i = 0; i < m_nThread; ++i)
    {
        if (m_IOCPThreads[i] && m_IOCPThreads[i]->joinable())
        {
            m_IOCPThreads[i]->join();
        }
        delete m_IOCPThreads[i];
    }
    m_IOCPThreads.clear();

    // 크리티컬 섹션 해제
    DeleteCriticalSection(&m_criticalSection);

    m_receiveCallbacks.clear();

    WSACleanup();

    delete this;
}

void ServerCore::TriggerShutdown()
{
    m_bEndServer = true;

    if (m_hIOCP)
    {
        // 스레드 종료, ProcessThread는 GetQueuedCompletionStatus에서 무한 대기하므로 별도로 처리가 필요하다.
        for (int i = 0; i < m_nThread; ++i)
        {
            PostQueuedCompletionStatus(m_hIOCP, 0, 0, NULL);
        }
    }
}

SOCKET ServerCore::CreateSocket()
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

    if(newSocket == NULL)
    {
        closesocket(newSocket);
        printf("create Socket failed: %d\n", GetLastError());
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
        m_pListenSocketCtxt->acceptedSocket = INVALID_SOCKET;
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

    // Accept 게시
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

    rt = listen(listenSocket, m_backlog);
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

void ServerCore::CloseSession(Session* session, bool needLock)
{
    if(needLock)
		EnterCriticalSection(&m_criticalSection);

    linger lingerStruct;
    lingerStruct.l_onoff = 1;
    lingerStruct.l_linger = 0;
    setsockopt(session->clientSocket, SOL_SOCKET, SO_LINGER, (char*)&lingerStruct, sizeof(lingerStruct));

    if (m_bEndServer)
    {
        while (!(HasOverlappedIoCompleted(&session->recvOverlapped)))
            Sleep(0);
    }

    if(needLock)
		LeaveCriticalSection(&m_criticalSection);

    delete session;
}

void ServerCore::RegisterSession(Session* session)
{
    session->state = Session::eStateType::MAINTAIN;
    m_sessionMap.insert({ session->sessionId, session });
}

void ServerCore::UnregisterSession(SessionId sessionId)
{
    auto iter = m_sessionMap.find(sessionId);
    if (iter == m_sessionMap.end())
    {
        printf("UnregisterSession : can't find session\n");
        return;
    }
    Session* session = iter->second;
    if(!session)
    {
        printf("UnregisterSession : unvalid session\n");
        return;
    }

    EnterCriticalSection(&m_criticalSection);
    m_sessionMap.unsafe_erase(iter);
    CloseSession(session, false);
    LeaveCriticalSection(&m_criticalSection);
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

    while(!m_bEndServer)
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
            HandleAcceptCompletion(nTransferredByte);
        }
        else if(session && session->type == eCompletionKeyType::SESSION)
        {
            // 좀비 클라이언트 예외처리
            if(nTransferredByte == 0)
            {
                UnregisterSession(session->sessionId);
                continue;
            }

            // READ or WRITE
            switch (overlappedStruct->IOOperation)
            {
            case OVERLAPPED_STRUCT::eIOType::READ:
            {
                OnReceiveData(session, session->recvOverlapped.buffer, nTransferredByte);
                StartReceive(session);
                //HandleReadCompletion(session, nTransferredByte);
                break;
            }
            case OVERLAPPED_STRUCT::eIOType::WRITE:
            {
                HandleWriteCompletion(session);
                break;
            }
            default:
            {
                printf("정의되지 않은 IO Type\n");
                return;
            }
            }
        }
        else if(session->type == eCompletionKeyType::SESSION && !session)
        {
            printf("이미 해제된 세션\n");
            continue;
        }
        
    }
}

void ServerCore::HandleAcceptCompletion(DWORD nTransferredByte)
{
    sockaddr_in* localAddr = nullptr;
    sockaddr_in* remoteAddr = nullptr;
    int localAddrLen = 0, remoteAddrLen = 0;

    GetAcceptExSockaddrs(
        m_pListenSocketCtxt->acceptBuffer,
        INIT_DATA_SIZE,
        sizeof(SOCKADDR_IN) + IP_SIZE,
        sizeof(SOCKADDR_IN) + IP_SIZE,
        (sockaddr**)&localAddr,
        &localAddrLen,
        (sockaddr**)&remoteAddr,
        &remoteAddrLen
    );

    // 수락 클라이언트 소켓 옵션 설정
    int rt = setsockopt(m_pListenSocketCtxt->acceptedSocket,
        SOL_SOCKET,
        SO_UPDATE_ACCEPT_CONTEXT,
        (char*)&m_pListenSocketCtxt->listenSocket,
        sizeof(m_pListenSocketCtxt->listenSocket));
    if (rt == SOCKET_ERROR)
    {
        printf("setsockopt(SO_UPDATE_ACCEPT_CONTEXT) failed to update accept socket : %d\n", GetLastError());
        m_bEndServer = true;
        return;
    }

    // 세션 생성, iocp 등록
    Session* session = CreateSession();
    session->clientSocket = m_pListenSocketCtxt->acceptedSocket;
    session->clientIP = *remoteAddr;

    m_hIOCP = CreateIoCompletionPort((HANDLE)session->clientSocket, m_hIOCP, (ULONG_PTR)session, 0);
    if (!m_hIOCP)
    {
        printf("CreateIoCompletionPort failed to associate socket with error: %d\n", GetLastError());
        m_bEndServer = true;
        return;
    }

    char addr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &localAddr->sin_addr, addr, INET_ADDRSTRLEN);
    printf("Local Address: %s\n", addr);
    inet_ntop(AF_INET, &remoteAddr->sin_addr, addr, INET_ADDRSTRLEN);
    printf("Remote Address: %s\n", addr);

    OnReceiveData(session, m_pListenSocketCtxt->acceptBuffer, nTransferredByte);

    // 다른 수락 작업 게시
    StartAccept();
    StartReceive(session);
}

void ServerCore::HandleReadCompletion(Session* session, DWORD bytesTransferred)
{
    if (bytesTransferred > 0) 
    {
        StartSend(session, session->recvOverlapped.buffer, bytesTransferred);
    }
}

void ServerCore::HandleWriteCompletion(Session* session)
{ 
    switch(session->state)
    {
    case Session::eStateType::REGISTER:
	    {
        RegisterSession(session);
        break;
	    }
    case Session::eStateType::CLOSE:
	    {
        CloseSession(session);
        break;
	    }
    case Session::eStateType::UNREGISTER:
	    {
        UnregisterSession(session->sessionId);
        break;
	    }
    default:
    	break;
    }
}

bool ServerCore::Unicast(Session* session, const char* data, int length)
{
    if (!session)
        return false;

    return StartSend(session, data, length);
}

bool ServerCore::Broadcast(const char* data, int length)
{
    if (m_sessionMap.empty())
        return true;

    for(const auto& session : m_sessionMap)
    {
        if (!session.second)
            continue;

        if (!StartSend(session.second, data, length))
        {
            return false;
        }
    }
    return true;
}

bool ServerCore::StartAccept()
{
    // 새로운 acceptedSocket 생성
    m_pListenSocketCtxt->acceptedSocket = CreateSocket();
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
        closesocket(m_pListenSocketCtxt->acceptedSocket);
        m_pListenSocketCtxt->acceptedSocket = INVALID_SOCKET; // 소켓 상태 초기화
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
        UnregisterSession(session->sessionId);
        return false;
    }

    return true;
}

bool ServerCore::StartSend(Session* session, const char* data, int length)
{
    if (m_bEndServer)
        return false;

    session->sendOverlapped.IOOperation = OVERLAPPED_STRUCT::eIOType::WRITE;
    ZeroMemory(session->sendOverlapped.buffer, MAX_BUF_SIZE);
    memcpy(session->sendOverlapped.buffer, data, length);
    session->sendOverlapped.wsaBuffer.len = length;

    int rt = WSASend(session->clientSocket, &session->sendOverlapped.wsaBuffer, 1, NULL, 0, &session->sendOverlapped, NULL);
    if (rt == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) 
    {
        printf("WSASend() failed: %d\n", WSAGetLastError());
        UnregisterSession(session->sessionId);
        return false;
    }

    return true;
}

void ServerCore::OnReceiveData(Session* session, char* data, int nReceivedByte)
{
    for (const auto& callback : m_receiveCallbacks)
        callback(session, data, nReceivedByte);
}
