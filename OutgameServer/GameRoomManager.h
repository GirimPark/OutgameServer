#pragma once

typedef unsigned int RoomId;

class GameRoom;

class GameRoomManager
{
public:
	GameRoomManager();
	~GameRoomManager();

	void HandleCreateRoomRequest(std::shared_ptr<ReceiveStruct> receiveStructure);
	void HandleJoinRoomRequest(std::shared_ptr<ReceiveStruct> receiveStructure);
	void HandleQuitRoomRequest(std::shared_ptr<ReceiveStruct> receiveStructure);

private:
	std::shared_ptr<GameRoom> CreateGameRoom(SessionId sessionId);
	bool JoinGameRoom(SessionId sessionId, std::string roomCode, std::string& ipAddress);
	bool QuitGameRoom(SessionId sessionId);

private:
	std::unordered_map<std::string, RoomId> m_roomCodes;
	std::unordered_map<RoomId, std::shared_ptr<GameRoom>> m_activeGameRooms;
};