#pragma once

class ServerCore;

struct Packet
{
	Session* session;
	char* data;
	unsigned int nRecvByte;
};

class OutgameServer
{
public:
	OutgameServer();
	~OutgameServer();

	void Start();

private:
	void DispatchReceivedData(Session* session, char* data, int nReceivedByte);
	void ProcessEchoPacket();

	/// Login Logic
	

private:
	ServerCore* m_serverCore;

	concurrency::concurrent_queue<Packet*> m_recvQueue;
};

