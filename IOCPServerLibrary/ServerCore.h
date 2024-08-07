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
	/// 府家胶 秦力
	void Finalize();

	/// 家南 包访 窃荐
	// 家南 积己, 可记 汲沥(傈价 滚欺傅 X)
	SOCKET CreateSocket();

	// 府郊 家南 牧咆胶飘 积己
	bool CreateListenContext();
	// 府郊 家南 积己 + acceptEx 霸矫
	SOCKET CreateListenSocket();

	/// 技记 包访 窃荐
	Session* CreateSession();						// 技记 积己
	void RegisterSession(Session* session);			// 技记 甘俊 眠啊
	void CloseSession(Session* session, bool needLock = true);			// 技记 磊盔 秦力
	void UnregisterSession(SessionId sessionId);	// 技记 甘俊辑 昏力+技记 磊盔 秦力.

	/// IO 累诀 包访 窃荐
	// GetQueuedCompletionStatus
	void ProcessThread();
	
	void HandleAcceptCompletion();
	void HandleWriteCompletion(Session* session);

	// IO 累诀 霸矫
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

