#pragma once
#pragma pack (push, 1)

#include <iostream>

enum EPacketType : short
{
	S2C_ECHO = 0,
	C2S_ECHO = 1,


};

struct PacketHeader
{
	uint32_t length;
	EPacketType type;

	PacketHeader(uint32_t length, EPacketType id)
		: length(length), type(id) { }

	// Packet to char*
	void Serialize(char* buffer) const
	{
		std::memcpy(buffer, &length, sizeof(length));
		std::memcpy(buffer + sizeof(length), &type, sizeof(type));
	}

	// char* to Packet
	static PacketHeader Deserialize(const char* buffer)
	{
		uint32_t _length;
		EPacketType _type;

		std::memcpy(&_length, buffer, sizeof(_length));
		std::memcpy(&_type, buffer + sizeof(_length), sizeof(_type));

		return PacketHeader(_length, _type);
	}

	static constexpr size_t Size()
	{
		return sizeof(length) + sizeof(type);
	}
};

#pragma pack(pop)