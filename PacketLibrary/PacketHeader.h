#pragma once
//#pragma pack (push, 1)

#include "PacketID.h"

struct PacketHeader
{
	unsigned short length;
	PacketID packetId;

	PacketHeader() = default;
	PacketHeader(short length, PacketID id)
		: length(length), packetId(id) { }

	// Packet to char*
	void Serialize(char* buffer) const
	{
		std::memcpy(buffer, &length, sizeof(length));
		std::memcpy(buffer + sizeof(length), &packetId, sizeof(packetId));
	}

	// char* to Packet
	static PacketHeader Deserialize(const char* buffer)
	{
		short _length;
		PacketID _type;

		std::memcpy(&_length, buffer, sizeof(_length));
		std::memcpy(&_type, buffer + sizeof(_length), sizeof(_type));

		return PacketHeader(_length, _type);
	}

	static constexpr short Size()
	{
		return sizeof(length) + sizeof(packetId);
	}
};

//#pragma pack(pop)