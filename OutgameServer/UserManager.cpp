#include "pch.h"
#include "UserManager.h"

#include "User.h"

bool UserManager::AuthenticateUser(const std::string_view& username, const std::string_view& password)
{
	// UserDB ��ȸ, validation Ȯ��
	std::shared_ptr<DBConnection> dbConn = DBConnectionPool::Instance().GetConnection();

	DBBind<1, 2> loginRequestBind(dbConn, L"SELECT password, status FROM [dbo].[User] WHERE username = (?)");

	std::wstring wUsername = std::wstring(username.begin(), username.end());
	loginRequestBind.BindParam(0, wUsername.c_str(), wUsername.size());

	// todo : db���� password�� �̻��ϰ� �о��
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
		LOG_CONTENTS("��ȿ���� ���� ID");
		return false;
	}

	// ��ȿ�� id. password Ȯ��
	std::wstring dbPassword(outPassword);
	std::wstring inputPassword(password.begin(), password.end());
	if (dbPassword == inputPassword)
	{
		// �α��� ������ Ȯ��
		if (outStatus == EUserStateType::ONLINE || outStatus == EUserStateType::IN_GAME)
		{
			LOG_CONTENTS("�α��� ���� ����");
			return false;
		}
		else
		{
			// DB Update �� ���� ��ȯ
			DBBind<1, 0> updateStatusBind(dbConn, L"UPDATE [dbo].[User] SET status = 1 WHERE username = (?)");
			updateStatusBind.BindParam(0, wUsername.c_str(), wUsername.size());
			assert(updateStatusBind.Execute());
			LOG_CONTENTS("�α��� ����");

			return true;
		}
	}
	else
	{
		LOG_CONTENTS("��ȿ���� ���� Password");
		return false;
	}
	
}
