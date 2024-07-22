#pragma once

class IOCPNetworkAPI
{
public:
	IOCPNetworkAPI();
	~IOCPNetworkAPI();

	void Initialize();
	void Finalize();

	bool StartAccept();
	bool StartReceive();
	bool StartSend();

	SOCKET CreateSocket();
	SOCKET CreateListenSocket();

private:
	HANDLE m_hIOCP;
	const char* m_listeningPort;
	int m_backlog;
};

