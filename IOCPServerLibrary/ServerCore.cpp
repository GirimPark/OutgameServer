#include "pch.h"
#include "ServerCore.h"

ServerCore::ServerCore(const char* port)
	: m_bEndServer(false)
	, m_listeningPort(port)
{
	SYSTEM_INFO systemInfo;
	GetSystemInfo(&systemInfo);
	m_nThread = systemInfo.dwNumberOfProcessors * 2;
	m_threads.resize(m_nThread);
}

ServerCore::~ServerCore()
{
	m_sessionMap.clear();
}

bool ServerCore::Run()
{
	InitializeCriticalSection(&m_criticalSection);

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

	// 리슨 소켓 생성 + acceptEx 게시
	if(!CreateListenSocket())
	{
		printf("Create Listen Socket failed\n");
		return false;
	}

	// 워커 스레드 생성/실행 + 대기
	for(int i=0; i<m_nThread; ++i)
	{
		m_threads[i] = new std::thread(&ServerCore::ProcessThread, this);
	}

	WSAWaitForMultipleEvents(1, m_hCleanupEnvent, true, WSA_INFINITE, false);

	// 해제
	Finalize();
}

bool ServerCore::Finalize()
{
	return true;
}

bool ServerCore::CreateListenSocket()
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
		return false;
	}

	if (addrlocal == NULL) {
		printf("getaddrinfo() failed to resolve/convert the interface\n");
		return false;
	}

	m_listenSocket = CreateSocket();
	if (m_listenSocket == INVALID_SOCKET) {
		freeaddrinfo(addrlocal);
		return false;
	}

	rt = bind(m_listenSocket, addrlocal->ai_addr, (int)addrlocal->ai_addrlen);
	if (rt == SOCKET_ERROR) {
		printf("bind() failed: %d\n", WSAGetLastError());
		freeaddrinfo(addrlocal);
		return false;
	}

	rt = listen(m_listenSocket, 5);
	if (rt == SOCKET_ERROR) {
		printf("listen() failed: %d\n", WSAGetLastError());
		freeaddrinfo(addrlocal);
		return false;
	}

	freeaddrinfo(addrlocal);

	// 리슨 소켓 컨텍스트 생성
	if(!CreateListenContext())
	{
		return false;
	}

	// Accept 게시
	if(!StartAccept())
	{
		return false;
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

	return newSocket;
}

bool ServerCore::CreateListenContext()
{
	m_pListenSocketCtxt = new ListenContext;

	EnterCriticalSection(&m_criticalSection);

	if(m_pListenSocketCtxt)
	{
		m_pListenSocketCtxt->listenSocket = m_listenSocket;
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

	LeaveCriticalSection(&m_criticalSection);

	return true;
}

Session* ServerCore::CreateSession(SOCKET clientSocket)
{
	static unsigned int nextSessionId = 1;
	Session* session = new Session();
	session->sessionId = nextSessionId++;
	session->clientSocket = clientSocket;
	ZeroMemory(&session->sendOverlapped, sizeof(OVERLAPPED));
	ZeroMemory(&session->recvOverlapped, sizeof(OVERLAPPED));
	CreateIoCompletionPort((HANDLE)clientSocket, m_hIOCP, (ULONG_PTR)session, 0);

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
	DWORD IOSize = 0;

	while(true)
	{
		bSuccess = GetQueuedCompletionStatus(m_hIOCP, &IOSize, &completionKey, &overlapped, INFINITE);
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

		overlappedStruct = static_cast<OVERLAPPED_STRUCT*>(overlapped);
		if(session = reinterpret_cast<Session*>(completionKey))
		{
			// READ or WRITE
			switch (overlappedStruct->IOOperation)
			{
			case OVERLAPPED_STRUCT::eIOType::READ:
				HandleReadCompletion(session);
				break;
			case OVERLAPPED_STRUCT::eIOType::WRITE:
				HandleWriteCompletion(session);
				break;
			}
		}
		else if(listenCtxt = reinterpret_cast<ListenContext*>(completionKey))
		{
			// ACCEPT
			HandleAcceptCompletion(listenCtxt);
		}
	}
}

void ServerCore::HandleReadCompletion(Session* session)
{

}

void ServerCore::HandleWriteCompletion(Session* session)
{
}

void ServerCore::HandleAcceptCompletion(ListenContext* listenCtxt)
{
	sockaddr_in* localAddr = nullptr;
	sockaddr_in* remoteAddr = nullptr;
	int localAddrLen = 0, remoteAddrLen = 0;

	GetAcceptExSockaddrs(
		listenCtxt->acceptBuffer,
		INIT_DATA_SIZE,  // 초기 데이터 길이
		sizeof(SOCKADDR_IN) + IP_SIZE,  // 로컬 주소 정보 길이
		sizeof(SOCKADDR_IN) + IP_SIZE,  // 원격 주소 정보 길이
		(sockaddr**)&localAddr,
		&localAddrLen,
		(sockaddr**)&remoteAddr,
		&remoteAddrLen
	);

	SOCKET clientSocket = CreateSocket();
	Session* session = CreateSession(clientSocket);
	session->clientIP = *remoteAddr;

	char addr[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &localAddr->sin_addr, addr, INET_ADDRSTRLEN);
	printf("Local Address: %s\n", addr);
	inet_ntop(AF_INET, &remoteAddr->sin_addr, addr, INET_ADDRSTRLEN);
	printf("Remote Address: %s\n", addr);

	// 초기 데이터 처리(인증, 세션 연결, 응답)
	ProcessInitialData(session, listenCtxt->acceptBuffer, 512);

	// 다른 수신, 수락 작업 게시
	StartReceive(session);
	StartAccept();
}

bool ServerCore::StartAccept()
{
	// 세션 생성
	Session* newSession = new Session;
	newSession->clientSocket = CreateSocket();
	if (newSession->clientSocket == INVALID_SOCKET)
	{
		printf("failed to create new accept socket\n");
		return false;
	}

	DWORD nRecvByte = 0;
	int rt = AcceptEx(
		m_listenSocket,
		newSession->clientSocket,
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
	session->recvOverlapped.hEvent = 0;

	int rt = WSARecv(session->clientSocket, &session->recvOverlapped.wsaBuffer, 1, NULL, &flags, &session->recvOverlapped, NULL);
	if(rt == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
	{
		printf("WSARecv() failed: %d\n", WSAGetLastError());
		return false;
	}
}

bool ServerCore::StartSend(Session* session, const char* data, int length)
{
	memcpy(session->sendOverlapped.buffer, data, length);
	session->sendOverlapped.wsaBuffer.len = length;
	session->sendOverlapped.nTotalByte = length;
	session->sendOverlapped.nCompletedByte = 0;
	session->sendOverlapped.IOOperation = OVERLAPPED_STRUCT::eIOType::WRITE;

	int rt = WSASend(session->clientSocket, &session->sendOverlapped.wsaBuffer, 1, &session->sendOverlapped.nCompletedByte, 0, &session->sendOverlapped, NULL);
	if (rt == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) 
	{
		printf("WSASend() failed: %d\n", WSAGetLastError());
		return false;
	}
}


void ServerCore::ProcessInitialData(Session* session, char* data, int length)
{
	const char* responseData;

	std::string username(data, data + 50);
	std::string password(data + 50, data + 100);
	if (AuthenticateUser(username, password))
	{
		// 세션 목록에 추가
		m_sessionMap.emplace(session->sessionId, session);
		responseData = "login success";
	}
	else
	{
		// Accept는 마저 처리해야 함. 연결 후 로그인 실패 응답 -> 연결 해제
		responseData = "login failed";
	}

	// 송신 작업 게시
	StartSend(session, responseData, sizeof(responseData));
}

bool ServerCore::AuthenticateUser(const std::string_view& username, const std::string_view& password)
{
	return true;
}