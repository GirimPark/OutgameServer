#pragma once

struct Session;
struct PacketHeader;
typedef google::protobuf::Message Payload;

enum class ESendType
{
	BROADCAST,
	UNICAST
};

struct ReceiveStruct
{
	std::shared_ptr<PacketHeader> header;
	Session* session;
	const char* data;
	int nReceivedByte;
};

struct SendStruct
{
	ESendType type;
	Session* session;
	std::shared_ptr<PacketHeader> header;
	std::shared_ptr<Payload> data;

	~SendStruct()
	{
		//header.reset();
		//data.reset();
	}
}; 