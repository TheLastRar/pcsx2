// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include <mutex>

#include "common/FileSystem.h"

#ifdef _WIN32
#include <winsock2.h>
#include <iphlpapi.h>
#elif defined(__POSIX__)
#include <sys/types.h>
#include <ifaddrs.h>
#endif

struct NetPacket;

namespace InternalServers
{
	class Pkt_Dumper
	{
	private:
		FileSystem::ManagedCFilePtr logFile{nullptr};
		std::mutex fileMutex;

	public:
		Pkt_Dumper(){};

#ifdef _WIN32
		void Init(PIP_ADAPTER_ADDRESSES adapter);
#elif defined(__POSIX__)
		void Init(ifaddrs* adapter);
#endif

		void InspectRecv(NetPacket* pkt);
		void InspectSend(NetPacket* pkt);

	private:
		void DumpPacket(NetPacket* pkt);
	};
} // namespace InternalServers
