#include "pch.h"
#include "OutgameServer.h"

#include "../IOCPServerLibrary/ServerCore.h"

#ifdef _DEBUG
#include <vld/vld.h>

#pragma comment(lib, "vld.lib")
#pragma comment(lib, "libprotobufd.lib")
#else
#pragma comment(lib, "libprotobuf.lib")
#endif

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Mswsock.lib")

int main()
{
	OutgameServer* server = new OutgameServer;
	server->Start();
}

OutgameServer::OutgameServer()
{
}

OutgameServer::~OutgameServer()
{
}

void OutgameServer::Start()
{
	m_serverCore = new ServerCore("5001", 5);

	m_serverCore->RegisterCallback([this](Session* session, char* data, int nReceivedByte)
		{
			DispatchReceivedData(session, data, nReceivedByte);
		});

	std::thread coreThread = std::thread(&ServerCore::Run, m_serverCore);
	std::thread processThread = std::thread(&OutgameServer::ProcessEchoPacket, this);

	if (coreThread.joinable())
		coreThread.join();
	if (processThread.joinable())
		processThread.join();
}

void OutgameServer::DispatchReceivedData(Session* session, char* data, int nReceivedByte)
{
	// todo: 알맞은 처리 큐에 넣기
	Packet* packet = new Packet;
	packet->session = session;
	packet->data = new char[nReceivedByte];
	memcpy(packet->data, data, nReceivedByte);
	packet->nRecvByte = nReceivedByte;

	m_recvQueue.push(packet);
}

void OutgameServer::ProcessEchoPacket()
{
	Packet* packet;
	while(true)
	{
		if(m_recvQueue.try_pop(packet))
			m_serverCore->StartSend(packet->session, packet->data, packet->nRecvByte);
	}
}
