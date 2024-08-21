#pragma once

enum class eCompletionKeyType
{
	LISTEN_CONTEXT,
	SESSION
};

struct OVERLAPPED_STRUCT : OVERLAPPED
{
	OVERLAPPED_STRUCT()
	{
		ZeroMemory(static_cast<OVERLAPPED*>(this), sizeof(OVERLAPPED));
		wsaBuffer.buf = buffer;
		wsaBuffer.len = sizeof(buffer);
	}

	enum class eIOType
	{
		READ,
		WRITE,

		NONE
	} IOOperation;
	WSABUF wsaBuffer;
	char buffer[MAX_BUF_SIZE];
};

enum class eSessionStateType
{
	MAINTAIN,
	REGISTER,
	CLOSE,
	UNREGISTER
};

static unsigned int staticSessionId = 0;

// READ, WRITE
class Session
{
public:
	Session()
	{
		sessionId = staticSessionId++;

		type = eCompletionKeyType::SESSION;
		state = eSessionStateType::MAINTAIN;

		sendOverlapped.IOOperation = OVERLAPPED_STRUCT::eIOType::WRITE;
		recvOverlapped.IOOperation = OVERLAPPED_STRUCT::eIOType::READ;

		InitializeCriticalSection(&writeLock);
		InitializeCriticalSection(&sendLock);
		InitializeCriticalSection(&recvLock);
	}

	~Session()
	{
		DeleteCriticalSection(&writeLock);
		DeleteCriticalSection(&sendLock);
		DeleteCriticalSection(&recvLock);
	}

	void Close()
	{
		linger lingerStruct;
		lingerStruct.l_onoff = 1;
		lingerStruct.l_linger = 0;
		setsockopt(clientSocket, SOL_SOCKET, SO_LINGER, (char*)&lingerStruct, sizeof(lingerStruct));

		closesocket(clientSocket);
		clientSocket = INVALID_SOCKET;

		//EnterCriticalSection(&writeLock);
		//EnterCriticalSection(&sendLock);
		//EnterCriticalSection(&recvLock);

		//linger lingerStruct;
		//lingerStruct.l_onoff = 1;
		//lingerStruct.l_linger = 0;
		//setsockopt(clientSocket, SOL_SOCKET, SO_LINGER, (char*)&lingerStruct, sizeof(lingerStruct));

		//while (!(HasOverlappedIoCompleted(&recvOverlapped)))
		//	Sleep(0);

		//closesocket(clientSocket);
		//clientSocket = INVALID_SOCKET;

		//LeaveCriticalSection(&writeLock);
		//LeaveCriticalSection(&sendLock);
		//LeaveCriticalSection(&recvLock);
	} 

	bool PostSend(const char* data, int length)
	{
		EnterCriticalSection(&sendLock);

		sendOverlapped.IOOperation = OVERLAPPED_STRUCT::eIOType::WRITE;
		ZeroMemory(sendOverlapped.buffer, MAX_BUF_SIZE);
		memcpy(sendOverlapped.buffer, data, length);
		sendOverlapped.wsaBuffer.len = length;
		
		int rt = WSASend(clientSocket, &sendOverlapped.wsaBuffer, 1, NULL, 0, &sendOverlapped, NULL);
		if (rt == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
		{
			printf("WSASend() failed: %d\n", WSAGetLastError());
			LeaveCriticalSection(&sendLock);
			return false;
		}

		LeaveCriticalSection(&sendLock);

		AddReferenceCount();
		return true;
	}

	bool PostReceive()
	{
		EnterCriticalSection(&recvLock);

		DWORD flags = 0;
		DWORD bytesReceived = 0;
		recvOverlapped.IOOperation = OVERLAPPED_STRUCT::eIOType::READ;
		recvOverlapped.wsaBuffer.len = sizeof(recvOverlapped.buffer);

		int rt = WSARecv(clientSocket, &recvOverlapped.wsaBuffer, 1, &bytesReceived, &flags, &recvOverlapped, NULL);
		if (rt == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
		{
			printf("WSARecv() failed: %d\n", WSAGetLastError());
			LeaveCriticalSection(&recvLock);
			return false;
		}

		LeaveCriticalSection(&recvLock);

		AddReferenceCount();
		return true;
	}

public:
	void SetState(eSessionStateType _state, bool needLock = true)
	{
		if(needLock)	EnterCriticalSection(&writeLock);
		state = _state;
		if(needLock)	LeaveCriticalSection(&writeLock);
	}
	void SetClientSocket(SOCKET _clientSocket, bool needLock = true)
	{
		if (needLock)	EnterCriticalSection(&writeLock);
		clientSocket = _clientSocket;
		if (needLock)	LeaveCriticalSection(&writeLock);
	}
	void SetClientIP(sockaddr_in _clientIP, bool needLock = true)
	{
		if (needLock)	EnterCriticalSection(&writeLock);
		clientIP = _clientIP;
		if (needLock)	LeaveCriticalSection(&writeLock);
	}

	void AddReferenceCount()
	{
		refCount.store(refCount.load() + 1);
	}

	void SubReferenceCount()
	{
		refCount.store(refCount.load() - 1);
	}

	const eCompletionKeyType& GetType() const { return type; }
	const eSessionStateType& GetState() const { return state; }
	const SessionId& GetSessionId() const { return sessionId; }
	const SOCKET& GetClientSocket() const { return clientSocket; }
	const sockaddr_in& GetClientIP() const { return clientIP; }
	const char* GetReceivedData() const { return recvOverlapped.buffer; }

private:
	eCompletionKeyType type;
	eSessionStateType state;
	SessionId sessionId;

	std::atomic<int> refCount = 0;

	SOCKET clientSocket;
	sockaddr_in clientIP;

	OVERLAPPED_STRUCT sendOverlapped;
	OVERLAPPED_STRUCT recvOverlapped;

	CRITICAL_SECTION writeLock;
	CRITICAL_SECTION sendLock;
	CRITICAL_SECTION recvLock;
};

// Accept
struct ListenContext
{
	ListenContext()
	{
		type = eCompletionKeyType::LISTEN_CONTEXT;
	}

	~ListenContext()
	{
		closesocket(listenSocket);
		listenSocket = INVALID_SOCKET;
		closesocket(acceptedSocket);
		acceptedSocket = INVALID_SOCKET;
	}

	eCompletionKeyType type;

	OVERLAPPED acceptOverlapped;
	SOCKET listenSocket;
	SOCKET acceptedSocket;	// 다중 Accept로 구조 바꾸면 변경 예정
	char acceptBuffer[INIT_DATA_SIZE + 2 * (sizeof(SOCKADDR_IN) + IP_SIZE)];
};