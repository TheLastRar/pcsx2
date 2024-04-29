// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "IGP_Logger.h"
#include "DEV9/PacketReader/Payload.h"
#include "DEV9/PacketReader/IP/IGMP/IGMP_Packet.h"
#include "DEV9/PacketReader/IP/UDP/UDP_Packet.h"
#include "DEV9/PacketReader/IP/TCP/TCP_Packet.h"

#ifdef _WIN32
#include <ws2tcpip.h>
#elif defined(__POSIX__)
#include <arpa/inet.h>
#endif

#include "common/Console.h"
#include "common/StringUtil.h"

using namespace PacketReader;
using namespace PacketReader::IP;
using namespace PacketReader::IP::IGMP;
using namespace PacketReader::IP::UDP;
using namespace PacketReader::IP::TCP;

namespace InternalServers
{
	const IP_Address IGP_Logger::ssdpAddress{{{239, 255, 255, 250}}};

	void IGP_Logger::InspectRecv(IP_Packet* packet)
	{
		switch (packet->protocol)
		{
			//case static_cast<u8>(IP_Type::IGMP):
			//{
			//	IP_PayloadPtr* ipPayload = static_cast<IP_PayloadPtr*>(packet->GetPayload());
			//	IGMP_Packet igmp(ipPayload->data, ipPayload->GetLength());

			//	Console.WriteLn("DEV9: IGMP: Packet received with group address %i.%i.%i.%i",
			//		igmp.groupAddress.bytes[0], igmp.groupAddress.bytes[1], igmp.groupAddress.bytes[2], igmp.groupAddress.bytes[3]);

			//	switch (igmp.type)
			//	{
			//		case static_cast<u8>(IGMP_Type::Query):
			//			Console.WriteLn("DEV9: IGMP: Query");
			//			break;
			//		default:
			//			Console.Error("DEV9: IGMP: Unsupported IGMP Type", igmp.type);
			//			break;
			//	}
			//}
			case static_cast<u8>(IP_Type::TCP):
			{
				//if (searchActive)
				{
					IP_PayloadPtr* ipPayload = static_cast<IP_PayloadPtr*>(packet->GetPayload());
					TCP_Packet tcp(ipPayload->data, ipPayload->GetLength());
					if (packet->sourceIP == IP_Address{{{192, 168, 1, 1}}})
					{
						PayloadPtr* tcpPayload = static_cast<PayloadPtr*>(tcp.GetPayload());
						std::string_view msg{reinterpret_cast<char*>(tcpPayload->data), static_cast<size_t>(tcpPayload->GetLength())};
						Console.WriteLn("DEV9: IGP: TCP packet received");
						Console.WriteLn(msg);
					}
				}
				break;
			}
			case static_cast<u8>(IP_Type::UDP):
			{
				if (searchActive)
				{
					IP_PayloadPtr* ipPayload = static_cast<IP_PayloadPtr*>(packet->GetPayload());
					UDP_Packet udp(ipPayload->data, ipPayload->GetLength());
					if (udp.destinationPort == 1900)
					{
						PayloadPtr* udpPayload = static_cast<PayloadPtr*>(udp.GetPayload());
						const std::string_view msg{reinterpret_cast<char*>(udpPayload->data), static_cast<size_t>(udpPayload->GetLength())};
						Console.WriteLn("DEV9: IGP: SSDP packet received");
						Console.WriteLn(msg);

						if (msg.starts_with("HTTP/1.1 200 OK"))
						{
							std::vector<std::string> msgLines = StringUtil::splitOnNewLine(std::string(msg));
							auto line = std::find_if(msgLines.begin(), msgLines.end(), [](const std::string& l) {
								return StringUtil::StartsWithNoCase(l, "ST:");
							});

							if (line == msgLines.end())
								break;

							constexpr size_t stStart = std::string::traits_type::length("ST:");
							const size_t versionPos = (*line).find_last_of(':');
							const std::string_view searchTarget = StringUtil::StripWhitespace(std::string_view(*line).substr(stStart, versionPos - stStart));
							//const std::string_view searchTargetVersion = StringUtil::StripWhitespace(std::string_view(*line).substr(versionPos + 1));

							//V2 spec https://openconnectivity.org/developer/specifications/upnp-resources/upnp/internet-gateway-device-igd-v-2-0/
							//V1 spec https://openconnectivity.org/wp-content/uploads/2015/11/UPnP-gw-InternetGatewayDevice-v1-Device.pdf

							/*
							 * InternetGatewayDevice will have the following services
							 * urn:schemas-upnp-org:service:Layer3Forwarding:1				urn:upnp-org:serviceId:L3Forwarding1		O
							 * urn:schemas-upnp-org:service:DeviceProtection:1				urn:upnp-org:serviceId:DeviceProtection1	O,New (should)
							 * urn:schemas-upnp-org:device:WANDevice:2																	R,Upgraded
							 * urn:schemas-upnp-org:service:WANCommonInterfaceConfig:1		urn:upnp-org:serviceId:WANCommonIFC1		R
							 * urn:schemas-upnp-org:device:WANConnectionDevice:2														R,Upgraded
							 * urn:schemas-upnp-org:service:WANPOTSLinkConfig:1				urn:upnp-org:serviceId:WANPOTSLinkC1		O
							 * urn:schemas-upnp-org:service:WANDSLLinkConfig:1				urn:upnp-org:serviceId:WANDSLLinkC1			O
							 * urn:schemas-upnp-org:service:WANCableLinkConfig:1			urn:upnp-org:serviceId:WANCableLinkC1		O
							 * urn:schemas-upnp-org:service:WANEthernetLinkConfig:1			urn:upnp-org:serviceId:WANEthLinkC1			O
							 * urn:schemas-upnp-org:service:WANPPPConnection:1				urn:upnp-org:serviceId:WANPPPConn1			R for PPP based connections
							 * urn:schemas-upnp-org:service:WANIPConnection:2				urn:upnp-org:serviceId:WANIPConn1			R,Upgraded
							 * urn:schemas-upnp-org:device:LANDevice:1																	O
							 * urn:schemas-upnp-org:device:LANHostConfigManagement:1		urn:upnp-org:serviceId:LANHostCfg1			O
							 */

							if (searchTarget.ends_with("urn:schemas-upnp-org:service:WANIPConnection") ||
								searchTarget.ends_with("urn:schemas-upnp-org:service:WANPPPConnection") ||
								searchTarget.ends_with("urn:schemas-upnp-org:service:WANCommonInterfaceConfig") ||
								searchTarget.ends_with("urn:schemas-upnp-org:device:InternetGatewayDevice"))
							{
								Console.WriteLn("DEV9: IGP: SSDP packet received was from internet gateway device");

								line = std::find_if(msgLines.begin(), msgLines.end(), [](const std::string& l) {
									return StringUtil::StartsWithNoCase(l, "LOCATION:");
								});

								// TODO: switch to regex?
								constexpr size_t locStart = std::string::traits_type::length("LOCATION:");
								constexpr size_t httpStart = std::string::traits_type::length("http://");
								std::string_view endpoint = StringUtil::StripWhitespace(std::string_view(*line).substr(locStart)).substr(httpStart);
								endpoint = endpoint.substr(0, endpoint.find_first_of('/'));

								std::vector<std::string_view> endpointSplit = StringUtil::SplitString(endpoint, ':');

								if (endpointSplit.size() != 2)
								{
									Console.WriteLn("DEV9: IGP: SSDP packet had unexpected location: %s", (*line).c_str());
									break;
								}

								IP_Address address;
								if (inet_pton(AF_INET, std::string(endpointSplit[0]).c_str(), &address) != 1)
								{
									Console.WriteLn("DEV9: IGP: SSDP packet had unexpected location: %s", (*line).c_str());
									break;
								}

								u16 port = std::stoi(std::string(endpointSplit[1]).c_str());

								Console.WriteLn("DEV9: IGP: Location endpoint %i.%i.%i.%i:%i",
									address.bytes[0], address.bytes[1], address.bytes[2], address.bytes[3], port);

								// We now have address + port, save it and then log TCP messages to/from that address
							}
						}
					}
				}
			}
			default:
				break;
		}
	}

	void IGP_Logger::InspectSend(IP_Packet* packet)
	{
		switch (packet->protocol)
		{
			//case static_cast<u8>(IP_Type::IGMP):
			//{
			//	IP_PayloadPtr* ipPayload = static_cast<IP_PayloadPtr*>(packet->GetPayload());
			//	IGMP_Packet igmp(ipPayload->data, ipPayload->GetLength());

			//	Console.WriteLn("DEV9: IGMP: Packet sent with group address %i.%i.%i.%i",
			//		igmp.groupAddress.bytes[0], igmp.groupAddress.bytes[1], igmp.groupAddress.bytes[2], igmp.groupAddress.bytes[3]);

			//	switch (igmp.type)
			//	{
			//		case static_cast<u8>(IGMP_Type::ReportV1):
			//		case static_cast<u8>(IGMP_Type::ReportV2):
			//			Console.WriteLn("DEV9: IGMP: Join");
			//			if (igmp.groupAddress == ssdpAddress)
			//				Console.WriteLn("DEV9: IGP: Joined SSDP Multicast group");
			//			break;
			//		case static_cast<u8>(IGMP_Type::Leave):
			//			Console.WriteLn("DEV9: IGMP: Leave");
			//			if (igmp.groupAddress == ssdpAddress)
			//				Console.WriteLn("DEV9: IGP: Left SSDP Multicast group");
			//			break;
			//		default:
			//			Console.Error("DEV9: IGMP: Unsupported IGMP Type", igmp.type);
			//			break;
			//	}
			//	break;
			//}
			case static_cast<u8>(IP_Type::TCP):
			{
				//if (searchActive)
				{
					IP_PayloadPtr* ipPayload = static_cast<IP_PayloadPtr*>(packet->GetPayload());
					TCP_Packet tcp(ipPayload->data, ipPayload->GetLength());
					if (packet->destinationIP == IP_Address{{{192, 168, 1, 1}}})
					{
						PayloadPtr* tcpPayload = static_cast<PayloadPtr*>(tcp.GetPayload());
						std::string_view msg{reinterpret_cast<char*>(tcpPayload->data), static_cast<size_t>(tcpPayload->GetLength())};
						Console.WriteLn("DEV9: IGP: TCP packet received");
						Console.WriteLn(msg);
					}
				}
				break;
			}
			case static_cast<u8>(IP_Type::UDP):
			{
				if (packet->destinationIP == ssdpAddress)
				{
					//if (!searchActive)
					//	Console.WriteLn("DEV9: IGP: SSDP packet sent when not in multicast group");

					searchActive = true; // TODO: timeout based on MX? field (maximum wait response time)

					IP_PayloadPtr* ipPayload = static_cast<IP_PayloadPtr*>(packet->GetPayload());
					UDP_Packet udp(ipPayload->data, ipPayload->GetLength());
					if (udp.destinationPort == 1900)
					{
						PayloadPtr* udpPayload = static_cast<PayloadPtr*>(udp.GetPayload());
						std::string_view msg{reinterpret_cast<char*>(udpPayload->data), static_cast<size_t>(udpPayload->GetLength())};
						Console.WriteLn("DEV9: IGP: SSDP packet sent");
						Console.WriteLn(msg);
					}
				}
			}
			default:
				break;
		}
	}
} // namespace InternalServers
