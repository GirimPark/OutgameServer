#pragma once

enum EPacketType;
struct PacketHeader;

typedef google::protobuf::Message DataPacket;

class PacketBuilder {
public:
    // Singleton
    static PacketBuilder& Instance() {
        static PacketBuilder instance;
        return instance;
    }

    char* Serialize(EPacketType type, const DataPacket& dataPacket);
    bool Deserialize(const char* buffer, size_t size, PacketHeader& header, DataPacket& dataPacket);

private:
    PacketBuilder() {}
    ~PacketBuilder() {}

    PacketBuilder(const PacketBuilder&) = delete;
    PacketBuilder& operator=(const PacketBuilder&) = delete;
};