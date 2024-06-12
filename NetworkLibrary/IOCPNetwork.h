#pragma once

enum class eIOType;
struct SocketContext;

class IOCPNetwork
{
public:
	IOCPNetwork();
	~IOCPNetwork();

	void Initialize();
	void Run();
	void Finalize();

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
	const char* m_port = DEFAULT_PORT;
	HANDLE m_hIOCP = INVALID_HANDLE_VALUE;
	SOCKET m_listenSocket = INVALID_SOCKET;
	WSAEVENT m_threadHandles[MAX_WORKER_THREAD];
	WSAEVENT m_hCleanupEvent[1];
	SocketContext* m_pListenSocketCtxt = nullptr;
	SocketContext* m_pClientSocketCtxtList = nullptr;

	CRITICAL_SECTION m_criticalSection;
};

