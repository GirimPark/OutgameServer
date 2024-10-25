#pragma once

#define MAX_PLAYER_NUM 4

class User;

enum class ERoomStateType
{
	WAIT,
	IN_GAME,
	DESTROYING,

	NONE
};

class GameRoom : public std::enable_shared_from_this<GameRoom>
{
public:
	GameRoom(std::shared_ptr<User> hostPlayer);
	~GameRoom();

	const std::string& GetRoomCode() const { return m_roomCode; }
	const std::string& GetRoomIpAddress() const { return m_hostIpAddress; }
	const ERoomStateType& GetRoomState() const { return m_roomState; }
	std::weak_ptr<User> GetHostPlayer() const { return m_hostPlayer; }

	void SetRoomState(ERoomStateType roomState) { m_roomState = roomState; }

	bool Enter(std::weak_ptr<User> playerRef);
	void Quit(std::weak_ptr<User> playerRef);

	void RegenerateRoomCode();

private:
	void GenerateRoomCode();

private:
	std::string m_roomCode;
	ERoomStateType m_roomState = ERoomStateType::NONE;

	std::weak_ptr<User> m_hostPlayer;
	std::string m_hostIpAddress;

	std::unordered_map<std::string, std::weak_ptr<User>> m_players;
};