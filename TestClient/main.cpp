#include "pch.h"
#include "EchoClient.h"

#ifdef _DEBUG
#pragma comment(lib, "libprotobufd.lib")
#else
#pragma comment(lib, "libprotobuf.lib")
#endif

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Mswsock.lib")

int main()
{
	EchoClient client("localhost","5001");
	client.Run();
}