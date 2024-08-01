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

#include <memory>

#include "UserManager.h"
#include "PacketHandler.h"

#include <sstream>

#include "PacketDataFactory.h"


#ifdef _DEBUG
#include <vld/vld.h>
#endif

int main()
{
	/// Database test
	ASSERT_CRASH(DBConnectionPool::Instance().Connect(5, L"Driver={SQL Server};Server=localhost\\SQLEXPRESS;Database=ServerDB;Trusted_Connection=Yes;"));
	//Server=(localdb)\\MSSQLLocalDB;Database=ServerDB;Trusted_Connection=Yes;
	//L"Driver={ODBC Driver 18 for SQL Server};Server=localhost\\SQLEXPRESS;Database=ServerDB;Trusted_Connection=Yes;"

	// Create Table
	{
		DBConnection* dbConn = DBConnectionPool::Instance().GetConnection();
		ASSERT_CRASH(dbConn->ExecuteFile("User.sql"));
		DBConnectionPool::Instance().ReturnConnection(dbConn);
	}

	// Add Data
	for (int i = 0; i < 1000; i++)
	{
		DBConnection* dbConn = DBConnectionPool::Instance().GetConnection();
		DBBind<2, 0> dbBind(dbConn, L"INSERT INTO [dbo].[User]([username], [password]) VALUES(?, ?)");

		std::wstring username = L"test" + std::to_wstring(i);
		dbBind.BindParam(0, username.c_str(), username.size());
		std::wstring password = L"1234";
		dbBind.BindParam(1, password.c_str(), password.size()); 

		ASSERT_CRASH(dbBind.Execute());
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

	m_pServerCore = new ServerCore("5001", SOMAXCONN);
	m_pPacketHandler = new PacketHandler;
	m_pUserManager = new UserManager;
	m_pUserManager->SetTimeout(std::chrono::milliseconds(300000));

	m_pServerCore->RegisterCallback([this](Session* session, char* data, int nReceivedByte)
		{
			m_pPacketHandler->HandlePacket(session, data, nReceivedByte);
		});

	/// Threads
	// Core
	m_workers.emplace_back(new std::thread(&ServerCore::Run, m_pServerCore));					// Server Core
	m_workers.emplace_back(new std::thread(&OutgameServer::SendThread, this));						// Send
	m_workers.emplace_back(new std::thread(&OutgameServer::QuitThread, this));						// Quit

	m_workers.emplace_back(new std::thread(&UserManager::BroadcastValidationPacket, m_pUserManager, std::chrono::milliseconds(100000)));	// Validation Request
	

	for(const auto& worker : m_workers)
	{
		if(worker->joinable()) 
		{
			worker->join();
			delete worker;
		}
	}
	m_workers.clear();

	Stop();
}

void OutgameServer::Stop()
{
	// core 자원 해제
	delete m_pPacketHandler;
	delete m_pUserManager;

	m_sendQueue.clear();

	google::protobuf::ShutdownProtobufLibrary();
}

void OutgameServer::TriggerShutdown()
{
	m_bRun = false;
	m_pServerCore->TriggerShutdown();
	m_pServerCore = nullptr;
}

void OutgameServer::RegisterPacketHanlder(EPacketType headerType, PacketHandlerCallback callback)
{
	m_pPacketHandler->RegisterHandler(headerType, callback);
}

void OutgameServer::InsertSendTask(std::shared_ptr<SendStruct> task)
{
	m_sendQueue.push(task);
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
			TriggerShutdown();
			printf("QuitThread() : Cleanup event triggered\n");
			break;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}
