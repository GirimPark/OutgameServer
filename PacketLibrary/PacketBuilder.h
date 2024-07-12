#pragma once

enum EPacketType : short;
struct PacketHeader;

typedef google::protobuf::Message DataPacket;

class PacketBuilder {
public:
    static PacketBuilder& Instance()
	{
        static PacketBuilder instance;
        return instance;
    }

    PacketHeader CreateHeader(EPacketType type, int dataSize);

    char* Serialize(EPacketType type, const DataPacket& dataPacket);
    bool Deserialize(const char* buffer, short size, PacketHeader& header, DataPacket& data);
    bool DeserializeHeader(const char* buffer, short size, PacketHeader& header);
    bool DeserializeData(const char* buffer, short size, const PacketHeader& header, DataPacket& data);

private:
    PacketBuilder() = default;
    ~PacketBuilder() = default;

    PacketBuilder(const PacketBuilder&) = delete;
    PacketBuilder& operator=(const PacketBuilder&) = delete;
};