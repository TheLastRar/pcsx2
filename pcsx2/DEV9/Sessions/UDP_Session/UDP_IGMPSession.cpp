// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include <bit>

#include "common/Assertions.h"
#include "common/Console.h"

#ifdef _WIN32
#include "common/RedtapeWindows.h"
#include <winsock2.h>
#include <ws2ipdef.h>
#else
#include <netinet/in.h>
#include <netinet/ip.h>
#endif

#include "UDP_IGMPSession.h"
#include "DEV9/PacketReader/IP/UDP/UDP_Packet.h"

using namespace PacketReader;
using namespace PacketReader::IP;
using namespace PacketReader::IP::UDP;

namespace Sessions
{
#ifdef _WIN32
	UDP_IGMPSession::UDP_IGMPSession(ConnectionKey parKey, IP_Address parAdapterIP, SOCKET parClient)
#elif defined(__POSIX__)
	UDP_IGMPSession::UDP_IGMPSession(ConnectionKey parKey, IP_Address parAdapterIP, int parClient)
#endif
		: UDP_BaseSession(parKey, parAdapterIP)
		, client(parClient)
	{
		ip_mreq mreq{};
		mreq.imr_multiaddr = std::bit_cast<in_addr>(parKey.ip);
		mreq.imr_interface = std::bit_cast<in_addr>(adapterIP);
		if (setsockopt(client, IPPROTO_IP, IP_ADD_MEMBERSHIP, reinterpret_cast<char*>(&mreq), sizeof(mreq)) < 0)
		{
			Console.Error("DEV9: IGMP: setsockopt failed");
		}
	}

	IP_Payload* UDP_IGMPSession::Recv()
	{
		return nullptr;
	}

	bool UDP_IGMPSession::WillRecive(IP_Address parDestIP)
	{
		return true;
	}

	bool UDP_IGMPSession::Send(IP_Payload* payload)
	{
		pxAssert(false);
		return false;
	}

	void UDP_IGMPSession::Reset()
	{
	}

	UDP_IGMPSession::~UDP_IGMPSession()
	{
		RaiseEventConnectionClosed();
	}
} // namespace Sessions
