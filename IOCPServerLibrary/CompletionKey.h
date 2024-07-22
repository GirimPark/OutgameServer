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

// READ, WRITE
struct Session
{
	Session()
	{
		type = eCompletionKeyType::SESSION;
		state = eStateType::MAINTAIN;

		sendOverlapped.IOOperation = OVERLAPPED_STRUCT::eIOType::WRITE;
		recvOverlapped.IOOperation = OVERLAPPED_STRUCT::eIOType::READ;
	}

	~Session()
	{
		closesocket(clientSocket);
		clientSocket = INVALID_SOCKET;
	}

	eCompletionKeyType type;

	enum class eStateType
	{
		MAINTAIN,
		REGISTER,
		CLOSE,
		UNREGISTER
	} state;	// Write 작업 후 세션 처리

	unsigned int sessionId;

	SOCKET clientSocket;
	sockaddr_in clientIP;

	OVERLAPPED_STRUCT sendOverlapped;
	OVERLAPPED_STRUCT recvOverlapped;
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