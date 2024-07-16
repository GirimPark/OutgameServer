#pragma once

struct Session;
struct PacketHeader;
typedef google::protobuf::Message DataPacket;

enum class ESendType
{
	BROADCAST,
	UNICAST
};

struct ReceiveStruct
{
	Session* session;
	std::shared_ptr<DataPacket> data;

	~ReceiveStruct()
	{
		data.reset();
	}
};

struct SendStruct
{
	ESendType type;
	Session* session;
	std::shared_ptr<PacketHeader> header;
	std::shared_ptr<DataPacket> data;

	~SendStruct()
	{
		header.reset();
		data.reset();
	}
};