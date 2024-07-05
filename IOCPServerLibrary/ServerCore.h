#pragma once
#include <functional>

enum class eIOType;

//typedef std::function<void(Session*, char*, int)> AcceptCallback;
typedef std::function<void(Session*, char*, int)> ReceiveDataCallback;
//typedef std::function<void(Session*)> SendDataCallback;

class ServerCore
{
public:
	ServerCore(const char* port, int backlog);
	~ServerCore();

	/// Interface
	void Run();
	void TriggerCleanupEvent();

	// Register Callbacks
	void RegisterCallback(ReceiveDataCallback callback);

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
	void HandleAcceptCompletion(DWORD nTransferredByte);
	void HandleReadCompletion(Session* session, DWORD nTransferredByte);
	void HandleWriteCompletion(Session* session);

public:
	// IO �۾� �Խ�
	bool StartAccept();
	bool StartReceive(Session* session);
	bool StartSend(SessionId sessionId, const char* data, int length);

	//! ���� �κ����� �Ѱܾ� ��
	// todo �ʱ� ������ ó��
	void ProcessInitialData(Session* session, char* data, int length);
	// todo ����
	bool AuthenticateUser(const std::string_view& username, const std::string_view& password);

	/// ���� ���� �ڿ� ���� Ȯ�ο� ������
	void QuitThread();

	/// Callbacks
	void OnReceiveData(Session* session, char* data, int nReceivedByte);

private:
	HANDLE m_hIOCP;
	bool m_bEndServer;
	WSAEVENT m_hCleanupEvent[1];

	ListenContext* m_pListenSocketCtxt;
	const char* m_listeningPort;
	int m_backlog;

	int m_nThread;
	std::vector<std::thread*> m_threads;

	concurrency::concurrent_unordered_map<SessionId, Session*> m_sessionMap;

	CRITICAL_SECTION m_criticalSection;

	std::vector<ReceiveDataCallback> m_receiveCallbacks;
};

