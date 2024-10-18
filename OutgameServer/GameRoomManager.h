#pragma once

class GameRoom;

class GameRoomManager
{
public:
	GameRoomManager();
	~GameRoomManager();


	static void UpdateActiveGameRooms(std::string delRoomCode)
	{
		EnterCriticalSection(&s_gameRoomsLock);
		s_activeGameRooms.unsafe_erase(delRoomCode);
		LeaveCriticalSection(&s_gameRoomsLock);
	}


	void HandleCreateRoomRequest(std::shared_ptr<ReceiveStruct> receiveStructure);
	void HandleJoinRoomRequest(std::shared_ptr<ReceiveStruct> receiveStructure);
	void HandleQuitRoomRequest(std::shared_ptr<ReceiveStruct> receiveStructure);
	void HandleInviteFriendRequest(std::shared_ptr<ReceiveStruct> receiveStructure);

private:
	std::shared_ptr<GameRoom> CreateGameRoom(SessionId sessionId);
	EJoinRoomResponse JoinGameRoom(SessionId sessionId, const std::string_view& roomCode, OUT std::string& ipAddress);
	bool QuitGameRoom(SessionId sessionId);
	bool InviteFriend(const std::string_view& username, const std::string_view& friendName);

private:
	static CRITICAL_SECTION s_gameRoomsLock;
	static concurrency::concurrent_unordered_map<std::string, std::shared_ptr<GameRoom>> s_activeGameRooms;
};