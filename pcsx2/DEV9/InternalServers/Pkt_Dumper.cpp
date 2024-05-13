// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "Pkt_Dumper.h"
#include "DEV9/net.h"

#include "common/Path.h"

#include <chrono>

#include "pcap.h"

using namespace std::chrono_literals;

namespace InternalServers
{
	// 32bit timeval used in file
	struct pcap_pkthdr_file
	{
		bpf_u_int32 ts_sec; /* timestamp seconds */
		bpf_u_int32 ts_usec; /* timestamp microseconds */
		bpf_u_int32 caplen; /* length of portion present */
		bpf_u_int32 len; /* length this packet (off wire) */
	};

#ifdef _WIN32
	void Pkt_Dumper::Init(PIP_ADAPTER_ADDRESSES adapter)
#elif defined(__POSIX__)
	void Pkt_Dumper::Init(ifaddrs* adapter)
#endif
	{
		std::string path = Path::Combine(EmuFolders::Logs, "packet_capture.pcap");
		logFile = FileSystem::OpenManagedCFile(path.c_str(), "wb");

		if (logFile == nullptr)
			return;

		pcap_file_header header{};
		header.magic = 0xA1B2C3D4;
		header.version_major = 2;
		header.version_minor = 4;
		header.thiszone = 0;
		header.sigfigs = 0;
		header.snaplen = 2048;
		header.linktype = DLT_EN10MB;

		// Write to file
		if (std::fwrite(&header, sizeof(header), 1, logFile.get()) != 1 || std::fflush(logFile.get()) != 0)
		{
			Console.Error("DEV9: Pkt: File write error");
			logFile = nullptr;
		}
	}

	void Pkt_Dumper::InspectRecv(NetPacket* pkt)
	{
		DumpPacket(pkt);
	}

	void Pkt_Dumper::InspectSend(NetPacket* pkt)
	{
		DumpPacket(pkt);
	}

	void Pkt_Dumper::DumpPacket(NetPacket* pkt)
	{
		std::scoped_lock lock(fileMutex);

		if (logFile == nullptr)
			return;

		// Get values needed for timeval
		const auto now = std::chrono::system_clock::now().time_since_epoch();
		auto s = std::chrono::duration_cast<std::chrono::seconds>(now);
		auto us = std::chrono::duration_cast<std::chrono::microseconds>(now - s);

		// Setup header
		pcap_pkthdr_file header{};
		header.ts_sec = s.count();
		header.ts_usec = us.count();
		header.caplen = pkt->size;
		header.len = pkt->size;

		// Write header
		if (std::fwrite(&header, sizeof(header), 1, logFile.get()) != 1)
		{
			Console.Error("DEV9: Pkt: File write error");
			logFile = nullptr;
			return;
		}

		// Write data
		if (std::fwrite(pkt->buffer, pkt->size, 1, logFile.get()) != 1 || std::fflush(logFile.get()) != 0)
		{
			Console.Error("DEV9: Pkt: File write error");
			logFile = nullptr;
		}
	}
} // namespace InternalServers
