// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "AdapterUtils.h"

#include <bit>

#include "common/Assertions.h"
#include "common/Console.h"
#include "common/ScopedGuard.h"
#include "common/StringUtil.h"

#ifdef __POSIX__
#include <unistd.h>
#include <fstream>
#include <net/if.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <string.h>
#include <unordered_set>
#include <dbus/dbus.h>
#include "common/ScopedGuard.h"

#if defined(__FreeBSD__) || (__APPLE__)
#include <sys/types.h>
#include <net/if_dl.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/sockio.h>
#include <net/route.h>
#include <net/if_var.h>
#endif
#endif

using namespace PacketReader;
using namespace PacketReader::IP;

/*
 * The socket api and its sockaddr_* types are somewhat tricky to work with while trying to avoid UB.
 * The various sockaddr_* types may be larger or smaller than the base sockaddr type, preventing the use of std::bit_cast().
 * std::memcpy casting can also be non-trivial if we are casting a large sockaddr_* type to a smaller sockaddr.
 * Using a reinterpret_cast would violate strict aliasing/object lifetime rules.
 * However, what if we consider that any sockaddr pointer is pre-aliased to an object already in the required type,
 * we can then just reinterpret_cast to the assumed original type (and hope the C++ object model agrees with us).
 * 
 * This, still violates strict aliasing rules when passing a sockaddr ptr to be read from/written to,
 * as these will be library functions, we will consider that not my problem(TM).
 * One could even argue that an implementation would need to reinterpret_cast back the pointer anyway.
 * 
 * Another problem this assumption raises, is when we have to determine which sockaddr_* type an object is based on sa_family.
 * Doing this via the provided sockaddr pointer would violate strict aliasing rules with our assumption.
 * We have to std::memcpy to the base sockaddr to read this safely.
 * 
 * https://man7.org/linux/man-pages/man3/sockaddr.3type.html has a note stating the following;
 * "POSIX Issue 8 will fix this by requiring that implementations make sure that these structures can be safely used as they were designed."
 * Where they plan to sweep the issue under the rug.
 */

/*
 * We assume that a sockaddr_* object is given to us pre-aliased via a sockaddr pointer, we need to read sa_family for the actual type.
 * Use std::memcpy to cast, but only copy enough to read the common initial layout, in case we somehow have a sockaddr_* smaller than sockaddr.
 * In practice, any smaller stucts are probably padded up, but that padding is not noted in current spec afaik.
 */
u16 AdapterUtils::ReadAddressFamily(const sockaddr* unknownAddr)
{
	sockaddr addr;
	// Structures are pointer-interconvertible with the first non-static field.
	// However, On FreeBSD & Mac, sa_family is not the first member, sa_len is.
	static_assert(std::is_standard_layout_v<sockaddr>);
	std::memcpy(&addr, unknownAddr, offsetof(sockaddr, sa_family) + sizeof(addr.sa_family));
	return addr.sa_family;
}

#ifdef _WIN32
AdapterUtils::Adapter* AdapterUtils::GetAllAdapters(AdapterBuffer* buffer, bool includeHidden)
{
	// It is recommend to pre-allocate enough space to be able to call GetAdaptersAddresses just once.
	// Also provide extra space if we are including hidden adapters.
	int neededSize = includeHidden ? 100000 : 50000;
	// Each IP_ADAPTER_ADDRESSES will have pointers other structures which are also copied into this buffer.
	// Subsequent IP_ADAPTER_ADDRESSES (accessed via .Next) are also not aligned to sizeof(IP_ADAPTER_ADDRESSES) boundaries.
	std::unique_ptr<std::byte[]> adapterInfo = std::make_unique_for_overwrite<std::byte[]>(neededSize);
	ULONG dwBufLen = neededSize;

	DWORD dwStatus = GetAdaptersAddresses(
		AF_UNSPEC,
		GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_INCLUDE_GATEWAYS | (includeHidden ? GAA_FLAG_INCLUDE_ALL_INTERFACES : 0),
		NULL,
		reinterpret_cast<PIP_ADAPTER_ADDRESSES>(adapterInfo.get()),
		&dwBufLen);

	if (dwStatus == ERROR_BUFFER_OVERFLOW)
	{
		DevCon.WriteLn("DEV9: GetWin32Adapter() buffer too small, resizing");
		neededSize = dwBufLen + 500;
		adapterInfo = std::make_unique_for_overwrite<std::byte[]>(neededSize);
		dwBufLen = neededSize;
		DevCon.WriteLn("DEV9: New size %i", neededSize);

		dwStatus = GetAdaptersAddresses(
			AF_UNSPEC,
			GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_INCLUDE_GATEWAYS | (includeHidden ? GAA_FLAG_INCLUDE_ALL_INTERFACES : 0),
			NULL,
			reinterpret_cast<PIP_ADAPTER_ADDRESSES>(adapterInfo.get()),
			&dwBufLen);
	}
	if (dwStatus != ERROR_SUCCESS)
		return nullptr;

	buffer->swap(adapterInfo);

	// Trigger implicit object creation.
	return std::launder(reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer->get()));
}
bool AdapterUtils::GetAdapter(const std::string& name, Adapter* adapter, AdapterBuffer* buffer)
{
	std::unique_ptr<std::byte[]> adapterInfo;
	PIP_ADAPTER_ADDRESSES pAdapter = GetAllAdapters(&adapterInfo);
	if (pAdapter == nullptr)
		return false;

	do
	{
		if (name == pAdapter->AdapterName)
		{
			*adapter = *pAdapter;
			buffer->swap(adapterInfo);
			return true;
		}

		pAdapter = pAdapter->Next;
	} while (pAdapter);

	return false;
}
bool AdapterUtils::GetAdapterAuto(Adapter* adapter, AdapterBuffer* buffer)
{
	std::unique_ptr<std::byte[]> adapterInfo;
	PIP_ADAPTER_ADDRESSES pAdapter = GetAllAdapters(&adapterInfo);
	if (pAdapter == nullptr)
		return false;

	do
	{
		if (pAdapter->IfType != IF_TYPE_SOFTWARE_LOOPBACK &&
			pAdapter->OperStatus == IfOperStatusUp)
		{
			// Search for an adapter with;
			// IPv4 Address,
			// DNS,
			// Gateway.

			bool hasIPv4 = false;
			bool hasDNS = false;
			bool hasGateway = false;

			// IPv4.
			if (GetAdapterIP(pAdapter).has_value())
				hasIPv4 = true;

			// DNS.
			if (GetDNS(pAdapter).size() > 0)
				hasDNS = true;

			// Gateway.
			if (GetGateways(pAdapter).size() > 0)
				hasGateway = true;

			if (hasIPv4 && hasDNS && hasGateway)
			{
				*adapter = *pAdapter;
				buffer->swap(adapterInfo);
				return true;
			}
		}

		pAdapter = pAdapter->Next;
	} while (pAdapter);

	return false;
}
#elif defined(__POSIX__)
AdapterUtils::Adapter* AdapterUtils::GetAllAdapters(AdapterBuffer* buffer)
{
	ifaddrs* ifa;

	int error = getifaddrs(&ifa);
	if (error)
		return nullptr;

	buffer->reset(ifa);

	return buffer->get();
}
bool AdapterUtils::GetAdapter(const std::string& name, Adapter* adapter, AdapterBuffer* buffer)
{
	std::unique_ptr<ifaddrs, IfAdaptersDeleter> adapterInfo;
	ifaddrs* pAdapter = GetAllAdapters(&adapterInfo);
	if (pAdapter == nullptr)
		return false;

	do
	{
		if (pAdapter->ifa_addr != nullptr &&
			ReadAddressFamily(pAdapter->ifa_addr) == AF_INET &&
			name == pAdapter->ifa_name)
		{
			*adapter = *pAdapter;
			buffer->swap(adapterInfo);
			return true;
		}

		pAdapter = pAdapter->ifa_next;
	} while (pAdapter);

	return false;
}
bool AdapterUtils::GetAdapterAuto(Adapter* adapter, AdapterBuffer* buffer)
{
	std::unique_ptr<ifaddrs, IfAdaptersDeleter> adapterInfo;
	ifaddrs* pAdapter = GetAllAdapters(&adapterInfo);
	if (pAdapter == nullptr)
		return false;

	do
	{
		if ((pAdapter->ifa_flags & IFF_LOOPBACK) == 0 &&
			(pAdapter->ifa_flags & IFF_UP) != 0)
		{
			// Search for an adapter with;
			// IPv4 Address,
			// Gateway.

			bool hasIPv4 = false;
			bool hasGateway = false;

			if (GetAdapterIP(pAdapter).has_value())
				hasIPv4 = true;

			if (GetGateways(pAdapter).size() > 0)
				hasGateway = true;

			if (hasIPv4 && hasGateway)
			{
				*adapter = *pAdapter;
				buffer->swap(adapterInfo);
				return true;
			}
		}

		pAdapter = pAdapter->ifa_next;
	} while (pAdapter);

	return false;
}
#endif

// Internal Helper Functions
#ifdef __POSIX__
int GetIfAdapterIndex(const AdapterUtils::Adapter* adapter)
{
	struct if_nameindex* ifNI;
	ifNI = if_nameindex();
	if (ifNI == nullptr)
	{
		Console.Error("DEV9: if_nameindex Failed");
		return -1;
	}

	struct if_nameindex* i = ifNI;
	while (i->if_index != 0 && i->if_name != nullptr)
	{
		if (strcmp(i->if_name, adapter->ifa_name) == 0)
		{
			if_freenameindex(ifNI);
			return i->if_index;
		}
		i++;
	}
	if_freenameindex(ifNI);
	return -1;
}
#endif

// AdapterMAC.
#ifdef _WIN32
std::optional<MAC_Address> AdapterUtils::GetAdapterMAC(const Adapter* adapter)
{
	if (adapter != nullptr && adapter->PhysicalAddressLength == 6)
	{
		MAC_Address macAddr{};
		std::memcpy(&macAddr, adapter->PhysicalAddress, sizeof(macAddr));
		return macAddr;
	}

	return std::nullopt;
}
#else
std::optional<MAC_Address> AdapterUtils::GetAdapterMAC(const Adapter* adapter)
{
	MAC_Address macAddr{};
#if defined(AF_LINK)
	ifaddrs* adapterInfo;
	const int error = getifaddrs(&adapterInfo);
	if (error)
	{
		Console.Error("DEV9: Failed to get adapter information %s", error);
		return std::nullopt;
	}

	const ScopedGuard adapterInfoGuard = [&adapterInfo]() {
		freeifaddrs(adapterInfo);
	};

	for (const ifaddrs* po = adapterInfo; po; po = po->ifa_next)
	{
		if (strcmp(po->ifa_name, adapter->ifa_name))
			continue;

		if (ReadAddressFamily(po->ifa_addr) != AF_LINK)
			continue;

		// We have a valid MAC address.
		std::memcpy(&macAddr, LLADDR(reinterpret_cast<sockaddr_dl*>(po->ifa_addr)), sizeof(macAddr));
		return macAddr;
	}
	Console.Error("DEV9: Failed to get MAC address for adapter using AF_LINK");
#else
	const int sd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
	if (sd < 0)
	{
		Console.Error("DEV9: Failed to open socket");
		return std::nullopt;
	}
	const ScopedGuard sd_guard = [&sd]() {
		close(sd);
	};
	struct ifreq ifr{};
	StringUtil::Strlcpy(ifr.ifr_name, adapter->ifa_name, std::size(ifr.ifr_name));
#if defined(SIOCGIFHWADDR)
	if (ioctl(sd, SIOCGIFHWADDR, &ifr) < 0)
	{
		Console.Error("DEV9: Failed to get MAC address for adapter using SIOCGIFHWADDR");
		return std::nullopt;
	}
	std::memcpy(&macAddr, &ifr.ifr_hwaddr.sa_data, sizeof(macAddr));
	return macAddr;
#elif defined(SIOCGENADDR)
	if (ioctl(sd, SIOCGENADDR, &ifr) < 0)
	{
		Console.Error("DEV9: Failed to get MAC address for adapter using SIOCGENADDR");
		return std::nullopt;
	}
	std::memcpy(&macAddr, &ifr.ifr_enaddr, sizeof(macAddr));
	return macAddr;
#endif
#endif
	Console.Error("DEV9: Unsupported OS, can't get network features");
	return std::nullopt;
}
#endif

// AdapterIP.
#ifdef _WIN32
std::optional<IP_Address> AdapterUtils::GetAdapterIP(const Adapter* adapter)
{
	PIP_ADAPTER_UNICAST_ADDRESS address = nullptr;
	if (adapter != nullptr)
	{
		address = adapter->FirstUnicastAddress;
		while (address != nullptr && ReadAddressFamily(address->Address.lpSockaddr) != AF_INET)
			address = address->Next;
	}

	if (address != nullptr)
	{
		const sockaddr_in* sockaddr = reinterpret_cast<sockaddr_in*>(address->Address.lpSockaddr);
		return std::bit_cast<IP_Address>(sockaddr->sin_addr);
	}
	return std::nullopt;
}
#elif defined(__POSIX__)
std::optional<IP_Address> AdapterUtils::GetAdapterIP(const Adapter* adapter)
{
	sockaddr_in* address = nullptr;
	if (adapter != nullptr)
	{
		if (adapter->ifa_addr != nullptr && ReadAddressFamily(adapter->ifa_addr) == AF_INET)
			address = reinterpret_cast<sockaddr_in*>(adapter->ifa_addr);
	}

	if (address != nullptr)
		return std::bit_cast<IP_Address>(address->sin_addr);

	return std::nullopt;
}
#endif

// Gateways.
#ifdef _WIN32
std::vector<IP_Address> AdapterUtils::GetGateways(const Adapter* adapter)
{
	if (adapter == nullptr)
		return {};

	std::vector<IP_Address> collection;

	PIP_ADAPTER_GATEWAY_ADDRESS address = adapter->FirstGatewayAddress;
	while (address != nullptr)
	{
		if (ReadAddressFamily(address->Address.lpSockaddr) == AF_INET)
		{
			const sockaddr_in* sockaddr = reinterpret_cast<sockaddr_in*>(address->Address.lpSockaddr);
			collection.push_back(std::bit_cast<IP_Address>(sockaddr->sin_addr));
		}
		address = address->Next;
	}

	return collection;
}
#elif defined(__POSIX__)
#ifdef __linux__
std::vector<IP_Address> AdapterUtils::GetGateways(const Adapter* adapter)
{
	// /proc/net/route contains some information about gateway addresses,
	// and separates the information about by each interface.
	if (adapter == nullptr)
		return {};

	std::vector<IP_Address> collection;
	std::vector<std::string> routeLines;
	std::fstream route("/proc/net/route", std::ios::in);
	if (route.fail())
	{
		route.close();
		Console.Error("DEV9: Failed to open /proc/net/route");
		return collection;
	}

	std::string line;
	while (std::getline(route, line))
		routeLines.push_back(line);
	route.close();

	// Columns are as follows (first-line header):
	// Iface  Destination  Gateway  Flags  RefCnt  Use  Metric  Mask  MTU  Window  IRTT.
	for (size_t i = 1; i < routeLines.size(); i++)
	{
		const std::string line = routeLines[i];
		if (line.rfind(adapter->ifa_name, 0) == 0)
		{
			const std::vector<std::string_view> split = StringUtil::SplitString(line, '\t', true);
			const std::string gatewayIPHex{split[2]};
			// stoi assumes hex values are unsigned, but tries to store it in a signed int,
			// this results in a std::out_of_range exception for addresses ending in a number > 128.
			// We don't have a stoui for (unsigned int), so instead use stoul for (unsigned long).
			const u32 addressValue = static_cast<u32>(std::stoul(gatewayIPHex, 0, 16));
			// Skip device routes without valid NextHop IP address.
			if (addressValue != 0)
				collection.push_back(std::bit_cast<IP_Address>(addressValue));
		}
	}
	return collection;
}
#elif defined(__FreeBSD__) || defined(__APPLE__)
std::vector<IP_Address> AdapterUtils::GetGateways(const Adapter* adapter)
{
	if (adapter == nullptr)
		return {};

	std::vector<IP_Address> collection;

	// Get index for our adapter by matching the adapter name.
	int ifIndex = GetIfAdapterIndex(adapter);

	// Check if we found the adapter.
	if (ifIndex == -1)
	{
		Console.Error("DEV9: Failed to get index for adapter");
		return collection;
	}

	// Find the gateway by looking though the routing information.
	// Ask only for AF_NET, so we can assume any given sockaddr is a sockaddr_in
	int name[] = {CTL_NET, PF_ROUTE, 0, AF_INET, NET_RT_DUMP, 0};
	size_t bufferLen = 0;

	if (sysctl(name, 6, NULL, &bufferLen, NULL, 0) != 0)
	{
		Console.Error("DEV9: Failed to perform NET_RT_DUMP");
		return collection;
	}

	// bufferLen is an estimate, double it to be safe.
	bufferLen *= 2;
	std::unique_ptr<std::byte[]> buffer = std::make_unique<std::byte[]>(bufferLen);

	if (sysctl(name, 6, buffer.get(), &bufferLen, NULL, 0) != 0)
	{
		Console.Error("DEV9: Failed to perform NET_RT_DUMP");
		return collection;
	}

	rt_msghdr* hdr;
	for (size_t i = 0; i < bufferLen; i += hdr->rtm_msglen)
	{
		// Relying on implicit object creation for following code
		hdr = reinterpret_cast<rt_msghdr*>(&buffer[i]);

		if (hdr->rtm_flags & RTF_GATEWAY && hdr->rtm_addrs & RTA_GATEWAY && (hdr->rtm_index == ifIndex))
		{
			sockaddr_in* sockaddrs = reinterpret_cast<sockaddr_in*>(hdr + 1);
			pxAssert(sockaddrs[RTAX_DST].sin_family == AF_INET);

			// Default gateway has no destination address.
			sockaddr_in* sockaddr = &sockaddrs[RTAX_DST];
			if (sockaddr->sin_addr.s_addr != 0)
				continue;

			sockaddr = &sockaddrs[RTAX_GATEWAY];
			collection.push_back(std::bit_cast<IP_Address>(sockaddr->sin_addr));
		}
	}
	return collection;
}
#else
std::vector<IP_Address> AdapterUtils::GetGateways(Adapter* adapter)
{
	Console.Error("DEV9: Unsupported OS, can't find Gateway");
	return {};
}
#endif
#endif

// DNS.
#ifdef _WIN32
std::vector<IP_Address> AdapterUtils::GetDNS(const Adapter* adapter)
{
	if (adapter == nullptr)
		return {};

	std::vector<IP_Address> collection;

	PIP_ADAPTER_DNS_SERVER_ADDRESS address = adapter->FirstDnsServerAddress;
	while (address != nullptr)
	{
		if (ReadAddressFamily(address->Address.lpSockaddr) == AF_INET)
		{
			const sockaddr_in* sockaddr = reinterpret_cast<sockaddr_in*>(address->Address.lpSockaddr);
			collection.push_back(std::bit_cast<IP_Address>(sockaddr->sin_addr));
		}
		address = address->Next;
	}

	return collection;
}
#elif defined(__POSIX__)
#ifdef __linux__
std::vector<IP_Address> GetSystemdDNS(const Adapter* adapter)
{
	std::vector<IP_Address> collection;

	// Get index for our adapter by matching the adapter name.
	int ifIndex = GetIfAdapterIndex(adapter);

	// Check if we found the adapter.
	if (ifIndex == -1)
	{
		Console.Error("DEV9: Failed to get index for adapter");
		return collection;
	}

	// "errorDBus" doesn't need to be cleared in the end with "dbus_message_unref" at least if there is
	// no error set, since calling "dbus_error_free" reinitializes it like "dbus_error_init" after freeing.
	DBusError errorDBus;
	dbus_error_init(&errorDBus);
	ScopedGuard errorCleanup = [&errorDBus]()
	{
		if (dbus_error_is_set(&errorDBus))
		{
			Console.Error("DEV9: DBUS error %s, %s", errorDBus.name, errorDBus.message);
			dbus_error_free(&errorDBus);
		}
	};

	// "dbus_bus_get" gets a pointer to the same connection in libdbus, if exists, without creating a new connection.
	// for existing connections the ref counter is incremented, so we must call "dbus_connection_unref" when we are done.
	DBusConnection* connection = nullptr;
	if (!(connection = dbus_bus_get(DBUS_BUS_SYSTEM, &errorDBus)))
		return collection;
	ScopedGuard connectionCleanup = [&connection]() { dbus_connection_unref(connection); };

	// Connection will contain regular DNS entries, followed by FallbackDNS entries.
	const char* resolverIface = "org.freedesktop.resolve1.Manager";
	const char* dnsProperties[] = {"DNS", "FallbackDNS"};

	for (size_t i = 0; i < std::size(dnsProperties); i++)
	{
		DBusMessage* messageGet = nullptr;
		if (!(messageGet = dbus_message_new_method_call("org.freedesktop.resolve1", "/org/freedesktop/resolve1", "org.freedesktop.DBus.Properties", "Get")))
			return collection;
		ScopedGuard MessageGetCleanup = [&messageGet]() { dbus_message_unref(messageGet); };

		dbus_message_append_args(messageGet,
			DBUS_TYPE_STRING, &resolverIface,
			DBUS_TYPE_STRING, &dnsProperties[i],
			DBUS_TYPE_INVALID);

		// Send message and get response.
		DBusMessage* response = nullptr;
		if (!(response = dbus_connection_send_with_reply_and_block(connection, messageGet, DBUS_TIMEOUT_USE_DEFAULT, &errorDBus)))
			return collection;
		ScopedGuard responseCleanup = [&response] { dbus_message_unref(response); };

		DBusMessageIter retVariant;
		DBusMessageIter retArray;
		DBusMessageIter retIter;

		// Initialize an iterator for the response, gets freed with the response message.
		dbus_message_iter_init(response, &retVariant);

		// DBus documentation advises us to validate type.
		if (dbus_message_iter_get_arg_type(&retVariant) != DBUS_TYPE_VARIANT)
		{
			Console.Error("DEV9: Unexpected type in DBus response");
			return collection;
		}
		dbus_message_iter_recurse(&retVariant, &retArray);

		if (dbus_message_iter_get_arg_type(&retArray) != DBUS_TYPE_ARRAY)
		{
			Console.Error("DEV9: Unexpected type in DBus variant");
			return collection;
		}
		dbus_message_iter_recurse(&retArray, &retIter);

		if (dbus_message_iter_get_arg_type(&retIter) != DBUS_TYPE_STRUCT)
		{
			Console.Error("DEV9: Unexpected type in DBus array");
			return collection;
		}

		std::vector<IP_Address> collectionAdapter;
		std::vector<IP_Address> collectionGlobal;
		do
		{
			DBusMessageIter retStruct;
			dbus_message_iter_recurse(&retIter, &retStruct);

			/* 
			 * Structure has layout
			 * Interface Index
			 * Address Family
			 * DNS Address
			 */

			int index = -1;
			int af = 0;
			IP_Address* dns = nullptr;

			if (dbus_message_iter_get_arg_type(&retStruct) != DBUS_TYPE_INT32)
			{
				Console.Error("DEV9: Error reading DBus DNS struct");
				break;
			}
			dbus_message_iter_get_basic(&retStruct, &index);

			if (!dbus_message_iter_next(&retStruct) ||
				dbus_message_iter_get_arg_type(&retStruct) != DBUS_TYPE_INT32)
			{
				Console.Error("DEV9: Error reading DBus DNS struct");
				break;
			}
			dbus_message_iter_get_basic(&retStruct, &af);

			// We only want IPv4
			if (af != AF_INET)
				continue;

			if (!dbus_message_iter_next(&retStruct) ||
				dbus_message_iter_get_arg_type(&retStruct) != DBUS_TYPE_ARRAY)
			{
				Console.Error("DEV9: Error reading DBus DNS struct");
				break;
			}

			DBusMessageIter retDNS;
			dbus_message_iter_recurse(&retStruct, &retDNS);

			int ipSize = 4;
			if (dbus_message_iter_get_element_count(&retStruct) != ipSize ||
				dbus_message_iter_get_arg_type(&retDNS) != DBUS_TYPE_BYTE)
			{
				Console.Error("DEV9: Error reading DBus DNS struct");
				break;
			}
			dbus_message_iter_get_fixed_array(&retDNS, &dns, &ipSize);

			if (index == 0)
				collectionGlobal.push_back(*dns);
			if (index == ifIndex)
				collectionAdapter.push_back(*dns);

		} while (dbus_message_iter_next(&retIter));

		// List adapter DNS addresses before global addresses.
		// Preallocate memory.
		collection.reserve(collection.size() + collectionAdapter.size() + collectionGlobal.size());
		collection.insert(collection.end(), collectionAdapter.begin(), collectionAdapter.end());
		collection.insert(collection.end(), collectionGlobal.begin(), collectionGlobal.end());
	}

	// Remove duplicates without sorting
	std::unordered_set<IP_Address> seen;

	auto newEnd = std::remove_if(collection.begin(), collection.end(), [&seen](const IP_Address& value)
	{
		if (seen.find(value) != seen.end())
			return true;

		seen.insert(value);
		return false;
	});

	collection.erase(newEnd, collection.end());

	return collection;
}
#endif

std::vector<IP_Address> AdapterUtils::GetDNS(const Adapter* adapter)
{
	// On Linux and OSX, DNS is system wide, not adapter specific, so we can ignore the adapter parameter.
	// Except when systemd resolved is in use, which provides that feature on linux.

	// Parse /etc/resolv.conf for all of the "nameserver" entries.
	// These are the DNS servers the machine is configured to use.
	// On OSX, this file is not directly used by most processes for DNS
	// queries/routing, but it is automatically generated instead, with
	// the machine's DNS servers listed in it.
	if (adapter == nullptr)
		return {};

	std::vector<IP_Address> collection;

	std::fstream servers("/etc/resolv.conf", std::ios::in);
	if (servers.fail())
	{
		servers.close();
		Console.Error("DEV9: Failed to open /etc/resolv.conf");
		return collection;
	}

	std::string line;
	std::vector<std::string> serversLines;
	while (std::getline(servers, line))
		serversLines.push_back(line);
	servers.close();

	for (size_t i = 1; i < serversLines.size(); i++)
	{
		const std::string line = serversLines[i];
		if (line.rfind("nameserver", 0) == 0)
		{
			std::vector<std::string_view> split = StringUtil::SplitString(line, '\t', true);
			if (split.size() == 1)
				split = StringUtil::SplitString(line, ' ', true);
			const std::string dns{split[1]};

			IP_Address address;
			if (inet_pton(AF_INET, dns.c_str(), &address) != 1)
				continue;

#ifdef __linux__
			const IP_Address systemdDNS{{{127, 0, 0, 53}}};
			if (address == systemdDNS)
				return GetSystemdDNS(adapter);
#endif

			collection.push_back(address);
		}
	}
	return collection;
}
#endif
