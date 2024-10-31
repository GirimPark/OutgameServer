#pragma once

typedef unsigned int SessionId;

#define MAX_BUF_SIZE 2048
#define MAX_THREAD 64
#define IP_SIZE 16
#define INIT_DATA_SIZE 256

#define WVARCHAR_MAX 4000
#define BINARY_MAX 8000

#define PRINT_SERVER_CORE(message) \
{\
	std::string file = __FILE__;\
	file = file.substr(file.find_last_of("/\\") + 1);\
	std::cout<<"[ServerCore] FILE : "<<file.c_str()<<", LINE : "<<__LINE__<<"\n[ServerCore] MESSAGE : "<<(message)<<std::endl<<std::endl;\
}

#define PRINT_DB(message) \
{\
	std::string file = __FILE__;\
	file = file.substr(file.find_last_of("/\\") + 1);\
	std::cout<<"[Database] FILE : "<<file.c_str()<<", LINE : "<<__LINE__<<"\n[Database] MESSAGE : "<<(message)<<std::endl<<std::endl;\
}

#define CRASH(cause)						\
{											\
	unsigned int* crash = nullptr;				\
	__analysis_assume(crash != nullptr);	\
	*crash = 0xDEADBEEF;					\
}

#define ASSERT_CRASH(expr)			\
{									\
	if (!(expr))					\
	{								\
		CRASH("ASSERT_CRASH");		\
		__analysis_assume(expr);	\
	}								\
}