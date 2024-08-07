#pragma once

#include <concurrent_queue.h>

/*
MemoryPoolTestClient
��Ƽ������� �α���/�α׾ƿ� �ݺ��Ͽ� ��Ʈ���� �׽�Ʈ�� ���ÿ� �޸� Ǯ ���� �˻�
*/
struct TestClient
{
	std::thread* thread;

	SOCKET socket = INVALID_SOCKET;
	std::string username;
	std::string password;

	int curCycle = 0;
	long long avgResponseTime = 0;
};

class MemoryPoolTestClient
{
public:
	MemoryPoolTestClient(const char* ip, const char* port, int clientCnt, int cycleCnt);
	~MemoryPoolTestClient() = default;

	void Run();

private:
	SOCKET CreateConnectedSocket();

	void TestThread(TestClient* client);
	bool Login(TestClient* client);
	bool Logout(TestClient* client);

private:
	const char* m_connectIP;
	const char* m_port;

	int m_clientCnt;
	int m_cycleCnt;

	std::vector<TestClient*> m_clients;

	CRITICAL_SECTION m_criticalSection;
};

