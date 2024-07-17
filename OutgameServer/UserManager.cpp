#include "pch.h"
#include "UserManager.h"

#include "User.h"

UserManager::UserManager()
{
	InitializeCriticalSection(&m_criticalSection);
}

UserManager::~UserManager()
{
	DeleteCriticalSection(&m_criticalSection);
}

void UserManager::CreateActiveUser(Session* session, std::string_view name)
{
	// 새 유저 생성, 세션과 연결
	User* user = new User(session, name);
	m_activeUserMap.insert({ session->sessionId, user });

	// DB Update
	std::wstring wUsername = std::wstring(name.begin(), name.end());
	DBConnection* dbConn = DBConnectionPool::Instance().GetConnection();
	DBBind<1, 0> updateStateBind(dbConn, L"UPDATE [dbo].[User] SET state = 1 WHERE username = (?)");
	updateStateBind.BindParam(0, wUsername.c_str(), wUsername.size());
	assert(updateStateBind.Execute());
	DBConnectionPool::Instance().ReturnConnection(dbConn);
}

void UserManager::InsertLoginRequest(std::shared_ptr<ReceiveStruct> task)
{
	m_loginRequests.push(task);
}

void UserManager::InsertValidationResponse(std::shared_ptr<ReceiveStruct> task)
{
	m_validationResponses.push(task);
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
		sendStruct->header = std::make_shared<PacketHeader>(PacketBuilder::Instance().CreateHeader(EPacketType::S2C_LOGIN_RESPONSE, serializedString.size()));

		OutgameServer::Instance().InsertSendTask(sendStruct);

		std::this_thread::sleep_for(period);
	}
}

void UserManager::HandleLoginRequest()
{
	std::shared_ptr<ReceiveStruct> loginStruct;
	while (OutgameServer::Instance().IsRunning())
	{
		if (!m_loginRequests.try_pop(loginStruct))
			continue;

		std::shared_ptr<SendStruct> sendStruct = std::make_shared<SendStruct>();
		// type
		sendStruct->type = ESendType::UNICAST;
		// session
		sendStruct->session = loginStruct->session;
		// data
		Protocol::C2S_LoginRequest* requestData = static_cast<Protocol::C2S_LoginRequest*>(loginStruct->data.get());
		std::shared_ptr<Protocol::S2C_LoginResponse> response = std::make_shared<Protocol::S2C_LoginResponse>();
		// 요청 데이터 인증 확인
		if (AuthenticateUser(sendStruct->session, requestData->username(), requestData->password()))
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

		OutgameServer::Instance().InsertSendTask(sendStruct);
	}
}

void UserManager::HandleValidationResponse()
{
	std::shared_ptr<ReceiveStruct> validationStruct;
	while (OutgameServer::Instance().IsRunning())
	{
		if (!m_validationResponses.try_pop(validationStruct))
			continue;

		m_activeUserMap[validationStruct->session->sessionId]->UpdateLastValidationTime(std::chrono::steady_clock::now());
	}
}

bool UserManager::AuthenticateUser(Session* session, const std::string_view& username, const std::string_view& password)
{
	// UserDB 조회, validation 확인
	DBConnection* dbConn = DBConnectionPool::Instance().GetConnection();

	DBBind<1, 2> loginRequestBind(dbConn, L"SELECT password, state FROM [dbo].[User] WHERE username = (?)");

	std::wstring wUsername = std::wstring(username.begin(), username.end());
	loginRequestBind.BindParam(0, wUsername.c_str(), wUsername.size());

	WCHAR outPassword[256];
	int outState = 2;
	loginRequestBind.BindCol(0, OUT outPassword);
	loginRequestBind.BindCol(1, OUT outState);

	if (!loginRequestBind.Execute())
	{
		LOG_CONTENTS("loginRequest Execute Failed");
		return false;
	}

	if (!loginRequestBind.Fetch())
	{
		LOG_CONTENTS("유효하지 않은 ID");
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
			LOG_CONTENTS("로그인 성공");

			return true;
		}
	}
	else
	{
		LOG_CONTENTS("유효하지 않은 Password");
		return false;
	}

}

void UserManager::UpdateActiveUserMap()
{
	// 유저 유효성 검사 및 목록 정리
	for(Concurrency::concurrent_unordered_map<unsigned int, User*>::iterator it = m_activeUserMap.begin(); it!=m_activeUserMap.end();)
	{
		auto now = std::chrono::steady_clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second->GetLastValidationTime());
		if(duration > m_userTimeout || it->second->GetState() == EUserStateType::OFFLINE)
		{
			it->second->UpdateStatus(EUserStateType::OFFLINE);

			EnterCriticalSection(&m_criticalSection);
			delete it->second;	// todo 이후 풀에 반환하는 형태로 수정
			it = m_activeUserMap.unsafe_erase(it);
			LeaveCriticalSection(&m_criticalSection);
		}
		else
		{
			++it;
		}
	}
}
