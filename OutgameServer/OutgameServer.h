#pragma once

class ServerCore;
class UserManager;

class OutgameServer
{
public:
	static OutgameServer& Instance()
	{
		static OutgameServer instance;
		return instance;
	}

	void Start();
	void Stop();

	bool IsRunning() { return m_bRun; }
	void TriggerShutdown();

	void InsertSendTask(std::shared_ptr<SendStruct> task);

	ServerCore* GetServerCore() const { return m_pServerCore; }

private:
	// ServerCore에서 OnReceive에 실행할 콜백 함수
	void DispatchReceivedData(Session* session, char* data, int nReceivedByte);

	// EchoQueue 처리 스레드
	void ProcessEchoQueue();
	
	// SendQueue 처리 스레드
	void SendThread();

	// 자원 해제 확인용 스레드
	void QuitThread();

private:
	std::atomic<bool> m_bRun;

	ServerCore* m_pServerCore;
	UserManager* m_pUserManager;

	concurrency::concurrent_queue<std::shared_ptr<ReceiveStruct>> m_recvEchoQueue;
	concurrency::concurrent_queue<std::shared_ptr<SendStruct>> m_sendQueue;

	std::vector<std::thread*> m_workers;

private:
	OutgameServer() = default;
	~OutgameServer() = default;

	OutgameServer(const OutgameServer&) = delete;
	OutgameServer& operator=(const OutgameServer&) = delete;
};

