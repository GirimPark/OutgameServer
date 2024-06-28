#pragma once

struct SocketPerThread
{
	std::thread* thread;
	SOCKET socket;

	int preRecvData = -1;
	int sequenceData = 0;
};

class EchoClient
{
public:
	EchoClient(const char* ip, const char* port);
	~EchoClient();

	/// Interface
	void Run();

private:
	SOCKET CreateConnectedSocket(int threadId);

	void EchoThread(int threadId);

	bool SendBuffer(int threadId, char* outbuf);
	bool RecvBuffer(int threadId, char* inbuf);


private:
	bool m_bEndClient;

	const char* m_connectIP;
	const char* m_port;

	std::vector<SocketPerThread> m_clients;

	WSAEVENT m_hCleanupEvent[1];
};

