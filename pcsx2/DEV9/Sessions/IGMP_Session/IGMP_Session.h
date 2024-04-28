// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "DEV9/ThreadSafeMap.h"
#include "DEV9/Sessions/BaseSession.h"
#include "DEV9/PacketReader/IP/IGMP/IGMP_Packet.h"

namespace Sessions
{
	class IGMP_Session : public BaseSession
	{
	private:
		ThreadSafeMap<u16, Sessions::BaseSession*>* fixedUDPPorts;

	public:
		IGMP_Session(ConnectionKey parKey, PacketReader::IP::IP_Address parAdapterIP, ThreadSafeMap<u16, Sessions::BaseSession*>* fixedUDPPorts);

		virtual PacketReader::IP::IP_Payload* Recv();
		virtual bool Send(PacketReader::IP::IP_Payload* payload);
		virtual void Reset();

		void AddToFixedPort(Sessions::BaseSession* session);

		virtual ~IGMP_Session();

	private:
		void HandleChildConnectionClosed(BaseSession* sender);
	};
} // namespace Sessions
