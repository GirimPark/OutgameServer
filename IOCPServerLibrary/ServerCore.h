#pragma once
#include <functional>

enum class eIOType;

typedef std::function<void(Session*, const char*, int)> ReceiveDataCallback;

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
	/// 리소스 해제
	void Finalize();

	/// 소켓 관련 함수
	// 소켓 생성, 옵션 설정(전송 버퍼링 X)
	SOCKET CreateSocket();

	// 리슨 소켓 컨텍스트 생성
	bool CreateListenContext();
	// 리슨 소켓 생성 + acceptEx 게시
	SOCKET CreateListenSocket();

	/// 세션 관련 함수
	void RegisterSession(Session* session);			// 세션 맵에 추가
	void UnregisterSession(SessionId sessionId);	// 세션 맵에서 삭제+세션 자원 해제.

	/// IO 작업 관련 함수
	// GetQueuedCompletionStatus
	void ProcessThread();
	
	void HandleAcceptCompletion();
	void HandleWriteCompletion(Session* session);

	// IO 작업 게시
	bool StartAccept();

	/// Callbacks
	void OnReceiveData(Session* session, const char* data, int nReceivedByte);

	HANDLE m_hIOCP;
	std::atomic<bool> m_bEndServer;

	ListenContext* m_pListenSocketCtxt;
	const char* m_listeningPort;
	int m_backlog;

	int m_nThread;
	std::vector<std::thread*> m_IOCPThreads;

	concurrency::concurrent_unordered_map<SessionId, Session*> m_sessionMap;
	CRITICAL_SECTION m_sessionMapLock;

	std::vector<ReceiveDataCallback> m_receiveCallbacks;
};

