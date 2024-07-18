#pragma once

enum EUserStateType
{
	OFFLINE,
	ONLINE,
	IN_GAME
};

/// 이후 USerPool로 관리될 예정
///	User 객체는 ONLINE, IN_GAME 상태일 때만 유효하게 관리한다.
class User
{
public:
	User(Session* session, std::string_view name);
	~User() = default;

	Session* GetSession() const { return m_session; }

	EUserStateType GetState() const { return m_state; }
	void UpdateState(EUserStateType state);

	std::chrono::steady_clock::time_point GetLastValidationTime() { return m_lastValidationTime; }
	void UpdateLastValidationTime(std::chrono::steady_clock::time_point timePoint) { m_lastValidationTime = timePoint; }

private:
	Session* m_session;

	std::string m_name;
	EUserStateType m_state;

	std::chrono::steady_clock::time_point m_lastValidationTime;

	// todo 친구 목록
};

