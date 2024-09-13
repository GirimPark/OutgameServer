#include "pch.h"
#include "User.h"
#include "GameRoom.h"

User::User(Session* session, std::string_view name)
	: m_session(session)
	, m_id(session->GetSessionId())
	, m_name(name)
	, m_state(EUserStateType::ONLINE)
	, m_lastValidationTime(std::chrono::steady_clock::now())

{
}

void User::UpdateState(EUserStateType state)
{
	m_state = state;

	std::wstring wUsername = std::wstring(m_name.begin(), m_name.end());
	int dbState = state;
	DBConnection* dbConn = DBConnectionPool::Instance().GetConnection();
	DBBind<2, 0> updateStateBind(dbConn, L"UPDATE [dbo].[User] SET state = (?) WHERE username = (?)");
	updateStateBind.BindParam(0,  dbState);
	updateStateBind.BindParam(1, wUsername.c_str(), wUsername.size());
	ASSERT_CRASH(updateStateBind.Execute());
	DBConnectionPool::Instance().ReturnConnection(dbConn);
}
