#include "pch.h"
#include "GameRoom.h"

#include "User.h"
#include <random>

GameRoom::GameRoom(std::shared_ptr<User> hostPlayer) : m_hostPlayer(hostPlayer)
{
	GenerateRoomCode();

	m_roomState = ERoomStateType::WAIT;

	std::shared_ptr<User> hostPlayerRef = m_hostPlayer.lock();

	char addr[INET_ADDRSTRLEN];
	m_hostIpAddress = inet_ntop(AF_INET, &hostPlayerRef->GetSession()->GetClientIP().sin_addr, addr, INET_ADDRSTRLEN);

	m_players.insert({ hostPlayerRef->GetName(), m_hostPlayer });
}

GameRoom::~GameRoom()
{
	for(auto& playerIter : m_players)
	{
		if(std::shared_ptr<User> user = playerIter.second.lock())
			user->ResetActiveGameRoom();
	}

	m_players.clear();
}

void GameRoom::SetRoomState(ERoomStateType roomState)
{
	m_roomState = roomState;

	if (m_roomState == ERoomStateType::IN_GAME)
	{
		for (auto& playerIter : m_players)
		{
			std::shared_ptr<User> player = playerIter.second.lock();
			if (player)
				player->UpdateState(EUserState::IN_GAME);
		}
	}
	else if(m_roomState == ERoomStateType::WAIT)
	{
		for (auto& playerIter : m_players)
		{
			std::shared_ptr<User> player = playerIter.second.lock();
			if (player)
				player->UpdateState(EUserState::ONLINE);
		}
	}
}

bool GameRoom::Enter(std::weak_ptr<User> playerRef)
{
	if (m_players.size() >= MAX_PLAYER_NUM)
		return false;

	std::shared_ptr<User> player = playerRef.lock();
	if (!player)
		return false;
	player->SetActiveGameRoomRef(shared_from_this());
	m_players.insert({ player->GetName(), player });

	return true;
}

void GameRoom::Quit(std::weak_ptr<User> playerRef)
{
	std::shared_ptr<User> player = playerRef.lock();
	if (!player)
		return;
	player->ResetActiveGameRoom();

	if((m_hostPlayer.lock()) && (player == m_hostPlayer.lock()))
	{
		m_roomState = ERoomStateType::DESTROYING;
	}
	m_players.erase(player->GetName());
}

void GameRoom::RegenerateRoomCode()
{
	GenerateRoomCode();
}

void GameRoom::GenerateRoomCode()
{
	//const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
	const std::string chars = "0123456789";
	std::random_device rd;
	std::mt19937 generator(rd());
	std::uniform_int_distribution<> distr(0, chars.size() - 1);

	for (int i = 0; i < 4; ++i)
	{
		m_roomCode += chars[distr(generator)];
	}
}
