#pragma once

// 특정 클래스 타입으로 생성하더라도 해당 타입으로는 저장할 수 없다.
// 결국 dynamic_cast를 해야하므로 폐기
class PacketDataFactory
{
public:
	using CreatePacketDataFunc = std::shared_ptr<PacketData>(*)();

	PacketDataFactory();
	~PacketDataFactory() = default;

	static void RegisterPacketDataClass(const std::string& className, CreatePacketDataFunc func);

	static std::shared_ptr<PacketData> CreateInstance(const std::string& className);

private: 
	static std::unordered_map<std::string, CreatePacketDataFunc>& GetClassMap();
};
