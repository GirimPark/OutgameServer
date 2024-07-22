#pragma once

#include "../IOCPNetworkLibrary/ListenContext.h"

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
	} state;	// Write �۾� �� ���� ó��

	unsigned int sessionId;

	SOCKET clientSocket;
	sockaddr_in clientIP;

	OVERLAPPED_STRUCT sendOverlapped;
	OVERLAPPED_STRUCT recvOverlapped;
};