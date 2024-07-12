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

	std::shared_ptr<DBConnection> GetConnection();
	void ReturnConnection(std::shared_ptr<DBConnection> connection);	// 일단 사용 X

private:
	SQLHENV m_environment;
	concurrency::concurrent_vector<std::shared_ptr<DBConnection>> m_connections;


	DBConnectionPool() = default;
	~DBConnectionPool()
	{
		Clear();
	}

	DBConnectionPool(const DBConnectionPool&) = delete;
	DBConnectionPool& operator=(const DBConnectionPool&) = delete;
};

