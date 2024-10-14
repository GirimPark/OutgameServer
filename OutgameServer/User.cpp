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

	// 활성 상태인 친구 목록 선별
	std::vector<Session*> activeFriends;
	for(const auto& friendInfo : m_friendList)
	{
		if(friendInfo.second != EUserState::OFFLINE)
		{
			activeFriends.emplace_back(UserManager::FindActiveUser(friendInfo.first).lock()->GetSession());
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
}
