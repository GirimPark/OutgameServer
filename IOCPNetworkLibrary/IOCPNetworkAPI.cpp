#include "pch.h"
#include "IOCPNetworkAPI.h"

IOCPNetworkAPI::IOCPNetworkAPI()
{
}

IOCPNetworkAPI::~IOCPNetworkAPI()
{
}

void IOCPNetworkAPI::Initialize()
{
}

void IOCPNetworkAPI::Finalize()
{
}

bool IOCPNetworkAPI::StartAccept()
{
	return true;
}

bool IOCPNetworkAPI::StartReceive()
{
	return true;
}

bool IOCPNetworkAPI::StartSend()
{
	return true;
}

SOCKET IOCPNetworkAPI::CreateSocket()
{
	return NULL;
}

SOCKET IOCPNetworkAPI::CreateListenSocket()
{
	return NULL;
}
