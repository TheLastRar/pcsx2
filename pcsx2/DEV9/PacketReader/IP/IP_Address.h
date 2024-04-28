// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "common/Pcsx2Defs.h"

namespace PacketReader::IP
{
	struct IP_Address
	{
		union
		{
			u8 bytes[4];
			u32 integer;
		};

		bool operator==(const IP_Address& other) const { return this->integer == other.integer; }
		bool operator!=(const IP_Address& other) const { return this->integer != other.integer; }
	};
} // namespace PacketReader::IP

//IP_Address Hash function
template <>
struct std::hash<PacketReader::IP::IP_Address>
{
	size_t operator()(PacketReader::IP::IP_Address const& ip) const noexcept
	{
		size_t hash = 17;
		hash = hash * 23 + std::hash<u8>{}(ip.bytes[0]);
		hash = hash * 23 + std::hash<u8>{}(ip.bytes[1]);
		hash = hash * 23 + std::hash<u8>{}(ip.bytes[2]);
		hash = hash * 23 + std::hash<u8>{}(ip.bytes[3]);
		return hash;
	}
};
