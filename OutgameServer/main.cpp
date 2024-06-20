#include "pch.h"
#include "../NetworkLibrary/IOCPNetwork.h"
#include "../IOCPServerLibrary/ServerCore.h"

/// link
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Mswsock.lib")
#pragma comment(lib, "tbb12.lib")
#pragma comment(lib, "tbb12_debug.lib")

int main()
{
	ServerCore server("7777");
	server.Run();
}