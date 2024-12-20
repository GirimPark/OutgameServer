#pragma once

class GameRoom;
typedef unsigned int UserId;

/// 이후 USerPool로 관리될 예정
///	User 객체는 ONLINE, IN_GAME 상태일 때만 유효하게 관리한다.
class User
{
public:
	User(Session* session, std::string_view name);
	~User();

	Session* GetSession() const { return m_session; }

	const UserId& GetId() const { return m_id; }
	const std::string& GetName() const { return m_name; }

	EUserState GetState() const { return m_state; }
	void UpdateState(EUserState state);

	std::weak_ptr<GameRoom> GetActiveGameRoomRef() const { return m_activeGameRoomRef; }
	void SetActiveGameRoomRef(std::weak_ptr<GameRoom> gameRoom);
	void ResetActiveGameRoom();

	void AppendFriend(const std::string_view& friendName, EUserState state);
	void AppendPendingUser(const std::string_view& userName, EUserState state);
	void RemoveFriend(const std::string_view& friendName);
	void RemovePendingUser(const std::string_view& userName);


	const std::unordered_map<std::string, EUserState>& GetFriendListRef() const { return m_friendList; }
	const std::unordered_map<std::string, EUserState>& GetPendingListRef() const { return m_acceptPendingList; }

private:
	Session* m_session;

	UserId m_id;	// == sessionId
	std::string m_name;
	EUserState m_state;

	std::weak_ptr<GameRoom> m_activeGameRoomRef;

	// weak_ptr lock시 항상 유효성 확인할 것
	std::unordered_map<std::string, EUserState> m_friendList;
	std::unordered_map<std::string, EUserState> m_acceptPendingList;

	CRITICAL_SECTION m_userInfoLock;
};

