#include "pch.h"
#include "DBConnection.h"

#include <fstream>
#include <filesystem>
namespace fs = std::filesystem;

bool DBConnection::Connect(SQLHENV henv, const WCHAR* connectionString)
{
	if (::SQLAllocHandle(SQL_HANDLE_DBC, henv, &m_connection) != SQL_SUCCESS)
	{
		LOG_DB("SQLHDBC : SQLAllocHandle failed");
		return false;
	}

	WCHAR stringBuffer[MAX_PATH] = { 0 };
	::wcscpy_s(stringBuffer, connectionString);

	WCHAR resultString[MAX_PATH] = { 0 };
	SQLSMALLINT resultStringLen = 0;

	SQLRETURN rt;
	if((rt = SQLDriverConnectW(
		m_connection,
		NULL,
		stringBuffer,
		_countof(stringBuffer),
		OUT resultString,
		_countof(resultString),
		OUT &resultStringLen,
		SQL_DRIVER_NOPROMPT
	)) != (SQL_SUCCESS || SQL_SUCCESS_WITH_INFO))
	{
		LOG_DB("SQLDriverConnect failed");
		HandleError(rt);
		return false;
	}

	if ((rt=::SQLAllocHandle(SQL_HANDLE_STMT, m_connection, &m_statement)) != SQL_SUCCESS)
	{
		LOG_DB("SQLHSTMT : SQLAllocHandle failed");
		HandleError(rt);
		return false;
	}

	return (rt == SQL_SUCCESS || rt == SQL_SUCCESS_WITH_INFO);
}

void DBConnection::Clear()
{
	if(m_connection != SQL_NULL_HANDLE)
	{
		::SQLFreeHandle(SQL_HANDLE_DBC, m_connection);
		m_connection = SQL_NULL_HANDLE;
	}

	if (m_statement != SQL_NULL_HANDLE)
	{
		::SQLFreeHandle(SQL_HANDLE_STMT, m_statement);
		m_statement = SQL_NULL_HANDLE;
	}
}

bool DBConnection::Execute(const WCHAR* query)
{
	SQLRETURN rt = ::SQLExecDirectW(m_statement, (SQLWCHAR*)query, SQL_NTSL);
	if (rt == SQL_SUCCESS || rt == SQL_SUCCESS_WITH_INFO || rt == SQL_NO_DATA)
		return true;

	HandleError(rt);
	return false;
}

bool DBConnection::ExecuteFile(const std::string_view fileName)
{
	std::string filePath = "..\\DatabaseTable\\" + std::string(fileName);
	try
	{
		if(!fs::exists(filePath))
		{
			LOG_DB("File does not exist");
			return false;
		}

		std::ifstream file(filePath);
		if (!file.is_open()) {
			LOG_DB("Failed to open file");
			return false;
		}

		std::stringstream buffer;
		buffer << file.rdbuf();
		std::string query = buffer.str();
		file.close();

		SQLRETURN rt = SQLExecDirectA(m_statement, (SQLCHAR*)query.c_str(), SQL_NTS);
		if (rt == SQL_SUCCESS || rt == SQL_SUCCESS_WITH_INFO)
			return true;

		HandleError(rt);
		return false;
	}
	catch (const fs::filesystem_error& e) 
	{
		LOG_DB("Filesystem error");
		return false;
	}
}

DB_RESULT DBConnection::Fetch()
{
	SQLRETURN rt = ::SQLFetch(m_statement);

	switch(rt)
	{
	case SQL_SUCCESS:
	case SQL_SUCCESS_WITH_INFO:
		return DB_RESULT::SUCCESS;
	case SQL_NO_DATA:
		return DB_RESULT::DB_NO_DATA;
	case SQL_ERROR:
		HandleError(rt);
		return DB_RESULT::FAILED;
	default:
		return DB_RESULT::SUCCESS;
	}
}

void DBConnection::Unbind()
{
	::SQLFreeStmt(m_statement, SQL_UNBIND);
	::SQLFreeStmt(m_statement, SQL_RESET_PARAMS);
	::SQLFreeStmt(m_statement, SQL_CLOSE);
}

int DBConnection::GetRowCount()
{
	SQLLEN count = 0;
	SQLRETURN rt = ::SQLRowCount(m_statement, OUT &count);

	if (rt == SQL_SUCCESS || rt == SQL_SUCCESS_WITH_INFO)
		return static_cast<int>(count);

	return -1;
}

bool DBConnection::BindParam(int paramIndex, bool* value, SQLLEN* index)
{
	return BindParam(paramIndex, SQL_C_TINYINT, SQL_TINYINT, sizeof(bool), value, index);
}

bool DBConnection::BindParam(int paramIndex, float* value, SQLLEN* index)
{
	return BindParam(paramIndex, SQL_C_FLOAT, SQL_REAL, 0, value, index);
}

bool DBConnection::BindParam(int paramIndex, double* value, SQLLEN* index)
{
	return BindParam(paramIndex, SQL_C_DOUBLE, SQL_DOUBLE, 0, value, index);
}

bool DBConnection::BindParam(int paramIndex, char* value, SQLLEN* index)
{
	return BindParam(paramIndex, SQL_C_TINYINT, SQL_TINYINT, sizeof(char), value, index);
}

bool DBConnection::BindParam(int paramIndex, short* value, SQLLEN* index)
{
	return BindParam(paramIndex, SQL_C_SHORT, SQL_SMALLINT, sizeof(short), value, index);
}

bool DBConnection::BindParam(int paramIndex, int* value, SQLLEN* index)
{
	return BindParam(paramIndex, SQL_C_LONG, SQL_INTEGER, sizeof(int), value, index);
}

bool DBConnection::BindParam(int paramIndex, long long* value, SQLLEN* index)
{
	return BindParam(paramIndex, SQL_C_SBIGINT, SQL_BIGINT, sizeof(long long), value, index);
}

bool DBConnection::BindParam(int paramIndex, TIMESTAMP_STRUCT* value, SQLLEN* index)
{
	return BindParam(paramIndex, SQL_C_TYPE_TIMESTAMP, SQL_TYPE_TIMESTAMP, sizeof(TIMESTAMP_STRUCT), value, index);
}

bool DBConnection::BindParam(int paramIndex, const WCHAR* str, SQLLEN* index)
{
	SQLULEN size = static_cast<SQLULEN>((::wcslen(str) + 1) * 2);
	*index = SQL_NTSL;

	if (size > WVARCHAR_MAX)
		return BindParam(paramIndex, SQL_C_WCHAR, SQL_WLONGVARCHAR, size, (SQLPOINTER)str, index);
	else
		return BindParam(paramIndex, SQL_C_WCHAR, SQL_WVARCHAR, size, (SQLPOINTER)str, index);
}

bool DBConnection::BindParam(int paramIndex, const BYTE* bin, int size, SQLLEN* index)
{
	if (bin == nullptr)
	{
		*index = SQL_NULL_DATA;
		size = 1;
	}
	else
		*index = size;

	if (size > BINARY_MAX)
		return BindParam(paramIndex, SQL_C_BINARY, SQL_LONGVARBINARY, size, (BYTE*)bin, index);
	else
		return BindParam(paramIndex, SQL_C_BINARY, SQL_BINARY, size, (BYTE*)bin, index);
}

bool DBConnection::BindCol(int columnIndex, bool* value, SQLLEN* index)
{
	return BindCol(columnIndex, SQL_C_TINYINT, sizeof(bool), value, index);
}

bool DBConnection::BindCol(int columnIndex, float* value, SQLLEN* index)
{
	return BindCol(columnIndex, SQL_C_FLOAT, sizeof(float), value, index);
}

bool DBConnection::BindCol(int columnIndex, double* value, SQLLEN* index)
{
	return BindCol(columnIndex, SQL_C_DOUBLE, sizeof(double), value, index);
}

bool DBConnection::BindCol(int columnIndex, char* value, SQLLEN* index)
{
	return BindCol(columnIndex, SQL_C_TINYINT, sizeof(char), value, index);
}

bool DBConnection::BindCol(int columnIndex, short* value, SQLLEN* index)
{
	return BindCol(columnIndex, SQL_C_SHORT, sizeof(short), value, index);
}

bool DBConnection::BindCol(int columnIndex, int* value, SQLLEN* index)
{
	return BindCol(columnIndex, SQL_C_LONG, sizeof(int), value, index);
}

bool DBConnection::BindCol(int columnIndex, long long* value, SQLLEN* index)
{
	return BindCol(columnIndex, SQL_C_SBIGINT, sizeof(long long), value, index);
}

bool DBConnection::BindCol(int columnIndex, TIMESTAMP_STRUCT* value, SQLLEN* index)
{
	return BindCol(columnIndex, SQL_C_TYPE_TIMESTAMP, sizeof(TIMESTAMP_STRUCT), value, index);
}

bool DBConnection::BindCol(int columnIndex, WCHAR* str, int size, SQLLEN* index)
{
	return BindCol(columnIndex, SQL_C_WCHAR, size, str, index);
}

bool DBConnection::BindCol(int columnIndex, BYTE* bin, int size, SQLLEN* index)
{
	return BindCol(columnIndex, SQL_BINARY, size, bin, index);
}


bool DBConnection::BindParam(SQLUSMALLINT paramIndex, SQLSMALLINT cType, SQLSMALLINT sqlType, SQLULEN len,
                             SQLPOINTER ptr, SQLLEN* index)
{
	SQLRETURN rt = ::SQLBindParameter(m_statement, paramIndex, SQL_PARAM_INPUT, cType, sqlType, len, 0, ptr, 0, index);
	if (rt != SQL_SUCCESS && rt != SQL_SUCCESS_WITH_INFO)
	{
		HandleError(rt);
		return false;
	}

	return true;
}

bool DBConnection::BindCol(SQLSMALLINT columnIndex, SQLSMALLINT cType, SQLULEN len, SQLPOINTER value, SQLLEN* index)
{
	SQLRETURN rt = ::SQLBindCol(m_statement, columnIndex, cType, value, len, index);
	if (rt != SQL_SUCCESS && rt != SQL_SUCCESS_WITH_INFO)
	{
		HandleError(rt);
		return false;
	}

	return true;
}

void DBConnection::HandleError(SQLRETURN rt)
{
	if (rt == SQL_SUCCESS)
		return;

	SQLSMALLINT index = 1;
	SQLWCHAR sqlState[MAX_PATH] = { 0 };
	SQLINTEGER nativeErr = 0;
	SQLWCHAR errMsg[MAX_PATH] = { 0 };
	SQLSMALLINT msgLen = 0;
	SQLRETURN errorRt = 0;

	while(true)
	{
		errorRt = ::SQLGetDiagRecW(
			SQL_HANDLE_STMT,
			m_statement,
			index,
			sqlState,
			OUT &nativeErr,
			errMsg,
			_countof(errMsg),
			OUT &msgLen
		);

		if (errorRt = SQL_NO_DATA)
			break;

		if (errorRt != SQL_SUCCESS && errorRt != SQL_SUCCESS_WITH_INFO)
			break;

		char* msg = new char[msgLen];
		size_t convertedLen;
		wcstombs_s(&convertedLen, msg, msgLen, errMsg, msgLen);
		LOG_DB(msg);
		delete msg;

		index++;
	}
}
