#include "pch.h"
#include "../NetworkLibrary/IOCPNetwork.h"

/// link
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Mswsock.lib")
//#pragma comment(lib, "tbb12.lib")
#pragma comment(lib, "tbb12_debug.lib")
//#pragma comment(lib, "tbbmalloc.lib")

int main()
{
	IOCPNetwork test;
	test.Run();
}