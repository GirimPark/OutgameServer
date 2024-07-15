#pragma once

class LoginClient
{
public:
	LoginClient(const char* ip, const char* port);
	~LoginClient() = default;

	void Run();

private:
	SOCKET CreateConnectedSocket();

private:
	const char* m_connectIP;
	const char* m_port;
	SOCKET socket;
};