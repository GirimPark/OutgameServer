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
	/// 리소스 해제
	bool Finalize();

	/// 소켓 관련 함수
	// 소켓 생성, 옵션 설정(전송 버퍼링 X)
	SOCKET CreateSocket();

	// 리슨 소켓 생성 + acceptEx 게시
	bool CreateListenSocket();
	// 리슨 소켓 컨텍스트 생성
	bool CreateListenContext();

	/// 세션 관련 함수
	Session* CreateSession();

	/// IO 작업 관련 함수
	// GetQueuedCompletionStatus
	void ProcessThread();

	//! 로직 부분으로 넘겨야 함
	// IO 작업 처리, 후에 콜백으로 넘길 예정
	void HandleAcceptCompletion(Session* session);
	void HandleReadCompletion(Session* session);
	void HandleWriteCompletion(Session* session);

	// IO 작업 게시
	bool StartAccept();
	bool StartReceive(Session* session);
	bool StartSend(Session* session, const char* data, int length);

	//! 로직 부분으로 넘겨야 함
	// todo 초기 데이터 처리
	void ProcessInitialData(Session* session, char* data, int length);
	// todo 인증
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

