#pragma once

class OutgameServer;

class IOCPNetworkAPI
{
public:
	static IOCPNetworkAPI& Instance()
	{
		static IOCPNetworkAPI instance;
		return instance;
	}

	bool InitializeIOCP(HANDLE& hIOCP);
	void FinalizeIOCP(HANDLE& hIOCP);

	SOCKET CreateSocket();
	SOCKET CreateListenSocket();
	bool CreateListenContext(ListenContext*& pListenContext, HANDLE& hIOCP);
	bool ConfigureAcceptedSocket(ListenContext*& pListenContext, sockaddr_in*& remoteAddr);

	bool StartAccept(ListenContext*& listenSocketCtxt);
	bool StartReceive(SOCKET& socket, OVERLAPPED_STRUCT& overlapped);
	bool StartSend(SOCKET& socket, OVERLAPPED_STRUCT& overlapped, const char* data, int length);
public:
	const char* GetListeningPort() const { return m_listeningPort; }
	void SetListeningPort(const char* port) { m_listeningPort = port; }

	int GetBacklog() const { return m_backlog; }
	void SetBacklog(int backlog) { m_backlog = backlog; }

private:
	const char* m_listeningPort;
	int m_backlog;

private:
	IOCPNetworkAPI() = default;
	~IOCPNetworkAPI() = default;

	IOCPNetworkAPI(const IOCPNetworkAPI&) = delete;
	IOCPNetworkAPI& operator=(const IOCPNetworkAPI&) = delete;
};

