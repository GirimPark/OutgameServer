#pragma once

// Ư�� Ŭ���� Ÿ������ �����ϴ��� �ش� Ÿ�����δ� ������ �� ����.
// �ᱹ dynamic_cast�� �ؾ��ϹǷ� ���
class PacketDataFactory
{
public:
	using CreatePacketDataFunc = std::shared_ptr<Payload>(*)();

	PacketDataFactory();
	~PacketDataFactory() = default;

	static void RegisterPacketDataClass(const std::string& className, CreatePacketDataFunc func);

	static std::shared_ptr<Payload> CreateInstance(const std::string& className);

private: 
	static std::unordered_map<std::string, CreatePacketDataFunc>& GetClassMap();
};
