#include "pch.h"
#include "../NetworkLibrary/IOCPNetwork.h"
#include "../IOCPServerLibrary/ServerCore.h"

#ifdef _DEBUG
#include <vld/vld.h>
#endif

/// link
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Mswsock.lib")
#pragma comment(lib, "tbb12.lib")
#pragma comment(lib, "tbb12_debug.lib")
#pragma comment(lib, "vld.lib")

int main()
{
	ServerCore server("5001");
	server.Run();
}