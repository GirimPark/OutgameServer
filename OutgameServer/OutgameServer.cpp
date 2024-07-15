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

#include "Define.h"
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
		std::shared_ptr<DBConnection> dbConn = DBConnectionPool::Instance().GetConnection();
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


	OutgameServer server;
	server.Start();
}

OutgameServer::OutgameServer()
{
	m_processThreads.resize(m_nProcessThread);
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
	m_processThreads[0] = new std::thread(&OutgameServer::ProcessLoginRequests, this);
	m_sendThread = new std::thread(&OutgameServer::SendThread, this);
	m_quitThread = new std::thread(&OutgameServer::QuitThread, this);

	if (m_coreThread->joinable())
	{
		m_coreThread->join();
		delete m_coreThread;
	}
	for(const auto& processThread : m_processThreads)
	{
		if(processThread->joinable())
		{
			processThread->join();
		}
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
				std::shared_ptr<ReceiveStruct> echoStruct = std::make_shared<ReceiveStruct>(session, echoRequest);
				m_recvEchoQueue.push(echoStruct);
			}
			else
			{
				LOG_CONTENTS("PakcetBuilder::Deserialize() failed");
			}
			break;
		}
		case EPacketType::C2S_LOGIN_REQUEST:
		{
			auto loginRequest = std::make_shared<Protocol::C2S_Login_Request>();
			if (PacketBuilder::Instance().DeserializeData(data, nReceivedByte, packetHeader, *loginRequest))
			{
				std::shared_ptr<ReceiveStruct> receiveStruct = std::make_shared<ReceiveStruct>(session, loginRequest);
				m_loginRequests.push(receiveStruct);
			}
			else
			{
				LOG_CONTENTS("PakcetBuilder::Deserialize() failed");
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

void OutgameServer::ProcessLoginRequests()
{
	std::shared_ptr<ReceiveStruct> loginStruct;
	while (m_bRun)
	{
		if (!m_loginRequests.try_pop(loginStruct))
			continue;
		
		std::shared_ptr<SendStruct> sendStruct = std::make_shared<SendStruct>();
		// type
		sendStruct->type = ESendType::UNICAST;
		// sessionId
		sendStruct->session = loginStruct->session;
		// data
		Protocol::C2S_Login_Request* requestData = static_cast<Protocol::C2S_Login_Request*>(loginStruct->data.get());
		std::shared_ptr<Protocol::S2C_Login_Response> response = std::make_shared<Protocol::S2C_Login_Response>();
		// 요청 데이터 인증 확인
		if(UserManager::Instance().AuthenticateUser(requestData->username(), requestData->password()))
		{
			response->mutable_sucess()->set_value(true);
			sendStruct->session->state = Session::eStateType::REGISTER;
		}
		else
		{
			response->mutable_sucess()->set_value(false);
			sendStruct->session->state = Session::eStateType::MAINTAIN;
		}
		sendStruct->data = response;
		// header
		std::string serializedString;
		(sendStruct->data)->SerializeToString(&serializedString);
		sendStruct->header = std::make_shared<PacketHeader>(PacketBuilder::Instance().CreateHeader(EPacketType::S2C_LOGIN_RESPONSE, serializedString.size()));

		m_sendQueue.push(sendStruct);
	}
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
			bool rt = m_serverCore->StartSend(sendStruct->session, serializedPacket, sendStruct->header->length);
			delete[] serializedPacket;

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
