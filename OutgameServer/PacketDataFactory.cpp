#include "pch.h"
#include "PacketDataFactory.h"

PacketDataFactory::PacketDataFactory()
{
	REGISTER_PACKET_DATA(Protocol::C2S_ValidationResponse);
	REGISTER_PACKET_DATA(Protocol::C2S_LoginRequest);
}

void PacketDataFactory::RegisterPacketDataClass(const std::string& className, CreatePacketDataFunc func)
{
	GetClassMap()[className] = func;
}

std::shared_ptr<PacketData> PacketDataFactory::CreateInstance(const std::string& className)
{
	auto it = GetClassMap().find(className);
	if(it != GetClassMap().end())
	{
		return it->second();
	}

	LOG_CONTENTS("PacketDataFactory::CreateInstance Failed");
	return nullptr;
}

std::unordered_map<std::string, PacketDataFactory::CreatePacketDataFunc>& PacketDataFactory::GetClassMap()
{
	static std::unordered_map<std::string, CreatePacketDataFunc> classMap;
	return classMap;
}
