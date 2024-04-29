// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "DEV9/PacketReader/IP/IP_Packet.h"

namespace InternalServers
{
	class IGP_Logger
	{
	private:
		static const PacketReader::IP::IP_Address ssdpAddress;
		bool searchActive{false};

	public:
		IGP_Logger(){};

		void InspectRecv(PacketReader::IP::IP_Packet* payload);
		void InspectSend(PacketReader::IP::IP_Packet* payload);

		// TODO, figure out resetting
		//void Reset() { searchActive = false; }
	};
} // namespace InternalServers
