#include "pch.h"
#include "UserManager.h"

#include "User.h"

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
		// sessionId
		sendStruct->session = loginStruct->session;
		// data
		Protocol::C2S_LoginRequest* requestData = static_cast<Protocol::C2S_LoginRequest*>(loginStruct->data.get());
		std::shared_ptr<Protocol::S2C_LoginResponse> response = std::make_shared<Protocol::S2C_LoginResponse>();
		// 요청 데이터 인증 확인
		if (AuthenticateUser(requestData->username(), requestData->password()))
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
	
}

bool UserManager::AuthenticateUser(const std::string_view& username, const std::string_view& password)
{
	// UserDB 조회, validation 확인
	std::shared_ptr<DBConnection> dbConn = DBConnectionPool::Instance().GetConnection();

	DBBind<1, 2> loginRequestBind(dbConn, L"SELECT password, status FROM [dbo].[User] WHERE username = (?)");

	std::wstring wUsername = std::wstring(username.begin(), username.end());
	loginRequestBind.BindParam(0, wUsername.c_str(), wUsername.size());

	// todo : db에서 password를 이상하게 읽어옴
	WCHAR outPassword[256];
	int outStatus = 2;
	loginRequestBind.BindCol(0, OUT outPassword);
	loginRequestBind.BindCol(1, OUT outStatus);

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

	// 유효한 id. password 확인
	std::wstring dbPassword(outPassword);
	std::wstring inputPassword(password.begin(), password.end());
	if (dbPassword == inputPassword)
	{
		// 로그인 중인지 확인
		if (outStatus == EUserStateType::ONLINE || outStatus == EUserStateType::IN_GAME)
		{
			LOG_CONTENTS("로그인 중인 계정");
			return false;
		}
		else
		{
			// DB Update 및 성공 반환
			DBBind<1, 0> updateStatusBind(dbConn, L"UPDATE [dbo].[User] SET status = 1 WHERE username = (?)");
			updateStatusBind.BindParam(0, wUsername.c_str(), wUsername.size());
			assert(updateStatusBind.Execute());
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
	for(const auto& user: m_activeUserMap)
	{
		auto now = std::chrono::steady_clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - user.second->GetLastValidationTime());
		if(duration>m_userTimeout)
		{
			user.second->UpdateStatus(EUserStateType::OFFLINE);

			std::lock_guard<std::mutex> lock(m_mutex);
			m_activeUserMap.unsafe_erase(user.first);
			// todo 세션맵도 같이 정리되어야 함
		}
	}
}
