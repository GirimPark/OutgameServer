#pragma once

#include "PacketHandler.h"

class ServerCore;
class UserManager;
class PacketHandler;


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

	void RegisterPacketHanlder(PacketID headerType, PacketHandlerCallback callback);
	void InsertSendTask(std::shared_ptr<SendStruct> task);

	ServerCore* GetServerCore() const { return m_pServerCore; }

private:
	// SendQueue 처리 스레드
	void SendThread();
	// 자원 해제 확인용 스레드
	void QuitThread();

private:
	std::atomic<bool> m_bRun;

	ServerCore* m_pServerCore;
	UserManager* m_pUserManager;
	PacketHandler* m_pPacketHandler;

	concurrency::concurrent_queue<std::shared_ptr<SendStruct>> m_sendQueue;

	std::vector<std::thread*> m_workers;

private:
	OutgameServer() = default;
	~OutgameServer() = default;

	OutgameServer(const OutgameServer&) = delete;
	OutgameServer& operator=(const OutgameServer&) = delete;
};

