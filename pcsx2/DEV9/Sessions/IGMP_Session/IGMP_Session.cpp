// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "common/Console.h"

#include "IGMP_Session.h"
#include "DEV9/PacketReader/NetLib.h"
#include "DEV9/Sessions/UDP_Session/UDP_FixedPort.h"

using namespace PacketReader;
using namespace PacketReader::IP;
using namespace PacketReader::IP::IGMP;

namespace Sessions
{
	IGMP_Session::IGMP_Session(ConnectionKey parKey, IP_Address parAdapterIP, ThreadSafeMap<u16, Sessions::BaseSession*>* fixedUDPPorts)
		: BaseSession(parKey, parAdapterIP)
		, fixedUDPPorts{fixedUDPPorts}
	{
	}

	IP_Payload* IGMP_Session::Recv()
	{
		return nullptr;
	}

	bool IGMP_Session::Send(PacketReader::IP::IP_Payload* payload)
	{
		IP_PayloadPtr* ipPayload = static_cast<IP_PayloadPtr*>(payload);
		IGMP_Packet igmp(ipPayload->data, ipPayload->GetLength());

		Console.WriteLn("DEV9: IGMP: Group Address %i.%i.%i.%i",
			igmp.groupAddress.bytes[0], igmp.groupAddress.bytes[1], igmp.groupAddress.bytes[2], igmp.groupAddress.bytes[3]);

		switch (igmp.type)
		{
			case static_cast<u8>(IGMP_Type::ReportV1):
			case static_cast<u8>(IGMP_Type::ReportV2):
			{
				DevCon.WriteLn("Join");

				// Add to prexisting
				const std::vector<u16> keys = fixedUDPPorts->GetKeys();
				for (const u16 key : keys)
				{
					BaseSession* fSession;
					if (!fixedUDPPorts->TryGetValue(key, &fSession))
						continue;

					const UDP_FixedPort* fPort = static_cast<UDP_FixedPort*>(fSession);

					// Add FixedPort
				}

				break;
			}
			case static_cast<u8>(IGMP_Type::Leave):
				DevCon.WriteLn("Leave TODO");
				break;
			case static_cast<u8>(IGMP_Type::Query):
				DevCon.WriteLn("Query");
				break;
			default:
				Console.Error("DEV9: IGMP: Unsupported IGMP Type", igmp.type);
				break;
		}
		return false;
	}

	void IGMP_Session::Reset()
	{
		//TODO, Handle as if we got Leave message
		//RaiseEventConnectionClosed();
	}

	void IGMP_Session::AddToFixedPort(Sessions::BaseSession* session)
	{
		UDP_FixedPort* fPort = static_cast<UDP_FixedPort*>(session);
		fPort->NewIGMPSession(key);
		//TODO Track IGMPSession
	}

	IGMP_Session::~IGMP_Session()
	{
	}
} // namespace Sessions
