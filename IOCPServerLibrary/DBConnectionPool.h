#pragma once

#include "DBConnection.h"

class DBConnectionPool
{
public:
	static DBConnectionPool& Instance()
	{
		static DBConnectionPool instance;
		return instance;
	}

	bool Connect(int connectionCount, const WCHAR* connectionString);
	void Clear();

	DBConnection* GetConnection();
	void ReturnConnection(DBConnection* connection);

private:
	SQLHENV m_environment;
	concurrency::concurrent_vector<DBConnection*> m_connections;

	std::atomic<unsigned int> m_getCnt;
	std::atomic<unsigned int> m_returnCnt;

	DBConnectionPool() = default;
	~DBConnectionPool()
	{
		Clear();
	}

	DBConnectionPool(const DBConnectionPool&) = delete;
	DBConnectionPool& operator=(const DBConnectionPool&) = delete;
};

