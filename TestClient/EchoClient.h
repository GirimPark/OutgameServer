#pragma once

struct SocketPerThread
{
	std::thread thread;
	SOCKET socket;
};

class EchoClient
{
public:
	EchoClient(char* port);

private:
	char* m_port;
	int m_nThread;

	std::vector<SocketPerThread> m_clients;


};

