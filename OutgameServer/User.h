#pragma once

class GameRoom;
typedef unsigned int UserId;

/// 이후 USerPool로 관리될 예정
///	User 객체는 ONLINE, IN_GAME 상태일 때만 유효하게 관리한다.
class User
{
public:
	User(Session* session, std::string_view name);
	~User() = default;

	Session* GetSession() const { return m_session; }

	const UserId& GetId() const { return m_id; }
	const std::string& GetName() const { return m_name; }

	EUserState GetState() const { return m_state; }
	void UpdateState(EUserState state);

	//std::chrono::steady_clock::time_point GetLastValidationTime() const { return m_lastValidationTime; }
	//void UpdateLastValidationTime(std::chrono::steady_clock::time_point timePoint) { m_lastValidationTime = timePoint; }

	std::weak_ptr<GameRoom> GetActiveGameRoomRef() const { return m_activeGameRoomRef; }
	void SetActiveGameRoomRef(std::weak_ptr<GameRoom> gameRoom) { m_activeGameRoomRef = gameRoom; }
	void ResetActiveGameRoom() { m_activeGameRoomRef.reset(); }

	void AppendFriend(EUserState state, const std::string_view& friendName) { m_friendList[state].emplace_back(friendName); }
	void AppendPendingFriend(EUserState state, const std::string_view& appendingFriendName) { m_acceptPendingList[state].emplace_back(appendingFriendName); }

private:
	Session* m_session;

	UserId m_id;	// == sessionId
	std::string m_name;
	EUserState m_state;

	std::weak_ptr<GameRoom> m_activeGameRoomRef;

	// weak_ptr lock시 항상 유효성 확인할 것
	std::vector<std::vector<std::string>> m_friendList;	// EUserState, Username
	std::vector<std::vector<std::string>> m_acceptPendingList;
	
	//std::chrono::steady_clock::time_point m_lastValidationTime;

};

