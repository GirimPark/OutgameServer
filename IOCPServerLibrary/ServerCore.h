#pragma once
#include <functional>

enum class eIOType;

typedef std::function<void(Session*, char*, int)> ReceiveDataCallback;

class ServerCore
{
public:
	ServerCore(const char* port, int backlog);
	~ServerCore();

	/// Interface
	void Run();
	void TriggerShutdown();

	// Register Callbacks
	void RegisterCallback(ReceiveDataCallback callback);

	bool Unicast(Session* session, const char* data, int length);
	bool Broadcast(const char* data, int length);

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
	Session* CreateSession();						// ���� ����
	void RegisterSession(Session* session);			// ���� �ʿ� �߰�
	void CloseSession(Session* session, bool needLock = true);			// ���� �ڿ� ����
	void UnregisterSession(SessionId sessionId);	// ���� �ʿ��� ����+���� �ڿ� ����. lock�� �����Ƿ� ���� ����

	/// IO �۾� ���� �Լ�
	// GetQueuedCompletionStatus
	void ProcessThread();
	
	void HandleAcceptCompletion();
	void HandleWriteCompletion(Session* session);

	// IO �۾� �Խ�
	bool StartAccept();
	bool StartReceive(Session* session);
	bool StartSend(Session* session, const char* data, int length);

	/// Callbacks
	void OnReceiveData(Session* session, char* data, int nReceivedByte);

	HANDLE m_hIOCP;
	std::atomic<bool> m_bEndServer;

	ListenContext* m_pListenSocketCtxt;
	const char* m_listeningPort;
	int m_backlog;

	int m_nThread;
	std::vector<std::thread*> m_IOCPThreads;

	concurrency::concurrent_unordered_map<SessionId, Session*> m_sessionMap;

	CRITICAL_SECTION m_criticalSection;

	std::vector<ReceiveDataCallback> m_receiveCallbacks;
};

