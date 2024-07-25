#pragma once

enum EPacketType : short;
struct PacketHeader;

typedef google::protobuf::Message PacketData;

class PacketBuilder {
public:
    static PacketBuilder& Instance()
	{
        static PacketBuilder instance;
        return instance;
    }

    PacketHeader CreateHeader(EPacketType type, int dataSize);

    char* Serialize(EPacketType type, const PacketData& dataPacket);
    bool Deserialize(const char* buffer, short size, PacketHeader& header, PacketData& data);
    bool DeserializeHeader(const char* buffer, short size, PacketHeader& header);
    bool DeserializeData(const char* buffer, short size, const PacketHeader& header, PacketData& data);

private:
    PacketBuilder() = default;
    ~PacketBuilder() = default;

    PacketBuilder(const PacketBuilder&) = delete;
    PacketBuilder& operator=(const PacketBuilder&) = delete;
};