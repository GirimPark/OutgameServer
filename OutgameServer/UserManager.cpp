#include "pch.h"
#include "UserManager.h"

#include "User.h"
#include "GameRoom.h"
#include "GameRoomManager.h"

concurrency::concurrent_unordered_map<SessionId, std::shared_ptr<User>> UserManager::s_activeUserMap;
concurrency::concurrent_unordered_map<std::string, UserId> UserManager::s_activeUsername;

UserManager::UserManager()
{
	InitializeCriticalSection(&m_userMapLock);

	int offlineState = EUserState::OFFLINE;
#ifdef DB_INCLUDE_VERSION
	DBConnection* dbConn = DBConnectionPool::Instance().GetConnection();
	DBBind<1, 0> updateStateBind(dbConn, L"UPDATE [dbo].[User] SET State = (?)");
	updateStateBind.BindParam(0, offlineState);
	ASSERT_CRASH(updateStateBind.Execute());
	DBConnectionPool::Instance().ReturnConnection(dbConn);
#endif

	//OutgameServer::Instance().RegisterPacketHanlder(PacketID::C2S_VALIDATION_RESPONSE, [this](std::shared_ptr<ReceiveStruct> receiveStruct)
	//{
	//		HandleValidationResponse(receiveStruct);
	//});

	OutgameServer::Instance().RegisterPacketHanlder(PacketID::C2S_LOGIN_REQUEST, [this](std::shared_ptr<ReceiveStruct> receiveStruct)
		{
			HandleLoginRequest(receiveStruct);
		});
	OutgameServer::Instance().RegisterPacketHanlder(PacketID::C2S_LOGOUT_REQUEST, [this](std::shared_ptr<ReceiveStruct> receiveStruct)
		{
			HandleLogoutRequest(receiveStruct);
		});
	OutgameServer::Instance().RegisterPacketHanlder(PacketID::C2S_JOIN_REQUEST, [this](std::shared_ptr<ReceiveStruct> receiveStruct)
		{
			HandleJoinRequest(receiveStruct);
		});
	OutgameServer::Instance().RegisterPacketHanlder(PacketID::C2S_FIND_FRIEND_REQUEST, [this](std::shared_ptr<ReceiveStruct> receiveStruct)
		{
			HandleFindFriendRequest(receiveStruct);
		});
}

UserManager::~UserManager()
{
	DeleteCriticalSection(&m_userMapLock);

	int offlineState = EUserState::OFFLINE;
#ifdef DB_INCLUDE_VERSION
	DBConnection* dbConn = DBConnectionPool::Instance().GetConnection();
	DBBind<1, 0> updateStateBind(dbConn, L"UPDATE [dbo].[User] SET State = (?)");
	updateStateBind.BindParam(0, offlineState);
	ASSERT_CRASH(updateStateBind.Execute());
	DBConnectionPool::Instance().ReturnConnection(dbConn);
#endif

	s_activeUserMap.clear();
}

void UserManager::UpdateActiveUserMap(std::chrono::milliseconds period)
{
	while(OutgameServer::Instance().IsRunning())
	{
		EnterCriticalSection(&m_userMapLock);
		auto snapshot = s_activeUserMap;
		LeaveCriticalSection(&m_userMapLock);
		std::list<UserId> invalidUserList;
		for(const auto& iter : snapshot)
		{
			if (!iter.second->GetSession()->IsValid())
				invalidUserList.push_back(iter.second->GetId());
		}


		for(const auto& id : invalidUserList)
		{
			EnterCriticalSection(&m_userMapLock);
			// user 종료 처리
			auto userIter = s_activeUserMap.find(id);
			userIter->second->UpdateState(EUserState::OFFLINE);
			delete userIter->second->GetSession();

			// 방 퇴장 및 처리
			std::shared_ptr<GameRoom> activeGameRoom = userIter->second->GetActiveGameRoomRef().lock();
			if(activeGameRoom)
			{
				activeGameRoom->Quit(userIter->second);
				if (activeGameRoom->GetRoomState() == ERoomStateType::DESTROYING)
				{
					GameRoomManager::UpdateActiveGameRooms(activeGameRoom->GetRoomCode());
				}
			}

			// user 삭제
			s_activeUsername.unsafe_erase(userIter->second->GetName());
			s_activeUserMap.unsafe_erase(userIter);
			LeaveCriticalSection(&m_userMapLock);
		}

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
	// packetID
	sendStruct->type = ESendType::UNICAST;
	// session
	sendStruct->session = receiveStructure->session;
	// data
	std::shared_ptr<Protocol::S2C_LoginResponse> response = std::make_shared<Protocol::S2C_LoginResponse>();
	// 요청 데이터 인증 확인
	if (AuthenticateUser(sendStruct->session, loginRequest->username(), loginRequest->password()))
	{
		response->mutable_success()->set_value(true);
		// friendList
		std::shared_ptr<User> user = s_activeUserMap.find(sendStruct->session->GetSessionId())->second;
		for(const auto& friendInfo : user->GetFriendListRef())
		{
			Protocol::FriendInfo* protoFriendInfo = response->add_friendlist();
			protoFriendInfo->set_username(friendInfo.first);
			protoFriendInfo->set_state(friendInfo.second);
		}

		//pendingList
		for (const auto& pendingInfo : user->GetPendingListRef())
		{
			Protocol::FriendInfo* protoPendingInfo = response->add_pendinglist();
			protoPendingInfo->set_username(pendingInfo.first);
			protoPendingInfo->set_state(pendingInfo.second);
		}

		sendStruct->session->SetState(eSessionStateType::REGISTER);

	}
	else
	{
		response->mutable_success()->set_value(false);
		sendStruct->session->SetState(eSessionStateType::CLOSE);
	}
	sendStruct->data = response;
	// header
	std::string serializedString;
	(sendStruct->data)->SerializeToString(&serializedString);
	sendStruct->header = std::make_shared<PacketHeader>(PacketBuilder::Instance().CreateHeader(PacketID::S2C_LOGIN_RESPONSE, serializedString.size()));

	OutgameServer::Instance().InsertSendTask(sendStruct);
}

void UserManager::HandleLogoutRequest(std::shared_ptr<ReceiveStruct> receiveStructure)
{
	if(!LogoutUser(receiveStructure->session))
	{
		LOG_CONTENTS("Logout User Failed");
		return;
	}
	if(receiveStructure->session->GetState() != eSessionStateType::MAINTAIN)
	{
		int a = 0;
	}

	//LOG_CONTENTS("로그아웃 성공");

	// 결과 송신
	std::shared_ptr<SendStruct> sendStruct = std::make_shared<SendStruct>();
	// packetID
	sendStruct->type = ESendType::UNICAST;
	// session
	sendStruct->session = receiveStructure->session;
	sendStruct->session->SetState(eSessionStateType::UNREGISTER);
	// data
	sendStruct->data = std::make_shared<Protocol::S2C_LogoutResponse>();
	// header
	std::string serializedString;
	(sendStruct->data)->SerializeToString(&serializedString);
	sendStruct->header = std::make_shared<PacketHeader>(PacketBuilder::Instance().CreateHeader(PacketID::S2C_LOGOUT_RESPONSE, serializedString.size()));

	OutgameServer::Instance().InsertSendTask(sendStruct);
}

void UserManager::HandleJoinRequest(std::shared_ptr<ReceiveStruct> receiveStructure)
{
	auto joinRequest = std::make_shared<Protocol::C2S_JoinRequest>();
	if (!PacketBuilder::Instance().DeserializeData(receiveStructure->data, receiveStructure->nReceivedByte, *(receiveStructure->header), *joinRequest))
	{
		LOG_CONTENTS("C2S_JOIN_REQUEST: PakcetBuilder::Deserialize() failed");
		return;
	}

	std::shared_ptr<SendStruct> sendStruct = std::make_shared<SendStruct>();
	// packetID
	sendStruct->type = ESendType::UNICAST;
	// session
	sendStruct->session = receiveStructure->session;
	// data
	std::shared_ptr<Protocol::S2C_JoinResponse> response = std::make_shared<Protocol::S2C_JoinResponse>();
	// 아이디 중복 확인
	if(IsAvailableID(joinRequest->username(), joinRequest->password()))
	{
		response->mutable_success()->set_value(true);

#ifdef DB_INCLUDE_VERSION
		// DB에 계정 정보 추가
		DBConnection* dbConn = DBConnectionPool::Instance().GetConnection();
		DBBind<2, 0> dbBind(dbConn, L"INSERT INTO [dbo].[User]([Nickname], [Password]) VALUES(?, ?)");

		std::wstring username = std::wstring(joinRequest->username().begin(), joinRequest->username().end());
		dbBind.BindParam(0, username.c_str(), username.size());
		std::wstring password = std::wstring(joinRequest->password().begin(), joinRequest->password().end());
		dbBind.BindParam(1, password.c_str(), password.size());

		if(!dbBind.Execute())
			response->mutable_success()->set_value(false);
		DBConnectionPool::Instance().ReturnConnection(dbConn);
#endif
	}
	else
		response->mutable_success()->set_value(false);

	sendStruct->session->SetState(eSessionStateType::CLOSE);
	sendStruct->data = response;
	// header
	std::string serializedString;
	(sendStruct->data)->SerializeToString(&serializedString);
	sendStruct->header = std::make_shared<PacketHeader>(PacketBuilder::Instance().CreateHeader(PacketID::S2C_JOIN_RESPONSE, serializedString.size()));

	OutgameServer::Instance().InsertSendTask(sendStruct);
}

void UserManager::HandleFindFriendRequest(std::shared_ptr<ReceiveStruct> receiveStructure)
{
	auto findFriendRequest = std::make_shared<Protocol::C2S_FindFriendRequest>();
	if (!PacketBuilder::Instance().DeserializeData(receiveStructure->data, receiveStructure->nReceivedByte, *(receiveStructure->header), *findFriendRequest))
	{
		LOG_CONTENTS("C2S_FIND_FRIEND_REQUEST: PakcetBuilder::Deserialize() failed");
		return;
	}

	std::shared_ptr<SendStruct> sendStruct = std::make_shared<SendStruct>();
	// packetID
	sendStruct->type = ESendType::UNICAST;
	// session
	sendStruct->session = receiveStructure->session;
	// data
	std::shared_ptr<Protocol::S2C_FindFriendResponse> response = std::make_shared<Protocol::S2C_FindFriendResponse>();
	int userState; int requestState;
	if(FindUser(FindActiveUser(sendStruct->session->GetSessionId()).lock()->GetName(), findFriendRequest->username(),  userState, requestState))
	{
		response->mutable_exist()->set_value(true);
		Protocol::FriendInfo* friendInfo = response->mutable_friendinfo();
		friendInfo->set_username(findFriendRequest->username());
		friendInfo->set_state(userState);
		response->set_requeststate(requestState);
	}
	else
	{
		response->mutable_exist()->set_value(false);
	}

	sendStruct->data = response;
	// header
	std::string serializedString;
	(sendStruct->data)->SerializeToString(&serializedString);
	sendStruct->header = std::make_shared<PacketHeader>(PacketBuilder::Instance().CreateHeader(PacketID::S2C_FIND_FRIEND_RESPONSE, serializedString.size()));

	OutgameServer::Instance().InsertSendTask(sendStruct);
}

//void UserManager::HandleValidationResponse(std::shared_ptr<ReceiveStruct> receiveStructure)
//{
//	auto validationResponse = std::make_shared<Protocol::C2S_ValidationResponse>();
//	if(PacketBuilder::Instance().DeserializeData(receiveStructure->data, receiveStructure->nReceivedByte, *(receiveStructure->header), *validationResponse))
//	{
//		EnterCriticalSection(&m_userMapLock);
//		s_activeUserMap[receiveStructure->session->GetSessionId()]->UpdateLastValidationTime(std::chrono::steady_clock::now());
//		LeaveCriticalSection(&m_userMapLock);
//	}
//	else
//	{
//		LOG_CONTENTS("C2S_VALIDATION_RESPONSE: PakcetBuilder::Deserialize() failed");
//	}
//}

void UserManager::CreateActiveUser(Session* session, std::string_view name)
{
	// 새 유저 생성, 세션과 연결
	EnterCriticalSection(&m_userMapLock);
	std::shared_ptr<User> user = std::make_shared<User>(session, name);
	s_activeUsername.insert({ user->GetName(), session->GetSessionId() });
	s_activeUserMap.insert({ session->GetSessionId(), user });
	LeaveCriticalSection(&m_userMapLock);

#ifdef DB_INCLUDE_VERSION
	// User State Update
	DBConnection* dbConn = DBConnectionPool::Instance().GetConnection();
	std::wstring wUsername = std::wstring(name.begin(), name.end());
	DBBind<1, 0> updateStateBind(dbConn, L"UPDATE [dbo].[User] SET State = 1 WHERE Nickname = (?)");
	updateStateBind.BindParam(0, wUsername.c_str(), wUsername.size());
	ASSERT_CRASH(updateStateBind.Execute());

	// Get Friend List(IsMutualFriend == 1)
	DBBind<1, 2> getFriendBind(dbConn, L"SELECT f.FriendName, u.State FROM[dbo].[Friends] f INNER JOIN[dbo].[User] u ON f.FriendName = u.Nickname WHERE f.UserName = (?) AND f.IsMutualFriend = 1");
	getFriendBind.BindParam(0, wUsername.c_str(), wUsername.size());

	WCHAR outFriendName[256];
	int outState;
	getFriendBind.BindCol(0, outFriendName);
	getFriendBind.BindCol(1, outState);
	if(!getFriendBind.Execute())
	{
		LOG_CONTENTS("getFriendBind.Execute() Failed");
		return;
	}

	while(getFriendBind.Fetch())
	{
		std::wstring wFriendName(outFriendName);
		ZeroMemory(outFriendName, 256);
		std::string friendName(wFriendName.begin(), wFriendName.end());

		user->AppendFriend(friendName, static_cast<EUserState>(outState));
	}

	// Get Accept Pending List(IsMutualFriend == 0)
	DBBind<1, 2> getPendingBind(dbConn, L"SELECT f.UserName, u.State FROM[dbo].[Friends] f INNER JOIN[dbo].[User] u ON f.UserName = u.Nickname WHERE f.FriendName = (?) AND f.IsMutualFriend = 0");
	getPendingBind.BindParam(0, wUsername.c_str(), wUsername.size());

	getPendingBind.BindCol(0, outFriendName);
	getPendingBind.BindCol(1, outState);
	if (!getPendingBind.Execute())
	{
		LOG_CONTENTS("getPendingBind.Execute() Failed");
		return;
	}

	while(getPendingBind.Fetch())
	{
		std::wstring wFriendName(outFriendName);
		ZeroMemory(outFriendName, 256);
		std::string friendName(wFriendName.begin(), wFriendName.end());

		user->AppendPendingFriend(friendName, static_cast<EUserState>(outState));
	}
	DBConnectionPool::Instance().ReturnConnection(dbConn);
#endif

	user->UpdateState(EUserState::ONLINE);
}

bool UserManager::AuthenticateUser(Session* session, const std::string_view& username, const std::string_view& password)
{
#ifdef DB_INCLUDE_VERSION
	// UserDB 조회, validation 확인
	DBConnection* dbConn = DBConnectionPool::Instance().GetConnection();

	DBBind<1, 2> loginRequestBind(dbConn, L"SELECT Password, State FROM [dbo].[User] WHERE Nickname = (?)");
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
		if (outState == EUserState::ONLINE || outState == EUserState::IN_GAME)
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
#else
	if ((username == "host" || username == "guest")
		&& password == "1234")
	{
		CreateActiveUser(session, username);
		return true;
	}
	else
		return false;
#endif
}

bool UserManager::LogoutUser(Session* session)
{
	// DB Update
	EnterCriticalSection(&m_userMapLock);
	auto iter = s_activeUserMap.find(session->GetSessionId());
	ASSERT_CRASH(iter != s_activeUserMap.end());
	std::wstring username = std::wstring(iter->second->GetName().begin(), iter->second->GetName().end());

#ifdef DB_INCLUDE_VERSION
	DBConnection* dbConn = DBConnectionPool::Instance().GetConnection();
	DBBind<1, 0> updateStateBind(dbConn, L"UPDATE [dbo].[User] SET State = 0 WHERE Nickname = (?)");
	updateStateBind.BindParam(0, username.c_str(), username.size());
	ASSERT_CRASH(updateStateBind.Execute());
	DBConnectionPool::Instance().ReturnConnection(dbConn);
#endif

	// active User Map에서 해당 유저 정리
	iter->second->UpdateState(EUserState::OFFLINE);

	s_activeUsername.unsafe_erase(iter->second->GetName());
	s_activeUserMap.unsafe_erase(iter);
	LeaveCriticalSection(&m_userMapLock);

	return true;
}

bool UserManager::IsAvailableID(const std::string_view& username, const std::string_view& password)
{
#ifdef DB_INCLUDE_VERSION
	// UserDB 조회, validation 확인
	DBConnection* dbConn = DBConnectionPool::Instance().GetConnection();

	DBBind<1, 1> joinRequestBind(dbConn, L"SELECT 1 FROM [dbo].[User] WHERE Nickname = (?)");
	std::wstring wUsername = std::wstring(username.begin(), username.end());
	joinRequestBind.BindParam(0, wUsername.c_str(), wUsername.size());

	int exist = 0;
	joinRequestBind.BindCol(0, OUT exist);

	if (!joinRequestBind.Execute())
	{
		LOG_CONTENTS("joinRequest Execute Failed");
		DBConnectionPool::Instance().ReturnConnection(dbConn);
		return false;
	}

	if (joinRequestBind.Fetch())
	{
		LOG_CONTENTS("중복된 ID");
		DBConnectionPool::Instance().ReturnConnection(dbConn);
		return false;
	}

	DBConnectionPool::Instance().ReturnConnection(dbConn);
#endif
	return true;
}

bool UserManager::FindUser(const std::string& username, const std::string& friendName, int& friendState, int& requestState)
{
#ifdef DB_INCLUDE_VERSION
	// 유저 상태 쿼리
	DBConnection* dbConn = DBConnectionPool::Instance().GetConnection();
	DBBind<1, 1> findStateBind(dbConn, L"SELECT State FROM [dbo].[User] WHERE Nickname = (?)");

	std::wstring wFriendName = std::wstring(friendName.begin(), friendName.end());
	findStateBind.BindParam(0, wFriendName.c_str(), wFriendName.size());
	findStateBind.BindCol(0, friendState);

	if (!findStateBind.Execute())
		return false;

	// 친구 신청 상태 쿼리
	DBBind<2, 1> requestStateBind(dbConn, L"SELECT IsMutualFriend FROM [dbo].[Friends] WHERE UserName = (?) AND FriendName = (?)");

	std::wstring wUserName = std::wstring(username.begin(), username.end());
	requestStateBind.BindParam(0, wUserName.c_str(), wUserName.size());
	requestStateBind.BindParam(1, wFriendName.c_str(), wFriendName.size());
	requestStateBind.BindCol(0, requestState);

	if (!requestStateBind.Execute())
		return false;
	
	DBConnectionPool::Instance().ReturnConnection(dbConn);
#endif

	return true;
}
