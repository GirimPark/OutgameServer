#pragma once

#include "DBConnection.h"

// Bind 수가 유효한지 확인하는 재귀적 구조체
template<int C>
struct FullBits { enum { value = (1 << (C - 1)) | FullBits<C - 1>::value }; };

template<>
struct FullBits<1> { enum { value = 1 }; };

template<>
struct FullBits<0> { enum { value = 0 }; };

/// DBBind
///	내부에서 index와 그에 따른 SQLLEN을 매칭하여 들고 있는다.
///	사용자가 처음 계획한 수만큼 Param, Column이 Bind 되었는지 확인하여 실수를 최소화 한다.
template<int ParamCount, int ColumnCount>
class DBBind
{
public:
	DBBind(DBConnection* dbConnection, const WCHAR* query)
		: m_DBConnection(dbConnection), m_query(query)
	{
		::memset(m_paramIndex, 0, sizeof(m_paramIndex));
		::memset(m_columnIndex, 0, sizeof(m_columnIndex));
		m_paramFlag = 0;
		m_columnFlag = 0;
		m_DBConnection->Unbind();
	}

	bool Validata()
	{
		int a = FullBits<ParamCount>::value;
		int b = FullBits<ColumnCount>::value;
		return m_paramFlag == FullBits<ParamCount>::value && m_columnFlag == FullBits<ColumnCount>::value;
	}

	bool Execute()
	{
		ASSERT_CRASH(Validata());
		return m_DBConnection->Execute(m_query);
	}

	DB_RESULT Fetch()
	{
		return m_DBConnection->Fetch();
	}

	// Bind Functions
	template<typename T>
	void BindParam(int idx, T& value)
	{
		m_DBConnection->BindParam(idx + 1, &value, &m_paramIndex[idx]);
		m_paramFlag |= (1 << idx);
	}

	void BindParam(int idx, const WCHAR* value)
	{
		m_DBConnection->BindParam(idx + 1, value, &m_paramIndex[idx]);
		m_paramFlag |= (1 << idx);
	}

	template<typename T, int N>
	void BindParam(int idx, T(&value)[N])
	{
		m_DBConnection->BindParam(idx + 1, (const BYTE*)value, sizeof(T) * N, &m_paramIndex[idx]);
		m_paramFlag |= (1 << idx);
	}

	template<typename T>
	void BindParam(int idx, T* value, int N)
	{
		m_DBConnection->BindParam(idx + 1, (const BYTE*)value, sizeof(T) * N, &m_paramIndex[idx]);
		m_paramFlag |= (1 << idx);
	}

	template<typename T>
	void BindCol(int idx, T& value)
	{
		m_DBConnection->BindCol(idx + 1, &value, &m_columnIndex[idx]);
		m_columnFlag |= (1 << idx);
	}

	template<int N>
	void BindCol(int idx, WCHAR(&value)[N])
	{
		m_DBConnection->BindCol(idx + 1, value, N - 1, &m_columnIndex[idx]);
		m_columnFlag |= (1 << idx);
	}

	void BindCol(int idx, WCHAR* value, int len)
	{
		m_DBConnection->BindCol(idx + 1, value, len - 1, &m_columnIndex[idx]);
		m_columnFlag |= (1 << idx);
	}

	template<typename T, int N>
	void BindCol(int idx, T(&value)[N])
	{
		m_DBConnection->BindCol(idx + 1, value, sizeof(T) * N, &m_columnIndex[idx]);
		m_columnFlag |= (1 << idx);
	}

private:
	DBConnection* m_DBConnection;
	const WCHAR* m_query;
	SQLLEN m_paramIndex[ParamCount > 0 ? ParamCount : 1];
	SQLLEN m_columnIndex[ColumnCount > 0 ? ColumnCount : 1];
	unsigned int m_paramFlag;
	unsigned int m_columnFlag;
};

