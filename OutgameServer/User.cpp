#include "pch.h"
#include "User.h"

User::User(Session* session, std::string_view name)
	: m_session(session)
	, m_name(name)
	, m_state(EUserStateType::ONLINE)
	, m_lastValidationTime(std::chrono::steady_clock::now())

{
}

// todo 풀에 반환하는 형태로 변경
User::~User()
{
	OutgameServer::Instance().GetServerCore()->UnregisterSession(m_session->sessionId);
}

void User::UpdateStatus(EUserStateType state)
{
	m_state = state;

	//todo DBUpdate
	std::wstring wUsername = std::wstring(m_name.begin(), m_name.end());
	int dbState = state;
	DBConnection* dbConn = DBConnectionPool::Instance().GetConnection();
	DBBind<2, 0> updateStateBind(dbConn, L"UPDATE [dbo].[User] SET state = (?) WHERE username = (?)");
	updateStateBind.BindParam(0,  dbState);
	updateStateBind.BindParam(1, wUsername.c_str(), wUsername.size());
	assert(updateStateBind.Execute());
	DBConnectionPool::Instance().ReturnConnection(dbConn);
}
