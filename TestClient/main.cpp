#include "pch.h"
#include "EchoClient.h"
#include "LoginClient.h"
#include "MemoryPoolTestClient.h"

#ifdef _DEBUG
#pragma comment(lib, "libprotobufd.lib")
#else
#pragma comment(lib, "libprotobuf.lib")
#endif

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Mswsock.lib")

int main()
{
	MemoryPoolTestClient client("localhost","5001", 100, 3000);
	client.Run();

	system("pause");
	return 0;
}