#ifdef _DEBUG
#pragma comment(lib, "vld.lib")

#pragma comment(lib, "libprotobufd.lib")
#else
#pragma comment(lib, "libprotobuf.lib")
#endif

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Mswsock.lib")


#include "pch.h"
#include "OutgameServer.h"

#include "../IOCPServerLibrary/ServerCore.h"
#include "../PacketLibrary/Echo.pb.h"
#ifdef _DEBUG
#include <vld/vld.h>
#endif



int main()
{
	OutgameServer server;
	server.Start();
}

OutgameServer::OutgameServer()
{
}

OutgameServer::~OutgameServer()
{
	if(m_bRun)
		Stop();

	if (m_serverCore)
	{
		delete m_serverCore;
		m_serverCore = nullptr;
	}
}

void OutgameServer::Start()
{
	m_bRun = true;

	m_serverCore = new ServerCore("5001", 5);

	m_serverCore->RegisterCallback([this](Session* session, char* data, int nReceivedByte)
		{
			DispatchReceivedData(session, data, nReceivedByte);
		});

	m_coreThread = new std::thread(&ServerCore::Run, m_serverCore);
	m_processThread = new std::thread(&OutgameServer::ProcessEchoQueue, this);
	m_sendThread = new std::thread(&OutgameServer::SendThread, this);
	m_quitThread = new std::thread(&OutgameServer::QuitThread, this);

	if (m_coreThread->joinable())
	{
		m_coreThread->join();
		delete m_coreThread;
	}
	if (m_processThread->joinable())
	{
		m_processThread->join();
		delete m_processThread;
	}
	if (m_sendThread->joinable())
	{
		m_sendThread->join();
		delete m_sendThread;
	}
	if (m_quitThread->joinable())
	{
		m_quitThread->join();
		delete m_quitThread;
	}
}

void OutgameServer::Stop()
{
	m_bRun = false;

	// core 자원 해제
	m_serverCore->TriggerCleanupEvent();
	m_serverCore = nullptr;

	m_recvEchoQueue.clear();
	m_sendQueue.clear();

	google::protobuf::ShutdownProtobufLibrary();
}

void OutgameServer::DispatchReceivedData(Session* session, char* data, int nReceivedByte)
{
	// todo: 알맞은 처리 큐에 넣기
	PacketHeader packetHeader;

	if (PacketBuilder::Instance().DeserializeHeader(data, nReceivedByte, packetHeader))
	{
		switch (packetHeader.type)
		{
		case EPacketType::C2S_ECHO:
		{
			auto echoRequest = std::make_shared<Protocol::C2S_Echo>();
			if (PacketBuilder::Instance().DeserializeData(data, nReceivedByte, packetHeader, *echoRequest))
			{
				std::shared_ptr<EchoQStruct> echoStruct = std::make_shared<EchoQStruct>(session->sessionId, echoRequest);
				//echoRequest.reset();
				m_recvEchoQueue.push(echoStruct);
			}
			else
			{
				printf("PakcetBuilder::Deserialize() failed\n");
			}
			break;
		}
		default:
			printf("Unknown packet type\n");
			break;
		}
	}

}

void OutgameServer::ProcessEchoQueue()
{
	std::shared_ptr<EchoQStruct> echoStruct;
	while (m_bRun)
	{
		if (!m_recvEchoQueue.try_pop(echoStruct))
			continue;
		
		std::shared_ptr<SendQStrct> sendStruct = std::make_shared<SendQStrct>();
		sendStruct->type = ESendType::UNICAST;
		sendStruct->sessionId = echoStruct->sessionId;
		sendStruct->data = echoStruct->data;
		std::string serializedString;
		(echoStruct->data)->SerializeToString(&serializedString);
		sendStruct->header = std::make_shared<PacketHeader>(PacketBuilder::Instance().CreateHeader(EPacketType::S2C_ECHO, serializedString.size()));

		//echoStruct.reset();
		m_sendQueue.push(sendStruct);
	}
}

void OutgameServer::SendThread()
{
	// todo 브로드캐스트, 유니캐스트 타입 구분해서 실행
	std::shared_ptr<SendQStrct> sendStruct;
	while (m_bRun)
	{
		if (!m_sendQueue.try_pop(sendStruct))
			continue;

		char* serializedPacket = PacketBuilder::Instance().Serialize(sendStruct->header->type, *sendStruct->data);
		if (serializedPacket)
		{
			bool rt = m_serverCore->StartSend(sendStruct->sessionId, serializedPacket, sendStruct->header->length);
			delete[] serializedPacket;
			//sendStruct.reset();

			if (!rt)
				return;
		}
	}
}

void OutgameServer::QuitThread()
{
	while (m_bRun)
	{
		if (GetAsyncKeyState(VK_ESCAPE) & 0x8000)
		{
			m_bRun = false;
			Stop();
			printf("QuitThread() : Cleanup event triggered\n");
			break;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}
