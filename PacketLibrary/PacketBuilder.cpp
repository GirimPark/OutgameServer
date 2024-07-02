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
    // ��� ����ȭ
    header.Serialize(packet);
    // ������ ����
    std::memcpy(packet + header.Size(), serializedMessage.data(), dataSize);

    return packet;
}

bool PacketBuilder::Deserialize(const char* buffer, size_t size, PacketHeader& header, DataPacket& dataPacket)
{
    if (size < PacketHeader::Size()) 
    {
        return false; // ���ŵ� ũ�Ⱑ �ʹ� ����
    }

    header = PacketHeader::Deserialize(buffer);
    std::string data(buffer + PacketHeader::Size(), size - PacketHeader::Size());

    return dataPacket.ParseFromString(data);
}
