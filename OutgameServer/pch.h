#ifndef PCH_H
#define PCH_H

#define WIN32_LEAN_AND_MEAN

#include "../NetworkLibrary/define.h"

/// Network
#include <WinSock2.h>	// winsock���� ������ ������ ���� ���� ������ �Ǿ����
#include <mswsock.h>
#include <WS2tcpip.h>

/// windows
#include <Windows.h>

/// IO
#include <iostream>

/// Thread
#include <thread>

/// container
#include <vector>
//#include "tbb/concurrent_vector.h"	// ??????????????????????
#include "tbb/concurrent_queue.h"
//#include "tbb/concurrent_hash_map.h"
//#include "tbb/concurrent_map.h"
#include "tbb/concurrent_unordered_map.h"
//#include "tbb/concurrent_set.h"

#endif //PCH_H