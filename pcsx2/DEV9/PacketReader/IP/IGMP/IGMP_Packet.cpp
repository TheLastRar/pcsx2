// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "IGMP_Packet.h"
#include "DEV9/PacketReader/NetLib.h"

#include "common/Console.h"

namespace PacketReader::IP::IGMP
{
	//IGMP_Packet::IGMP_Packet(Payload* data)
	//	: payload{data}
	//{
	//}
	IGMP_Packet::IGMP_Packet(u8* buffer, int bufferSize)
	{
		int offset = 0;
		//Bits 0-31
		NetLib::ReadByte08(buffer, &offset, &type);
		NetLib::ReadByte08(buffer, &offset, &maxResponseTime);
		NetLib::ReadUInt16(buffer, &offset, &checksum);

		//Bits 32-63
		NetLib::ReadIPAddress(buffer, &offset, &groupAddress);

		if ((IGMP_Type)type == IGMP_Type::ReportV3)
			Console.Error("IGMPv3 Not Supported");
	}
	IGMP_Packet::IGMP_Packet(const IGMP_Packet& original)
		: type{original.type}
		, maxResponseTime{original.maxResponseTime}
		, checksum{original.checksum}
		, groupAddress{original.groupAddress}
	{
	}

	int IGMP_Packet::GetLength()
	{
		return 8;
	}

	void IGMP_Packet::WriteBytes(u8* buffer, int* offset)
	{
		NetLib::WriteByte08(buffer, offset, type);
		NetLib::WriteByte08(buffer, offset, maxResponseTime);
		NetLib::WriteUInt16(buffer, offset, checksum);
		NetLib::WriteIPAddress(buffer, offset, groupAddress);
	}

	IGMP_Packet* IGMP_Packet::Clone() const
	{
		return new IGMP_Packet(*this);
	}

	u8 IGMP_Packet::GetProtocol()
	{
		return static_cast<u8>(protocol);
	}

	void IGMP_Packet::CalculateChecksum(IP_Address srcIP, IP_Address dstIP)
	{
		int pHeaderLen = 8;
		if ((pHeaderLen & 1) != 0)
		{
			pHeaderLen += 1;
		}

		u8* segment = new u8[pHeaderLen];
		int counter = 0;

		checksum = 0;
		WriteBytes(segment, &counter);

		//Zero alignment byte
		if (counter != pHeaderLen)
			NetLib::WriteByte08(segment, &counter, 0);

		checksum = IP_Packet::InternetChecksum(segment, pHeaderLen);
		delete[] segment;
	}
	bool IGMP_Packet::VerifyChecksum(IP_Address srcIP, IP_Address dstIP)
	{
		int pHeaderLen = 8;
		if ((pHeaderLen & 1) != 0)
		{
			pHeaderLen += 1;
		}

		u8* segment = new u8[pHeaderLen];
		int counter = 0;

		WriteBytes(segment, &counter);

		//Zero alignment byte
		if (counter != pHeaderLen)
			NetLib::WriteByte08(segment, &counter, 0);

		const u16 csumCal = IP_Packet::InternetChecksum(segment, pHeaderLen);
		delete[] segment;

		return (csumCal == 0);
	}
} // namespace PacketReader::IP::IGMP
