#include "pch.h"
#include "GameRoomManager.h"

#include "GameRoom.h"
#include "User.h"
#include "UserManager.h"

concurrency::concurrent_unordered_map<std::string, std::shared_ptr<GameRoom>> GameRoomManager::s_activeGameRooms;
CRITICAL_SECTION GameRoomManager::s_gameRoomsLock;

GameRoomManager::GameRoomManager()
{
	InitializeCriticalSection(&s_gameRoomsLock);

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
	DeleteCriticalSection(&s_gameRoomsLock);

	s_activeGameRooms.clear();
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

	switch (EJoinRoomResponse rt = JoinGameRoom(receiveStructure->session->GetSessionId(), joinRoomRequest->roomcode(), OUT ipAddress))
	{
	case SUCCESS:
		{
		response->set_resultcode(rt);
		response->set_ipaddress(ipAddress);
		break;
		}
	default:
		{
		response->set_resultcode(rt);
		break;
		}
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

	EnterCriticalSection(&s_gameRoomsLock);
	auto snapshot = s_activeGameRooms;
	LeaveCriticalSection(&s_gameRoomsLock);

	while(snapshot.find(gameRoom->GetRoomCode()) != snapshot.end())
	{
		gameRoom->RegenerateRoomCode();
	}

	s_activeGameRooms.insert({ gameRoom->GetRoomCode(), gameRoom });

	if (!gameRoom->Enter(hostPlayer))
		return nullptr;

	return gameRoom;
}

EJoinRoomResponse GameRoomManager::JoinGameRoom(SessionId sessionId, std::string roomCode, std::string& ipAddress)
{
	// Find Player
	std::shared_ptr<User> player = UserManager::FindActiveUser(sessionId).lock();
	if (!player)
		return EJoinRoomResponse::UNKNOWN;

	// Find Room
	EnterCriticalSection(&s_gameRoomsLock);
	auto gameRoomIter = s_activeGameRooms.find(roomCode);
	if (gameRoomIter == s_activeGameRooms.end())	// ¾ø´Â ¹æ
	{
		LeaveCriticalSection(&s_gameRoomsLock);
		return EJoinRoomResponse::INVALID_ROOM;
	}
	std::shared_ptr<GameRoom> gameRoom = gameRoomIter->second;

	// Check Room State
	if (gameRoom->GetRoomState() == ERoomStateType::DESTROYING)
	{
		LeaveCriticalSection(&s_gameRoomsLock);
		return EJoinRoomResponse::INVALID_ROOM;
	}

	// Enter
	if (!gameRoom->Enter(player))
	{
		LeaveCriticalSection(&s_gameRoomsLock);
		return EJoinRoomResponse::OVER_PLAYER;
	}
	LeaveCriticalSection(&s_gameRoomsLock);

	ipAddress = gameRoom->GetRoomIpAddress();
	return SUCCESS;
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
		EnterCriticalSection(&s_gameRoomsLock);
		s_activeGameRooms.unsafe_erase(activeRoom->GetRoomCode());
		LeaveCriticalSection(&s_gameRoomsLock);
	}

	return true;
}