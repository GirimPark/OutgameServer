#include "pch.h"
#include "UserManager.h"

#include "User.h"

UserManager::UserManager()
{
	InitializeCriticalSection(&m_criticalSection);

	OutgameServer::Instance().RegisterPacketHanlder(EPacketType::C2S_VALIDATION_RESPONSE, [this](std::shared_ptr<ReceiveStruct> receiveStruct)
	{
			HandleValidationResponse(receiveStruct);
	});
	OutgameServer::Instance().RegisterPacketHanlder(EPacketType::C2S_LOGIN_REQUEST, [this](std::shared_ptr<ReceiveStruct> receiveStruct)
		{
			HandleLoginRequest(receiveStruct);
		});
	OutgameServer::Instance().RegisterPacketHanlder(EPacketType::C2S_LOGOUT_REQUEST, [this](std::shared_ptr<ReceiveStruct> receiveStruct)
		{
			HandleLogoutRequest(receiveStruct);
		});
}

UserManager::~UserManager()
{
	DeleteCriticalSection(&m_criticalSection);

	for(auto& user : m_activeUserMap)
	{
		delete user.second;
	}
	m_activeUserMap.clear();
}

void UserManager::BroadcastValidationPacket(std::chrono::milliseconds period)
{
	while(OutgameServer::Instance().IsRunning())
	{
		UpdateActiveUserMap();

		// 유효성 검사 패킷 송신 게시
		std::shared_ptr<SendStruct> sendStruct = std::make_shared<SendStruct>();
		sendStruct->type = ESendType::BROADCAST;

		std::shared_ptr<Protocol::S2C_ValidationRequest> validationRequest = std::make_shared<Protocol::S2C_ValidationRequest>();
		sendStruct->data = validationRequest;
		
		std::string serializedString;
		(sendStruct->data)->SerializeToString(&serializedString);
		sendStruct->header = std::make_shared<PacketHeader>(PacketBuilder::Instance().CreateHeader(EPacketType::S2C_VALIDATION_REQUEST, serializedString.size()));

		OutgameServer::Instance().InsertSendTask(sendStruct);

		std::this_thread::sleep_for(period);
	}
}

void UserManager::HandleLoginRequest(std::shared_ptr<ReceiveStruct> receiveStructure)
{
	auto loginRequest = std::make_shared<Protocol::C2S_LoginRequest>();
	if(!PacketBuilder::Instance().DeserializeData(receiveStructure->data, receiveStructure->nReceivedByte, *(receiveStructure->header), *loginRequest))
	{
		LOG_CONTENTS("C2S_LOGIN_REQUEST: PakcetBuilder::Deserialize() failed");
		return;
	}

	std::shared_ptr<SendStruct> sendStruct =std::make_shared<SendStruct>();
	// type
	sendStruct->type = ESendType::UNICAST;
	// session
	sendStruct->session = receiveStructure->session;
	// data
	std::shared_ptr<Protocol::S2C_LoginResponse> response = std::make_shared<Protocol::S2C_LoginResponse>();
	// 요청 데이터 인증 확인
	if (AuthenticateUser(sendStruct->session, loginRequest->username(), loginRequest->password()))
	{
		response->mutable_success()->set_value(true);
		sendStruct->session->state =Session::eStateType::REGISTER;
	}
	else
	{
		response->mutable_success()->set_value(false);
		sendStruct->session->state = Session::eStateType::CLOSE;
	}
	sendStruct->data = response;
	// header
	std::string serializedString;
	(sendStruct->data)->SerializeToString(&serializedString);
	sendStruct->header = std::make_shared<PacketHeader>(PacketBuilder::Instance().CreateHeader(EPacketType::S2C_LOGIN_RESPONSE, serializedString.size()));

	OutgameServer::Instance().InsertSendTask(sendStruct);
}

void UserManager::HandleLogoutRequest(std::shared_ptr<ReceiveStruct> receiveStructure)
{
	if(!LogoutUser(receiveStructure->session))
	{
		LOG_CONTENTS("Logout User Failed");
		return;
	}

	//LOG_CONTENTS("로그아웃 성공");

	// 결과 송신
	std::shared_ptr<SendStruct> sendStruct = std::make_shared<SendStruct>();
	// type
	sendStruct->type = ESendType::UNICAST;
	// session
	sendStruct->session = receiveStructure->session;
	sendStruct->session->state = Session::eStateType::UNREGISTER;
	// data
	sendStruct->data = std::make_shared<Protocol::S2C_LogoutResponse>();
	// header
	std::string serializedString;
	(sendStruct->data)->SerializeToString(&serializedString);
	sendStruct->header = std::make_shared<PacketHeader>(PacketBuilder::Instance().CreateHeader(EPacketType::S2C_LOGOUT_RESPONSE, serializedString.size()));

	OutgameServer::Instance().InsertSendTask(sendStruct);
}

void UserManager::HandleValidationResponse(std::shared_ptr<ReceiveStruct> receiveStructure)
{
	auto validationResponse = std::make_shared<Protocol::C2S_ValidationResponse>();
	if(PacketBuilder::Instance().DeserializeData(receiveStructure->data, receiveStructure->nReceivedByte, *(receiveStructure->header), *validationResponse))
	{
		concurrency::concurrent_unordered_map<SessionId, User*> snapshot = m_activeUserMap;	// 락을 최소화하기 위해 스냅샷 생성
		snapshot[receiveStructure->session->sessionId]->UpdateLastValidationTime(std::chrono::steady_clock::now());
	}
	else
	{
		LOG_CONTENTS("C2S_VALIDATION_RESPONSE: PakcetBuilder::Deserialize() failed");
	}
}

void UserManager::CreateActiveUser(Session* session, std::string_view name)
{
	// 새 유저 생성, 세션과 연결
	User* user = new User(session, name);
	EnterCriticalSection(&m_criticalSection);
	m_activeUserMap.insert({ session->sessionId, user });
	LeaveCriticalSection(&m_criticalSection);

	// DB Update
	std::wstring wUsername = std::wstring(name.begin(), name.end());
	DBConnection* dbConn = DBConnectionPool::Instance().GetConnection();
	DBBind<1, 0> updateStateBind(dbConn, L"UPDATE [dbo].[User] SET state = 1 WHERE username = (?)");
	updateStateBind.BindParam(0, wUsername.c_str(), wUsername.size());
	ASSERT_CRASH(updateStateBind.Execute());
	DBConnectionPool::Instance().ReturnConnection(dbConn);
}

bool UserManager::AuthenticateUser(Session* session, const std::string_view& username, const std::string_view& password)
{
	// UserDB 조회, validation 확인
	DBConnection* dbConn = DBConnectionPool::Instance().GetConnection();

	DBBind<1, 2> loginRequestBind(dbConn, L"SELECT password, state FROM [dbo].[User] WHERE username = (?)");
	if(dbConn->m_bUsable.load() == true)
	{
		int a = 0;
	}
	std::wstring wUsername = std::wstring(username.begin(), username.end());
	loginRequestBind.BindParam(0, wUsername.c_str(), wUsername.size());

	WCHAR outPassword[256];
	int outState = 2;
	loginRequestBind.BindCol(0, OUT outPassword);
	loginRequestBind.BindCol(1, OUT outState);

	if (!loginRequestBind.Execute())
	{
		LOG_CONTENTS("loginRequest Execute Failed");
		DBConnectionPool::Instance().ReturnConnection(dbConn);
		return false;
	}

	if (!loginRequestBind.Fetch())
	{
		LOG_CONTENTS("유효하지 않은 ID");
		DBConnectionPool::Instance().ReturnConnection(dbConn);
		return false;
	}

	DBConnectionPool::Instance().ReturnConnection(dbConn);

	// 유효한 id. password 확인
	std::wstring dbPassword(outPassword);
	std::wstring inputPassword(password.begin(), password.end());
	if (dbPassword == inputPassword)
	{
		// 로그인 중인지 확인
		if (outState == EUserStateType::ONLINE || outState == EUserStateType::IN_GAME)
		{
			LOG_CONTENTS("로그인 중인 계정");
			return false;
		}
		else
		{
			// 새 유저 생성, 로그인
			CreateActiveUser(session, username);
			//LOG_CONTENTS("로그인 성공");

			return true;
		}
	}
	else
	{
		LOG_CONTENTS("유효하지 않은 Password");
		return false;
	}

}

bool UserManager::LogoutUser(Session* session)
{
	// DB Update
	EnterCriticalSection(&m_criticalSection);
	auto iter = m_activeUserMap.find(session->sessionId);
	ASSERT_CRASH(iter != m_activeUserMap.end());
	std::wstring username = std::wstring(iter->second->GetName().begin(), iter->second->GetName().end());

	DBConnection* dbConn = DBConnectionPool::Instance().GetConnection();
	DBBind<1, 0> updateStateBind(dbConn, L"UPDATE [dbo].[User] SET state = 0 WHERE username = (?)");
	updateStateBind.BindParam(0, username.c_str(), username.size());
	ASSERT_CRASH(updateStateBind.Execute());
	DBConnectionPool::Instance().ReturnConnection(dbConn);

	// active User Map에서 해당 유저 정리
	iter->second->UpdateState(EUserStateType::OFFLINE);

	delete iter->second;
	iter = m_activeUserMap.unsafe_erase(iter);
	LeaveCriticalSection(&m_criticalSection);

	return true;
}

void UserManager::UpdateActiveUserMap()
{
	// 유저 유효성 검사 및 목록 정리
	concurrency::concurrent_unordered_map<SessionId, User*> snapshot = m_activeUserMap;
	for(Concurrency::concurrent_unordered_map<unsigned int, User*>::iterator it = snapshot.begin(); it!= snapshot.end();)
	{
		auto now = std::chrono::steady_clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second->GetLastValidationTime());
		// 세션 만료 시간이 지났다면
		if(duration > m_userTimeout || it->second->GetState() == EUserStateType::OFFLINE)
		{
			it->second->UpdateState(EUserStateType::OFFLINE);

			// 세션 만료 통지 패킷 송신
			std::shared_ptr<SendStruct> sendStruct = std::make_shared<SendStruct>();
			sendStruct->type = ESendType::UNICAST;
			sendStruct->session = it->second->GetSession();
			sendStruct->session->state = Session::eStateType::UNREGISTER;
			sendStruct->data = std::make_shared<Protocol::S2C_SessionExpiredNotification>();
			std::string serializedString;
			(sendStruct->data)->SerializeToString(&serializedString);
			sendStruct->header = std::make_shared<PacketHeader>(PacketBuilder::Instance().CreateHeader(EPacketType::S2C_SESSION_EXPIRED_NOTIFICATION, serializedString.size()));

			OutgameServer::Instance().InsertSendTask(sendStruct);

			delete it->second;
			EnterCriticalSection(&m_criticalSection);
			m_activeUserMap.unsafe_erase(it);
			LeaveCriticalSection(&m_criticalSection);
			it = snapshot.unsafe_erase(it);
		}
		else
		{
			++it;
		}
	}
}
