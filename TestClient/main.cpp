//#include "pch.h"
//#include "EchoClient.h"
//#include "LoginClient.h"
//#include "MemoryPoolTestClient.h"
//
//#ifdef _DEBUG
//#pragma comment(lib, "libprotobufd.lib")
//#else
//#pragma comment(lib, "libprotobuf.lib")
//#endif
//
//#pragma comment(lib, "Ws2_32.lib")
//#pragma comment(lib, "Mswsock.lib")
//
//int main(int argc, char* argv[])
//{
//	if(argc<2)
//	{
//		printf("명령 인자 개수 불충분\n");
//		system("pause");
//		return 0;
//	}
//
//	MemoryPoolTestClient client(argv[1], "5001", 200, 1);
//	client.Run();
//
//	system("pause");
//	return 0;
//}