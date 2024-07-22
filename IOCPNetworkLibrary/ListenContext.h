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
	SOCKET acceptedSocket;	// ���� Accept�� ���� �ٲٸ� ���� ����
	char acceptBuffer[INIT_DATA_SIZE + 2 * (sizeof(SOCKADDR_IN) + IP_SIZE)];
};