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
	// ServerCore���� OnReceive�� ������ �ݹ� �Լ�
	void DispatchReceivedData(Session* session, char* data, int nReceivedByte);

	// EchoQueue ó�� ������
	void ProcessEchoQueue();
	
	// SendQueue ó�� ������
	void SendThread();

	// �ڿ� ���� Ȯ�ο� ������
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

