#pragma once

enum class eIOType;
struct SocketContext;

class IOCPNetwork
{
public:
	IOCPNetwork();
	~IOCPNetwork();

	//! test, 해당 클래스 멤버함수 조합해서 서버 솔루션 쪽에서 메인 루프 돌리도록 후에 변경 예정
	void Run();

	// WSAStartup, InitCriticalSection, createiocp
	bool Initialize();
	void RunWorkerThread();
	// 리슨 소켓 생성, RegisterSocketCtxt+AcceptEx 로드
	bool CreateListenSocket();
	// 수락 소켓 생성, AcceptEx 게시 
	bool CreateAcceptSocket();
	// 스레드 종료, 리슨 소켓 닫기, 리슨 소켓 컨텍스트 해제, iocp 핸들 해제
	void Finalize();

private:
	SOCKET CreateSocket();
	// 소켓 컨텍스트 생성해서 iocp에 등록, 리스트에 추가
	SocketContext* RegisterSocketCtxt(SOCKET socket, eIOType IOType, bool bAddToList);

	SocketContext* CreateSocketCtxt(SOCKET socket, eIOType IOType);

	void WorkerThread();

	// 임시로 사용
	void CloseSocketCtxt(SocketContext* socketCtxt);
	void InsertCtxtList(SocketContext* socketCtxt);
	void RemoveCtxtList(SocketContext* socketCtxt);
	void FreeCtxtList();


private:
	// 크리티컬 섹션 삭제 예정
	bool m_bEndServer = false;

	const char* m_port = DEFAULT_PORT;
	HANDLE m_hIOCP = INVALID_HANDLE_VALUE;
	SOCKET m_listenSocket = INVALID_SOCKET;
	int m_nThread = 0;
	std::vector<std::thread*> m_threads;
	WSAEVENT m_hCleanupEvent[1];
	SocketContext* m_pListenSocketCtxt = nullptr;
	SocketContext* m_pClientSocketCtxtList = nullptr;

	CRITICAL_SECTION m_criticalSection;
};

