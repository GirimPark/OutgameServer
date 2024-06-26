#pragma once

enum class eIOType;

class ServerCore
{
public:
	ServerCore(const char* port);
	~ServerCore();

	/// Interface
	void Run();

private:
	/// ���ҽ� ����
	void Finalize();

	/// ���� ���� �Լ�
	// ���� ����, �ɼ� ����(���� ���۸� X)
	SOCKET CreateSocket();

	// ���� ���� ���ؽ�Ʈ ����
	bool CreateListenContext();
	// ���� ���� ���� + acceptEx �Խ�
	SOCKET CreateListenSocket();

	/// ���� ���� �Լ�
	Session* CreateSession();
	void CloseSession(SessionId sessionId);

	/// IO �۾� ���� �Լ�
	// GetQueuedCompletionStatus
	void ProcessThread();
	
	//! ���� �κ����� �Ѱܾ� ��
	// IO �۾� ó��, �Ŀ� �ݹ����� �ѱ� ����
	void HandleAcceptCompletion();
	void HandleReadCompletion(Session* session, DWORD nTransferredByte);
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

	// ���� ���� �ڿ� ���� Ȯ�ο� ������
	void QuitThread();

private:
	HANDLE m_hIOCP;

	bool m_bEndServer;

	ListenContext* m_pListenSocketCtxt;
	const char* m_listeningPort;

	int m_nThread;
	std::vector<std::thread*> m_threads;

	WSAEVENT m_hCleanupEvent[1];

	tbb::concurrent_unordered_map<SessionId, Session*> m_sessionMap;

	CRITICAL_SECTION m_criticalSection;

	std::thread* m_quitThread;
};

