#pragma once

typedef std::function<void(std::shared_ptr<ReceiveStruct>)> PacketHandlerCallback;

class PacketHandler
{
public:
	PacketHandler() = default;
	~PacketHandler() = default;

	void RegisterHandler(EPacketType headerType, PacketHandlerCallback callback);
	void HandlePacket(Session* session, const char* data, int nReceivedByte);

private:
	std::unordered_map<EPacketType, PacketHandlerCallback> m_packetCallbacks;
};