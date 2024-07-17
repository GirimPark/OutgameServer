#pragma once

#include "DBConnection.h"

#include <mutex>

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

	DBConnectionPool() = default;
	~DBConnectionPool()
	{
		Clear();
	}

	DBConnectionPool(const DBConnectionPool&) = delete;
	DBConnectionPool& operator=(const DBConnectionPool&) = delete;
};

