#include "pch.h"
#include "GameRoom.h"

#include "User.h"

GameRoom::GameRoom(std::shared_ptr<User> hostPlayer) : m_hostPlayer(hostPlayer)
{
	static unsigned int roomIdCnt = 0;
	m_roomId = roomIdCnt++;
	GenerateRoomCode();

	m_roomState = ERoomStateType::WAIT;

	std::shared_ptr<User> hostPlayerRef = m_hostPlayer.lock();

	char addr[INET_ADDRSTRLEN];
	m_hostIpAddress = inet_ntop(AF_INET, &hostPlayerRef->GetSession()->GetClientIP().sin_addr, addr, INET_ADDRSTRLEN);

	m_players.insert({ hostPlayerRef->GetName(), m_hostPlayer });
}

GameRoom::~GameRoom()
{
	m_players.clear();
}

bool GameRoom::Enter(std::weak_ptr<User> playerRef)
{
	if (m_players.size() >= MAX_PLAYER_NUM)
		return false;

	std::shared_ptr<User> player = playerRef.lock();

	player->SetActiveGameRoomRef(shared_from_this());
	m_players.insert({ player->GetName(), player });

	return true;
}

void GameRoom::Quit(std::weak_ptr<User> playerRef)
{
	std::shared_ptr<User> player = playerRef.lock();

	player->ResetActiveGameRoom();
	if(player == m_hostPlayer.lock())
	{
		m_roomState = ERoomStateType::DESTROYING;
		//m_players.erase()
	}
	else
	{
		m_players.erase(player->GetName());
	}

}

void GameRoom::GenerateRoomCode()
{
	m_roomCode = std::to_string(m_roomId);
}
