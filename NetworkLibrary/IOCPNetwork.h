#pragma once

#include <mswsock.h>

enum class eIOType;
struct SocketContext;

class IOCPNetwork
{
public:
	IOCPNetwork();
	~IOCPNetwork();

	//! test, �ش� Ŭ���� ����Լ� �����ؼ� ���� �ַ�� �ʿ��� ���� ���� �������� �Ŀ� ���� ����
	void Run();

	// WSAStartup, InitCriticalSection, createiocp
	bool Initialize();
	void RunWorkerThread();
	// ���� ���� ����, RegisterSocketCtxt+AcceptEx �ε�
	bool CreateListenSocket();
	// ���� ���� ����, AcceptEx �Խ� 
	bool CreateAcceptSocket();
	// ������ ����, ���� ���� �ݱ�, ���� ���� ���ؽ�Ʈ ����, iocp �ڵ� ����
	void Finalize();

private:
	SOCKET CreateSocket();
	// ���� ���ؽ�Ʈ �����ؼ� iocp�� ���, ����Ʈ�� �߰�
	SocketContext* RegisterSocketCtxt(SOCKET socket, eIOType IOType);

	void WorkerThread();

	///
	//bool CreateListenSocket();
	//bool CreateAcceptSocket(bool bUpdateIOCP);

	//SocketContext* UpdateCompletionPort(SOCKET socket, eIOType IOType, bool bAddToList);

	//void CloseClient(SocketContext* socketCtxt);

	//UINT WorkerThread(LPVOID workContext);


	//SocketContext* CtxtAllocate(SOCKET socket, eIOType IOType);
	//void CleanupCtxtList();
	//void AddToCtxtList(SocketContext* socketCtxt);
	//void RemoveFromCtxtList(SocketContext* socketCtxt);


private:
	// cleaupEvent�� �Ű�������,,
	bool m_bEndServer = false;

	const char* m_port = DEFAULT_PORT;
	HANDLE m_hIOCP = INVALID_HANDLE_VALUE;
	SOCKET m_listenSocket = INVALID_SOCKET;
	int m_nThread = 0;
	std::vector<std::thread*> m_threads;
	//HANDLE m_threadHandles[MAX_WORKER_THREAD];
	//WSAEVENT m_hCleanupEvent[1];
	SocketContext* m_pListenSocketCtxt = nullptr;
	SocketContext* m_pClientSocketCtxtList = nullptr;

	CRITICAL_SECTION m_criticalSection;

	LPFN_ACCEPTEX m_fnAcceptEx;
};

