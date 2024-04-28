// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#ifdef _WIN32
#include <winsock2.h>
#elif defined(__POSIX__)
#define INVALID_SOCKET -1
#include <sys/socket.h>
#endif

#include "UDP_BaseSession.h"

namespace Sessions
{
	class UDP_IGMPSession : public UDP_BaseSession
	{
	private:
#ifdef _WIN32
		SOCKET client = INVALID_SOCKET;
#elif defined(__POSIX__)
		int client = INVALID_SOCKET;
#endif

	public:
#ifdef _WIN32
		UDP_IGMPSession(ConnectionKey parKey, PacketReader::IP::IP_Address parAdapterIP, SOCKET parClient);
#elif defined(__POSIX__)
		UDP_IGMPSession(ConnectionKey parKey, PacketReader::IP::IP_Address parAdapterIP, int parClient);
#endif

		virtual PacketReader::IP::IP_Payload* Recv();
		virtual bool WillRecive(PacketReader::IP::IP_Address parDestIP);
		virtual bool Send(PacketReader::IP::IP_Payload* payload);
		virtual void Reset();

		virtual ~UDP_IGMPSession();

	private:
		void CloseSocket();
	};
} // namespace Sessions
