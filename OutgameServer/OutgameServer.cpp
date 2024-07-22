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

#include "UserManager.h"

#include <sstream>

#include "../PacketLibrary/Protocol.pb.h"

#ifdef _DEBUG
#include <vld/vld.h>
#endif



int main()
{
	/// Database test
	assert(DBConnectionPool::Instance().Connect(5, L"Driver={ODBC Driver 17 for SQL Server};Server=(localdb)\\MSSQLLocalDB;Database=ServerDB;Trusted_Connection=Yes;"));

	// Create Table
	{
		DBConnection* dbConn = DBConnectionPool::Instance().GetConnection();
		assert(dbConn->ExecuteFile("User.sql"));
		DBConnectionPool::Instance().ReturnConnection(dbConn);
	}

	// Add Data
	for (int i = 0; i < 3; i++)
	{
		DBConnection* dbConn = DBConnectionPool::Instance().GetConnection();
		DBBind<2, 0> dbBind(dbConn, L"INSERT INTO [dbo].[User]([username], [password]) VALUES(?, ?)");

		std::wstring username = L"test" + std::to_wstring(i + 1);
		dbBind.BindParam(0, username.c_str(), username.size());
		std::wstring password = L"1234";
		dbBind.BindParam(1, password.c_str(), password.size()); 

		assert(dbBind.Execute());
		DBConnectionPool::Instance().ReturnConnection(dbConn);
	}

	//// Read
	//{
	//	std::shared_ptr<DBConnection> dbConn = DBConnectionPool::Instance().GetConnection();

	//	DBBind<1, 4> dbBind(dbConn, L"SELECT id, username, password, status FROM [dbo].[User] WHERE username = (?)");

	//	std::wstring username = L"test2";
	//	dbBind.BindParam(0, username.c_str(), username.size());

	//	int outId = 0;
	//	WCHAR outUsername[100];
	//	WCHAR outPassword[100];
	//	int outStatus = 0;
	//	dbBind.BindCol(0, OUT outId);
	//	dbBind.BindCol(1, OUT outUsername);
	//	dbBind.BindCol(2, OUT outPassword);
	//	dbBind.BindCol(3, OUT outStatus);

	//	assert(dbBind.Execute());

	//	std::wcout.imbue(std::locale("kor"));
	//	while (dbConn->Fetch())
	//	{
	//		std::wcout << "Id: " << outId << " /Username : " << outUsername << " /Password: " << outPassword << " /Status: "<< outStatus << '\n';
	//	}
	//}

	OutgameServer::Instance().Start();
}

void OutgameServer::Start()
{
	m_bRun = true;

	m_pServerCore = new ServerCore("5001", 5);
	m_pUserManager = new UserManager;
	m_pUserManager->SetTimeout(std::chrono::milliseconds(3000));

	m_pServerCore->RegisterCallback([this](Session* session, char* data, int nReceivedByte)
		{
			DispatchReceivedData(session, data, nReceivedByte);
		});

	/// Threads
	// Core
	m_workers.emplace_back(new std::thread(&ServerCore::Run, m_pServerCore));					// Server Core
	m_workers.emplace_back(new std::thread(&OutgameServer::SendThread, this));						// Send
	m_workers.emplace_back(new std::thread(&OutgameServer::QuitThread, this));						// Quit
	// Logic
	m_workers.emplace_back(new std::thread(&OutgameServer::ProcessEchoQueue, this));				// Echo - Test
	m_workers.emplace_back(new std::thread(&UserManager::HandleLoginRequest, m_pUserManager));	// Login
	m_workers.emplace_back(new std::thread(&UserManager::BroadcastValidationPacket, m_pUserManager, std::chrono::milliseconds(1000)));	// Validation Request
	m_workers.emplace_back(new std::thread(&UserManager::HandleValidationResponse, m_pUserManager));	// Validation Response
	

	for(const auto& worker : m_workers)
	{
		if(worker->joinable()) 
		{
			worker->join();
			delete worker;
		}
	}
	m_workers.clear();
}

void OutgameServer::Stop()
{
	m_bRun = false;

	// core 자원 해제
	m_pServerCore->TriggerCleanupEvent();
	m_pServerCore = nullptr;

	delete m_pUserManager;

	m_recvEchoQueue.clear();
	m_sendQueue.clear();

	google::protobuf::ShutdownProtobufLibrary();
}

void OutgameServer::InsertSendTask(std::shared_ptr<SendStruct> task)
{
	m_sendQueue.push(task);
}

void OutgameServer::DispatchReceivedData(Session* session, char* data, int nReceivedByte)
{
	PacketHeader packetHeader;

	if (PacketBuilder::Instance().DeserializeHeader(data, nReceivedByte, packetHeader))
	{
		switch (packetHeader.type)
		{
		// echo
		case EPacketType::C2S_ECHO:
		{ 
			auto echoRequest = std::make_shared<Protocol::C2S_Echo>();
			if (PacketBuilder::Instance().DeserializeData(data, nReceivedByte, packetHeader, *echoRequest))
			{
				std::shared_ptr<ReceiveStruct> echoStruct = std::make_shared<ReceiveStruct>(session, echoRequest);
				m_recvEchoQueue.push(echoStruct);
			}
			else
			{
				LOG_CONTENTS("C2S_ECHO: PakcetBuilder::Deserialize() failed");
			}
			break;
		}
		// validation
		case EPacketType::C2S_VALIDATION_RESPONSE:
		{
			auto validationResponse = std::make_shared<Protocol::C2S_ValidationResponse>();
			if (PacketBuilder::Instance().DeserializeData(data, nReceivedByte, packetHeader, *validationResponse))
			{
				std::shared_ptr<ReceiveStruct> receiveStruct = std::make_shared<ReceiveStruct>(session, validationResponse);
				m_pUserManager->InsertValidationResponse(receiveStruct);
			}
			else
			{
				LOG_CONTENTS("C2S_VALIDATION_RESPONSE: PakcetBuilder::Deserialize() failed");
			}
			break;
		}
		// login
		case EPacketType::C2S_LOGIN_REQUEST:
		{
			auto loginRequest = std::make_shared<Protocol::C2S_LoginRequest>();
			if (PacketBuilder::Instance().DeserializeData(data, nReceivedByte, packetHeader, *loginRequest))
			{
				std::shared_ptr<ReceiveStruct> receiveStruct = std::make_shared<ReceiveStruct>(session, loginRequest);
				m_pUserManager->InsertLoginRequest(receiveStruct);
			}
			else
			{
				LOG_CONTENTS("C2S_LOGIN_REQUEST: PakcetBuilder::Deserialize() failed");
			}
			break;
		}
		default:
			LOG_CONTENTS("Unknown packet type");
			break;
		}
	}

}

void OutgameServer::ProcessEchoQueue()
{
	std::shared_ptr<ReceiveStruct> echoStruct;
	while (m_bRun)
	{
		if (m_recvEchoQueue.empty())
			continue;

		if (!m_recvEchoQueue.try_pop(echoStruct))
			continue;

		std::shared_ptr<SendStruct> sendStruct = std::make_shared<SendStruct>();
		sendStruct->type = ESendType::UNICAST;
		sendStruct->session = echoStruct->session;
		sendStruct->data = echoStruct->data;
		std::string serializedString;
		(echoStruct->data)->SerializeToString(&serializedString);
		sendStruct->header = std::make_shared<PacketHeader>(PacketBuilder::Instance().CreateHeader(EPacketType::S2C_ECHO, serializedString.size()));

		m_sendQueue.push(sendStruct);
	}
}

void OutgameServer::SendThread()
{
	std::shared_ptr<SendStruct> sendStruct;
	while (m_bRun)
	{
		if (m_sendQueue.empty())
			continue;

		if (!m_sendQueue.try_pop(sendStruct))
			continue;

		char* serializedPacket = PacketBuilder::Instance().Serialize(sendStruct->header->type, *sendStruct->data);
		if (!serializedPacket)
		{
			LOG_CONTENTS("Packet Serialize Failed");
			return;
		}

		switch (sendStruct->type)
		{
		case ESendType::UNICAST:
			{
			if(!m_pServerCore->Unicast(sendStruct->session, serializedPacket, sendStruct->header->length))
				LOG_CONTENTS("Unicast Failed");

			break;
			}
		case ESendType::BROADCAST:
			{
			if (!m_pServerCore->Broadcast(serializedPacket, sendStruct->header->length))
				LOG_CONTENTS("Broadcast Failed");

			break;
			}
		default:
			{
			LOG_CONTENTS("Undefined Send Type");
			break;
			}
		}

		delete[] serializedPacket;
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
