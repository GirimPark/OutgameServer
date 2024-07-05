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
	Session* CreateSession();
	void CloseSession(SessionId sessionId);

	/// IO 작업 관련 함수
	// GetQueuedCompletionStatus
	void ProcessThread();
	
	//! 로직 부분으로 넘겨야 함
	// IO 작업 처리, 후에 콜백으로 넘길 예정
	void HandleAcceptCompletion(DWORD nTransferredByte);
	void HandleReadCompletion(Session* session, DWORD nTransferredByte);
	void HandleWriteCompletion(Session* session);

public:
	// IO 작업 게시
	bool StartAccept();
	bool StartReceive(Session* session);
	bool StartSend(SessionId sessionId, const char* data, int length);

	//! 로직 부분으로 넘겨야 함
	// todo 초기 데이터 처리
	void ProcessInitialData(Session* session, char* data, int length);
	// todo 인증
	bool AuthenticateUser(const std::string_view& username, const std::string_view& password);

	/// 서버 종료 자원 해제 확인용 스레드
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

