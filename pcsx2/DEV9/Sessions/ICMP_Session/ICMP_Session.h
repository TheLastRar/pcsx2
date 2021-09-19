/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once
#include <atomic>
#include <mutex>
#include <vector>

#include "DEV9/SimpleQueue.h"
#include "DEV9/ThreadSafeMap.h"
#include "DEV9/Sessions/BaseSession.h"
#include "DEV9/PacketReader/IP/ICMP/ICMP_Packet.h"

namespace Sessions
{
	class ICMP_Session : public BaseSession
	{
	private:
		class Ping
		{
		public:
			u8 headerData[4];
			std::unique_ptr<PacketReader::IP::IP_Packet> originalPacket;

		private:
			HANDLE icmpFile{INVALID_HANDLE_VALUE};
			HANDLE icmpEvent{NULL};

			//Return buffers
			int icmpResponseBufferLen;
			std::unique_ptr<u8[]> icmpResponseBuffer;

		public:
			Ping(int requestSize);
			bool IsInitialised();
			ICMP_ECHO_REPLY* Recv();
			bool Send(PacketReader::IP::IP_Address parAdapterIP, PacketReader::IP::IP_Address parDestIP, int parTimeToLive, PacketReader::PayloadPtr* parPayload);

			~Ping();
		};

		SimpleQueue<PacketReader::IP::ICMP::ICMP_Packet*> _recvBuff;
		std::mutex ping_mutex;
		std::vector<Ping*> pings;
		ThreadSafeMap<Sessions::ConnectionKey, Sessions::BaseSession*>* connections;

		std::atomic<int> open{0};

	public:
		ICMP_Session(ConnectionKey parKey, PacketReader::IP::IP_Address parAdapterIP, ThreadSafeMap<Sessions::ConnectionKey, Sessions::BaseSession*>* parConnections);

		virtual PacketReader::IP::IP_Payload* Recv();
		virtual bool Send(PacketReader::IP::IP_Payload* payload);
		bool Send(PacketReader::IP::IP_Payload* payload, PacketReader::IP::IP_Packet* packet);
		virtual void Reset();

		virtual ~ICMP_Session();
	};
} // namespace Sessions
