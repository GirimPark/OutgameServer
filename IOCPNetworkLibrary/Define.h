#pragma once

#define LOG_NETWORK_CORE(message) \
{\
	std::string file = __FILE__;\
	file = file.substr(file.find_last_of("/\\") + 1);\
	std::cout<<"[NetworkCore] FILE : "<<file.c_str()<<", LINE : "<<__LINE__<<"\n[NetworkCore] MESSAGE : "<<(message)<<std::endl<<std::endl;\
}