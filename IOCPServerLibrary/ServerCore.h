#pragma once


class ServerCore
{
public:
	ServerCore();
	~ServerCore();

/// Interface
	void Initialize();
	void Run();
	void Finalize();

private:


	
private:
	int m_listeningPort;


};

