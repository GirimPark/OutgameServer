#pragma once

enum PacketID : unsigned short;
struct PacketHeader;

#include <google/protobuf/message.h>
typedef google::protobuf::Message Payload;

class PacketBuilder {
public:
    static PacketBuilder& Instance()
	{
        static PacketBuilder instance;
        return instance;
    }

    PacketHeader CreateHeader(PacketID type, int dataSize);

    char* Serialize(PacketID type, const Payload& dataPacket);
    bool Deserialize(const char* buffer, short size, PacketHeader& header, Payload& data);
    bool DeserializeHeader(const char* buffer, short size, PacketHeader& header);
    bool DeserializeData(const char* buffer, short size, const PacketHeader& header, Payload& data);

private:
    PacketBuilder() = default;
    ~PacketBuilder() = default;

    PacketBuilder(const PacketBuilder&) = delete;
    PacketBuilder& operator=(const PacketBuilder&) = delete;
};