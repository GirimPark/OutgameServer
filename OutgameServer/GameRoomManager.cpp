#include "pch.h"
#include "GameRoomManager.h"

#include "GameRoom.h"
#include "User.h"
#include "UserManager.h"

GameRoomManager::GameRoomManager()
{
	OutgameServer::Instance().RegisterPacketHanlder(PacketID::C2S_CREATE_ROOM_REQUEST, [this](std::shared_ptr<ReceiveStruct> receiveStruct)
		{
			HandleCreateRoomRequest(receiveStruct);
		});
	OutgameServer::Instance().RegisterPacketHanlder(PacketID::C2S_JOIN_ROOM_REQUEST, [this](std::shared_ptr<ReceiveStruct> receiveStruct)
		{
			HandleJoinRoomRequest(receiveStruct);
		});
	OutgameServer::Instance().RegisterPacketHanlder(PacketID::C2S_QUIT_ROOM_REQUEST, [this](std::shared_ptr<ReceiveStruct> receiveStruct)
		{
			HandleQuitRoomRequest(receiveStruct);
		});
}

GameRoomManager::~GameRoomManager()
{
	m_activeGameRooms.clear();
}

void GameRoomManager::HandleCreateRoomRequest(std::shared_ptr<ReceiveStruct> receiveStructure)
{
	auto createRoomRequest = std::make_shared<Protocol::C2S_CreateRoomRequest>();
	if(!PacketBuilder::Instance().DeserializeData(receiveStructure->data, receiveStructure->nReceivedByte, *(receiveStructure->header), *createRoomRequest))
	{
		LOG_CONTENTS("C2S_CREATE_ROOM_REQUEST: PakcetBuilder::Deserialize() failed");
		return;
	}

	std::shared_ptr<SendStruct> sendStruct = std::make_shared<SendStruct>();
	// packetID
	sendStruct->type = ESendType::UNICAST;
	// session
	sendStruct->session = receiveStructure->session;
	// data
	std::shared_ptr<Protocol::S2C_CreateRoomResponse> response = std::make_shared<Protocol::S2C_CreateRoomResponse>();
	std::shared_ptr<GameRoom> createGameRoom = CreateGameRoom(sendStruct->session->GetSessionId());
	if(createGameRoom.get())
	{
		response->mutable_success()->set_value(true);
		response->set_roomcode(createGameRoom->GetRoomCode());
	}
	else
	{
		response->mutable_success()->set_value(false);
	}
	sendStruct->data = response;
	//header
	std::string serializedString;
	(sendStruct->data)->SerializeToString(&serializedString);
	sendStruct->header = std::make_shared<PacketHeader>(PacketBuilder::Instance().CreateHeader(PacketID::S2C_CREATE_ROOM_RESPONSE, serializedString.size()));

	OutgameServer::Instance().InsertSendTask(sendStruct);
}

void GameRoomManager::HandleJoinRoomRequest(std::shared_ptr<ReceiveStruct> receiveStructure)
{
	auto joinRoomRequest = std::make_shared<Protocol::C2S_JoinRoomRequest>();
	if (!PacketBuilder::Instance().DeserializeData(receiveStructure->data, receiveStructure->nReceivedByte, *(receiveStructure->header), *joinRoomRequest))
	{
		LOG_CONTENTS("C2S_JOIN_ROOM_REQUEST: PakcetBuilder::Deserialize() failed");
		return;
	}

	std::shared_ptr<SendStruct> sendStruct = std::make_shared<SendStruct>();
	// packetID
	sendStruct->type = ESendType::UNICAST;
	// session
	sendStruct->session = receiveStructure->session;
	// data
	std::shared_ptr<Protocol::S2C_JoinRoomResponse> response = std::make_shared<Protocol::S2C_JoinRoomResponse>();
	std::string ipAddress;
	if (JoinGameRoom(receiveStructure->session->GetSessionId(), joinRoomRequest->roomcode(), OUT ipAddress))
	{
		response->mutable_success()->set_value(true);
		response->set_ipaddress(ipAddress);
	}
	else
	{
		response->mutable_success()->set_value(false);
	}
	sendStruct->data = response;
	//header
	std::string serializedString;
	(sendStruct->data)->SerializeToString(&serializedString);
	sendStruct->header = std::make_shared<PacketHeader>(PacketBuilder::Instance().CreateHeader(PacketID::S2C_JOIN_ROOM_RESPONSE, serializedString.size()));

	OutgameServer::Instance().InsertSendTask(sendStruct);
}

void GameRoomManager::HandleQuitRoomRequest(std::shared_ptr<ReceiveStruct> receiveStructure)
{
	auto quitRoomRequest = std::make_shared<Protocol::C2S_QuitRoomRequest>();
	if (!PacketBuilder::Instance().DeserializeData(receiveStructure->data, receiveStructure->nReceivedByte, *(receiveStructure->header), *quitRoomRequest))
	{
		LOG_CONTENTS("C2S_QUIT_ROOM_REQUEST: PakcetBuilder::Deserialize() failed");
		return;
	}

	std::shared_ptr<SendStruct> sendStruct = std::make_shared<SendStruct>();
	// packetID
	sendStruct->type = ESendType::UNICAST;
	// session
	sendStruct->session = receiveStructure->session;
	// data
	std::shared_ptr<Protocol::S2C_QuitRoomResponse> response = std::make_shared<Protocol::S2C_QuitRoomResponse>();
	if (QuitGameRoom(receiveStructure->session->GetSessionId()))
	{
		response->mutable_success()->set_value(true);
	}
	else
	{
		response->mutable_success()->set_value(false);
	}
	sendStruct->data = response;
	//header
	std::string serializedString;
	(sendStruct->data)->SerializeToString(&serializedString);
	sendStruct->header = std::make_shared<PacketHeader>(PacketBuilder::Instance().CreateHeader(PacketID::S2C_QUIT_ROOM_RESPONSE, serializedString.size()));

	OutgameServer::Instance().InsertSendTask(sendStruct);
}

std::shared_ptr<GameRoom> GameRoomManager::CreateGameRoom(SessionId sessionId)
{
	std::shared_ptr<User> hostPlayer = UserManager::FindActiveUser(sessionId).lock();
	if (!hostPlayer)
		return nullptr;

	std::shared_ptr<GameRoom> gameRoom = std::make_shared<GameRoom>(hostPlayer);
	m_roomCodes.insert({ gameRoom->GetRoomCode(), gameRoom->GetRoomId() });
	m_activeGameRooms.insert({ gameRoom->GetRoomId(), gameRoom });

	if (!gameRoom->Enter(hostPlayer))
		return nullptr;

	return gameRoom;
}

bool GameRoomManager::JoinGameRoom(SessionId sessionId, std::string roomCode, std::string& ipAddress)
{
	// Find Room
	auto roomCodeIter = m_roomCodes.find(roomCode);
	if(roomCodeIter == m_roomCodes.end())
		return false;

	auto gameRoomIter = m_activeGameRooms.find(roomCodeIter->second);
	ASSERT_CRASH(gameRoomIter != m_activeGameRooms.end());

	// Find Player
	std::shared_ptr<User> player = UserManager::FindActiveUser(sessionId).lock();
	if (!player)
		return false;

	// Enter
	if (!gameRoomIter->second->Enter(player))
		return false;

	ipAddress = gameRoomIter->second->GetRoomIpAddress();
	return true;
}

bool GameRoomManager::QuitGameRoom(SessionId sessionId)
{
	std::shared_ptr<User> player = UserManager::FindActiveUser(sessionId).lock();
	if (!player)
		return false;

	std::shared_ptr<GameRoom> activeRoom = player->GetActiveGameRoomRef().lock();
	activeRoom->Quit(player);

	if (activeRoom->GetRoomState() == ERoomStateType::DESTROYING)
	{
		m_roomCodes.erase(m_roomCodes.find(activeRoom->GetRoomCode()));
		m_activeGameRooms.erase(m_activeGameRooms.find(activeRoom->GetRoomId()));
	}

	return true;
}