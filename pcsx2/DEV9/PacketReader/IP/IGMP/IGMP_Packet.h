// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once
#include "DEV9/PacketReader/IP/IP_Packet.h"

namespace PacketReader::IP::IGMP
{
	enum struct IGMP_Type : u8
	{
		Query = 0x11,
		ReportV1 = 0x12,
		ReportV2 = 0x16,
		ReportV3 = 0x22,
		Leave = 0x17
	};

	class IGMP_Packet : public IP_Payload
	{
	public:
		u8 type;
		// units of 1/10 of a second
		// Is maxResponseCode on ICMPv3, which has special meaing >128
		u8 maxResponseTime;

	private:
		u16 checksum;

	public:
		PacketReader::IP::IP_Address groupAddress;

	private:
		const static IP_Type protocol = IP_Type::IGMP;

	public:
		//Takes ownership of payload
		//IGMP_Packet(Payload* data);
		IGMP_Packet(u8* buffer, int bufferSize);
		IGMP_Packet(const IGMP_Packet&);

		virtual int GetLength();
		virtual void WriteBytes(u8* buffer, int* offset);
		virtual IGMP_Packet* Clone() const;

		virtual u8 GetProtocol();

		virtual bool VerifyChecksum(IP_Address srcIP, IP_Address dstIP);
		virtual void CalculateChecksum(IP_Address srcIP, IP_Address dstIP);
	};
} // namespace PacketReader::IP::IGMP
