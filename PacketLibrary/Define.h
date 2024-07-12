#pragma once

#define LOG_PACKET(message) \
{\
	std::string file = __FILE__;\
	file = file.substr(file.find_last_of("/\\") + 1);\
	printf("[Packet] FILE : %s, LINE : %d\n[Packet] MESSAGE : %s\n\n", file.c_str(), __LINE__, message);\
}