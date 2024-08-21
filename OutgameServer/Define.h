#pragma once

#define LOG_CONTENTS(message) \
{\
	std::string file = __FILE__;\
	file = file.substr(file.find_last_of("/\\") + 1);\
	printf("[Contents] FILE : %s, LINE : %d\n[Contents] MESSAGE : %s\n\n", file.c_str(), __LINE__, message);\
}

//#define REGISTER_PACKET_DATA(className) \
//        PacketDataFactory::RegisterPacketDataClass(#className, []() -> std::shared_ptr<Payload> { \
//            return std::make_shared<className>(); \
//        })