#ifdef _DEBUG
#pragma comment(lib, "vld.lib")

#pragma comment(lib, "libprotobufd.lib")
#else
#pragma comment(lib, "libprotobuf.lib")
#endif

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Mswsock.lib")
#pragma comment(lib, "dbghelp.lib")

#include "pch.h"
#include "OutgameServer.h"

#include <DbgHelp.h>
#include <sstream>
#include <iomanip>

#ifdef _DEBUG
#include <vld/vld.h>
#endif

//#include "../UtilityLibrary/Logger.h"

void CreateDirectoryIfNotExists(const std::wstring& dir);
void CreateMiniDump(EXCEPTION_POINTERS* pep);
LONG WINAPI MyExceptionFilter(EXCEPTION_POINTERS* pExceptionPointers);

int main()
{
    SetUnhandledExceptionFilter(MyExceptionFilter);
    //LOG_INFO("test log");
	OutgameServer::Instance().Start();

    system("pause");
    return 0;
}

void CreateDirectoryIfNotExists(const std::wstring& dir)
{
    DWORD ftyp = GetFileAttributesW(dir.c_str());
    if (ftyp == INVALID_FILE_ATTRIBUTES) {
        CreateDirectoryW(dir.c_str(), nullptr);
    }
}

void CreateMiniDump(EXCEPTION_POINTERS* pep)
{
    std::wstring dumpDir = L".\\Dump";
    CreateDirectoryIfNotExists(dumpDir);

    SYSTEMTIME lt;
    GetLocalTime(&lt);

    std::wstringstream wss;
    wss << dumpDir << L"\\dump_"
        << lt.wYear << L"-"
        << std::setw(2) << std::setfill(L'0') << lt.wMonth << L"-"
        << std::setw(2) << std::setfill(L'0') << lt.wDay << L"_"
        << std::setw(2) << std::setfill(L'0') << lt.wHour << L"-"
        << std::setw(2) << std::setfill(L'0') << lt.wMinute << L"-"
        << std::setw(2) << std::setfill(L'0') << lt.wSecond << L".dmp";

    std::wstring dumpFile = wss.str();

    HANDLE hFile = CreateFile(dumpFile.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        std::cerr << "Could not create dump file\n";
        return;
    }

    MINIDUMP_EXCEPTION_INFORMATION mdei;
    mdei.ThreadId = GetCurrentThreadId();
    mdei.ExceptionPointers = pep;
    mdei.ClientPointers = FALSE;

    MINIDUMP_TYPE mdt = MiniDumpNormal;

    BOOL rv = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile, mdt, (pep != nullptr) ? &mdei : nullptr, nullptr, nullptr);
    if (!rv)
    {
        std::cerr << "MiniDumpWriteDump failed\n";
    }
    else
    {
        std::cout << "Minidump created\n";
    }

    CloseHandle(hFile);
}

LONG WINAPI MyExceptionFilter(EXCEPTION_POINTERS* pExceptionPointers)
{
    CreateMiniDump(pExceptionPointers);
    return EXCEPTION_EXECUTE_HANDLER;
}