#pragma once

enum class eIOType
{
	ACCEPT,
	READ,
	WRITE,

	NONE
};

struct IOContext
{
	WSAOVERLAPPED overlapped;
	char buffer[MAX_BUF_SIZE];
	WSABUF wsabuf;
	int nTotalBytes;
	int nSentBytes;
	eIOType IOOperation;
	SOCKET acceptedSocket;

	IOContext* pNextIOCtxt;
};

struct SocketContext
{
	SOCKET socket;

	IOContext* pIOContext;

	SocketContext* pPrevSocketCtxt;
	SocketContext* pNextSocketCtxt;
};