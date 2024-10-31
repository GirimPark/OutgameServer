#include "pch.h"
#include "PacketHandler.h"

void PacketHandler::RegisterHandler(PacketID headerType, PacketHandlerCallback callback)
{
	auto iter = m_packetCallbacks.find(headerType);
	if(iter != m_packetCallbacks.end())
	{
		PRINT_CONTENTS("PacketHandler::RegisterHandler: 이미 등록된 콜백에서 대체됩니다.");
	}

	m_packetCallbacks.emplace(headerType, callback);
}

void PacketHandler::ReceivePacket(Session* session, const char* data, int nReceivedByte)
{
	std::shared_ptr<PacketHeader> packetHeader = std::make_shared<PacketHeader>();
	ASSERT_CRASH(PacketBuilder::Instance().DeserializeHeader(data, nReceivedByte, *packetHeader));

	auto iter = m_packetCallbacks.find(packetHeader->packetId);
	ASSERT_CRASH(iter != m_packetCallbacks.end());

	std::shared_ptr<ReceiveStruct> receivedStruct = std::make_shared<ReceiveStruct>(packetHeader, session, data, nReceivedByte);

	m_recvQueue.push(receivedStruct); 
}

void PacketHandler::Run()
{
	while(OutgameServer::Instance().IsRunning())
	{
		if (m_recvQueue.empty())
			continue;

		std::shared_ptr<ReceiveStruct> packet = std::make_shared<ReceiveStruct>();
		if (!m_recvQueue.try_pop(packet))
			continue;

		auto iter = m_packetCallbacks.find(packet->header->packetId);
		iter->second(packet);
	}
}