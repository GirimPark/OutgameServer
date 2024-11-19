#include "pch.h"
#include "User.h"

#include "UserManager.h"

User::User(Session* session, std::string_view name)
	: m_session(session)
	, m_id(session->GetSessionId())
	, m_name(name)
	, m_state(EUserState::ONLINE)

{
	InitializeCriticalSection(&m_userInfoLock);
}

User::~User()
{
	DeleteCriticalSection(&m_userInfoLock);
}

void User::UpdateState(EUserState state)
{
	EnterCriticalSection(&m_userInfoLock);
	m_state = state;

#ifdef DB_INCLUDE_VERSION
	std::wstring wUsername = std::wstring(m_name.begin(), m_name.end());
	int dbState = state;
	DBConnection* dbConn = DBConnectionPool::Instance().GetConnection();
	DBBind<2, 0> updateStateBind(dbConn, L"UPDATE [dbo].[User] SET State = (?) WHERE Nickname = (?)");
	updateStateBind.BindParam(0,  dbState);
	updateStateBind.BindParam(1, wUsername.c_str(), wUsername.size());
	ASSERT_CRASH(updateStateBind.Execute());
	DBConnectionPool::Instance().ReturnConnection(dbConn);
#endif

	// 활성 상태인 친구 목록 선별
	std::vector<Session*> activeFriends;
	for(const auto& friendInfo : m_friendList)
	{
		if(friendInfo.second != EUserState::OFFLINE)
		{
			std::shared_ptr<User> curUser = UserManager::FindActiveUser(friendInfo.first).lock();
			if(curUser)
				activeFriends.emplace_back(curUser->GetSession());
		}
	}

	// 상태 변경에 대해 친구들에게 패킷 송신
	for(const auto& session : activeFriends)
	{
		std::shared_ptr<SendStruct> sendStruct = std::make_shared<SendStruct>();
		// packetID
		sendStruct->type = ESendType::UNICAST;
		// session
		sendStruct->session = session;
		// data
		std::shared_ptr<Protocol::S2O_UpdateStateNotification> response = std::make_shared<Protocol::S2O_UpdateStateNotification>();
		response->mutable_friendinfo()->set_username(m_name);
		response->mutable_friendinfo()->set_state(state);
		sendStruct->data = response;
		// header
		std::string serializedString;
		(sendStruct->data)->SerializeToString(&serializedString);
		sendStruct->header = std::make_shared<PacketHeader>(PacketBuilder::Instance().CreateHeader(PacketID::S2O_UPDATE_STATE_NOTIFICATION, serializedString.size()));

		OutgameServer::Instance().InsertSendTask(sendStruct);
	}
	LeaveCriticalSection(&m_userInfoLock);
}

void User::SetActiveGameRoomRef(std::weak_ptr<GameRoom> gameRoom)
{
	EnterCriticalSection(&m_userInfoLock);
	m_activeGameRoomRef = gameRoom;
	LeaveCriticalSection(&m_userInfoLock);
}

void User::ResetActiveGameRoom()
{
	EnterCriticalSection(&m_userInfoLock);
	m_activeGameRoomRef.reset();
	LeaveCriticalSection(&m_userInfoLock);
}

void User::AppendFriend(const std::string_view& friendName, EUserState state)
{
	EnterCriticalSection(&m_userInfoLock);
	m_friendList.insert({ std::string(friendName.begin(), friendName.end()), state });
	LeaveCriticalSection(&m_userInfoLock);
}

void User::AppendPendingUser(const std::string_view& userName, EUserState state)
{
	EnterCriticalSection(&m_userInfoLock);
	m_acceptPendingList.insert({ std::string(userName.begin(), userName.end()), state });
	LeaveCriticalSection(&m_userInfoLock);
}

void User::RemoveFriend(const std::string_view& friendName)
{
	EnterCriticalSection(&m_userInfoLock);
	auto it = m_friendList.find(std::string(friendName.begin(), friendName.end()));
	if(it == m_friendList.end())
	{
		PRINT_CONTENTS("RemoveFriend Failed: FriendName is Invalid");
		return;
	}
	m_friendList.erase(it);
	LeaveCriticalSection(&m_userInfoLock);
}

void User::RemovePendingUser(const std::string_view& userName)
{
	EnterCriticalSection(&m_userInfoLock);
	auto it = m_acceptPendingList.find(std::string(userName.begin(), userName.end()));
	if (it == m_acceptPendingList.end())
	{
		PRINT_CONTENTS("RemovePendingUser Failed: UserName is Invalid");
		return;
	}
	m_acceptPendingList.erase(it);
	LeaveCriticalSection(&m_userInfoLock);
}
