// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "EthernetFrame.h"
#include "NetLib.h"

namespace PacketReader
{
	EthernetFrame::EthernetFrame(Payload* data)
		: payload{data}
	{
	}
	EthernetFrame::EthernetFrame(NetPacket* pkt)
	{
		int offset = 0;
		NetLib::ReadMACAddress(pkt->buffer.data(), &offset, &destinationMAC);
		NetLib::ReadMACAddress(pkt->buffer.data(), &offset, &sourceMAC);

		headerLength = 14; //(6+6+2)

		//Note: we don't have to worry about the Ethernet Frame CRC as it is not included in the packet

		NetLib::ReadUInt16(pkt->buffer.data(), &offset, &protocol);

		//Note: We don't support tagged frames

		payload = std::make_unique<PayloadPtr>(&pkt->buffer[offset], pkt->size - headerLength);
	}

	Payload* EthernetFrame::GetPayload()
	{
		return payload.get();
	}

	void EthernetFrame::WritePacket(NetPacket* pkt)
	{
		int counter = 0;

		pkt->size = headerLength + payload->GetLength();
		NetLib::WriteMACAddress(pkt->buffer.data(), &counter, destinationMAC);
		NetLib::WriteMACAddress(pkt->buffer.data(), &counter, sourceMAC);
		//
		NetLib::WriteUInt16(pkt->buffer.data(), &counter, protocol);
		//
		payload->WriteBytes(pkt->buffer.data(), &counter);
	}
} // namespace PacketReader
