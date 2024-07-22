#pragma once

#include "../IOCPNetworkLibrary/Define.h"

typedef unsigned int SessionId;

#define MAX_THREAD 64
#define WVARCHAR_MAX 4000
#define BINARY_MAX 8000

#define LOG_SERVER_CORE(message) \
{\
	std::string file = __FILE__;\
	file = file.substr(file.find_last_of("/\\") + 1);\
	printf("[ServerCore] FILE : %s, LINE : %d\n[ServerCore] MESSAGE : %s\n\n", file.c_str(), __LINE__, message);\
}

#define LOG_DB(message) \
{\
	std::string file = __FILE__;\
	file = file.substr(file.find_last_of("/\\") + 1);\
	printf("[Database] FILE : %s, LINE : %d\n[Database] MESSAGE : %s\n\n", file.c_str(), __LINE__, message);\
}