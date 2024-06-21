#pragma once

enum class eIOType;

class ServerCore
{
public:
	ServerCore(const char* port);
	~ServerCore();

	/// Interface
	bool Run();

private:
	/// ���ҽ� ����
	bool Finalize();

	/// ���� ���� �Լ�
	// ���� ����, �ɼ� ����(���� ���۸� X)
	SOCKET CreateSocket();

	// ���� ���� ���� + acceptEx �Խ�
	bool CreateListenSocket();
	// ���� ���� ���ؽ�Ʈ ����
	bool CreateListenContext();

	/// ���� ���� �Լ�
	Session* CreateSession();

	/// IO �۾� ���� �Լ�
	// GetQueuedCompletionStatus
	void ProcessThread();

	//! ���� �κ����� �Ѱܾ� ��
	// IO �۾� ó��, �Ŀ� �ݹ����� �ѱ� ����
	void HandleAcceptCompletion(Session* session);
	void HandleReadCompletion(Session* session);
	void HandleWriteCompletion(Session* session);

	// IO �۾� �Խ�
	bool StartAccept();
	bool StartReceive(Session* session);
	bool StartSend(Session* session, const char* data, int length);

	//! ���� �κ����� �Ѱܾ� ��
	// todo �ʱ� ������ ó��
	void ProcessInitialData(Session* session, char* data, int length);
	// todo ����
	bool AuthenticateUser(const std::string_view& username, const std::string_view& password);
	
private:
	HANDLE m_hIOCP;

	bool m_bEndServer;

	SOCKET m_listenSocket;
	ListenContext* m_pListenSocketCtxt;
	const char* m_listeningPort;

	int m_nThread;
	std::vector<std::thread*> m_threads;

	WSAEVENT m_hCleanupEvent[1];

	tbb::concurrent_hash_map<SessionId, Session*> m_sessionMap;

	CRITICAL_SECTION m_criticalSection;
};

