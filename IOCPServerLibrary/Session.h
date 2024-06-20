#pragma once

struct OVERLAPPED_STRUCT : OVERLAPPED
{
	OVERLAPPED_STRUCT()
	{
		ZeroMemory(this, sizeof(OVERLAPPED));
		IOOperation = eIOType::NONE;
		wsaBuffer.buf = buffer;
		wsaBuffer.len = sizeof(buffer);
		nTotalByte = 0;
		nCompletedByte = 0;
	}

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
};

struct Session
{
	unsigned int sessionId;

	SOCKET clientSocket;
	sockaddr_in clientIP;

	OVERLAPPED_STRUCT sendOverlapped;
	OVERLAPPED_STRUCT recvOverlapped;
};