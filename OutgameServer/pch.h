#ifndef PCH_H
#define PCH_H

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

/// Network
#include <WinSock2.h>	// winsock과의 재정의 문제로 가장 먼저 컴파일 되어야함
#include <mswsock.h>
#include <WS2tcpip.h>

/// windows
#include <Windows.h>

#include <memory>

/// IO
#include <iostream>

/// Thread
#include <thread>

/// container
#include <vector>

/// concurrent container
#include <concurrent_unordered_map.h>
#include <concurrent_queue.h>
#include <concurrent_vector.h>

/// protobuf
#include <google/protobuf/message.h>

/// sql
#include <sql.h>
#include <sqlext.h>

/// custom
#include "Define.h"
#include "SharedStructure.h"

#include "OutgameServer.h"

#include "../IOCPServerLibrary/Define.h"
#include "../IOCPServerLibrary/CompletionKey.h"
#include "../IOCPServerLibrary/ServerCore.h"
#include "../IOCPServerLibrary/DBConnectionPool.h"
#include "../IOCPServerLibrary/DBBind.h"

#include "../PacketLibrary/PacketHeader.h"
#include "../PacketLibrary/PacketBuilder.h"
#include "../PacketLibrary/Protocol.pb.h"

#endif //PCH_H