#include "pch.h"
#include "DBConnectionPool.h"

bool DBConnectionPool::Connect(int connectionCount, const WCHAR* connectionString)
{
	if (::SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &m_environment) != SQL_SUCCESS)
	{
		LOG_DB("SQLHENV: SQLAllocHandle failed");
		return false;
	}

	if(::SQLSetEnvAttr(m_environment, SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0) != SQL_SUCCESS)
	{
		LOG_DB("SQLSetEnvAttr failed");
		return false;
	}

	for(int i=0; i<connectionCount; i++)
	{
		std::shared_ptr<DBConnection> connection = std::make_shared<DBConnection>();
		if (!connection->Connect(m_environment, connectionString))
			return false;

		m_connections.push_back(connection);
	}

	return true;
}

void DBConnectionPool::Clear()
{
	if(m_environment != SQL_NULL_HANDLE)
	{
		::SQLFreeHandle(SQL_HANDLE_ENV, m_environment); 
		m_environment = SQL_NULL_HANDLE;
	}

	for (const auto& connection : m_connections)
	{
		connection->Clear();
	}
	m_connections.clear();
}

// 반드시 유효한 커넥션을 반환한다.
std::shared_ptr<DBConnection> DBConnectionPool::GetConnection()
{
	while(true)
	{
		for(const auto& con : m_connections)
		{
			if (con.use_count() == 1)
				return con;
		}

		std::this_thread::yield();
	}
}

void DBConnectionPool::ReturnConnection(std::shared_ptr<DBConnection> connection)
{
	connection.reset();
}