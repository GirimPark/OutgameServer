#include "pch.h"
#include "IOCPNetwork.h"

#include "ContextStructure.h"

IOCPNetwork::IOCPNetwork()
{
	//for(int i=0; i<MAX_WORKER_THREAD; ++i)
	//{
	//	m_threadHandles[i] = INVALID_HANDLE_VALUE;
	//}

	SYSTEM_INFO systemInfo;
	GetSystemInfo(&systemInfo);
	m_nThread = systemInfo.dwNumberOfProcessors * 2;
	m_threads.resize(m_nThread);
}

IOCPNetwork::~IOCPNetwork()
{
	
}

// 서버 솔루션으로 옮겨질 내용들
void IOCPNetwork::Run()
{
	if(WSA_INVALID_EVENT == (m_hCleanupEvent[0] = WSACreateEvent()))
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

	if(!CreateListenSocket())
	{
		Finalize();
		return;
	}

	WSAWaitForMultipleEvents(1, m_hCleanupEvent, true, WSA_INFINITE, false);

	Finalize();
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
	for (int i = 0; i < m_nThread; ++i)
	{
		m_threads[i] = new std::thread(&IOCPNetwork::WorkerThread, this);
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
	m_pListenSocketCtxt = RegisterSocketCtxt(m_listenSocket, eIOType::ACCEPT, false);
	if(!m_pListenSocketCtxt)
	{
		printf("failed to register listen socket to IOCP\n");
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
	int rt = AcceptEx(
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
	m_bEndServer = true;

	if(m_hIOCP)
	{
		for(int i=0; i<m_nThread; ++i)
		{
			PostQueuedCompletionStatus(m_hIOCP, 0, 0, NULL);
			//여기서 바로 join?
			//if(m_threads[i]->joinable())
			//	m_threads[i]->join();
		}

		for(int i=0; i<m_nThread;++i)
		{
			if (m_threads[i] && m_threads[i]->joinable())
				m_threads[i]->join();
		}

		if(m_listenSocket!=INVALID_SOCKET)
		{
			closesocket(m_listenSocket);
			m_listenSocket = INVALID_SOCKET;
		}

		if(m_pListenSocketCtxt)
		{
			while (!HasOverlappedIoCompleted(&m_pListenSocketCtxt->pIOContext->overlapped))
				Sleep(0);

			if (m_pListenSocketCtxt->pIOContext->acceptedSocket != INVALID_SOCKET)
			{
				closesocket(m_pListenSocketCtxt->pIOContext->acceptedSocket);
				m_pListenSocketCtxt->pIOContext->acceptedSocket = INVALID_SOCKET;
			}

			if (m_pListenSocketCtxt->pIOContext)
				delete m_pListenSocketCtxt->pIOContext;

			if (m_pListenSocketCtxt)
				delete m_pListenSocketCtxt;
			m_pListenSocketCtxt = NULL;
		}

		FreeCtxtList();

		if(m_hIOCP)
		{
			CloseHandle(m_hIOCP);
			m_hIOCP = NULL;
		}
	}

	DeleteCriticalSection(&m_criticalSection);
	if(m_hCleanupEvent[0] != WSA_INVALID_EVENT)
	{
		WSACloseEvent(m_hCleanupEvent[0]);
		m_hCleanupEvent[0] = WSA_INVALID_EVENT;
	}
	WSACleanup();
}

SOCKET IOCPNetwork::CreateSocket()
{
	SOCKET newSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED);
	if(newSocket == INVALID_SOCKET)
	{
		printf("WSASocket(sdSocket) failed: %d\n", WSAGetLastError());
		return newSocket;
	}

	int zero = 0;
	int rt = setsockopt(newSocket, SOL_SOCKET, SO_SNDBUF, (char*)&zero, sizeof(zero));
	if(rt==SOCKET_ERROR)
	{
		printf("setsockopt(SNDBUF) failed: %d\n", WSAGetLastError());
		return(newSocket);
	}

	return newSocket;
}

SocketContext* IOCPNetwork::RegisterSocketCtxt(SOCKET socket, eIOType IOType, bool bAddToList)
{
	SocketContext* newSocketCtxt = CreateSocketCtxt(socket, IOType);
	if(!newSocketCtxt)
	{
		return nullptr;
	}

	m_hIOCP = CreateIoCompletionPort((HANDLE)socket, m_hIOCP, (DWORD_PTR)newSocketCtxt, 0);
	if(!m_hIOCP)
	{
		printf("CreateIoCompletionPort() failed: %d\n", GetLastError());
		if (newSocketCtxt->pIOContext)
			delete newSocketCtxt->pIOContext;
		delete newSocketCtxt;
		return nullptr;
	}

	if(bAddToList)
	{
		InsertCtxtList(newSocketCtxt);
	}

	return newSocketCtxt;
}

SocketContext* IOCPNetwork::CreateSocketCtxt(SOCKET socket, eIOType IOType)
{
	SocketContext* newSocketCtxt = new SocketContext;

	EnterCriticalSection(&m_criticalSection);

	if(newSocketCtxt)
	{
		newSocketCtxt->pIOContext = new IOContext;
		if(newSocketCtxt->pIOContext)
		{
			newSocketCtxt->socket = socket;
			newSocketCtxt->pPrevSocketCtxt = nullptr;
			newSocketCtxt->pNextSocketCtxt = nullptr;

			newSocketCtxt->pIOContext->overlapped.Internal = 0;
			newSocketCtxt->pIOContext->overlapped.InternalHigh = 0;
			newSocketCtxt->pIOContext->overlapped.Offset = 0;
			newSocketCtxt->pIOContext->overlapped.OffsetHigh = 0;
			newSocketCtxt->pIOContext->overlapped.hEvent = NULL;
			newSocketCtxt->pIOContext->IOOperation = IOType;
			newSocketCtxt->pIOContext->pNextIOCtxt = NULL;
			newSocketCtxt->pIOContext->nTotalBytes = 0;
			newSocketCtxt->pIOContext->nSentBytes = 0;
			newSocketCtxt->pIOContext->wsabuf.buf = newSocketCtxt->pIOContext->buffer;
			newSocketCtxt->pIOContext->wsabuf.len = sizeof(newSocketCtxt->pIOContext->buffer);
			newSocketCtxt->pIOContext->acceptedSocket = INVALID_SOCKET;

			ZeroMemory(newSocketCtxt->pIOContext->wsabuf.buf, newSocketCtxt->pIOContext->wsabuf.len);
		}
		else
		{
			delete newSocketCtxt;
			printf("new IOContext failed: %d\n", GetLastError());
		}
	}
	else
	{
		printf("new SocketContext failed: %d\n", GetLastError());
		return nullptr;
	}

	LeaveCriticalSection(&m_criticalSection);

	return newSocketCtxt;
}

void IOCPNetwork::WorkerThread()
{
	bool bSuccess = false;
	int rt = 0;

	OVERLAPPED* overlapped = nullptr;
	SocketContext* socketCtxt = nullptr;
	SocketContext* acceptedSocketCtxt = nullptr;
	IOContext* IOCtxt = nullptr;
	WSABUF rcvBuf;
	WSABUF sndBuf;
	DWORD nRcvByte = 0;
	DWORD nSndByte = 0;
	DWORD flags = 0;
	DWORD IOSize = 0;
	

	while (true)
	{
		bSuccess = GetQueuedCompletionStatus(m_hIOCP, &IOSize, (PULONG_PTR)&socketCtxt, &overlapped, INFINITE);
		if (!bSuccess)
		{
			printf("GetQueuedCompletionStatus() failed: %d\n", GetLastError());
		}

		if (!socketCtxt)
		{
			return;
		}

		if (m_bEndServer)
		{
			return;
		}

		IOCtxt = reinterpret_cast<IOContext*>(overlapped);
		if (IOCtxt->IOOperation != eIOType::ACCEPT
			&& (!bSuccess || (bSuccess && (IOSize == 0))))
		{
			CloseSocketCtxt(socketCtxt);
			continue;
		}

		switch (IOCtxt->IOOperation)
		{
		case eIOType::ACCEPT:
		{
			rt = setsockopt(socketCtxt->pIOContext->acceptedSocket,
				SOL_SOCKET,
				SO_UPDATE_ACCEPT_CONTEXT,
				(char*)&m_listenSocket,
				sizeof(m_listenSocket));
			if (rt == SOCKET_ERROR)
			{
				printf("setsockopt(SO_UPDATE_ACCEPT_CONTEXT) failed to update accept socket\n");
				WSASetEvent(m_hCleanupEvent[0]);
				return;
			}

			acceptedSocketCtxt = RegisterSocketCtxt(socketCtxt->pIOContext->acceptedSocket, eIOType::ACCEPT, true);

			if (!acceptedSocketCtxt)
			{
				printf("failed to update accept socket to IOCP\n");
				WSASetEvent(m_hCleanupEvent[0]);
				return;
			}

			if (IOSize)
			{
				// Accept와 함께 Recv가 이루어졌다면
				acceptedSocketCtxt->pIOContext->IOOperation = eIOType::WRITE;
				acceptedSocketCtxt->pIOContext->nTotalBytes = IOSize;
				acceptedSocketCtxt->pIOContext->nSentBytes = 0;
				acceptedSocketCtxt->pIOContext->wsabuf.len = IOSize;
				memcpy(acceptedSocketCtxt->pIOContext->buffer, socketCtxt->pIOContext->buffer, sizeof(socketCtxt->pIOContext->buffer));
				acceptedSocketCtxt->pIOContext->wsabuf.buf = acceptedSocketCtxt->pIOContext->buffer;

				// 에코 로직
				rt = WSASend(
					socketCtxt->pIOContext->acceptedSocket,
					&acceptedSocketCtxt->pIOContext->wsabuf, 1,
					&nSndByte,
					0,
					&(acceptedSocketCtxt->pIOContext->overlapped), NULL);

				if (rt == SOCKET_ERROR && (ERROR_IO_PENDING != WSAGetLastError()))
				{
					printf("WSASend() failed: %d\n", WSAGetLastError());
					CloseSocketCtxt(acceptedSocketCtxt);
				}
			}
			else
			{
				acceptedSocketCtxt->pIOContext->IOOperation = eIOType::READ;
				nRcvByte = 0;
				flags = 0;
				rcvBuf.buf = acceptedSocketCtxt->pIOContext->buffer;
				rcvBuf.len = MAX_BUF_SIZE;

				rt = WSARecv(
					acceptedSocketCtxt->socket,
					&rcvBuf, 1,
					&nRcvByte,
					&flags,
					&acceptedSocketCtxt->pIOContext->overlapped, NULL);
				if (rt == SOCKET_ERROR && (ERROR_IO_PENDING != WSAGetLastError()))
				{
					printf("WSARecv() failed: %d\n", WSAGetLastError());
					CloseSocketCtxt(acceptedSocketCtxt);
				}
			}

			if (!CreateAcceptSocket())
			{
				printf("Please shut down and reboot the server.\n");
				WSASetEvent(m_hCleanupEvent[0]);
				return;
			}
			break;
		}


		case eIOType::READ:
		{
			IOCtxt->IOOperation = eIOType::WRITE;
			IOCtxt->nTotalBytes = IOSize;
			IOCtxt->nSentBytes = 0;
			IOCtxt->wsabuf.len = IOSize;
			flags = 0;

			rt = WSASend(
				socketCtxt->socket,
				&IOCtxt->wsabuf, 1, &nSndByte,
				flags,
				&(IOCtxt->overlapped), NULL);
			if (rt = SOCKET_ERROR && (ERROR_IO_PENDING != WSAGetLastError()))
			{
				printf("WSASend() failed: %d\n", WSAGetLastError());
				CloseSocketCtxt(socketCtxt);
			}

			break;
		}


		case eIOType::WRITE:
		{
			IOCtxt->IOOperation = eIOType::WRITE;
			IOCtxt->nSentBytes += IOSize;
			flags = 0;
			if (IOCtxt->nSentBytes < IOCtxt->nTotalBytes) {
				// 이전 송신을 완료되지 않았다면
				sndBuf.buf = IOCtxt->buffer + IOCtxt->nSentBytes;
				sndBuf.len = IOCtxt->nTotalBytes - IOCtxt->nSentBytes;

				rt = WSASend(
					socketCtxt->socket,
					&sndBuf, 1, &nSndByte,
					flags,
					&(IOCtxt->overlapped), NULL);
				if (rt == SOCKET_ERROR && (ERROR_IO_PENDING != WSAGetLastError())) {
					printf("WSASend() failed: %d\n", WSAGetLastError());
					CloseSocketCtxt(socketCtxt);
				}
			}
			else
			{
				// 이전 송신이 완료됐다면
				IOCtxt->IOOperation = eIOType::READ;
				nRcvByte = 0;
				flags = 0;
				rcvBuf.buf = IOCtxt->buffer,
					rcvBuf.len = MAX_BUF_SIZE;
				rt = WSARecv(
					socketCtxt->socket,
					&rcvBuf, 1, &nRcvByte,
					&flags,
					&IOCtxt->overlapped, NULL);
				if (rt == SOCKET_ERROR && (ERROR_IO_PENDING != WSAGetLastError())) {
					printf("WSARecv() failed: %d\n", WSAGetLastError());
					CloseSocketCtxt(socketCtxt);
				}
			}

			break;
		}
		}
	}
}

void IOCPNetwork::CloseSocketCtxt(SocketContext* socketCtxt)
{
	EnterCriticalSection(&m_criticalSection);

	if (socketCtxt) {

		//
		// force the subsequent closesocket to be abortative.
		//
		LINGER  lingerStruct;

		lingerStruct.l_onoff = 1;
		lingerStruct.l_linger = 0;
		setsockopt(socketCtxt->socket, SOL_SOCKET, SO_LINGER,
			(char*)&lingerStruct, sizeof(lingerStruct));
		
		if (socketCtxt->pIOContext->acceptedSocket != INVALID_SOCKET) {
			closesocket(socketCtxt->pIOContext->acceptedSocket);
			socketCtxt->pIOContext->acceptedSocket = INVALID_SOCKET;
		};

		closesocket(socketCtxt->socket);
		socketCtxt->socket = INVALID_SOCKET;
		RemoveCtxtList(socketCtxt);
		socketCtxt = nullptr;
	}
	else 
	{
		printf("CloseSocketCtxt: socketCtxt is NULL\n");
	}

	LeaveCriticalSection(&m_criticalSection);
}

void IOCPNetwork::InsertCtxtList(SocketContext* socketCtxt)
{
	SocketContext* pTemp;

	EnterCriticalSection(&m_criticalSection);

	if (m_pClientSocketCtxtList == NULL) {

		//
		// add the first node to the linked list
		//
		socketCtxt->pPrevSocketCtxt = nullptr;
		socketCtxt->pNextSocketCtxt = nullptr;
		m_pClientSocketCtxtList = socketCtxt;
	}
	else {

		//
		// add node to head of list
		//
		pTemp = m_pClientSocketCtxtList;

		m_pClientSocketCtxtList = socketCtxt;
		socketCtxt->pPrevSocketCtxt = nullptr;
		socketCtxt->pNextSocketCtxt = pTemp;

		pTemp->pPrevSocketCtxt = socketCtxt;
	}

	LeaveCriticalSection(&m_criticalSection);
}

void IOCPNetwork::RemoveCtxtList(SocketContext* socketCtxt)
{
	SocketContext* pPrev;
	SocketContext* pNext;
	IOContext*     pNextIO = nullptr;
	IOContext*     pTempIO = nullptr;

	EnterCriticalSection(&m_criticalSection);

	if (socketCtxt) {
		pPrev = socketCtxt->pPrevSocketCtxt;
		pNext = socketCtxt->pNextSocketCtxt;

		if (!pPrev && !pNext) {

			//
			// This is the only node in the list to delete
			//
			m_pClientSocketCtxtList = nullptr;
		}
		else if (!pPrev && pNext) {

			//
			// This is the start node in the list to delete
			//
			pNext->pPrevSocketCtxt = nullptr;
			m_pClientSocketCtxtList = pNext;
		}
		else if (pPrev && !pNext) {

			//
			// This is the end node in the list to delete
			//
			pPrev->pNextSocketCtxt = nullptr;
		}
		else if (pPrev && pNext) {

			//
			// Neither start node nor end node in the list
			//
			pPrev->pNextSocketCtxt = pNext;
			pNext->pPrevSocketCtxt = pPrev;
		}

		//
		// Free all i/o context structures per socket
		//
		pTempIO = socketCtxt->pIOContext;
		while(pTempIO)
		{
			pNextIO = (IOContext*)(pTempIO->pNextIOCtxt);

			if (m_bEndServer)
			{
				// 종료 상황에서의 컨텍스트 해제
				// GetQueuedCompletionStatus에 의해 해제되지 않은 컨텍스트에 대해서만 확인한다.
				while (!HasOverlappedIoCompleted((OVERLAPPED*)pTempIO))
					Sleep(0);
			}
			delete pTempIO;

			pTempIO = pNextIO;
		}

		delete socketCtxt;
		socketCtxt = nullptr;
	}
	else 
	{
		printf("RemoveCtxtList: SocketContext is NULL\n");
	}

	LeaveCriticalSection(&m_criticalSection);
}

void IOCPNetwork::FreeCtxtList()
{
	SocketContext* pTemp1;
	SocketContext* pTemp2;

	EnterCriticalSection(&m_criticalSection);

	pTemp1 = m_pClientSocketCtxtList;
	while (pTemp1) {
		pTemp2 = pTemp1->pNextSocketCtxt;
		CloseSocketCtxt(pTemp1);
		pTemp1 = pTemp2;
	}

	LeaveCriticalSection(&m_criticalSection);
}