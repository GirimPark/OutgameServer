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
	// SendQueue ó�� ������
	void SendThread();
	// �ڿ� ���� Ȯ�ο� ������
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

