#include "PacketBuilder.h"
#include "PacketHeader.h"

PacketHeader PacketBuilder::CreateHeader(PacketID type, int dataSize)
{
    short length = PacketHeader::Size() + dataSize;
    return PacketHeader(length, type);
}

char* PacketBuilder::Serialize(PacketID type, const Payload& dataPacket)
{
    std::string serializedMessage;
    dataPacket.SerializeToString(&serializedMessage);

    short dataSize = serializedMessage.size();
    short length = PacketHeader::Size() + dataSize;
    PacketHeader header = CreateHeader(type, dataSize);

    char* packet = new char[length];
    // ��� ����ȭ
    header.Serialize(packet);
    // ������ ����
    std::memcpy(packet + header.Size(), serializedMessage.data(), dataSize);
    
    return packet;
}

bool PacketBuilder::Deserialize(const char* buffer, short size, PacketHeader& header, Payload& data)
{
    if (size < PacketHeader::Size())
    {
        return false; // �����Ͱ� �ʹ� ����
    }
    header = PacketHeader::Deserialize(buffer);

    if (size < header.length)
    {
        return false; // �����Ͱ� �ʹ� ����
    }
    std::string rt(buffer + PacketHeader::Size(), size - PacketHeader::Size());
    return data.ParseFromString(rt);
}

bool PacketBuilder::DeserializeHeader(const char* buffer, short size, PacketHeader& header)
{
    if (size < PacketHeader::Size()) 
    {
        return false; // �����Ͱ� �ʹ� ����
    }
    header = PacketHeader::Deserialize(buffer);
    return true;
}

bool PacketBuilder::DeserializeData(const char* buffer, short size, const PacketHeader& header, Payload& data)
{
    if (size < header.length) 
    {
        return false; // �����Ͱ� �ʹ� ����
    }
    std::string rt(buffer + PacketHeader::Size(), size - PacketHeader::Size());
    return data.ParseFromString(rt);
}
