#include "pch.h"
#include "IOCPNetwork.h"

#include "ContextStructure.h"

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Mswsock.lib")

IOCPNetwork::IOCPNetwork()
{
	//for(int i=0; i<MAX_WORKER_THREAD; ++i)
	//{
	//	m_threadHandles[i] = INVALID_HANDLE_VALUE;
	//}

	SYSTEM_INFO systemInfo;
	GetSystemInfo(&systemInfo);
	m_nThread = systemInfo.dwNumberOfProcessors * 2;
}

IOCPNetwork::~IOCPNetwork()
{
	
}

// 서버 솔루션으로 옮겨질 내용들
void IOCPNetwork::Run()
{
	// 종료 이벤트 생성
	HANDLE hCleanupEvent[1];
	if(WSA_INVALID_EVENT == (hCleanupEvent[0] = WSACreateEvent()))
	{
		printf("WSACreateEvent() failed: %d\n", WSAGetLastError());
		return;
	}

	if(!Initialize())
	{
		Finalize();
		return;
	}

	RunWorkerThread();

	CreateListenSocket();

}

bool IOCPNetwork::Initialize()
{
	WSADATA wsaData;

	int rt = 0;
	if ((rt = WSAStartup(MAKEWORD(2, 2), &wsaData)) != 0)
	{
		printf("WSAStartup() failed: %d\n", rt);
		return false;
	}

	InitializeCriticalSection(&m_criticalSection);

	m_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if(m_hIOCP == NULL)
	{
		printf("CreateIoCompletionPort() failed to create I/O completion port: %d\n",
			GetLastError());
		return false;
	}
}

void IOCPNetwork::RunWorkerThread()
{
	for(int i=0; i<m_nThread; ++i)
	{
		std::thread thread(&IOCPNetwork::WorkerThread, this);
		m_threads.emplace_back(&thread);
	}
}

bool IOCPNetwork::CreateListenSocket()
{
	int rt = 0;
	LINGER lingerStruct;
	addrinfo hints = { 0 };
	addrinfo* addrlocal = NULL;

	lingerStruct.l_onoff = 1;
	lingerStruct.l_linger = 0;

	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_IP;

	if (getaddrinfo(NULL, m_port, &hints, &addrlocal) != 0) {
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

	// 소켓 컨텍스트 생성, 등록
	m_pListenSocketCtxt = RegisterSocketCtxt(m_listenSocket, eIOType::ACCEPT);
	if(!m_pListenSocketCtxt)
	{
		printf("failed to register listen socket to IOCP\n");
		return false;
	}

	// Load AcceptEx
	GUID acceptex_guid = WSAID_ACCEPTEX;
	DWORD bytes = 0;
	rt = WSAIoctl(
		m_listenSocket,
		SIO_GET_EXTENSION_FUNCTION_POINTER,
		&acceptex_guid,
		sizeof(acceptex_guid),
		&m_fnAcceptEx,
		sizeof(m_fnAcceptEx),
		&bytes,
		NULL,
		NULL
		);
	if(rt == SOCKET_ERROR)
	{
		printf("failed to load AcceptEx: %d\n", WSAGetLastError());
		return false;
	}

	if (!CreateAcceptSocket())
	{
		return false;
	}

	return true;
}

bool IOCPNetwork::CreateAcceptSocket()
{
	m_pListenSocketCtxt->pIOContext->acceptedSocket = CreateSocket();
	if(m_pListenSocketCtxt->pIOContext->acceptedSocket == INVALID_SOCKET)
	{
		printf("failed to create new accept socket\n");
		return false;
	}

	DWORD recvNumBytes = 0;
	int rt = m_fnAcceptEx(
		m_listenSocket,
		m_pListenSocketCtxt->pIOContext->acceptedSocket,
		m_pListenSocketCtxt->pIOContext->buffer,
		MAX_BUF_SIZE - (2 * (sizeof(SOCKADDR_STORAGE) + 16)),
		sizeof(SOCKADDR_STORAGE) + 16,
		sizeof(SOCKADDR_STORAGE) + 16,
		&recvNumBytes,
		&(m_pListenSocketCtxt->pIOContext->overlapped)
	);
	if(rt==SOCKET_ERROR&&(ERROR_IO_PENDING!=WSAGetLastError()))
	{
		printf("AcceptEx() failed: %d\n", WSAGetLastError());
		return false;
	}

	return true;
}

void IOCPNetwork::Finalize()
{
}

SOCKET IOCPNetwork::CreateSocket()
{


	return NULL;
}

SocketContext* IOCPNetwork::RegisterSocketCtxt(SOCKET socket, eIOType IOType)
{
	return nullptr;
}

void IOCPNetwork::WorkerThread()
{
}
