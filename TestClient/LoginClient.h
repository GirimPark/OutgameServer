#pragma once
#include <concurrent_queue.h>

struct ClientStruct
{
	std::shared_ptr<PacketHeader> header;
	std::shared_ptr<PacketData> data;
};

class LoginClient
{
public:
	LoginClient(const char* ip, const char* port);
	~LoginClient() = default;

	void Run();

private:
	SOCKET CreateConnectedSocket();

	void SendThread();
	void ReceiveThread();

private:
	const char* m_connectIP;
	const char* m_port;
	SOCKET m_socket;

	std::string m_username;
	std::string m_password;

	concurrency::concurrent_queue<std::shared_ptr<ClientStruct>> m_sendQueue;
};
