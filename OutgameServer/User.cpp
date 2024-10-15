#include "pch.h"
#include "User.h"

#include "UserManager.h"

User::User(Session* session, std::string_view name)
	: m_session(session)
	, m_id(session->GetSessionId())
	, m_name(name)
	, m_state(EUserState::ONLINE)

{
}

void User::UpdateState(EUserState state)
{
#ifdef DB_INCLUDE_VERSION
	m_state = state;

	std::wstring wUsername = std::wstring(m_name.begin(), m_name.end());
	int dbState = state;
	DBConnection* dbConn = DBConnectionPool::Instance().GetConnection();
	DBBind<2, 0> updateStateBind(dbConn, L"UPDATE [dbo].[User] SET State = (?) WHERE Nickname = (?)");
	updateStateBind.BindParam(0,  dbState);
	updateStateBind.BindParam(1, wUsername.c_str(), wUsername.size());
	ASSERT_CRASH(updateStateBind.Execute());
	DBConnectionPool::Instance().ReturnConnection(dbConn);
#endif

	// Ȱ�� ������ ģ�� ��� ����
	std::vector<Session*> activeFriends;
	for(const auto& friendInfo : m_friendList)
	{
		if(friendInfo.second != EUserState::OFFLINE)
		{
			activeFriends.emplace_back(UserManager::FindActiveUser(friendInfo.first).lock()->GetSession());
		}
	}

	// ���� ���濡 ���� ģ���鿡�� ��Ŷ �۽�
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
}

void User::RemoveFriend(const std::string_view& friendName)
{
	auto it = m_friendList.find(std::string(friendName.begin(), friendName.end()));
	if(it == m_friendList.end())
	{
		LOG_CONTENTS("RemoveFriend Failed: FriendName is Invalid");
		return;
	}
	m_friendList.erase(it);
}

void User::RemovePendingUser(const std::string_view& userName)
{
	auto it = m_acceptPendingList.find(std::string(userName.begin(), userName.end()));
	if (it == m_acceptPendingList.end())
	{
		LOG_CONTENTS("RemovePendingUser Failed: UserName is Invalid");
		return;
	}
	m_acceptPendingList.erase(it);
}
