#pragma once

#define MAX_BUF_SIZE 2048
#define IP_SIZE 16
#define INIT_DATA_SIZE 2048 //256

#define LOG_NETWORK_CORE(message) \
{\
	std::string file = __FILE__;\
	file = file.substr(file.find_last_of("/\\") + 1);\
	printf("[NetworkCore] FILE : %s, LINE : %d\n[NetworkCore] MESSAGE : %s\n\n", file.c_str(), __LINE__, message);\
}