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
	OutgameServer::Instance().RegisterPacketHanlder(PacketID::C2S_INVITE_FRIEND_REQUEST, [this](std::shared_ptr<ReceiveStruct> receiveStruct)
		{
			HandleInviteFriendRequest(receiveStruct);
		});
	OutgameServer::Instance().RegisterPacketHanlder(PacketID::C2S_START_GAME_REQUEST, [this](std::shared_ptr<ReceiveStruct> receiveStruct)
		{
			HandleStartGameRequest(receiveStruct);
		});
	OutgameServer::Instance().RegisterPacketHanlder(PacketID::C2S_END_GAME_REQUEST, [this](std::shared_ptr<ReceiveStruct> receiveStruct)
		{
			HandleEndGameRequest(receiveStruct);
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

void GameRoomManager::HandleInviteFriendRequest(std::shared_ptr<ReceiveStruct> receiveStructure)
{
	auto inviteFriendRequest = std::make_shared<Protocol::C2S_InviteFriendRequest>();
	if (!PacketBuilder::Instance().DeserializeData(receiveStructure->data, receiveStructure->nReceivedByte, *(receiveStructure->header), *inviteFriendRequest))
	{
		LOG_CONTENTS("C2S_INVITE_FRIEND_REQUEST: PakcetBuilder::Deserialize() failed");
		return;
	}

	std::shared_ptr<SendStruct> sendStruct = std::make_shared<SendStruct>();
	// packetID
	sendStruct->type = ESendType::UNICAST;
	// session
	sendStruct->session = receiveStructure->session;
	// data
	std::shared_ptr<Protocol::S2C_InviteFriendResponse> response = std::make_shared<Protocol::S2C_InviteFriendResponse>();
	if (InviteFriend(UserManager::FindActiveUser(receiveStructure->session->GetSessionId()).lock()->GetName(), inviteFriendRequest->username()))
	{
		response->mutable_success()->set_value(true);
		response->set_invitedusername(inviteFriendRequest->username());
	}
	else
	{
		response->mutable_success()->set_value(false);
	}
	sendStruct->data = response;
	//header
	std::string serializedString;
	(sendStruct->data)->SerializeToString(&serializedString);
	sendStruct->header = std::make_shared<PacketHeader>(PacketBuilder::Instance().CreateHeader(PacketID::S2C_INVITE_FRIEND_RESPONSE, serializedString.size()));

	OutgameServer::Instance().InsertSendTask(sendStruct);
}

void GameRoomManager::HandleStartGameRequest(std::shared_ptr<ReceiveStruct> receiveStructure)
{
	auto startGameRequest = std::make_shared<Protocol::C2S_StartGameRequest>();
	if (!PacketBuilder::Instance().DeserializeData(receiveStructure->data, receiveStructure->nReceivedByte, *(receiveStructure->header), *startGameRequest))
	{
		LOG_CONTENTS("C2S_START_GAME_REQUEST: PakcetBuilder::Deserialize() failed");
		return;
	}

	std::shared_ptr<SendStruct> sendStruct = std::make_shared<SendStruct>();
	// packetID
	sendStruct->type = ESendType::UNICAST;
	// session
	sendStruct->session = receiveStructure->session;
	// data
	std::shared_ptr<Protocol::S2C_StartGameResponse> response = std::make_shared<Protocol::S2C_StartGameResponse>();
	if (StartGame(receiveStructure->session->GetSessionId()))
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
	sendStruct->header = std::make_shared<PacketHeader>(PacketBuilder::Instance().CreateHeader(PacketID::S2C_START_GAME_RESPONSE, serializedString.size()));

	OutgameServer::Instance().InsertSendTask(sendStruct);
}

void GameRoomManager::HandleEndGameRequest(std::shared_ptr<ReceiveStruct> receiveStructure)
{
	auto endGameRequest = std::make_shared<Protocol::C2S_EndGameRequest>();
	if (!PacketBuilder::Instance().DeserializeData(receiveStructure->data, receiveStructure->nReceivedByte, *(receiveStructure->header), *endGameRequest))
	{
		LOG_CONTENTS("C2S_END_GAME_REQUEST: PakcetBuilder::Deserialize() failed");
		return;
	}

	std::shared_ptr<SendStruct> sendStruct = std::make_shared<SendStruct>();
	// packetID
	sendStruct->type = ESendType::UNICAST;
	// session
	sendStruct->session = receiveStructure->session;
	// data
	std::shared_ptr<Protocol::S2C_EndGameResponse> response = std::make_shared<Protocol::S2C_EndGameResponse>();
	if (EndGame(receiveStructure->session->GetSessionId()))
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
	sendStruct->header = std::make_shared<PacketHeader>(PacketBuilder::Instance().CreateHeader(PacketID::S2C_END_GAME_RESPONSE, serializedString.size()));

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

EJoinRoomResponse GameRoomManager::JoinGameRoom(SessionId sessionId, const std::string_view& roomCode, OUT std::string& ipAddress)
{
	// Find Player
	std::shared_ptr<User> player = UserManager::FindActiveUser(sessionId).lock();
	if (!player)
		return EJoinRoomResponse::UNKNOWN;

	// Find Room
	EnterCriticalSection(&s_gameRoomsLock);
	auto gameRoomIter = s_activeGameRooms.find(std::string(roomCode));
	if (gameRoomIter == s_activeGameRooms.end())	// 없는 방
	{
		LeaveCriticalSection(&s_gameRoomsLock);
		return EJoinRoomResponse::INVALID_ROOM;
	}
	std::shared_ptr<GameRoom> gameRoom = gameRoomIter->second;

	// Check Room State
	if (gameRoom->GetRoomState() != ERoomStateType::WAIT)
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

	ipAddress = gameRoom->GetRoomIpAddress();
	LeaveCriticalSection(&s_gameRoomsLock);

	return EJoinRoomResponse::SUCCESS;
}

bool GameRoomManager::QuitGameRoom(SessionId sessionId)
{
	EnterCriticalSection(&s_gameRoomsLock);
	std::shared_ptr<User> player = UserManager::FindActiveUser(sessionId).lock();
	if (!player)
		return false;

	std::shared_ptr<GameRoom> activeRoom = player->GetActiveGameRoomRef().lock();
	activeRoom->Quit(player);

	if (activeRoom->GetRoomState() == ERoomStateType::DESTROYING)
	{
		s_activeGameRooms.unsafe_erase(activeRoom->GetRoomCode());
	}
	LeaveCriticalSection(&s_gameRoomsLock);
	return true;
}

bool GameRoomManager::InviteFriend(const std::string_view& username, const std::string_view& friendName)
{
	// 상대가 온라인이고, 방 상태가 WAIT인지 확인(다른 확인절차는 JoinRoom에서 거친다)
	// Notification 발송
	std::shared_ptr<User> inviteUser = UserManager::FindActiveUser(username).lock();
	if (!inviteUser)
		return false;
	std::shared_ptr<GameRoom> GameRoom = inviteUser->GetActiveGameRoomRef().lock();
	if (GameRoom->GetRoomState() != ERoomStateType::WAIT)
		return false;

	std::shared_ptr<User> invitedUser = UserManager::FindActiveUser(friendName).lock();
	if (!invitedUser)
		return false;

	std::shared_ptr<SendStruct> sendStruct = std::make_shared<SendStruct>();
	// packetID
	sendStruct->type = ESendType::UNICAST;
	// session
	sendStruct->session = invitedUser->GetSession();
	// data
	std::shared_ptr<Protocol::S2O_InviteFriendNotification> response = std::make_shared<Protocol::S2O_InviteFriendNotification>();
	response->set_username(std::string(username));
	response->set_roomcode(GameRoom->GetRoomCode());
	sendStruct->data = response;
	// header
	std::string serializedString;
	(sendStruct->data)->SerializeToString(&serializedString);
	sendStruct->header = std::make_shared<PacketHeader>(PacketBuilder::Instance().CreateHeader(PacketID::S2O_INVITE_FRIEND_NOTIFICATION, serializedString.size()));

	OutgameServer::Instance().InsertSendTask(sendStruct);

	return true;
}

bool GameRoomManager::StartGame(SessionId sessionId)
{
	// Find Player
	std::shared_ptr<User> player = UserManager::FindActiveUser(sessionId).lock();
	if (!player)
		return false;

	// Find Room
	EnterCriticalSection(&s_gameRoomsLock);
	auto gameRoomIter = s_activeGameRooms.find(player->GetActiveGameRoomRef().lock()->GetRoomCode());
	if (gameRoomIter == s_activeGameRooms.end())
	{
		LeaveCriticalSection(&s_gameRoomsLock);
		return false;
	}
	std::shared_ptr<GameRoom> gameRoom = gameRoomIter->second;

	// Check Room State
	if (gameRoom->GetRoomState() != ERoomStateType::WAIT)
	{
		LeaveCriticalSection(&s_gameRoomsLock);
		return false;
	}

	gameRoom->SetRoomState(ERoomStateType::IN_GAME);
	LeaveCriticalSection(&s_gameRoomsLock);

	return true;
}

bool GameRoomManager::EndGame(SessionId sessionId)
{
	// Find Player
	std::shared_ptr<User> player = UserManager::FindActiveUser(sessionId).lock();
	if (!player)
		return false;

	// Find Room
	EnterCriticalSection(&s_gameRoomsLock);
	auto gameRoomIter = s_activeGameRooms.find(player->GetActiveGameRoomRef().lock()->GetRoomCode());
	if (gameRoomIter == s_activeGameRooms.end())
	{
		LeaveCriticalSection(&s_gameRoomsLock);
		return false;
	}
	std::shared_ptr<GameRoom> gameRoom = gameRoomIter->second;

	// Check Room State
	if (gameRoom->GetRoomState() != ERoomStateType::IN_GAME)
	{
		LeaveCriticalSection(&s_gameRoomsLock);
		return false;
	}

	gameRoom->SetRoomState(ERoomStateType::WAIT);
	LeaveCriticalSection(&s_gameRoomsLock);

	return true;
}
