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

	// send 게시
	bool Unicast(Session* session, const char* data, int length);
	bool Broadcast(const char* data, int length);

private:
	/// 리소스 해제
	void Finalize();

	/// 세션 관련 함수
	Session* CreateSession();						// 세션 생성
	void RegisterSession(Session* session);			// 세션 맵에 추가
	void CloseSession(Session* session, bool needLock = true);			// 세션 자원 해제
	void UnregisterSession(SessionId sessionId);	// 세션 맵에서 삭제+세션 자원 해제. lock이 있으므로 남용 유의

	/// IO 작업 관련 함수
	bool StartSend(Session* session, const char* data, int length);	// IOCPNetworkAPI::StartSend 래핑

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

