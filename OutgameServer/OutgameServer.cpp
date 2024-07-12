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
#include <sstream>

//#include "../PacketLibrary/Echo.pb.h"
#include "../PacketLibrary/Protocol.pb.h"
#ifdef _DEBUG
#include <vld/vld.h>
#endif



int main()
{
	/// Database test
	assert(DBConnectionPool::Instance().Connect(1, L"Driver={ODBC Driver 17 for SQL Server};Server=(localdb)\\MSSQLLocalDB;Database=ServerDB;Trusted_Connection=Yes;"));

	// Create Table
	{
		std::shared_ptr<DBConnection> dbConn = DBConnectionPool::Instance().GetConnection();;
		assert(dbConn->ExecuteFile("User.sql"));
	}

	// Add Data
	for (int i = 0; i < 3; i++)
	{
		std::shared_ptr<DBConnection> dbConn = DBConnectionPool::Instance().GetConnection();
		DBBind<2, 0> dbBind(dbConn, L"INSERT INTO [dbo].[User]([username], [password]) VALUES(?, ?)");

		std::wstring username = L"test" + std::to_wstring(i + 1);
		dbBind.BindParam(0, username.c_str(), username.size());
		std::wstring password = L"1234";
		dbBind.BindParam(1, password.c_str(), password.size());

		assert(dbBind.Execute());
	}

	// Read
	{
		std::shared_ptr<DBConnection> dbConn = DBConnectionPool::Instance().GetConnection();

		DBBind<1, 4> dbBind(dbConn, L"SELECT id, username, password, status FROM [dbo].[User] WHERE username = (?)");

		std::wstring username = L"test2";
		dbBind.BindParam(0, username.c_str(), username.size());

		int outId = 0;
		WCHAR outUsername[100];
		WCHAR outPassword[100];
		int outStatus = 0;
		dbBind.BindCol(0, OUT outId);
		dbBind.BindCol(1, OUT outUsername);
		dbBind.BindCol(2, OUT outPassword);
		dbBind.BindCol(3, OUT outStatus);

		assert(dbBind.Execute());

		std::wcout.imbue(std::locale("kor"));
		while (dbConn->Fetch())
		{
			std::wcout << "Id: " << outId << " /Username : " << outUsername << " /Password: " << outPassword << " /Status: "<< outStatus << '\n';
		}
	}


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
				std::shared_ptr<ReceiveStruct> echoStruct = std::make_shared<ReceiveStruct>(session->sessionId, echoRequest);
				m_recvEchoQueue.push(echoStruct);
			}
			else
			{
				printf("PakcetBuilder::Deserialize() failed\n");
			}
			break;
		}
		case EPacketType::C2S_LOGIN_REQUEST:
		{
			auto loginRequest = std::make_shared<Protocol::C2S_Login_Request>();
			if (PacketBuilder::Instance().DeserializeData(data, nReceivedByte, packetHeader, *loginRequest))
			{
				std::shared_ptr<ReceiveStruct> receiveStruct = std::make_shared<ReceiveStruct>(session->sessionId, loginRequest);
				m_loginRequestQueue.push(receiveStruct);
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
	std::shared_ptr<ReceiveStruct> echoStruct;
	while (m_bRun)
	{
		if (!m_recvEchoQueue.try_pop(echoStruct))
			continue;
		
		std::shared_ptr<SendStruct> sendStruct = std::make_shared<SendStruct>();
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

void OutgameServer::ProcessLoginQueue()
{
}

void OutgameServer::SendThread()
{
	// todo 브로드캐스트, 유니캐스트 타입 구분해서 실행
	std::shared_ptr<SendStruct> sendStruct;
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
