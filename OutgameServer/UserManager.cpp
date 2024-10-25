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
	OutgameServer::Instance().RegisterPacketHanlder(PacketID::C2S_ADD_FRIEND_REQUEST, [this](std::shared_ptr<ReceiveStruct> receiveStruct)
		{
			HandleAddFriendRequest(receiveStruct);
		});
	OutgameServer::Instance().RegisterPacketHanlder(PacketID::C2S_CANCEL_ADD_FRIEND_REQUEST, [this](std::shared_ptr<ReceiveStruct> receiveStruct)
		{
			HandleCancelAddFriendRequest(receiveStruct);
		});
	OutgameServer::Instance().RegisterPacketHanlder(PacketID::C2S_ACCEPT_FRIEND_REQUEST, [this](std::shared_ptr<ReceiveStruct> receiveStruct)
		{
			HandleAcceptFriendRequest(receiveStruct);
		});
	OutgameServer::Instance().RegisterPacketHanlder(PacketID::C2S_REFUSE_FRIEND_REQUEST, [this](std::shared_ptr<ReceiveStruct> receiveStruct)
		{
			HandleRefuseFriendRequest(receiveStruct);
		});
	OutgameServer::Instance().RegisterPacketHanlder(PacketID::C2S_DEL_FRIEND_REQUEST, [this](std::shared_ptr<ReceiveStruct> receiveStruct)
		{
			HandleDeleteFriendRequest(receiveStruct);
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
	LOG_CONTENTS("로그아웃 성공");

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

void UserManager::HandleAddFriendRequest(std::shared_ptr<ReceiveStruct> receiveStructure)
{
	auto addFriendRequest = std::make_shared<Protocol::C2S_AddFriendRequest>();
	if (!PacketBuilder::Instance().DeserializeData(receiveStructure->data, receiveStructure->nReceivedByte, *(receiveStructure->header), *addFriendRequest))
	{
		LOG_CONTENTS("C2S_ADD_FRIEND_REQUEST: PakcetBuilder::Deserialize() failed");
		return;
	}

	std::shared_ptr<SendStruct> sendStruct = std::make_shared<SendStruct>();
	// packetID
	sendStruct->type = ESendType::UNICAST;
	// session
	sendStruct->session = receiveStructure->session;
	// data
	std::shared_ptr<Protocol::S2C_AddFriendResponse> response = std::make_shared<Protocol::S2C_AddFriendResponse>();
	if (AddFriend(FindActiveUser(sendStruct->session->GetSessionId()).lock()->GetName(), addFriendRequest->username()))
	{
		response->mutable_success()->set_value(true);
	}
	else
	{
		response->mutable_success()->set_value(false);
	}

	sendStruct->data = response;
	// header
	std::string serializedString;
	(sendStruct->data)->SerializeToString(&serializedString);
	sendStruct->header = std::make_shared<PacketHeader>(PacketBuilder::Instance().CreateHeader(PacketID::S2C_ADD_FRIEND_RESPONSE, serializedString.size()));

	OutgameServer::Instance().InsertSendTask(sendStruct);
}

void UserManager::HandleCancelAddFriendRequest(std::shared_ptr<ReceiveStruct> receiveStructure)
{
	auto cancelAddFriendRequest = std::make_shared<Protocol::C2S_CancelAddFriendRequest>();
	if (!PacketBuilder::Instance().DeserializeData(receiveStructure->data, receiveStructure->nReceivedByte, *(receiveStructure->header), *cancelAddFriendRequest))
	{
		LOG_CONTENTS("C2S_CANCEL_ADD_FRIEND_REQUEST: PakcetBuilder::Deserialize() failed");
		return;
	}

	std::shared_ptr<SendStruct> sendStruct = std::make_shared<SendStruct>();
	// packetID
	sendStruct->type = ESendType::UNICAST;
	// session
	sendStruct->session = receiveStructure->session;
	// data
	std::shared_ptr<Protocol::S2C_CancelAddFriendResponse> response = std::make_shared<Protocol::S2C_CancelAddFriendResponse>();
	if (CancelAddFriend(FindActiveUser(sendStruct->session->GetSessionId()).lock()->GetName(), cancelAddFriendRequest->username()))
	{
		response->mutable_success()->set_value(true);
	}
	else
	{
		response->mutable_success()->set_value(false);
	}

	sendStruct->data = response;
	// header
	std::string serializedString;
	(sendStruct->data)->SerializeToString(&serializedString);
	sendStruct->header = std::make_shared<PacketHeader>(PacketBuilder::Instance().CreateHeader(PacketID::S2C_CANCEL_ADD_FRIEND_RESPONSE, serializedString.size()));

	OutgameServer::Instance().InsertSendTask(sendStruct);
}

void UserManager::HandleAcceptFriendRequest(std::shared_ptr<ReceiveStruct> receiveStructure)
{
	auto acceptFriendRequest = std::make_shared<Protocol::C2S_AcceptFriendRequest>();
	if (!PacketBuilder::Instance().DeserializeData(receiveStructure->data, receiveStructure->nReceivedByte, *(receiveStructure->header), *acceptFriendRequest))
	{
		LOG_CONTENTS("C2S_ACCEPT_FRIEND_REQUEST: PakcetBuilder::Deserialize() failed");
		return;
	}

	std::shared_ptr<SendStruct> sendStruct = std::make_shared<SendStruct>();
	// packetID
	sendStruct->type = ESendType::UNICAST;
	// session
	sendStruct->session = receiveStructure->session;
	// data
	std::shared_ptr<Protocol::S2C_AcceptFriendResponse> response = std::make_shared<Protocol::S2C_AcceptFriendResponse>();
	int friendState;
	if (AcceptFriend(FindActiveUser(sendStruct->session->GetSessionId()).lock()->GetName(), acceptFriendRequest->acceptedusername(), friendState))
	{
		response->mutable_success()->set_value(true);
		response->mutable_newfriendinfo()->set_username(acceptFriendRequest->acceptedusername());
		response->mutable_newfriendinfo()->set_state(friendState);
	}
	else
	{
		response->mutable_success()->set_value(false);
	}

	sendStruct->data = response;
	// header
	std::string serializedString;
	(sendStruct->data)->SerializeToString(&serializedString);
	sendStruct->header = std::make_shared<PacketHeader>(PacketBuilder::Instance().CreateHeader(PacketID::S2C_ACCEPT_FRIEND_RESPONSE, serializedString.size()));

	OutgameServer::Instance().InsertSendTask(sendStruct);
}

void UserManager::HandleRefuseFriendRequest(std::shared_ptr<ReceiveStruct> receiveStructure)
{
	auto refuseFriendRequest = std::make_shared<Protocol::C2S_RefuseFriendRequest>();
	if (!PacketBuilder::Instance().DeserializeData(receiveStructure->data, receiveStructure->nReceivedByte, *(receiveStructure->header), *refuseFriendRequest))
	{
		LOG_CONTENTS("C2S_REFUSE_FRIEND_REQUEST: PakcetBuilder::Deserialize() failed");
		return;
	}

	std::shared_ptr<SendStruct> sendStruct = std::make_shared<SendStruct>();
	// packetID
	sendStruct->type = ESendType::UNICAST;
	// session
	sendStruct->session = receiveStructure->session;
	// data
	std::shared_ptr<Protocol::S2C_RefuseFriendResponse> response = std::make_shared<Protocol::S2C_RefuseFriendResponse>();
	if (RefuseFriend(FindActiveUser(sendStruct->session->GetSessionId()).lock()->GetName(), refuseFriendRequest->refusedusername()))
	{
		response->mutable_success()->set_value(true);
		response->set_refusedusername(refuseFriendRequest->refusedusername());
	}
	else
	{
		response->mutable_success()->set_value(false);
	}

	sendStruct->data = response;
	// header
	std::string serializedString;
	(sendStruct->data)->SerializeToString(&serializedString);
	sendStruct->header = std::make_shared<PacketHeader>(PacketBuilder::Instance().CreateHeader(PacketID::S2C_REFUSE_FRIEND_RESPONSE, serializedString.size()));

	OutgameServer::Instance().InsertSendTask(sendStruct);
}

void UserManager::HandleDeleteFriendRequest(std::shared_ptr<ReceiveStruct> receiveStructure)
{
	auto deleteFriendRequest = std::make_shared<Protocol::C2S_DelFriendRequest>();
	if (!PacketBuilder::Instance().DeserializeData(receiveStructure->data, receiveStructure->nReceivedByte, *(receiveStructure->header), *deleteFriendRequest))
	{
		LOG_CONTENTS("C2S_DEL_FRIEND_REQUEST: PakcetBuilder::Deserialize() failed");
		return;
	}

	std::shared_ptr<SendStruct> sendStruct = std::make_shared<SendStruct>();
	// packetID
	sendStruct->type = ESendType::UNICAST;
	// session
	sendStruct->session = receiveStructure->session;
	// data
	std::shared_ptr<Protocol::S2C_DelFriendResponse> response = std::make_shared<Protocol::S2C_DelFriendResponse>();
	if (DeleteFriend(FindActiveUser(sendStruct->session->GetSessionId()).lock()->GetName(), deleteFriendRequest->friendname()))
	{
		response->mutable_success()->set_value(true);
		response->set_delfriendname(deleteFriendRequest->friendname());
	}
	else
	{
		response->mutable_success()->set_value(false);
	}

	sendStruct->data = response;
	// header
	std::string serializedString;
	(sendStruct->data)->SerializeToString(&serializedString);
	sendStruct->header = std::make_shared<PacketHeader>(PacketBuilder::Instance().CreateHeader(PacketID::S2C_DEL_FRIEND_RESPONSE, serializedString.size()));

	OutgameServer::Instance().InsertSendTask(sendStruct);
}

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
		DBConnectionPool::Instance().ReturnConnection(dbConn);
		return;
	}

	while(getFriendBind.Fetch() == DB_RESULT::SUCCESS)
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
		DBConnectionPool::Instance().ReturnConnection(dbConn);
		return;
	}

	while(getPendingBind.Fetch() == DB_RESULT::SUCCESS)
	{
		std::wstring wFriendName(outFriendName);
		ZeroMemory(outFriendName, 256);
		std::string friendName(wFriendName.begin(), wFriendName.end());

		user->AppendPendingUser(friendName, static_cast<EUserState>(outState));
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

	if (loginRequestBind.Fetch() != DB_RESULT::SUCCESS)
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

	if (joinRequestBind.Fetch() != DB_RESULT::DB_NO_DATA)
	{
		LOG_CONTENTS("중복된 ID");
		DBConnectionPool::Instance().ReturnConnection(dbConn);
		return false;
	}

	DBConnectionPool::Instance().ReturnConnection(dbConn);
#endif
	return true;
}

bool UserManager::FindUser(const std::string_view& username, const std::string_view& friendName, int& friendState, int& requestState)
{
	if (username == friendName)
		return false;

#ifdef DB_INCLUDE_VERSION
	// 유저 상태 쿼리
	DBConnection* dbConn = DBConnectionPool::Instance().GetConnection();

	DBBind<1, 1> findStateBind(dbConn, L"SELECT State FROM [dbo].[User] WHERE Nickname = (?)");

	std::wstring wFriendName = std::wstring(friendName.begin(), friendName.end());
	findStateBind.BindParam(0, wFriendName.c_str(), wFriendName.size());
	findStateBind.BindCol(0, friendState);

	if (!findStateBind.Execute())
	{
		DBConnectionPool::Instance().ReturnConnection(dbConn);
		return false;
	}
	if (findStateBind.Fetch() != DB_RESULT::SUCCESS)
	{
		DBConnectionPool::Instance().ReturnConnection(dbConn);
		return false;
	}

	// 친구 신청 상태 쿼리
	DBBind<2, 1> requestStateBind(dbConn, L"SELECT IsMutualFriend FROM [dbo].[Friends] WHERE UserName = (?) AND FriendName = (?)");

	std::wstring wUserName = std::wstring(username.begin(), username.end());
	bool bMutual;
	requestStateBind.BindParam(0, wUserName.c_str(), wUserName.size());
	requestStateBind.BindParam(1, wFriendName.c_str(), wFriendName.size());
	requestStateBind.BindCol(0, bMutual);

	if (!requestStateBind.Execute())
	{
		DBConnectionPool::Instance().ReturnConnection(dbConn);
		return false;
	}

	DB_RESULT rt = requestStateBind.Fetch();
	if (rt == DB_RESULT::FAILED)
	{
		DBConnectionPool::Instance().ReturnConnection(dbConn);
		return false;
	}

	if (rt == DB_RESULT::SUCCESS)
	{
		if (bMutual)
			requestState = ERequestState::ALREADY_FRIEND;
		else
			requestState = ERequestState::ALREADY_REQUEST;
	}
	else if(rt == DB_RESULT::DB_NO_DATA)
	{
		requestState = ERequestState::NON;
	}
	DBConnectionPool::Instance().ReturnConnection(dbConn);
#endif
	return true;
}

bool UserManager::AddFriend(const std::string_view& username, const std::string_view& friendName)
{
#ifdef DB_INCLUDE_VERSION
	// DB 테이블에 새로운 항목 추가
	DBConnection* dbConn = DBConnectionPool::Instance().GetConnection();

	DBBind<2, 0> insertFriendBind(dbConn, L"INSERT INTO [dbo].[Friends]([UserName], [FriendName], [IsMutualFriend]) VALUES(? , ? , 0)");
	std::wstring wUserName = std::wstring(username.begin(), username.end());
	std::wstring wFriendName = std::wstring(friendName.begin(), friendName.end());
	insertFriendBind.BindParam(0, wUserName.c_str(), wUserName.size());
	insertFriendBind.BindParam(1, wFriendName.c_str(), wFriendName.size());
	if (!insertFriendBind.Execute())
	{
		DBConnectionPool::Instance().ReturnConnection(dbConn);
		return false;
	}

	DBConnectionPool::Instance().ReturnConnection(dbConn);
#endif

	// 상대가 활성 상태라면, 상대 유저 정보 수정 및 AddFriendNotification 발송
	if (std::shared_ptr<User> otherUser = FindActiveUser(friendName).lock())
	{
		otherUser->AppendPendingUser(username, EUserState::ONLINE);

		std::shared_ptr<SendStruct> sendStruct = std::make_shared<SendStruct>();
		// packetID
		sendStruct->type = ESendType::UNICAST;
		// session
		sendStruct->session = otherUser->GetSession();
		// data
		std::shared_ptr<Protocol::S2O_AddFriendNotification> response = std::make_shared<Protocol::S2O_AddFriendNotification>();
		response->set_username(std::string(username.begin(), username.end()));
		sendStruct->data = response;
		// header
		std::string serializedString;
		(sendStruct->data)->SerializeToString(&serializedString);
		sendStruct->header = std::make_shared<PacketHeader>(PacketBuilder::Instance().CreateHeader(PacketID::S2O_ADD_FRIEND_NOTIFICATION, serializedString.size()));

		OutgameServer::Instance().InsertSendTask(sendStruct);
	}

	return true;
}

bool UserManager::CancelAddFriend(const std::string_view& username, const std::string_view& friendName)
{
#ifdef DB_INCLUDE_VERSION
	// 신청한 내역 삭제
	DBConnection* dbConn = DBConnectionPool::Instance().GetConnection();

	DBBind<2, 0> cancelRequestBind(dbConn, L"DELETE FROM [dbo].[Friends] WHERE UserName = (?) AND FriendName = (?)");
	std::wstring wUserName = std::wstring(username.begin(), username.end());
	std::wstring wFriendName = std::wstring(friendName.begin(), friendName.end());
	cancelRequestBind.BindParam(0, wUserName.c_str(), wUserName.size());
	cancelRequestBind.BindParam(1, wFriendName.c_str(), wFriendName.size());
	if (!cancelRequestBind.Execute())
	{
		LOG_DB("cancelRequestBind Execute Failed");
		DBConnectionPool::Instance().ReturnConnection(dbConn);
		return false;
	}

	DBConnectionPool::Instance().ReturnConnection(dbConn);
#endif

	// 상대가 활성 상태라면, 상대 유저 정보 수정 및 CancelAddFriendNotification 발송
	if (std::shared_ptr<User> otherUser = FindActiveUser(friendName).lock())
	{
		otherUser->RemovePendingUser(username);

		std::shared_ptr<SendStruct> sendStruct = std::make_shared<SendStruct>();
		// packetID
		sendStruct->type = ESendType::UNICAST;
		// session
		sendStruct->session = otherUser->GetSession();
		// data
		std::shared_ptr<Protocol::S2O_CancelAddFriendNotification> notification = std::make_shared<Protocol::S2O_CancelAddFriendNotification>();
		notification->set_username(std::string(username.begin(), username.end()));
		sendStruct->data = notification;
		// header
		std::string serializedString;
		(sendStruct->data)->SerializeToString(&serializedString);
		sendStruct->header = std::make_shared<PacketHeader>(PacketBuilder::Instance().CreateHeader(PacketID::S2O_CANCEL_ADD_FRIEND_NOTIFICATION, serializedString.size()));

		OutgameServer::Instance().InsertSendTask(sendStruct);
	}

	return true;
}

bool UserManager::AcceptFriend(const std::string_view& username, const std::string_view& friendName, OUT int& friendState)
{
#ifdef DB_INCLUDE_VERSION
	// DB 테이블에 새 항목 추가 및 IsMutualFriend 수정
	DBConnection* dbConn = DBConnectionPool::Instance().GetConnection();

	// 새로운 항목 추가
	DBBind<2, 0> insertFriendBind(dbConn, L"INSERT INTO　[dbo].[Friends]([UserName], [FriendName], [IsMutualFriend]) VALUES(? , ? , 1)");
	std::wstring wUserName = std::wstring(username.begin(), username.end());
	std::wstring wFriendName = std::wstring(friendName.begin(), friendName.end());
	insertFriendBind.BindParam(0, wUserName.c_str(), wUserName.size());
	insertFriendBind.BindParam(1, wFriendName.c_str(), wFriendName.size());
	if (!insertFriendBind.Execute())
	{
		DBConnectionPool::Instance().ReturnConnection(dbConn);
		LOG_DB("insertFriendBind Execute Failed");
		return false;
	}

	// 이미 신청된 내역의 IsMutualFriend 수정
	DBBind<2, 0> updateMutualBind(dbConn, L"UPDATE [dbo].[Friends] SET IsMutualFriend = 1 WHERE UserName = (?) AND FriendName = (?)");
	updateMutualBind.BindParam(0, wFriendName.c_str(), wFriendName.size());
	updateMutualBind.BindParam(1, wUserName.c_str(), wUserName.size());
	if(!updateMutualBind.Execute())
	{
		DBConnectionPool::Instance().ReturnConnection(dbConn);
		LOG_DB("updateMutualBind Execute Failed");
		return false;
	}

	// 상대 친구 상태 확인
	DBBind<1, 1> checkStateBind(dbConn, L"SELECT State FROM [dbo].[User] WHERE Nickname = (?)");
	checkStateBind.BindParam(0, wFriendName.c_str(), wFriendName.size());
	checkStateBind.BindCol(0, friendState);
	if(!checkStateBind.Execute())
	{
		DBConnectionPool::Instance().ReturnConnection(dbConn);
		LOG_DB("checkStateBind Execute Failed");
		return false;
	}
	if(checkStateBind.Fetch() != DB_RESULT::SUCCESS)
	{
		DBConnectionPool::Instance().ReturnConnection(dbConn);
		LOG_DB("checkStateBind Fetch Failed");
		return false;
	}

	DBConnectionPool::Instance().ReturnConnection(dbConn);
#endif
	// user의 펜딩 리스트 및 친구 리스트 수정
	std::shared_ptr<User> user = FindActiveUser(username).lock();
	if (!user)
		return false;
	user->RemovePendingUser(friendName);
	user->AppendFriend(friendName, static_cast<EUserState>(friendState));

	// 상대가 활성 상태라면 User 정보 수정 및 AcceptFriendNotification 발송
	if (std::shared_ptr<User> otherUser = FindActiveUser(friendName).lock())
	{
		// 친구의 친구 리스트에 추가
		otherUser->AppendFriend(username, EUserState::ONLINE);

		std::shared_ptr<SendStruct> sendStruct = std::make_shared<SendStruct>();
		// packetID
		sendStruct->type = ESendType::UNICAST;
		// session
		sendStruct->session = otherUser->GetSession();
		// data
		std::shared_ptr<Protocol::S2O_AcceptFriendNotification> response = std::make_shared<Protocol::S2O_AcceptFriendNotification>();
		response->mutable_newfriendinfo()->set_username(std::string(username.begin(), username.end()));
		response->mutable_newfriendinfo()->set_state(EUserState::ONLINE);
		sendStruct->data = response;
		// header
		std::string serializedString;
		(sendStruct->data)->SerializeToString(&serializedString);
		sendStruct->header = std::make_shared<PacketHeader>(PacketBuilder::Instance().CreateHeader(PacketID::S2O_ACCEPT_FRIEND_NOTIFICATION, serializedString.size()));

		OutgameServer::Instance().InsertSendTask(sendStruct);
	}

	return true;
}

bool UserManager::RefuseFriend(const std::string_view& username, const std::string_view& friendName)
{
#ifdef DB_INCLUDE_VERSION
	// 상대편의 신청 내역 삭제
	DBConnection* dbConn = DBConnectionPool::Instance().GetConnection();

	DBBind<2, 0> deleteRequestBind(dbConn, L"DELETE FROM [dbo].[Friends] WHERE UserName = (?) AND FriendName = (?)");
	std::wstring wUserName = std::wstring(username.begin(), username.end());
	std::wstring wFriendName = std::wstring(friendName.begin(), friendName.end());
	deleteRequestBind.BindParam(0, wFriendName.c_str(), wFriendName.size());
	deleteRequestBind.BindParam(1, wUserName.c_str(), wUserName.size());

	if(!deleteRequestBind.Execute())
	{
		LOG_DB("deleteRequestBind Execute Failed");
		DBConnectionPool::Instance().ReturnConnection(dbConn);
		return false;
	}

	DBConnectionPool::Instance().ReturnConnection(dbConn);
#endif
	return true;
}

bool UserManager::DeleteFriend(const std::string_view& username, const std::string_view& friendName)
{
#ifdef DB_INCLUDE_VERSION
	// 친구 내역 삭제
	DBConnection* dbConn = DBConnectionPool::Instance().GetConnection();

	DBBind<4, 0> deleteFriendBind(dbConn, L"DELETE FROM [dbo].[Friends] WHERE (UserName = (?) AND FriendName = (?)) OR (UserName = (?) AND FriendName = (?))");
	std::wstring wUserName = std::wstring(username.begin(), username.end());
	std::wstring wFriendName = std::wstring(friendName.begin(), friendName.end());
	deleteFriendBind.BindParam(0, wFriendName.c_str(), wFriendName.size());
	deleteFriendBind.BindParam(1, wUserName.c_str(), wUserName.size());
	deleteFriendBind.BindParam(2, wUserName.c_str(), wUserName.size());
	deleteFriendBind.BindParam(3, wFriendName.c_str(), wFriendName.size());

	if (!deleteFriendBind.Execute())
	{
		LOG_DB("deleteFriendBind Execute Failed");
		DBConnectionPool::Instance().ReturnConnection(dbConn);
		return false;
	}

	DBConnectionPool::Instance().ReturnConnection(dbConn);
#endif

	std::shared_ptr<User> user = FindActiveUser(username).lock();
	if (!user)
		return false;
	user->RemoveFriend(friendName);

	// 상대가 활성 상태라면 User 정보 수정 및 DelFriendNotification 발송
	if (std::shared_ptr<User> otherUser = FindActiveUser(friendName).lock())
	{
		// 친구의 친구 리스트에 추가
		otherUser->RemoveFriend(username);

		std::shared_ptr<SendStruct> sendStruct = std::make_shared<SendStruct>();
		// packetID
		sendStruct->type = ESendType::UNICAST;
		// session
		sendStruct->session = otherUser->GetSession();
		// data
		std::shared_ptr<Protocol::S2O_DelFriendNotification> response = std::make_shared<Protocol::S2O_DelFriendNotification>();
		response->set_delfriendname(std::string(username.begin(), username.end()));
		sendStruct->data = response;
		// header
		std::string serializedString;
		(sendStruct->data)->SerializeToString(&serializedString);
		sendStruct->header = std::make_shared<PacketHeader>(PacketBuilder::Instance().CreateHeader(PacketID::S2O_DEL_FRIEND_NOTIFICATION, serializedString.size()));

		OutgameServer::Instance().InsertSendTask(sendStruct);
	}

	return true;
}
