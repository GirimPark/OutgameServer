#include "pch.h"
#include "EchoClient.h"

EchoClient::EchoClient(const char* ip, const char* port)
	: m_bEndClient(false)
	, m_connectIP(ip)
	, m_port(port)
{
	m_clients.resize(MAX_CLIENT);
	for(auto client : m_clients)
	{
		client.thread = nullptr;
		client.socket = INVALID_SOCKET;
	}

	m_hCleanupEvent[0] = WSA_INVALID_EVENT;
}

EchoClient::~EchoClient()
{
}

void EchoClient::Run()
{
	if ((m_hCleanupEvent[0] = WSACreateEvent()) == WSA_INVALID_EVENT)
	{
		printf("WSACreateEvent() failed: %d\n", WSAGetLastError());
		return;
	}

	WSADATA WSAData;
	int rt;
	if ((rt = WSAStartup(MAKEWORD(2, 2), &WSAData)) != 0) 
	{
		printf("WSAStartup() failed: %d", rt);
		return;
	}

	for(int i=0; i<MAX_CLIENT; ++i)
	{
		if (m_bEndClient)
			break;

		m_clients[i].socket = CreateConnectedSocket(i);
		if(m_clients[i].socket == INVALID_SOCKET)
		{
			break;
		}
		m_clients[i].thread = new std::thread(&EchoClient::EchoThread, this, i);
		if(!m_clients[i].thread)
		{
			printf("CreateThread(%d) failed: %d\n", i, GetLastError());
			return;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	WSAWaitForMultipleEvents(1, m_hCleanupEvent, true, WSA_INFINITE, false);

	/// ÇØÁ¦
	for(auto iter : m_clients)
	{
		if(iter.thread->joinable())
		{
			closesocket(iter.socket);
			iter.socket = INVALID_SOCKET;
			iter.thread->join();
			delete iter.thread;
		}
	}

	if (m_hCleanupEvent[0] != WSA_INVALID_EVENT)
	{
		WSACloseEvent(m_hCleanupEvent[0]);
		m_hCleanupEvent[0] = WSA_INVALID_EVENT;
	}

	WSACleanup();
}

SOCKET EchoClient::CreateConnectedSocket(int threadId)
{
	addrinfo hints = { 0 };
	addrinfo* addr_srv = nullptr;

	hints.ai_flags = 0;
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	if (getaddrinfo(m_connectIP, m_port, &hints, &addr_srv) != 0) 
	{
		printf("getaddrinfo() failed with error %d\n", WSAGetLastError());
		return INVALID_SOCKET;
	}
	if (!addr_srv) 
	{
		printf("getaddrinfo() failed to resolve/convert the interface\n");
		return INVALID_SOCKET;
	}

	SOCKET socket = ::socket(addr_srv->ai_family, addr_srv->ai_socktype, addr_srv->ai_protocol);
 	if(socket == INVALID_SOCKET)
	{
		printf("socket() failed: %d\n", WSAGetLastError());
		return INVALID_SOCKET;
	}

	int rt = connect(socket, addr_srv->ai_addr, (int)addr_srv->ai_addrlen);
	if(rt == SOCKET_ERROR)
	{
		printf("connect(thread %d) failed: %d\n", threadId, WSAGetLastError());
		return INVALID_SOCKET;
	}

	freeaddrinfo(addr_srv);

	printf("connected on thread %d\n", threadId);
	return socket;
}

void EchoClient::EchoThread(int threadId)
{
	char* inbuf = nullptr;
	char* outbuf = nullptr;

	inbuf = new char[MAX_BUF_SIZE];
	outbuf = new char[MAX_BUF_SIZE];

	if((inbuf) && (outbuf))
	{
		ZeroMemory(outbuf, MAX_BUF_SIZE);

		while(true)
		{
			if (SendBuffer(threadId, outbuf) && RecvBuffer(threadId, inbuf))
			{
				if ((inbuf[0] != outbuf[0]) ||
					(inbuf[MAX_BUF_SIZE - 1] != outbuf[MAX_BUF_SIZE - 1]))
				{
					printf("nak(%d) in[0]=%c, out[0]=%c, in[%d]=%c out[%d]%c\n",
						threadId,
						inbuf[0], outbuf[0],
						MAX_BUF_SIZE - 1, inbuf[MAX_BUF_SIZE - 1],
						MAX_BUF_SIZE - 1, outbuf[MAX_BUF_SIZE - 1]);
					break;
				}
			}
			else
				break;
		}
	}

	if (inbuf)
		delete inbuf;
	if (outbuf)
		delete outbuf;
}

bool EchoClient::SendBuffer(int threadId, char* outbuf)
{

	memcpy(outbuf, std::to_string(m_clients[threadId].sequenceData).c_str(), std::to_string(m_clients[threadId].sequenceData).size());

	bool rt = true;
	char* bufp = outbuf;
	int nTotalSend = 0;
	int nSend = 0;

	while (nTotalSend < MAX_BUF_SIZE) 
	{
		nSend = send(m_clients[threadId].socket, bufp, MAX_BUF_SIZE - nTotalSend, 0);
		if (nSend == SOCKET_ERROR) 
		{
			printf("send(thread=%d) failed: %d\n", threadId, WSAGetLastError());
			rt = false;
			return rt;
		}
		if (nSend == 0) 
		{
			printf("connection closed\n");
			rt = false;
			return rt;
		}
		
		nTotalSend += nSend;
		bufp += nSend;
	}

	//printf("send: %s\n", outbuf);

	return rt;
}

bool EchoClient::RecvBuffer(int threadId, char* inbuf)
{
	char* bufp = inbuf;
	int nTotalRecv = 0;
	int nRecv = 0;

	while (nTotalRecv < MAX_BUF_SIZE) 
	{
		nRecv = recv(m_clients[threadId].socket, bufp, MAX_BUF_SIZE - nTotalRecv, 0);
		if (nRecv == SOCKET_ERROR)
		{
			printf("recv(thread=%d) failed: %d\n", threadId, WSAGetLastError());
			return false;
		}
		if (nRecv == 0) 
		{
			printf("connection closed\n");
			return false;
		}
		
		nTotalRecv += nRecv;
		bufp += nRecv;
	}

	if(std::atoi(inbuf) != (m_clients[threadId].preRecvData + 1))
	{
		printf("sequence is not correct\n");
		return false;
	}
	
	m_clients[threadId].preRecvData = m_clients[threadId].sequenceData++;

	//printf("recv: %s\n", inbuf);

	return true;
}
