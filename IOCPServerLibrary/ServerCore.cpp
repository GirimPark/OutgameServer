#include "pch.h"
#include "ServerCore.h"

ServerCore::ServerCore(const char* port, int backlog)
    : m_bEndServer(false)
{
    SYSTEM_INFO systemInfo;
    GetSystemInfo(&systemInfo);
    m_nThread = systemInfo.dwNumberOfProcessors;
    m_IOCPThreads.resize(m_nThread);

    IOCPNetworkAPI::Instance().SetListeningPort(port);
    IOCPNetworkAPI::Instance().SetBacklog(backlog);
}

ServerCore::~ServerCore()
{
}

void ServerCore::Run()
{
    InitializeCriticalSection(&m_criticalSection);

    if((m_hCleanupEvent[0] = WSACreateEvent()) == WSA_INVALID_EVENT)
    {
        printf("WSACreateEvent() failed: %d\n", WSAGetLastError());
        Finalize();
        return;
    }

    if (!IOCPNetworkAPI::Instance().InitializeIOCP(m_hIOCP))
        Finalize();

    // 리슨 소켓 및 컨텍스트 생성 + acceptEx 게시
    if(!IOCPNetworkAPI::Instance().CreateListenContext(m_pListenSocketCtxt, m_hIOCP))
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

    WSAWaitForMultipleEvents(1, m_hCleanupEvent, true, WSA_INFINITE, false);

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
    for (auto& sessionPair : m_sessionMap)
    {
        UnregisterSession(sessionPair.first);
    }
    m_sessionMap.clear();

    if(m_hIOCP)
    {
        // 스레드 종료
	    for(int i=0; i<m_nThread; ++i)
	    {
            PostQueuedCompletionStatus(m_hIOCP, 0, 0, NULL);
	    }

        // IOCP 핸들 해제
        IOCPNetworkAPI::Instance().FinalizeIOCP(m_hIOCP);
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

    // 이벤트 해제
    if(m_hCleanupEvent[0] != WSA_INVALID_EVENT)
    {
        WSACloseEvent(m_hCleanupEvent[0]);
        m_hCleanupEvent[0] = WSA_INVALID_EVENT;
    }

    // 크리티컬 섹션 해제
    DeleteCriticalSection(&m_criticalSection);

    m_receiveCallbacks.clear();

    WSACleanup();   // todo 이거 위에서 해도 멀쩡한지 테스트

    delete this;
}

void ServerCore::TriggerCleanupEvent()
{
    SetEvent(m_hCleanupEvent[0]);
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

bool ServerCore::StartSend(Session* session, const char* data, int length)
{
    if (m_bEndServer)
        return false;

    if(!IOCPNetworkAPI::Instance().StartSend(session->clientSocket, session->sendOverlapped, data, length))
    {
        UnregisterSession(session->sessionId);
        return false;
    }

    return true;
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
                IOCPNetworkAPI::Instance().StartReceive(session->clientSocket, session->recvOverlapped);
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
    sockaddr_in* remoteAddr = nullptr;

    if (!IOCPNetworkAPI::Instance().ConfigureAcceptedSocket(m_pListenSocketCtxt, remoteAddr))
        WSASetEvent(m_hCleanupEvent[0]);

    // 세션 생성, iocp 등록
    Session* session = CreateSession();
    session->clientSocket = m_pListenSocketCtxt->acceptedSocket;
    session->clientIP = *remoteAddr;

    m_hIOCP = CreateIoCompletionPort((HANDLE)session->clientSocket, m_hIOCP, (ULONG_PTR)session, 0);
    if (!m_hIOCP)
    {
        printf("CreateIoCompletionPort failed to associate socket with error: %d\n", GetLastError());
        WSASetEvent(m_hCleanupEvent[0]);
        return;
    }

    OnReceiveData(session, m_pListenSocketCtxt->acceptBuffer, nTransferredByte);

    // 다른 수락, 수신 작업 게시
    IOCPNetworkAPI::Instance().StartAccept(m_pListenSocketCtxt);
    IOCPNetworkAPI::Instance().StartReceive(session->clientSocket, session->recvOverlapped);
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
    for(const auto& session : m_sessionMap)
    {
        if (!session.second)
            continue;

        if (!StartSend(session.second, data, length))
            return false;
    }
    return true;
}

void ServerCore::OnReceiveData(Session* session, char* data, int nReceivedByte)
{
    for (const auto& callback : m_receiveCallbacks)
        callback(session, data, nReceivedByte);
}