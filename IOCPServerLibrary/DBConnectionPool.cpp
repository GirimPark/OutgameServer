#include "pch.h"
#include "DBConnectionPool.h"

#include "../UtilityLibrary/Logger.h"

bool DBConnectionPool::Connect(int connectionCount, const WCHAR* connectionString)
{
	if (::SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &m_environment) != SQL_SUCCESS)
	{
		PRINT_DB("SQLHENV: SQLAllocHandle failed");
		return false;
	}

	if(::SQLSetEnvAttr(m_environment, SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0) != SQL_SUCCESS)
	{
		PRINT_DB("SQLSetEnvAttr failed");
		return false;
	}

	for(int i=0; i<connectionCount; i++)
	{
		DBConnection* connection = new DBConnection;
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
		delete connection;
	}
	m_connections.clear();
}

// 반드시 유효한 커넥션을 반환한다.
DBConnection* DBConnectionPool::GetConnection()
{
	ASSERT_CRASH(!m_connections.empty());

	while(true)
	{
		for (const auto& con : m_connections)
		{
			bool expected = true;
			if (con->m_bUsable.compare_exchange_strong(expected, false))
			{
				++m_getCnt;
				return con;
			}
		}

		if ((m_getCnt - m_returnCnt) > 5)
		{
			LOG_WARNING("DBConnectionPool GetConnectionCount and ReturnConnectionCount have difference over 5 counts");
			PRINT_DB("DBConnectionPool GetConnectionCount and ReturnConnectionCount have difference over 5 counts");
		}
		std::this_thread::yield();
	}
}

void DBConnectionPool::ReturnConnection(DBConnection* connection)
{
	connection->m_bUsable.store(true);
	++m_returnCnt;
}