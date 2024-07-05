#ifndef PCH_H
#define PCH_H

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

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

#include <concurrent_unordered_map.h>
#include <concurrent_queue.h>

/// protobuf
#include <google/protobuf/message.h>

/// custom
#include "../IOCPServerLibrary/NetworkDefine.h"
#include "../IOCPServerLibrary/CompletionKey.h"

#include "../PacketLibrary/PacketHeader.h"
#include "../PacketLibrary/PacketBuilder.h"


#endif //PCH_H