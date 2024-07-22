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
	void TriggerCleanupEvent();

	// Register Callbacks
	void RegisterCallback(ReceiveDataCallback callback);

	// send �Խ�
	bool Unicast(Session* session, const char* data, int length);
	bool Broadcast(const char* data, int length);

private:
	/// ���ҽ� ����
	void Finalize();

	/// ���� ���� �Լ�
	Session* CreateSession();						// ���� ����
	void RegisterSession(Session* session);			// ���� �ʿ� �߰�
	void CloseSession(Session* session, bool needLock = true);			// ���� �ڿ� ����
	void UnregisterSession(SessionId sessionId);	// ���� �ʿ��� ����+���� �ڿ� ����. lock�� �����Ƿ� ���� ����

	/// IO �۾� ���� �Լ�
	bool StartSend(Session* session, const char* data, int length);	// IOCPNetworkAPI::StartSend ����

	void ProcessThread();	// GetQueuedCompletionStatus

	void HandleAcceptCompletion(DWORD nTransferredByte);
	void HandleWriteCompletion(Session* session);

	/// Callbacks
	void OnReceiveData(Session* session, char* data, int nReceivedByte);

private:
	bool m_bEndServer;

	HANDLE m_hIOCP;
	WSAEVENT m_hCleanupEvent[1];

	ListenContext* m_pListenSocketCtxt;

	int m_nThread;
	std::vector<std::thread*> m_IOCPThreads;

	concurrency::concurrent_unordered_map<SessionId, Session*> m_sessionMap;

	CRITICAL_SECTION m_criticalSection;

	std::vector<ReceiveDataCallback> m_receiveCallbacks;
};

