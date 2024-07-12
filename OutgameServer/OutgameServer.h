#pragma once

class ServerCore;

enum class ESendType
{
	BROADCAST,
	UNICAST
};

struct ReceiveStruct
{
	SessionId sessionId;
	std::shared_ptr<DataPacket> data;

	~ReceiveStruct()
	{
		data.reset();
	}
};

struct SendStruct
{
	ESendType type;
	SessionId sessionId;
	std::shared_ptr<PacketHeader> header;
	std::shared_ptr<DataPacket> data;

	~SendStruct()
	{
		header.reset();
		data.reset();
	}
};

class OutgameServer
{
public:
	OutgameServer();
	~OutgameServer();

	void Start();
	void Stop();

private:
	// ServerCore에서 OnReceive에 실행할 콜백 함수
	void DispatchReceivedData(Session* session, char* data, int nReceivedByte);
	// EchoQueue 처리 스레드
	void ProcessEchoQueue();
	// LoginQueue 처리 스레드
	void ProcessLoginQueue();
	// SendQueue 처리 스레드
	void SendThread();

	void QuitThread();

	/// Login Logic
	

private:
	std::atomic<bool> m_bRun;

	ServerCore* m_serverCore;

	concurrency::concurrent_queue<std::shared_ptr<ReceiveStruct>> m_recvEchoQueue;
	concurrency::concurrent_queue<std::shared_ptr<ReceiveStruct>> m_loginRequestQueue;

	concurrency::concurrent_queue<std::shared_ptr<SendStruct>> m_sendQueue;

	std::thread* m_coreThread;
	std::thread* m_processThread;
	std::thread* m_sendThread;
	std::thread* m_quitThread;
};

