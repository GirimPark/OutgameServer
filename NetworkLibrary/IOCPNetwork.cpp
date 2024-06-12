#include "pch.h"
#include "IOCPNetwork.h"

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Mswsock.lib")

IOCPNetwork::IOCPNetwork()
{
	m_threadHandles[0] = (HANDLE)WSA_INVALID_EVENT;
}

IOCPNetwork::~IOCPNetwork()
{
}

void IOCPNetwork::Initialize()
{
}

void IOCPNetwork::Run()
{
}

void IOCPNetwork::Finalize()
{
}

