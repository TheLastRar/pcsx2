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

#include "PrecompiledHeader.h"

//Need Vista SP1 or newer for IcmpSendEcho2Ex, WX defines a lower version
#if _WIN32_WINNT < _WIN32_WINNT_WIN7
#undef _WIN32_WINNT
#define _WIN32_WINNT _WIN32_WINNT_WIN7
#endif

#include <algorithm>

#include <iphlpapi.h>
#include <icmpapi.h>

#include "ICMP_Session.h"
#include "DEV9/PacketReader/NetLib.h"

using namespace PacketReader;
using namespace PacketReader::IP;
using namespace PacketReader::IP::ICMP;

/*  Ping is kindof annoying to do crossplatform
    All platforms restrict raw sockets

    Windows provides an api for ICMP
    ICMP_ECHO_REPLY should always be used, ignore ICMP_ECHO_REPLY32
    IP_OPTION_INFORMATION32 however does need to be used on 64bit

    Linux
    We have access to raw sockets via CAP_NET_RAW (for pcap)
        however we may be missing that cap on some builds
    Linux has socket(PF_INET, SOCK_DGRAM, IPPROTO_ICMP), used similar to raw sockets but for ICMP only
        requires net.ipv4.ping_group_range sysctl, default off on alot of distros
    We can use the ping cli

	Mac
	We have access to raw sockets via CAP_NET_RAW (for pcap)
		I think we have this set on mac?
	Mac also has socket(PF_INET, SOCK_DGRAM, IPPROTO_ICMP)
		Implementation differs, is more versatile than linux, no restriction to using it

	Other Unix
	Linux capabilities conforms to a withdrawn POSIX.1e standard, probably valid in other Unix systems
	assume ping cli is identical across Unix

	Based on above, raw sockets or ping CLI is our best bet
	Original purpose of CLR_DEV9 was to allow networking from a portable setup without admin
	To this effect, we will utilise the ping cli on unix
 */

namespace Sessions
{
	ICMP_Session::Ping::Ping(int requestSize)
	{
		icmpFile = IcmpCreateFile();
		if (icmpFile == INVALID_HANDLE_VALUE)
		{
			Console.Error("DEV9: ICMP: Failed to Create Icmp File");
			return;
		}

		icmpEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
		if (icmpEvent == NULL)
		{
			Console.Error("DEV9: ICMP: Failed to Create Event");
			IcmpCloseHandle(icmpFile);
			icmpFile = INVALID_HANDLE_VALUE;
			return;
		}

		//Allocate return buffer
		//Documentation says + 8 to allow for
		icmpResponseBufferLen = sizeof(ICMP_ECHO_REPLY) + requestSize + 8;
		icmpResponseBuffer = std::make_unique<u8[]>(icmpResponseBufferLen);
	}

	bool ICMP_Session::Ping::IsInitialised()
	{
		return icmpFile != INVALID_HANDLE_VALUE;
	}

	//Note, we can finish reading but have no data
	ICMP_ECHO_REPLY* ICMP_Session::Ping::Recv()
	{
		if (WaitForSingleObject(icmpEvent, 0) == WAIT_OBJECT_0)
		{
			ResetEvent(icmpEvent);
			//Prep buffer for reasing
			int count = IcmpParseReplies(icmpResponseBuffer.get(), icmpResponseBufferLen);
			pxAssert(count == 1);
			return (ICMP_ECHO_REPLY*)icmpResponseBuffer.get();
		}
		else
			return nullptr;
	}

	bool ICMP_Session::Ping::Send(IP_Address parAdapterIP, IP_Address parDestIP, int parTimeToLive, PayloadPtr* parPayload)
	{
#if defined(_WIN64)
		IP_OPTION_INFORMATION32 ipInfo{0};
#else
		IP_OPTION_INFORMATION ipInfo{0};
#endif
		ipInfo.Ttl = parTimeToLive;
		DWORD ret;
		if (parAdapterIP.integer != 0)
			ret = IcmpSendEcho2Ex(icmpFile, icmpEvent, nullptr, nullptr, parAdapterIP.integer, parDestIP.integer, parPayload->data, parPayload->GetLength(), &ipInfo, icmpResponseBuffer.get(), icmpResponseBufferLen, 30 * 1000);
		else
			ret = IcmpSendEcho2(icmpFile, icmpEvent, nullptr, nullptr, parDestIP.integer, parPayload->data, parPayload->GetLength(), &ipInfo, icmpResponseBuffer.get(), icmpResponseBufferLen, 30 * 1000);

		//Documentation states that IcmpSendEcho2 returns ERROR_IO_PENDING
		//However, it actully returns zero, with the error set to ERROR_IO_PENDING
		if (ret == 0)
			ret = GetLastError();

		if (ret != ERROR_IO_PENDING)
		{
			Console.Error("DEV9: ICMP: Failed to Send Echo, %d", GetLastError());
			return false;
		}

		return true;
	}

	ICMP_Session::Ping::~Ping()
	{
		if (icmpFile != INVALID_HANDLE_VALUE)
		{
			IcmpCloseHandle(icmpFile);
			icmpFile = INVALID_HANDLE_VALUE;
		}

		if (icmpEvent != NULL)
		{
			CloseHandle(icmpEvent);
			icmpEvent = NULL;
		}
	}

	ICMP_Session::ICMP_Session(ConnectionKey parKey, IP_Address parAdapterIP, ThreadSafeMap<Sessions::ConnectionKey, Sessions::BaseSession*>* parConnections)
		: BaseSession(parKey, parAdapterIP)
	{
		connections = parConnections;
	}

	IP_Payload* ICMP_Session::Recv()
	{
		std::unique_lock lock(ping_mutex);

		for (size_t i = 0; i < pings.size(); i++)
		{
			ICMP_ECHO_REPLY* pingRet = nullptr;
			pingRet = pings[i]->Recv();
			if (pingRet != nullptr)
			{
				Ping* ping = pings[i];
				//Remove ping from list and unlock
				pings.erase(pings.begin() + i);
				lock.unlock();

				//Map status to ICMP type/code
				int type, code;
				switch (pingRet->Status)
				{
					case (IP_SUCCESS):
						type = 0;
						code = 0;
						break;
					case (IP_DEST_NET_UNREACHABLE):
						type = 3;
						code = 0;
						break;
					case (IP_DEST_HOST_UNREACHABLE):
						type = 3;
						code = 1;
						break;
					case (IP_DEST_PROT_UNREACHABLE):
						type = 3;
						code = 2;
						break;
					case (IP_DEST_PORT_UNREACHABLE):
						type = 3;
						code = 3;
						break;
					case (IP_PACKET_TOO_BIG):
						type = 3;
						code = 4;
						break;
					case (IP_BAD_ROUTE): //Bad source route
						type = 3;
						code = 5;
						break;
					case (IP_BAD_DESTINATION):
						//I think this could be either
						//Destination network unknown
						//or
						//Destination host unknown
						//Use host unkown
						type = 3;
						code = 7;
						break;
					case (IP_REQ_TIMED_OUT):
						//Return nothing
						type = -1;
						code = IP_REQ_TIMED_OUT;
						break;
					case (IP_TTL_EXPIRED_TRANSIT):
						type = 11;
						code = 0;
						break;
					case (IP_TTL_EXPIRED_REASSEM):
						type = 11;
						code = 1;
						break;
					case (IP_SOURCE_QUENCH):
						type = 4;
						code = 0;
						break;

						//Unexpected Errors
					case (IP_BUF_TOO_SMALL):
					case (IP_NO_RESOURCES):
					case (IP_BAD_OPTION):
					case (IP_HW_ERROR):
					case (IP_BAD_REQ):
					case (IP_PARAM_PROBLEM):
					case (IP_OPTION_TOO_BIG):
					case (IP_GENERAL_FAILURE):
					default:
						type = -1;
						code = pingRet->Status;
						break;
				}

				//Create return ICMP packet
				ICMP_Packet* ret = nullptr;
				if (type != -1)
				{
					PayloadData* data;
					if (type == 0)
					{
						data = new PayloadData(pingRet->DataSize);
						memcpy(data->data.get(), pingRet->Data, pingRet->DataSize);
					}
					else
					{
						//We will copy the original packet back here
						//Allocate fullsize buffer
						u8* temp = new u8[ping->originalPacket->GetLength()];
						//Allocate data
						int responseSize = ping->originalPacket->GetHeaderLength() + 8;
						data = new PayloadData(responseSize);

						//Write packet back into bytes
						int offset = 0;
						ping->originalPacket->WriteBytes(temp, &offset);

						//Copy only needed bytes
						memcpy(data->data.get(), temp, responseSize);

						//cleanup
						delete[] temp;
					}

					ret = new ICMP_Packet(data);
					ret->type = type;
					ret->code = code;
					memcpy(ret->headerData, ping->headerData, 4);

					//Check
					//pxAssert(pingRet->DataSize == pings[i]->payloadLen);
					//pxAssert(memcmp(pingRet->Data, pings[i]->payloadBuffer.get(), pings[i]->payloadLen) == 0);
					if (destIP.integer != pingRet->Address)
						destIP.integer = pingRet->Address;
				}
				else if (code != IP_REQ_TIMED_OUT)
					Console.Error("DEV9: ICMP: Unexpected ICMP status %d", code);
				else
					Console.Error("DEV9: ICMP: ICMP timeout");

				//free ping
				delete ping;

				if (--open == 0)
					RaiseEventConnectionClosed();

				Console.Error("Return ICMP");
				//Return packet
				return ret;
			}
		}

		lock.unlock();
		return nullptr;
	}

	bool ICMP_Session::Send(PacketReader::IP::IP_Payload* payload)
	{
		Console.Error("Invalid Call");
		pxAssert(false);
		return false;
	}

	//Note, expects caller to set ipTimeToLive before calling
	bool ICMP_Session::Send(PacketReader::IP::IP_Payload* payload, IP_Packet* packet)
	{
		IP_PayloadPtr* ipPayload = static_cast<IP_PayloadPtr*>(payload);
		ICMP_Packet icmp(ipPayload->data, ipPayload->GetLength());

		PayloadPtr* icmpPayload = static_cast<PayloadPtr*>(icmp.GetPayload());

		switch (icmp.type)
		{
			case 3: //Port Closed
				switch (icmp.code)
				{
					case 3:
					{
						Console.Error("DEV9: ICMP: Recived Packet Rejected, Port Closed");

						//RE:Outbreak Hackfix
						//TODO, check if still needed

						std::unique_ptr<IP_Packet> retPkt;
						if ((icmpPayload->data[0] & 0xF0) == (4 << 4))
							retPkt = std::make_unique<IP_Packet>(icmpPayload->data, icmpPayload->GetLength(), true);
						else
						{
							Console.Error("DEV9: ICMP: Malformed ICMP Packet");
							int off = 1;
							while ((icmpPayload->data[off] & 0xF0) != (4 << 4))
								off += 1;

							Console.Error("DEV9: ICMP: Payload delayed %d bytes", off);

							retPkt = std::make_unique<IP_Packet>(&icmpPayload->data[off], icmpPayload->GetLength(), true);
						}

						IP_Address srvIP = retPkt->sourceIP;
						u8 prot = retPkt->protocol;
						u16 srvPort = 0;
						u16 ps2Port = 0;
						switch (prot)
						{
							case (u8)IP_Type::TCP:
							case (u8)IP_Type::UDP:
								//Read ports directly from the payload
								//both UDP and TCP have the same locations for ports
								IP_PayloadPtr* payload = static_cast<IP_PayloadPtr*>(retPkt->GetPayload());
								int offset = 0;
								NetLib::ReadUInt16(payload->data, &offset, &srvPort); //src
								NetLib::ReadUInt16(payload->data, &offset, &ps2Port); //dst
						}

						ConnectionKey Key{0};
						Key.ip = srvIP;
						Key.protocol = prot;
						Key.ps2Port = ps2Port;
						Key.srvPort = srvPort;

						//is from Normal Port?
						BaseSession* s;
						connections->TryGetValue(Key, &s);

						if (s != nullptr)
						{
							s->Reset();
							Console.WriteLn("DEV9: ICMP: Reset Rejected Connection");
							break;
						}

						//Is from Fixed Port?
						Key.ip = {0};
						Key.srvPort = 0;
						connections->TryGetValue(Key, &s);
						if (s != nullptr)
						{
							s->Reset();
							Console.WriteLn("DEV9: ICMP: Reset Rejected Connection");
							break;
						}

						Console.Error("DEV9: ICMP: Failed To Reset Rejected Connection");
						break;
					}
					default:
						Console.Error("DEV9: ICMP: Unsupported ICMP Code For Destination Unreachable %d", icmp.code);
				}
				break;
			case 8: //Echo
			{
				Console.Error("Send Ping");
				open++;

				Ping* ping = new Ping(icmpPayload->GetLength());

				if (!ping->IsInitialised())
				{
					if (--open == 0)
						RaiseEventConnectionClosed();
					delete ping;
					return false;
				}

				if (!ping->Send(adapterIP, destIP, packet->timeToLive, icmpPayload))
				{
					if (--open == 0)
						RaiseEventConnectionClosed();
					delete ping;
					return false;
				}

				memcpy(ping->headerData, icmp.headerData, 4);

				//Need to copy IP_Packet, original is stack allocated
				ping->originalPacket = std::make_unique<IP_Packet>(*packet);

				{
					std::scoped_lock lock(ping_mutex);
					pings.push_back(ping);
				}

				break;
			}
			default:
				Console.Error("DEV9: ICMP: Unsupported ICMP Type %d", icmp.type);
				break;
		}
		return true;
	}

	void ICMP_Session::Reset()
	{
		RaiseEventConnectionClosed();
	}

	ICMP_Session::~ICMP_Session()
	{
		std::scoped_lock lock(ping_mutex);

		//Cleanup
		for (size_t i = 0; i < pings.size(); i++)
			delete pings[i];
	}
} // namespace Sessions