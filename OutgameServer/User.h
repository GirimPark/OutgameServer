#pragma once

class GameRoom;

typedef unsigned int UserId;

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

	const UserId& GetId() const { return m_id; }
	const std::string& GetName() const { return m_name; }

	EUserStateType GetState() const { return m_state; }
	void UpdateState(EUserStateType state);

	std::chrono::steady_clock::time_point GetLastValidationTime() const { return m_lastValidationTime; }
	void UpdateLastValidationTime(std::chrono::steady_clock::time_point timePoint) { m_lastValidationTime = timePoint; }

	std::weak_ptr<GameRoom> GetActiveGameRoomRef() const { return m_activeGameRoomRef; }
	void SetActiveGameRoomRef(std::weak_ptr<GameRoom> gameRoom) { m_activeGameRoomRef = gameRoom; }
	void ResetActiveGameRoom() { m_activeGameRoomRef.reset(); }

private:
	Session* m_session;

	UserId m_id;
	std::string m_name;
	EUserStateType m_state;

	std::chrono::steady_clock::time_point m_lastValidationTime;

	std::weak_ptr<GameRoom> m_activeGameRoomRef;
	// todo ģ�� ���
};

