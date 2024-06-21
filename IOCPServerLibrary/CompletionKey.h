#pragma once

enum class eCompletionKeyType
{
	LISTEN_CONTEXT,
	SESSION
};

struct OVERLAPPED_STRUCT : OVERLAPPED
{
	enum class eIOType
	{
		ACCEPT,
		READ,
		WRITE,

		NONE
	} IOOperation;
	WSABUF wsaBuffer;
	char buffer[MAX_BUF_SIZE];
	DWORD nTotalByte;
	DWORD nCompletedByte;

	OVERLAPPED_STRUCT()
	{
		ZeroMemory(static_cast<OVERLAPPED*>(this), sizeof(OVERLAPPED));
		wsaBuffer.buf = buffer;
		wsaBuffer.len = sizeof(buffer);
		nTotalByte = 0;
		nCompletedByte = 0;
	}
};

// READ, WRITE
struct Session
{
	eCompletionKeyType type = eCompletionKeyType::SESSION;

	unsigned int sessionId;

	SOCKET clientSocket;
	sockaddr_in clientIP;

	OVERLAPPED_STRUCT sendOverlapped;
	OVERLAPPED_STRUCT recvOverlapped;

	Session()
	{
		sendOverlapped.IOOperation = OVERLAPPED_STRUCT::eIOType::WRITE;
		recvOverlapped.IOOperation = OVERLAPPED_STRUCT::eIOType::READ;
	}
};


struct ListenContext
{
	eCompletionKeyType type = eCompletionKeyType::LISTEN_CONTEXT;

	OVERLAPPED acceptOverlapped;
	SOCKET listenSocket;
	char acceptBuffer[INIT_DATA_SIZE + 2 * (sizeof(SOCKADDR_IN) + IP_SIZE)];
};