#ifndef PCH_H
#define PCH_H

#include "define.h"

/// Network
#include <WinSock2.h>
#include <mswsock.h>
#include <WS2tcpip.h>

/// windows
#include <Windows.h>

/// IO
#include <iostream>

/// String
#include <string>
#include <string_view>

/// Thread
#include <thread>

/// Container
#include <vector>

/// packet
#include <google/protobuf/message.h>
#include "../PacketLibrary/PacketHeader.h"
#include "../PacketLibrary/PacketBuilder.h"

#endif //PCH_H