#pragma once

enum EUserStateType
{
	OFFLINE,
	ONLINE,
	IN_GAME
};

/// ���� USerPool�� ������ ����
///	User ��ü�� ONLINE, IN_GAME ������ ���� ��ȿ�ϰ� �����Ѵ�.
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

	// todo ģ�� ���
};

