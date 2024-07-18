#pragma once
#pragma pack (push, 1)

enum EPacketType : short
{
	S2C_ECHO = 0,
	C2S_ECHO = 1,

	S2C_VALIDATION_REQUEST,
	C2S_VALIDATION_RESPONSE,

	S2C_SESSION_EXPIRED_NOTIFICATION,

	C2S_LOGIN_REQUEST,
	S2C_LOGIN_RESPONSE,

	C2S_JOIN_REQUEST,
	S2C_JOIN_RESPONSE,


};

struct PacketHeader
{
	short length;
	EPacketType type;

	PacketHeader() = default;
	PacketHeader(short length, EPacketType id)
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
		short _length;
		EPacketType _type;

		std::memcpy(&_length, buffer, sizeof(_length));
		std::memcpy(&_type, buffer + sizeof(_length), sizeof(_type));

		return PacketHeader(_length, _type);
	}

	static constexpr short Size()
	{
		return sizeof(length) + sizeof(type);
	}
};

#pragma pack(pop)