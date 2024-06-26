#include "pch.h"
#include "EchoClient.h"

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Mswsock.lib")

int main()
{
	EchoClient client("localhost","5001");
	client.Run();
}