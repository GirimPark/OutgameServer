#include "pch.h"
#include "PacketBuilder.h"

char* PacketBuilder::Serialize(EPacketType type, const DataPacket& dataPacket)
{
    std::string serializedMessage;
    dataPacket.SerializeToString(&serializedMessage);

    uint32_t dataSize = serializedMessage.size();
    uint32_t length = PacketHeader::Size() + dataSize;
    PacketHeader header(length, type);

    char* packet = new char[length];
    // 헤더 직렬화
    header.Serialize(packet);
    // 데이터 복사
    std::memcpy(packet + header.Size(), serializedMessage.data(), dataSize);

    return packet;
}

bool PacketBuilder::Deserialize(const char* buffer, size_t size, PacketHeader& header, DataPacket& dataPacket)
{
    if (size < PacketHeader::Size()) 
    {
        return false; // 수신된 크기가 너무 작음
    }

    header = PacketHeader::Deserialize(buffer);
    std::string data(buffer + PacketHeader::Size(), size - PacketHeader::Size());

    return dataPacket.ParseFromString(data);
}
