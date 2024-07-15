#include "pch.h"
#include "UserManager.h"

#include "User.h"

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

	if(!loginRequestBind.Fetch())
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
