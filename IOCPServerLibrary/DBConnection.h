#pragma once

enum class DB_RESULT
{
	SUCCESS,
	DB_NO_DATA,
	FAILED
};

class DBConnection
{
public:
	DBConnection() = default;
	~DBConnection() = default;

	bool Connect(SQLHENV henv, const WCHAR* connectionString);
	void Clear();

	bool Execute(const WCHAR* query);
	bool ExecuteFile(const std::string_view fileName);
	DB_RESULT Fetch();

	void Unbind();
	int GetRowCount();

	bool			BindParam(int paramIndex, bool* value, SQLLEN* index);
	bool			BindParam(int paramIndex, float* value, SQLLEN* index);
	bool			BindParam(int paramIndex, double* value, SQLLEN* index);
	bool			BindParam(int paramIndex, char* value, SQLLEN* index);
	bool			BindParam(int paramIndex, short* value, SQLLEN* index);
	bool			BindParam(int paramIndex, int* value, SQLLEN* index);
	bool			BindParam(int paramIndex, long long* value, SQLLEN* index);
	bool			BindParam(int paramIndex, TIMESTAMP_STRUCT* value, SQLLEN* index);
	bool			BindParam(int paramIndex, const WCHAR* str, SQLLEN* index);
	bool			BindParam(int paramIndex, const BYTE* bin, int size, SQLLEN* index);

	bool			BindCol(int columnIndex, bool* value, SQLLEN* index);
	bool			BindCol(int columnIndex, float* value, SQLLEN* index);
	bool			BindCol(int columnIndex, double* value, SQLLEN* index);
	bool			BindCol(int columnIndex, char* value, SQLLEN* index);
	bool			BindCol(int columnIndex, short* value, SQLLEN* index);
	bool			BindCol(int columnIndex, int* value, SQLLEN* index);
	bool			BindCol(int columnIndex, long long* value, SQLLEN* index);
	bool			BindCol(int columnIndex, TIMESTAMP_STRUCT* value, SQLLEN* index);
	bool			BindCol(int columnIndex, WCHAR* str, int size, SQLLEN* index);
	bool			BindCol(int columnIndex, BYTE* bin, int size, SQLLEN* index);

private:
	bool BindParam(SQLUSMALLINT paramIndex, SQLSMALLINT cType, SQLSMALLINT sqlType, SQLULEN len, SQLPOINTER ptr, SQLLEN* index);
	bool BindCol(SQLSMALLINT columnIndex, SQLSMALLINT cType, SQLULEN len, SQLPOINTER value, SQLLEN* index);

	void HandleError(SQLRETURN rt);

private:
	SQLHDBC m_connection;
	SQLHSTMT m_statement;

public:
	std::atomic<bool> m_bUsable = true;
};

