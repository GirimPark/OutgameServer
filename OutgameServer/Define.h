#pragma once

#define PRINT_CONTENTS(message) \
{\
	std::string file = __FILE__;\
	file = file.substr(file.find_last_of("/\\") + 1);\
	std::cout<<"[Contents] FILE : "<<file.c_str()<<", LINE : "<<__LINE__<<"\n[Contents] MESSAGE : "<<(message)<<std::endl<<std::endl;\
}

//#define REGISTER_PACKET_DATA(className) \
//        PacketDataFactory::RegisterPacketDataClass(#className, []() -> std::shared_ptr<Payload> { \
//            return std::make_shared<className>(); \
//        })