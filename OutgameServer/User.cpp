#include "pch.h"
#include "User.h"

User::User(Session* session, std::string_view name)
	: m_session(session)
	, m_id(session->GetSessionId())
	, m_name(name)
	, m_state(EUserState::ONLINE)

{
	m_friendList.resize(EUserState::END);
	m_acceptPendingList.resize(EUserState::END);
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
}
