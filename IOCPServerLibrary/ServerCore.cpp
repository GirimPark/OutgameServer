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

	// ���� ���� ���� + acceptEx �Խ�
	if(!CreateListenSocket())
	{
		printf("Create Listen Socket failed\n");
		return false;
	}

	// ��Ŀ ������ ����/���� + ���
	for(int i=0; i<m_nThread; ++i)
	{
		m_threads[i] = new std::thread(&ServerCore::ProcessThread, this);
	}

	WSAWaitForMultipleEvents(1, m_hCleanupEnvent, true, WSA_INFINITE, false);

	// ����
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

	// ���� ���� ���ؽ�Ʈ ����
	if(!CreateListenContext())
	{
		return false;
	}

	// Accept �Խ�
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
		INIT_DATA_SIZE,  // �ʱ� ������ ����
		sizeof(SOCKADDR_IN) + IP_SIZE,  // ���� �ּ� ���� ����
		sizeof(SOCKADDR_IN) + IP_SIZE,  // ���� �ּ� ���� ����
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

	// �ʱ� ������ ó��(����, ���� ����, ����)
	ProcessInitialData(session, listenCtxt->acceptBuffer, 512);

	// �ٸ� ����, ���� �۾� �Խ�
	StartReceive(session);
	StartAccept();
}

bool ServerCore::StartAccept()
{
	// ���� ����
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
	StartSend(session, responseData, sizeof(responseData));
}

bool ServerCore::AuthenticateUser(const std::string_view& username, const std::string_view& password)
{
	return true;
}