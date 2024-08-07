#pragma once

#include <concurrent_queue.h>

/*
MemoryPoolTestClient
멀티스레드로 로그인/로그아웃 반복하여 스트레스 테스트와 동시에 메모리 풀 성능 검사
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

