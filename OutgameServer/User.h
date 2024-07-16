#pragma once

enum EUserStateType
{
	OFFLINE,
	ONLINE,
	IN_GAME
};

class User
{
public:
	User() = default;
	~User() = default;

	EUserStateType GetStatus() { return m_status; }
	void UpdateStatus(EUserStateType status);

	std::chrono::steady_clock::time_point GetLastValidationTime() { return m_lastValidationTime; }
	void UpdateLastValidationTime(std::chrono::steady_clock::time_point timePoint) { m_lastValidationTime = timePoint; }

private:
	Session* m_session;

	std::string m_name;
	std::string m_password;
	EUserStateType m_status;

	std::chrono::steady_clock::time_point m_lastValidationTime;

	// todo 친구 목록
};

