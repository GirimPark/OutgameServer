#pragma once

typedef std::function<void(std::shared_ptr<ReceiveStruct>)> PacketHandlerCallback;

class PacketHandler
{
public:
	PacketHandler() = default;
	~PacketHandler() = default;

	void RegisterHandler(PacketID headerType, PacketHandlerCallback callback);
	void ReceivePacket(Session* session, const char* data, int nReceivedByte);

	void Run();


private:
	std::unordered_map<PacketID, PacketHandlerCallback> m_packetCallbacks;

	concurrency::concurrent_queue<std::shared_ptr<ReceiveStruct>> m_recvQueue;
};