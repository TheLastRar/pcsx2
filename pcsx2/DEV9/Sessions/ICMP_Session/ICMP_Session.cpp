// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#ifdef _WIN32
#include "common/RedtapeWindows.h"
#include <iphlpapi.h>
#include <icmpapi.h>
#elif defined(__POSIX__)

#if defined(__linux__)
#define ICMP_SOCKETS_LINUX
#endif
#if defined(__FreeBSD__) || defined(__APPLE__)
#define ICMP_SOCKETS_BSD
#endif

#define ALLOW_PING_CLI

#include <algorithm>
#include <bit>
#include <thread>
#include <errno.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#ifdef __linux__
#include <linux/errqueue.h>
#endif

#ifdef ALLOW_PING_CLI
#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>

#include <sys/wait.h>
#include "common/StringUtil.h"

extern char** environ;
#endif
#endif

#include "ICMP_Session.h"
#include "DEV9/PacketReader/NetLib.h"

using namespace PacketReader;
using namespace PacketReader::IP;
using namespace PacketReader::IP::ICMP;

using namespace std::chrono_literals;

/*
 * Ping is kindof annoying to do crossplatform
 * All platforms restrict raw sockets
 * 
 * Windows provides an api for ICMP
 * ICMP_ECHO_REPLY should always be used, ignore ICMP_ECHO_REPLY32
 * IP_OPTION_INFORMATION should always be used, ignore IP_OPTION_INFORMATION32
 * 
 * Linux
 * We have access to raw sockets via CAP_NET_RAW (for pcap)
 *     However we may be missing that cap on some builds
 * Also hava socket(PF_INET, SOCK_DGRAM, IPPROTO_ICMP), used similarly to raw sockets, but for ICMP only
 *     Auto filters responses
 *     Requires net.ipv4.ping_group_range sysctl, default off on a lot of distros
 * Timeouts reported via sock_extended_err control messages (with IP_RECVERR socket option set)
 * 
 * Mac
 * Raw sockets restricted
 * Mac has socket(PF_INET, SOCK_DGRAM, IPPROTO_ICMP)
 *     No restriction to using it with ICMP_ECHO
 *     Implementation differs, is more versatile than linux
 *     Does not auto filter responses
 * Timeouts reported as a normal packet
 * 
 * FreeBSD
 * Raw sockets restricted
 * No unprivilaged ICMP sockets
 * Timeouts reported as a normal packet??
 * 
 * Ping cli
 * Present for all platforms, but command args differ
 * Not used here
 */

namespace Sessions
{
	const std::chrono::duration<std::chrono::steady_clock::rep, std::chrono::steady_clock::period>
		ICMP_Session::Ping::ICMP_TIMEOUT = 30s;

#ifdef __POSIX__
	ICMP_Session::Ping::PingType ICMP_Session::Ping::icmpConnectionKind = ICMP_Session::Ping::PingType::ICMP;
#endif

	ICMP_Session::Ping::Ping(int requestSize)
#ifdef _WIN32
		: icmpFile{IcmpCreateFile()}
	{
		if (icmpFile == INVALID_HANDLE_VALUE)
		{
			Console.Error("DEV9: ICMP: Failed to Create Icmp File");
			return;
		}

		icmpEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (icmpEvent == nullptr)
		{
			Console.Error("DEV9: ICMP: Failed to Create Event");
			IcmpCloseHandle(icmpFile);
			icmpFile = INVALID_HANDLE_VALUE;
			return;
		}

		/* 
		 * Allocate response buffer
		 * Documentation says + 8 to allow for an ICMP error message
		 * In testing, ICMP_ECHO_REPLY structure itself was returned with data set to null
		 */
		icmpResponseBufferLen = sizeof(ICMP_ECHO_REPLY) + requestSize + 8;
		icmpResponseBuffer = std::make_unique<std::byte[]>(icmpResponseBufferLen);
#elif defined(__POSIX__)
	{
		/* 
		 * Allocate response buffer
		 * Size needed depends on which socket protocol (ICMP or raw) we use aswell as os
		 */
		switch (icmpConnectionKind)
		{
			// Two different methods for raw/icmp sockets between the Unix OSes
			// Play it safe and only enable when we know which of the two methods we use
#if defined(ICMP_SOCKETS_LINUX) || defined(ICMP_SOCKETS_BSD)
			case (PingType::ICMP):
				icmpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP);
				if (icmpSocket != -1)
				{
#if defined(ICMP_SOCKETS_LINUX)
					// Only need space for ICMP header, as MSG_ERRQUEUE returns data we sent
					// Testing found an extra + 8 was required sometimes, for some reason?
					icmpResponseBufferLen = 8 + requestSize + 8;
#elif defined(ICMP_SOCKETS_BSD)
					// Returned IP Header, ICMP Header & either data or failed ICMP packet
					// Sometimes get full packet in ICMP error response
					icmpResponseBufferLen = 20 + 8 + (20 + 8 + requestSize);
#endif
					break;
				}

				DevCon.WriteLn("DEV9: ICMP: Failed To Open ICMP Socket");
				icmpConnectionKind = ICMP_Session::Ping::PingType::RAW;

				[[fallthrough]];

			case (PingType::RAW):
				icmpSocket = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
				if (icmpSocket != -1)
				{
#if defined(ICMP_SOCKETS_LINUX)
					// We get IP packet + ICMP header
					icmpResponseBufferLen = 20 + 8 + requestSize;
#elif defined(ICMP_SOCKETS_BSD)
					// As above, but we will also directly receive error ICMP messages
					icmpResponseBufferLen = 20 + 8 + std::max(20 + 8, requestSize);
#endif
					break;
				}

				DevCon.WriteLn("DEV9: ICMP: Failed To Open RAW Socket");
#ifdef ALLOW_PING_CLI
				icmpConnectionKind = ICMP_Session::Ping::PingType::CLI;
#endif
				[[fallthrough]];
#endif
#ifdef ALLOW_PING_CLI
			case (PingType::CLI):
#ifdef __APPLE__
				pxAssert(false);
#endif
				icmpResponseBufferLen = requestSize;
				break;
#endif
			default:
				Console.Error("DEV9: ICMP: Failed To Ping");
				return;
		}

		icmpResponseBuffer = std::make_unique<std::byte[]>(icmpResponseBufferLen);
#endif
	}

	bool ICMP_Session::Ping::IsInitialised() const
	{
#ifdef _WIN32
		return icmpFile != INVALID_HANDLE_VALUE;
#else
		switch (icmpConnectionKind)
		{
			case (PingType::ICMP):
			case (PingType::RAW):
				return icmpSocket != -1;
#ifdef ALLOW_PING_CLI
			case (PingType::CLI):
				return true;
#endif
			default:
				return false;
		}
#endif
	}

	// Returned PingResult.data is only valid when PingResult.type is 0
	ICMP_Session::PingResult* ICMP_Session::Ping::Recv()
	{
#ifdef _WIN32
		if (WaitForSingleObject(icmpEvent, 0) == WAIT_OBJECT_0)
		{
			ResetEvent(icmpEvent);

			const int count = IcmpParseReplies(icmpResponseBuffer.get(), icmpResponseBufferLen);
			pxAssert(count <= 1);

			// Timeout
			if (count == 0)
			{
				result.type = -2;
				result.code = 0;
				return &result;
			}

			// Rely on implicit object creation
			const ICMP_ECHO_REPLY* pingRet = reinterpret_cast<ICMP_ECHO_REPLY*>(icmpResponseBuffer.get());

			// Map status to ICMP type/code
			switch (pingRet->Status)
			{
				case (IP_SUCCESS):
					result.type = 0;
					result.code = 0;
					break;
				case (IP_DEST_NET_UNREACHABLE):
					result.type = 3;
					result.code = 0;
					break;
				case (IP_DEST_HOST_UNREACHABLE):
					result.type = 3;
					result.code = 1;
					break;
				case (IP_DEST_PROT_UNREACHABLE):
					result.type = 3;
					result.code = 2;
					break;
				case (IP_DEST_PORT_UNREACHABLE):
					result.type = 3;
					result.code = 3;
					break;
				case (IP_PACKET_TOO_BIG):
					result.type = 3;
					result.code = 4;
					break;
				case (IP_BAD_ROUTE): // Bad source route
					result.type = 3;
					result.code = 5;
					break;
				case (IP_BAD_DESTINATION):
					/*
					 * I think this could be mapped to either
					 * Destination network unknown
					 * or
					 * Destination host unknown
					 * Lets map to host unknown
					 */
					result.type = 3;
					result.code = 7;
					break;
				case (IP_REQ_TIMED_OUT):
					// Return nothing
					result.type = -2;
					result.code = 0;
					break;
				case (IP_TTL_EXPIRED_TRANSIT):
					result.type = 11;
					result.code = 0;
					break;
				case (IP_TTL_EXPIRED_REASSEM):
					result.type = 11;
					result.code = 1;
					break;
				case (IP_SOURCE_QUENCH):
					result.type = 4;
					result.code = 0;
					break;

					// Unexpected errors
				case (IP_BUF_TOO_SMALL):
				case (IP_NO_RESOURCES):
				case (IP_BAD_OPTION):
				case (IP_HW_ERROR):
				case (IP_BAD_REQ):
				case (IP_PARAM_PROBLEM):
				case (IP_OPTION_TOO_BIG):
				case (IP_GENERAL_FAILURE):
				default:
					result.type = -1;
					result.code = pingRet->Status;
					break;
			}

			result.dataLength = pingRet->DataSize;
			result.data = static_cast<u8*>(pingRet->Data);
			result.address.integer = pingRet->Address;

			return &result;
		}
		else
			return nullptr;
#elif defined(__POSIX__)
		switch (icmpConnectionKind)
		{
			case (PingType::ICMP):
			case (PingType::RAW):
			{
				sockaddr_in endpoint{};

				iovec iov;
				iov.iov_base = icmpResponseBuffer.get();
				iov.iov_len = icmpResponseBufferLen;

#if defined(ICMP_SOCKETS_LINUX)
				// Needs to hold cmsghdr + sock_extended_err + sockaddr_in
				// for ICMP error responses, this is a total of 44 bytes
				// Unknown size needed for other error types
				std::byte cbuff[64]{};
#endif

				msghdr msg{};
				msg.msg_name = &endpoint;
				msg.msg_namelen = sizeof(endpoint);
				msg.msg_iov = &iov;
				msg.msg_iovlen = 1;

				int ret = recvmsg(icmpSocket, &msg, 0);

				if (ret == -1)
				{
					int err = errno;
					if (err == EAGAIN || err == EWOULDBLOCK)
					{
						if (std::chrono::steady_clock::now() - icmpDeathClockStart > ICMP_TIMEOUT)
						{
							result.type = -2;
							result.code = 0;
							return &result;
						}
						else
							return nullptr;
					}
#if defined(ICMP_SOCKETS_LINUX)
					else
					{
						msg.msg_control = &cbuff;
						msg.msg_controllen = sizeof(cbuff);
						ret = recvmsg(icmpSocket, &msg, MSG_ERRQUEUE);
					}

					if (ret == -1)
#endif
					{
						Console.Error("DEV9: ICMP: RecvMsg Error: %d", err);
						result.type = -1;
						result.code = err;
						return &result;
					}
				}

				if (msg.msg_flags & MSG_TRUNC)
					Console.Error("DEV9: ICMP: RecvMsg Truncated");

#if defined(ICMP_SOCKETS_LINUX)
				if (msg.msg_flags & MSG_CTRUNC)
					Console.Error("DEV9: ICMP: RecvMsg Control Truncated");

				// On Linux, ICMP errors are stored in control messages retrieved using MSG_ERRQUEUE
				sock_extended_err* exErrorPtr = nullptr;
				cmsghdr* cmsg;

				// Search though control messages, taking the latest mesage
				// We should only have at most 1 message
				for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL; cmsg = CMSG_NXTHDR(&msg, cmsg))
				{
					if (cmsg->cmsg_level == SOL_IP && cmsg->cmsg_type == IP_RECVERR)
					{
						pxAssert(!exErrorPtr);
						exErrorPtr = reinterpret_cast<sock_extended_err*>(CMSG_DATA(cmsg));
						continue;
					}
					pxAssert(false);
				}

				if (exErrorPtr != nullptr)
				{
					/* 
					 * The pointer returned cannot be assumed to be suitably aligned for accessing arbitrary payload data types
					 * So we would need to memcpy sock_extended_err
					 */
					sock_extended_err exError;
					std::memcpy(&exError, exErrorPtr, sizeof(exError));

					// Process the error
					if (exError.ee_origin == SO_EE_ORIGIN_ICMP)
					{
						result.type = exError.ee_type;
						result.code = exError.ee_code;

						/* 
						 * SO_EE_OFFENDER reads data relative to, but not necessarily included in struct sock_extended_err
						 * So we need to pass the original pointer provided to us from CMSG_DATA()
						 * However, the input pointer needs to be of type sock_extended_err*, hence the reinterpret_cast
						 * The pointer returned may not be suitably aligned (see CMSG_DATA), so we need to memcpy
						 */
						sockaddr_in errorEndpoint;
						std::memcpy(&errorEndpoint, SO_EE_OFFENDER(exErrorPtr), sizeof(errorEndpoint));
						result.address = std::bit_cast<IP_Address>(errorEndpoint.sin_addr);
						return &result;
					}
					else
					{
						Console.Error("DEV9: ICMP: Recv error %d", exError.ee_errno);
						result.type = -1;
						result.code = exError.ee_errno;
						return &result;
					}
				}
				else
#endif
				{
					int offset;
					int length;
#if defined(ICMP_SOCKETS_LINUX)
					if (icmpConnectionKind == PingType::ICMP)
					{
						offset = 0;
						length = ret;
					}
					else
#endif
					{
						// Rely on implicit object creation
						const ip* ipHeader = reinterpret_cast<ip*>(icmpResponseBuffer.get());
						const int headerLength = ipHeader->ip_hl << 2;
						pxAssert(headerLength == 20);

						offset = headerLength;
#ifdef __APPLE__
						// Apple (old BSD)'s raw IP sockets implementation converts the ip_len field to host byte order
						// and additionally subtracts the header length.
						// https://www.unix.com/man-page/mojave/4/ip/
						length = ipHeader->ip_len;
#else
						length = ntohs(ipHeader->ip_len) - headerLength;
#endif
					}

					// Rely on implicit object creation for u8
					ICMP_Packet icmp(reinterpret_cast<u8*>(&icmpResponseBuffer[offset]), length);
					PayloadPtr* icmpPayload = static_cast<PayloadPtr*>(icmp.GetPayload());

					result.type = icmp.type;
					result.code = icmp.code;

					result.address = std::bit_cast<IP_Address>(endpoint.sin_addr);

					if (icmp.type == 0)
					{
						// Check if response is for us
						if (icmpConnectionKind == PingType::RAW)
						{
							const ICMP_HeaderDataIdentifier headerData(icmp.headerData);
							if (headerData.identifier != icmpId)
								return nullptr;
						}

						// While icmp (and its PayloadPtr) will be destroyed when leaving this function
						// the data it points to persists in icmpResponseBuffer
						result.dataLength = icmpPayload->GetLength();
						result.data = icmpPayload->data;
						return &result;
					}
#if defined(ICMP_SOCKETS_BSD)
					// On BSD/Mac, ICMP errors are returned as normal packets
					else if (icmp.type == 3 || icmp.type == 4 || icmp.type == 5 || icmp.type == 11)
					{
						// Extract the packet the ICMP message is responding to
						IP_Packet ipPacket(icmpPayload->data, icmpPayload->GetLength(), true);

						if (ipPacket.protocol != static_cast<u8>(IP_Type::ICMP))
							return nullptr;

						IP_PayloadPtr* ipPayload = static_cast<IP_PayloadPtr*>(ipPacket.GetPayload());
						ICMP_Packet icmpInner(ipPayload->data, ipPayload->GetLength());

						// Check if response is for us
						const ICMP_HeaderDataIdentifier headerData(icmpInner.headerData);
						if (headerData.identifier != icmpId)
							return nullptr;

						// This response is for us
						return &result;
					}
#endif
					else
					{
#if defined(ICMP_SOCKETS_LINUX)
						Console.Error("DEV9: ICMP: Unexpected packet");
						pxAssert(false);
#endif
						// Assume not for us
						return nullptr;
					}
				}
			}
#ifdef ALLOW_PING_CLI
			case (PingType::CLI):
			{
				char buffer[128];
				ssize_t readSize;
				while ((readSize = read(outRedirectPipe[0], buffer, 128)) > 0)
					pingStdOut += std::string(buffer, readSize);

				if (readSize == 0)
				{
					int status = -1;
					pid_t ret = waitpid(pingPid, &status, 0);
					::close(outRedirectPipe[0]);
					pingPid = -1;

					if (ret == -1)
					{
						int err = errno;
						Console.Error("DEV9: ICMP: Failed to waitpid, %d", err);
						result.type = -1;
						result.code = err;
						return &result;
					}
					if (status != 0)
					{
						Console.Error("DEV9: ICMP: Failed to ping, %d", status);
						result.type = -1;
						result.code = status;
						return &result;
					}

					// Ping result OK, time to read output
					// 64 bytes from 1.1.1.1: icmp_seq=1 ttl=59 time=17.0 ms
					// From 172.18.128.1 icmp_seq=1 Time to live exceeded
					//                              Frag reassembly time exceeded
					// What is timeout message (It's just blank...)
					std::vector<std::string_view> lines = StringUtil::SplitString(pingStdOut, '\n');
					if (lines.size() < 2)
					{
						Console.Error("DEV9: ICMP: Failed to split string");
						Console.Error(pingStdOut.c_str());
						Console.WriteLn();
						result.type = -1;
						result.code = 0;
						return &result;
					}
					if (lines[1].length() > 0)
					{
						std::vector<std::string_view> words = StringUtil::SplitString(lines[1], ' ');
						if (words[0] == "From")
						{
							// Packet Failed to reach
							// Get address
							if (inet_pton(AF_INET, std::string(words[1]).c_str(), &result.address) != 1)
							{
								Console.Error("DEV9: ICMP: Failed to parse IP");
								result.type = -1;
								result.code = 0;
								return &result;
							}

							// Translate error
							if (words.size() == 7 &&
								(words[3] == "Time" && words[4] == "to" && words[5] == "live" && words[6] == "exceeded"))
							{
								result.type = 11;
								result.code = 0;
								return &result;
							}
							if (words.size() == 7 &&
								(words[3] == "Frag" && words[4] == "reassembly" && words[5] == "time" && words[6] == "exceeded"))
							{
								result.type = 11;
								result.code = 1;
								return &result;
							}

							// Unkown Error
							Console.Error("DEV9: ICMP: Unknown Response");
							Console.Error(pingStdOut.c_str());
							result.type = -1;
							result.code = 0;
							return &result;
						}
						else if (words[1] == "bytes")
						{
							// Get address (also removes traling ':')
							if (inet_pton(AF_INET, std::string(words[3].substr(0, words[3].length() - 1)).c_str(), &result.address) != 1)
							{
								Console.Error("DEV9: ICMP: Failed to parse IP");
								result.type = -1;
								result.code = 0;
								return &result;
							}

							// Packet succeeded to reach
							// Note, we can get request size from the string, but we size response buffer to match anyway
							result.type = 0;
							result.code = 0;
							result.dataLength = icmpResponseBufferLen;
							// Rely on implicit object creation for u8
							result.data = reinterpret_cast<u8*>(icmpResponseBuffer.get());
							return &result;
						}
						else
						{
							Console.Error("DEV9: ICMP: Unknown Response");
							Console.Error(pingStdOut.c_str());
							result.type = -1;
							result.code = 0;
							return &result;
						}
					}
					else
					{
						// timeout
						// Return nothing
						result.type = -2;
						result.code = 0;
						return &result;
					}
				}
				else // readSize == -1
				{
					// Check if we are blocking
					int err = errno;
					if (err == EAGAIN)
						return nullptr;
					else
					{
						Console.Error("DEV9: ICMP: Failed to Read Pipe, %d", err);
						kill(pingPid, SIGKILL);
						::close(outRedirectPipe[0]);
						pingPid = -1;
						result.type = -1;
						result.code = err;
						return &result;
					}
				}
			}
#endif
			default:
				result.type = -1;
				result.code = 0;
				return &result;
		}
#endif
	}

	bool ICMP_Session::Ping::Send(IP_Address parAdapterIP, IP_Address parDestIP, int parTimeToLive, PayloadPtr* parPayload)
	{
#ifdef _WIN32
		// Documentation is incorrect, IP_OPTION_INFORMATION is to be used regardless of platform
		IP_OPTION_INFORMATION ipInfo{};
		ipInfo.Ttl = parTimeToLive;
		DWORD ret;
		if (parAdapterIP.integer != 0)
			ret = IcmpSendEcho2Ex(icmpFile, icmpEvent, nullptr, nullptr, parAdapterIP.integer, parDestIP.integer, const_cast<u8*>(parPayload->data), parPayload->GetLength(), &ipInfo, icmpResponseBuffer.get(), icmpResponseBufferLen,
				static_cast<DWORD>(std::chrono::duration_cast<std::chrono::milliseconds>(ICMP_TIMEOUT).count()));
		else
			ret = IcmpSendEcho2(icmpFile, icmpEvent, nullptr, nullptr, parDestIP.integer, const_cast<u8*>(parPayload->data), parPayload->GetLength(), &ipInfo, icmpResponseBuffer.get(), icmpResponseBufferLen,
				static_cast<DWORD>(std::chrono::duration_cast<std::chrono::milliseconds>(ICMP_TIMEOUT).count()));

		// Documentation states that IcmpSendEcho2 returns ERROR_IO_PENDING
		// However, it actually returns zero, with the error set to ERROR_IO_PENDING
		if (ret == 0)
			ret = GetLastError();

		if (ret != ERROR_IO_PENDING)
		{
			Console.Error("DEV9: ICMP: Failed to send echo, %d", GetLastError());
			return false;
		}

		return true;
#elif defined(__POSIX__)
		switch (icmpConnectionKind)
		{
			case (PingType::ICMP):
			case (PingType::RAW):
			{
				icmpDeathClockStart = std::chrono::steady_clock::now();

				// Broadcast and multicast might need extra setsockopts calls
				// I don't think any game will do a broadcast/multicast ping

				if (parAdapterIP.integer != 0)
				{
					sockaddr_in endpoint{};
					endpoint.sin_family = AF_INET;
					endpoint.sin_addr = std::bit_cast<in_addr>(parAdapterIP);

					if (bind(icmpSocket, reinterpret_cast<const sockaddr*>(&endpoint), sizeof(endpoint)) == -1)
					{
						Console.Error("DEV9: ICMP: Failed to bind socket. Error: %d", errno);
						::close(icmpSocket);
						icmpSocket = -1;
						return false;
					}
				}

#if defined(ICMP_SOCKETS_LINUX)
				const int value = 1;
				if (setsockopt(icmpSocket, SOL_IP, IP_RECVERR, reinterpret_cast<const char*>(&value), sizeof(value)))
				{
					Console.Error("DEV9: ICMP: Failed to setsockopt IP_RECVERR. Error: %d", errno);
					::close(icmpSocket);
					icmpSocket = -1;
					return false;
				}
#endif

				// TTL (Note multicast & regular ttl are separate)
				if (setsockopt(icmpSocket, IPPROTO_IP, IP_TTL, reinterpret_cast<const char*>(&parTimeToLive), sizeof(parTimeToLive)) == -1)
				{
					Console.Error("DEV9: ICMP: Failed to set TTL. Error: %d", errno);
					::close(icmpSocket);
					icmpSocket = -1;
					return false;
				}

				// Non-blocking
				int blocking = 1;
				if (ioctl(icmpSocket, FIONBIO, &blocking) == -1)
				{
					Console.Error("DEV9: ICMP: Failed to set non-blocking. Error: %d", errno);
					::close(icmpSocket);
					icmpSocket = -1;
					return false;
				}

#if defined(ICMP_SOCKETS_LINUX)
				if (icmpConnectionKind == PingType::ICMP)
				{
					// We get assigned a port/Id
					sockaddr_in endpoint{};
					socklen_t endpointsize = sizeof(endpoint);
					if (getsockname(icmpSocket, reinterpret_cast<sockaddr*>(&endpoint), &endpointsize) == -1)
					{
						Console.Error("DEV9: ICMP: Failed to get id. Error: %d", errno);
						::close(icmpSocket);
						icmpSocket = -1;
						return false;
					}

					icmpId = endpoint.sin_port;
				}
				else
#endif
				{
					// Use time, in ms, as id
					icmpId = static_cast<u16>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
				}

				ICMP_Packet icmp(parPayload->Clone());
				icmp.type = 8;
				icmp.code = 0;

				// We only send one icmp packet per identifier
				ICMP_HeaderDataIdentifier headerData(icmpId, 1);
				headerData.WriteHeaderData(icmp.headerData);

				icmp.CalculateChecksum(parAdapterIP, parDestIP);

				std::unique_ptr<u8[]> buffer = std::make_unique<u8[]>(icmp.GetLength());
				int offset = 0;
				icmp.WriteBytes(buffer.get(), &offset);

				sockaddr_in endpoint{};
				endpoint.sin_family = AF_INET;
				endpoint.sin_addr = std::bit_cast<in_addr>(parDestIP);

				int ret = -1;
				while (ret == -1)
				{
					ret = sendto(icmpSocket, buffer.get(), icmp.GetLength(), 0, reinterpret_cast<const sockaddr*>(&endpoint), sizeof(endpoint));
					if (ret == -1)
					{
						const int err = errno;
						if (err == EAGAIN || err == EWOULDBLOCK)
							std::this_thread::yield();
						else
						{
							Console.Error("DEV9: ICMP: Send error %d", errno);
							::close(icmpSocket);
							icmpSocket = -1;
							return false;
						}
					}
				}

				return true;
			}
#ifdef ALLOW_PING_CLI
			case (PingType::CLI):
			{
				// See this for CLI based ping
				// https://github.com/dotnet/runtime/blob/main/src/libraries/Common/src/System/Net/NetworkInformation/UnixCommandLinePing.cs

				// We need provide the "LC_ALL=C" enviroment variable to ensure we can read the output
				// Only execvpe allows both enviroment variables + shell like locating
				// but that is a gnu extension

				// Find path to ping
				const char* paths[] = {"/bin/ping", "/sbin/ping", "/usr/bin/ping"};
				const char* pingPath = nullptr;
				for (size_t i = 0; i < std::size(paths); i++)
				{
					if (access(paths[i], F_OK) == 0)
					{
						pingPath = paths[i];
						break;
					}
				}

				if (pingPath == nullptr)
				{
					Console.Error("DEV9: ICMP: Failed to Find Pipe");
					return false;
				}

				if (pipe(outRedirectPipe) == -1)
				{
					Console.Error("DEV9: ICMP: Failed to Create Pipe, %d", errno);
					return false;
				}

				// Copy to response
				memcpy(icmpResponseBuffer.get(), parPayload->data, parPayload->GetLength());

				if (pipe(outRedirectPipe) == -1)
				{
					Console.Error("DEV9: ICMP: Failed to Create Pipe, %d", errno);
					return false;
				}

				if (fcntl(outRedirectPipe[0], F_SETFD, FD_CLOEXEC) == -1)
				{
					Console.Error("DEV9: ICMP: Failed to Setup Pipe, %d", errno);
					::close(outRedirectPipe[0]);
					::close(outRedirectPipe[1]);
					return false;
				}

				if (fcntl(outRedirectPipe[1], F_SETFD, FD_CLOEXEC) == -1)
				{
					Console.Error("DEV9: ICMP: Failed to Setup Pipe, %d", errno);
					::close(outRedirectPipe[0]);
					::close(outRedirectPipe[1]);
					return false;
				}

				// Set non-blocking for read pipe
				if (fcntl(outRedirectPipe[0], F_SETFL, fcntl(outRedirectPipe[0], F_GETFD) | O_NONBLOCK) == -1)
				{
					Console.Error("DEV9: ICMP: Failed to Setup Pipe, %d", errno);
					::close(outRedirectPipe[0]);
					::close(outRedirectPipe[1]);
					return false;
				}

				pingPid = fork();
				if (pingPid == 0)
				{
					// Clear PCSX2 signals
					struct sigaction action{};
					action.sa_handler = SIG_DFL;
					if (sigaction(SIGBUS, &action, nullptr) == -1)
						_exit(errno);
					if (sigaction(SIGSEGV, &action, nullptr) == -1)
						_exit(errno);

					// Redirect stdout
					if (dup2(outRedirectPipe[1], STDOUT_FILENO) == -1)
						_exit(errno);

					// Prep enviroment variables
					std::vector<const char*> envp;
					int length = 0;
					while (environ[length] != nullptr)
					{
						envp.push_back(environ[length]);
						length++;
					}

					// So we can always read the output
					envp.push_back("LC_ALL=C");
					envp.push_back(nullptr);

					// Prep arguments
					std::vector<const char*> argv;
					argv.push_back(pingPath); // Arg 1
					argv.push_back("-c"); // Arg 2
					argv.push_back("1"); // Arg 3

					argv.push_back("-W"); // Arg 4
					char buffTimeout[8];
#if defined(__FreeBSD__) || defined(__APPLE__)
					// Timeout in MILLISECONDS
					snprintf(buffTimeout, 8, "%lld", std::chrono::duration_cast<std::chrono::milliseconds>(ICMP_TIMEOUT).count());
					argv.push_back(buffTimeout); // Arg 5
					// Hops
					argv.push_back("-m"); // Arg 6
#else /*linux*/
					// Timeout in SECONDS
					snprintf(buffTimeout, 8, "%ld", std::chrono::duration_cast<std::chrono::seconds>(ICMP_TIMEOUT).count());
					argv.push_back(buffTimeout); // Arg5
					// Hops
					argv.push_back("-t"); // Arg 6
#endif
					char ttlstr[8];
					snprintf(ttlstr, 8, "%d", parTimeToLive);
					argv.push_back(ttlstr); // Arg 7

					argv.push_back("-s"); //Arg 8
					char requestLenStr[8];
					// ping does not report timing information unless at least 16 bytes are sent.
					if (icmpResponseBufferLen < 16)
						argv.push_back("16"); // Arg 9
					else
					{
						snprintf(requestLenStr, 8, "%d", icmpResponseBufferLen);
						argv.push_back(requestLenStr); // Arg 9
					}

					char buffSrc[INET_ADDRSTRLEN];
					if (parAdapterIP.integer != 0)
					{
						inet_ntop(AF_INET, &parAdapterIP, buffSrc, INET_ADDRSTRLEN);

#if defined(__FreeBSD__) || defined(__APPLE__)
						// -S src_addr (Use the following IP address as the source address in outgoing packets. On hosts with more than one IP address, this option can be used to force the source address to be something other than the IP address of the interface the probe packet is sent on.)
						// Dosn't actually bind to the adapter?
						// Mac has -b boundif (Bind the socket to interface boundif for sending), but we will always use ICMP sockets instead
						argv.push_back("-S"); // Arg 10
						argv.push_back(buffSrc); // Arg 11
#else /*linux*/
						// -I interface address (Set source address to specified interface address. Argument may be numeric IP address or name of device)
						// -I on FreeBSD & Mac only affect ICMP sent to multicast address
						argv.push_back("-I"); // Arg 10
						argv.push_back(buffSrc); // Arg 11
#endif
					}

					char buffDest[INET_ADDRSTRLEN];
					inet_ntop(AF_INET, &parDestIP, buffDest, INET_ADDRSTRLEN);
					argv.push_back(buffDest); // Arg 10 or 12

					if (argv.size() == 10)
						execle(pingPath, argv[0], argv[1], argv[2], argv[3], argv[4], argv[5], argv[6], argv[7], argv[8], argv[9], nullptr, (char* const*)&envp[0]);
					else if (argv.size() == 12)
						execle(pingPath, argv[0], argv[1], argv[2], argv[3], argv[4], argv[5], argv[6], argv[7], argv[8], argv[9], argv[10], argv[11], nullptr, (char* const*)&envp[0]);
					else
						// Regular assert as child process
						assert(false);

					_exit(errno);
				}
				else if (pingPid != -1)
				{
					// Close our copy of the pipe write end
					::close(outRedirectPipe[1]);
				}
				else
				{
					Console.Error("DEV9: ICMP: Failed to Initiate Fork, %d", errno);
					::close(outRedirectPipe[0]);
					::close(outRedirectPipe[1]);
					return false;
				}

				return true;
			}
#endif
			default:
				return false;
		}
#endif
	}

	ICMP_Session::Ping::~Ping()
	{
#ifdef _WIN32
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
#else
		if (icmpSocket != -1)
		{
			::close(icmpSocket);
			icmpSocket = -1;
		}
#ifdef ALLOW_PING_CLI
		if (pingPid != -1)
		{
			kill(pingPid, SIGKILL);
			::close(outRedirectPipe[0]);
			pingPid = -1;
		}
#endif
#endif
	}

	ICMP_Session::ICMP_Session(ConnectionKey parKey, IP_Address parAdapterIP, ThreadSafeMap<Sessions::ConnectionKey, Sessions::BaseSession*>* parConnections)
		: BaseSession(parKey, parAdapterIP)
	{
		connections = parConnections;
	}

	std::optional<ReceivedPayload> ICMP_Session::Recv()
	{
		std::unique_lock lock(ping_mutex);

		for (size_t i = 0; i < pings.size(); i++)
		{
			const ICMP_Session::PingResult* pingRet = pings[i]->Recv();
			if (pingRet != nullptr)
			{
				std::unique_ptr<Ping> ping = std::move(pings[i]);
				// Remove ping from list and unlock mutex
				pings.erase(pings.begin() + i);
				lock.unlock();

				// Create return ICMP packet
				std::optional<ReceivedPayload> ret;
				if (pingRet->type >= 0)
				{
					PayloadData* data;
					if (pingRet->type == 0)
					{
						data = new PayloadData(pingRet->dataLength);
						memcpy(data->data.get(), pingRet->data, pingRet->dataLength);
					}
					else
					{
						// Copy the original packet into the returned ICMP packet
						// Allocate fullsize buffer
						std::vector<u8> temp = std::vector<u8>(ping->originalPacket->GetLength());
						// Allocate returned ICMP payload
						const int responseSize = ping->originalPacket->GetHeaderLength() + 8;
						data = new PayloadData(responseSize);

						// Write packet into buffer
						int offset = 0;
						ping->originalPacket->WriteBytes(temp.data(), &offset);

						// Copy only needed bytes
						memcpy(data->data.get(), temp.data(), responseSize);
					}

					std::unique_ptr<ICMP_Packet> pRet = std::make_unique<ICMP_Packet>(data);
					pRet->type = pingRet->type;
					pRet->code = pingRet->code;
					memcpy(pRet->headerData, ping->headerData, 4);

					ret = {pingRet->address, std::move(pRet)};
				}
				else if (pingRet->type == -1)
					Console.Error("DEV9: ICMP: Unexpected ICMP status %d", pingRet->code);
				else
					DevCon.WriteLn("DEV9: ICMP: ICMP timeout");

				if (ret.has_value())
					DevCon.WriteLn("DEV9: ICMP: Return Ping");

				if (--open == 0)
					RaiseEventConnectionClosed();

				// Return packet
				return ret;
			}
		}

		lock.unlock();
		return std::nullopt;
	}

	bool ICMP_Session::Send(PacketReader::IP::IP_Payload* payload)
	{
		Console.Error("DEV9: ICMP: Invalid Call");
		pxAssert(false);
		return false;
	}

	// Expects caller to set ipTimeToLive before calling
	bool ICMP_Session::Send(PacketReader::IP::IP_Payload* payload, IP_Packet* packet)
	{
		IP_PayloadPtr* ipPayload = static_cast<IP_PayloadPtr*>(payload);
		ICMP_Packet icmp(ipPayload->data, ipPayload->GetLength());

		PayloadPtr* icmpPayload = static_cast<PayloadPtr*>(icmp.GetPayload());

		switch (icmp.type)
		{
			case 3: // Port Closed
				switch (icmp.code)
				{
					case 3:
					{
						Console.Error("DEV9: ICMP: Received Packet Rejected, Port Closed");

						/*
						 * RE:Outbreak Hackfix
						 * ICMP port closed messages has an extra 4 bytes of padding before the packet copy
						 * this can be tested by trying to connect without using the resurrection server DNS
						 * turbo mode may be needed to trigger the bug, depending on the DNS server's latency
						 */
						std::unique_ptr<IP_Packet> retPkt;
						if ((icmpPayload->data[0] & 0xF0) == (4 << 4))
							retPkt = std::make_unique<IP_Packet>(icmpPayload->data, icmpPayload->GetLength(), true);
						else
						{
							Console.Error("DEV9: ICMP: Malformed ICMP Packet");
							int off = 1;
							while ((icmpPayload->data[off] & 0xF0) != (4 << 4))
							{
								off += 1;

								// Require space for the IP Header and source/dest port of a UDP/TCP packet
								// We don't generate packets with IP options, so IP header is always 20 bytes
								if (icmpPayload->GetLength() - off - 24 < 0)
								{
									off = -1;
									break;
								}
							}

							if (off == -1)
							{
								Console.Error("DEV9: ICMP: Unable To Recover Data");
								Console.Error("DEV9: ICMP: Failed To Reset Rejected Connection");
								break;
							}

							Console.Error("DEV9: ICMP: Payload delayed %d bytes", off);

							retPkt = std::make_unique<IP_Packet>(&icmpPayload->data[off], icmpPayload->GetLength() - off, true);
						}

						const IP_Address srvIP = retPkt->sourceIP;
						const u8 prot = retPkt->protocol;
						u16 srvPort = 0;
						u16 ps2Port = 0;
						switch (prot)
						{
							case static_cast<u8>(IP_Type::TCP):
							case static_cast<u8>(IP_Type::UDP):
								// Read ports directly from the payload
								// both UDP and TCP have the same locations for ports
								IP_PayloadPtr* payload = static_cast<IP_PayloadPtr*>(retPkt->GetPayload());
								int offset = 0;
								NetLib::ReadUInt16(payload->data, &offset, &srvPort); // src
								NetLib::ReadUInt16(payload->data, &offset, &ps2Port); // dst
						}

						ConnectionKey Key{};
						Key.ip = srvIP;
						Key.protocol = prot;
						Key.ps2Port = ps2Port;
						Key.srvPort = srvPort;

						// Is from Normal Port?
						BaseSession* s = nullptr;
						connections->TryGetValue(Key, &s);

						if (s != nullptr)
						{
							s->Reset();
							Console.WriteLn("DEV9: ICMP: Reset Rejected Connection");
							break;
						}

						// Is from Fixed Port?
						Key.ip = {};
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
			case 8: // Echo
			{
				DevCon.WriteLn("DEV9: ICMP: Send Ping");
				open++;

				std::unique_ptr<Ping> ping = std::make_unique<Ping>(icmpPayload->GetLength());

				if (!ping->IsInitialised())
				{
					if (--open == 0)
						RaiseEventConnectionClosed();
					return false;
				}

				if (!ping->Send(adapterIP, destIP, packet->timeToLive, icmpPayload))
				{
					if (--open == 0)
						RaiseEventConnectionClosed();
					return false;
				}

				memcpy(ping->headerData, icmp.headerData, 4);

				// Need to copy IP_Packet, original is stack allocated
				ping->originalPacket = std::make_unique<IP_Packet>(*packet);

				{
					std::scoped_lock lock(ping_mutex);
					pings.push_back(std::move(ping));
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
		// Cleanup
		std::scoped_lock lock(ping_mutex);
		pings.clear();
	}
} // namespace Sessions
