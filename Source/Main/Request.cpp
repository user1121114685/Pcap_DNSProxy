// This code is part of Pcap_DNSProxy
// Copyright (C) 2012-2014 Chengr28
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.


#include "Pcap_DNSProxy.h"

#define Interval          5000        //5000ms or 5s between every sending
#define OnceSend          3

extern Configuration Parameter;
extern PortTable PortList;

//Get Hop Limits/TTL with common DNS request
SSIZE_T __stdcall DomainTest(const size_t Protocol)
{
//Turn OFF Domain Test when running in TCP Mode
	if (Parameter.TCPMode)
		return 0;

//Initialization
	PSTR Buffer = nullptr, DNSQuery = nullptr;
	try {
		Buffer = new char[PACKET_MAXSIZE]();
		DNSQuery = new char[PACKET_MAXSIZE/4]();
	}
	catch (std::bad_alloc)
	{
		PrintError(1, _T("Memory allocation failed"), NULL, NULL);

		delete[] Buffer;
		delete[] DNSQuery;
		TerminateService();
		return RETURN_ERROR;
	}
	memset(Buffer, 0, PACKET_MAXSIZE);
	memset(DNSQuery, 0, PACKET_MAXSIZE/4);
	SOCKET_DATA SetProtocol = {0};

//Set request protocol
	if (Protocol == AF_INET6) //IPv6
		SetProtocol.AddrLen = sizeof(sockaddr_in6);
	else //IPv4
		SetProtocol.AddrLen = sizeof(sockaddr_in);

//Make a DNS request with Doamin Test packet
	dns_hdr *TestHdr = (dns_hdr *)Buffer;
	TestHdr->ID = Parameter.DomainTestOptions.DomainTestID;
	TestHdr->Flags = htons(0x0100); //System Standard query
	TestHdr->Questions = htons(0x0001);
	size_t TestLength =  0;

//From Parameter
	if (Parameter.DomainTestOptions.DomainTestCheck)
	{
		TestLength = CharToDNSQuery(Parameter.DomainTestOptions.DomainTest, DNSQuery);
		if (TestLength > 0 && TestLength < PACKET_MAXSIZE - sizeof(dns_hdr))
		{
			memcpy(Buffer + sizeof(dns_hdr), DNSQuery, TestLength);
			dns_qry *TestQry = (dns_qry *)(Buffer + sizeof(dns_hdr) + TestLength);
			TestQry->Classes = htons(Class_IN);
			if (Protocol == AF_INET6)
				TestQry->Type = htons(AAAA_Records);
			else 
				TestQry->Type = htons(A_Records);
			delete[] DNSQuery;
		}
		else {
			delete[] Buffer;
			delete[] DNSQuery;
			return RETURN_ERROR;
		}
	}

//Send
	size_t Times = 0;
	while (true)
	{
		if (Times == OnceSend)
		{
			Times = 0;
			if (Parameter.DNSTarget.IPv4 && Parameter.HopLimitOptions.IPv4TTL == 0 || //IPv4
				Parameter.DNSTarget.IPv6 && Parameter.HopLimitOptions.IPv6HopLimit == 0) //IPv6
			{
				Sleep(Interval); //5 seconds between every sending.
				continue;
			}

			Sleep((DWORD)Parameter.DomainTestOptions.DomainTestSpeed);
		}
		else {
		//Ramdom domain request
			if (!Parameter.DomainTestOptions.DomainTestCheck)
			{
				RamdomDomain(Parameter.DomainTestOptions.DomainTest, PACKET_MAXSIZE/8);
				TestLength = CharToDNSQuery(Parameter.DomainTestOptions.DomainTest, DNSQuery);
				memcpy(Buffer + sizeof(dns_hdr), DNSQuery, TestLength);
				
				dns_qry *TestQry = (dns_qry *)(Buffer + sizeof(dns_hdr) + TestLength);
				TestQry->Classes = htons(Class_IN);
				if (Protocol == AF_INET6)
					TestQry->Type = htons(AAAA_Records);
				else 
					TestQry->Type = htons(A_Records);
			}

			UDPRequest(Buffer, TestLength + sizeof(dns_hdr) + 4, SetProtocol, -1);
			Sleep(Interval);
			Times++;
		}
	}

	delete[] DNSQuery;
	delete[] Buffer;
	return 0;
}

//Internet Control Message Protocol Echo(Ping) request
SSIZE_T __stdcall ICMPEcho()
{
//Initialization
	PSTR Buffer = nullptr;
	try {
		Buffer = new char[PACKET_MAXSIZE]();
	}
	catch (std::bad_alloc)
	{
		PrintError(1, _T("Memory allocation failed"), NULL, NULL);

		TerminateService();
		return RETURN_ERROR;
	}
	memset(Buffer, 0, PACKET_MAXSIZE);
	sockaddr_storage Addr = {0};
	SYSTEM_SOCKET Request = 0;

//Make a ICMP request echo packet
	icmp_hdr *icmp = (icmp_hdr *)Buffer;
	icmp->Type = 8; //Echo(Ping) request type
	icmp->ID = Parameter.ICMPOptions.ICMPID;
	icmp->Sequence = Parameter.ICMPOptions.ICMPSequence;
	memcpy(Buffer + sizeof(icmp_hdr), Parameter.PaddingDataOptions.PaddingData, Parameter.PaddingDataOptions.PaddingDataLength - 1);
	icmp->Checksum = GetChecksum((PUSHORT)Buffer, sizeof(icmp_hdr) + Parameter.PaddingDataOptions.PaddingDataLength - 1);

	Request = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	((PSOCKADDR_IN)&Addr)->sin_family = AF_INET;
	((PSOCKADDR_IN)&Addr)->sin_addr = Parameter.DNSTarget.IPv4Target;

//Check socket
	if (Request == INVALID_SOCKET)
	{
		PrintError(4, _T("ICMP Echo(Ping) request error"), WSAGetLastError(), NULL);

		delete[] Buffer;
		closesocket(Request);
		return RETURN_ERROR;
	}

//Send
	size_t Times = 0;
	while (true)
	{
		sendto(Request, Buffer, (int)(sizeof(icmp_hdr) + Parameter.PaddingDataOptions.PaddingDataLength - 1), NULL, (PSOCKADDR)&Addr, sizeof(sockaddr_in));

		if (Times == OnceSend)
		{
			Times = 0;
			if (Parameter.HopLimitOptions.IPv4TTL == 0)
			{
				Sleep(Interval); //5 seconds between every sending.
				continue;
			}

			Sleep((DWORD)Parameter.ICMPOptions.ICMPSpeed);
		}
		else {
			Sleep(Interval);
			Times++;
		}
	}

	delete[] Buffer;
	closesocket(Request);
	return 0;
}

//ICMP Echo(Ping) request
SSIZE_T __stdcall ICMPv6Echo()
{
//Initialization
	PSTR Buffer = nullptr, ICMPv6Checksum = nullptr;
	try {
		Buffer = new char[PACKET_MAXSIZE]();
		ICMPv6Checksum = new char[PACKET_MAXSIZE]();
	}
	catch (std::bad_alloc)
	{
		PrintError(1, _T("Memory allocation failed"), NULL, NULL);

		delete[] Buffer;
		delete[] ICMPv6Checksum;
		TerminateService();
		return RETURN_ERROR;
	}
	memset(Buffer, 0, PACKET_MAXSIZE);
	memset(ICMPv6Checksum, 0, PACKET_MAXSIZE);
	sockaddr_storage Addr = {0}, Validate = {0};
	SYSTEM_SOCKET Request = 0;

//Make a IPv6 ICMPv6 request echo packet
	bool LocalIPv6 = false;
	icmpv6_hdr *icmpv6 = (icmpv6_hdr *)Buffer;
	icmpv6->Type = ICMPV6_REQUEST;
	icmpv6->Code = 0;
	icmpv6->ID = Parameter.ICMPOptions.ICMPID;
	icmpv6->Sequence = Parameter.ICMPOptions.ICMPSequence;

	ipv6_psd_hdr *psd = (ipv6_psd_hdr *)ICMPv6Checksum;
	psd->Dst = Parameter.DNSTarget.IPv6Target;

//Validate local IPv6 address
	if (!GetLocalAddress(Validate, AF_INET6))
	{
		PrintError(4, _T("Get local IPv6 address error"), NULL, NULL);

		delete[] Buffer;
		delete[] ICMPv6Checksum;
		return RETURN_ERROR;
	}
	psd->Src = ((PSOCKADDR_IN6)&Validate)->sin6_addr;
//End

	psd->Length = htonl((ULONG)(sizeof(icmpv6_hdr) + Parameter.PaddingDataOptions.PaddingDataLength - 1));
	psd->Next_Header = IPPROTO_ICMPV6;

	memcpy(ICMPv6Checksum + sizeof(ipv6_psd_hdr), icmpv6, sizeof(icmpv6_hdr));
	memcpy(ICMPv6Checksum + sizeof(ipv6_psd_hdr) + sizeof(icmpv6_hdr), &Parameter.PaddingDataOptions.PaddingData, Parameter.PaddingDataOptions.PaddingDataLength - 1);
	icmpv6->Checksum = htons(GetChecksum((PUSHORT)ICMPv6Checksum, sizeof(ipv6_psd_hdr) + sizeof(icmpv6_hdr) + Parameter.PaddingDataOptions.PaddingDataLength - 1));
	delete[] ICMPv6Checksum;

	Request = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
	((PSOCKADDR_IN6)&Addr)->sin6_family = AF_INET6;
	((PSOCKADDR_IN6)&Addr)->sin6_addr = Parameter.DNSTarget.IPv6Target;

//Check socket
	if (Request == INVALID_SOCKET)
	{
		PrintError(4, _T("ICMPv6 Echo(Ping) request error"), WSAGetLastError(), NULL);

		delete[] Buffer;
		closesocket(Request);
		return RETURN_ERROR;
	}

//Send
	size_t Times = 0;
	while (true)
	{
		sendto(Request, Buffer, (int)(sizeof(icmpv6_hdr) + Parameter.PaddingDataOptions.PaddingDataLength - 1), NULL, (PSOCKADDR)&Addr, sizeof(sockaddr_in6));

		if (Times == OnceSend)
		{
			Times = 0;
			if (Parameter.HopLimitOptions.IPv6HopLimit == 0)
			{
				Sleep(Interval);
				continue;
			}

			Sleep((DWORD)Parameter.ICMPOptions.ICMPSpeed);
		}
		else {
			Times++;
			Sleep(Interval);
		}
	}

	delete[] Buffer;
	closesocket(Request);
	return 0;
}

//Transmission and reception of TCP protocol
SSIZE_T __stdcall TCPRequest(const PSTR Send, PSTR Recv, const size_t SendSize, const size_t RecvSize, const SOCKET_DATA TargetData)
{
//Initialization
	PSTR OriginalSend = nullptr, SendBuffer = nullptr, RecvBuffer = nullptr;
	try {
		OriginalSend = new char[PACKET_MAXSIZE]();
		SendBuffer = new char[PACKET_MAXSIZE]();
		RecvBuffer = new char[PACKET_MAXSIZE]();
	}
	catch (std::bad_alloc)
	{
		PrintError(1, _T("Memory allocation failed"), NULL, NULL);

		delete[] OriginalSend;
		delete[] SendBuffer;
		delete[] RecvBuffer;
		TerminateService();
		return RETURN_ERROR;
	}
	memset(OriginalSend, 0, PACKET_MAXSIZE);
	memset(SendBuffer, 0, PACKET_MAXSIZE);
	memset(RecvBuffer, 0, PACKET_MAXSIZE);
	sockaddr_storage Addr = {0};
	SYSTEM_SOCKET TCPSocket = 0;
	memcpy(OriginalSend, Send, SendSize);

//Add length of request packet(It must be written in header when transpot with TCP protocol)
	USHORT DataLength = htons((USHORT)SendSize);
	memcpy(SendBuffer, &DataLength, sizeof(USHORT));
	memcpy(SendBuffer + sizeof(USHORT), OriginalSend, SendSize);
	delete[] OriginalSend;

//Socket initialization
	if (TargetData.AddrLen == sizeof(sockaddr_in6)) //IPv6
	{
		TCPSocket = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
		((PSOCKADDR_IN6)&Addr)->sin6_addr = Parameter.DNSTarget.IPv6Target;
		((PSOCKADDR_IN6)&Addr)->sin6_family = AF_INET6;
		((PSOCKADDR_IN6)&Addr)->sin6_port = htons(DNS_Port);
	}
	else { //IPv4
		TCPSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		((PSOCKADDR_IN)&Addr)->sin_addr = Parameter.DNSTarget.IPv4Target;
		((PSOCKADDR_IN)&Addr)->sin_family = AF_INET;
		((PSOCKADDR_IN)&Addr)->sin_port = htons(DNS_Port);
	}

/*
//TCP Keepalive mode
	BOOL bKeepAlive = TRUE;
	if (setsockopt(TCPSocket, SOL_SOCKET, SO_KEEPALIVE, (PSTR)&bKeepAlive, sizeof(bKeepAlive)) == SOCKET_ERROR)
	{
		delete[] SendBuffer;
		delete[] RecvBuffer;
		closesocket(TCPSocket);
		return RETURN_ERROR;
	}

	tcp_keepalive alive_in = {0};
	tcp_keepalive alive_out = {0};
	alive_in.keepalivetime = TIME_OUT;
	alive_in.keepaliveinterval = TIME_OUT*2;
	alive_in.onoff = TRUE;
	ULONG ulBytesReturn = 0;
	if (WSAIoctl(TCPSocket, SIO_KEEPALIVE_VALS, &alive_in, sizeof(alive_in), &alive_out, sizeof(alive_out), &ulBytesReturn, NULL, NULL) == SOCKET_ERROR)
	{
		delete[] SendBuffer;
		delete[] RecvBuffer;
		closesocket(TCPSocket);
		return RETURN_ERROR;
	}
*/

	if (TCPSocket == INVALID_SOCKET)
	{
		PrintError(4, _T("TCP request initialization failed"), WSAGetLastError(), NULL);

		delete[] SendBuffer;
		delete[] RecvBuffer;
		closesocket(TCPSocket);
		return RETURN_ERROR;
	}

	if (connect(TCPSocket, (PSOCKADDR)&Addr, TargetData.AddrLen) == SOCKET_ERROR) //The connection is RESET or other errors when connecting.
	{
		delete[] SendBuffer;
		delete[] RecvBuffer;
		closesocket(TCPSocket);
		return 0;
	}

//Send request
	if (send(TCPSocket, SendBuffer, (int)(SendSize + sizeof(USHORT)), NULL) == SOCKET_ERROR) //The connection is RESET or other errors when sending.
	{
		delete[] SendBuffer;
		delete[] RecvBuffer;
		closesocket(TCPSocket);
		return 0;
	}
	delete[] SendBuffer;

//Receive result
	SSIZE_T RecvLen = recv(TCPSocket, RecvBuffer, (int)RecvSize, NULL) - sizeof(USHORT);
	if (RecvLen < 0) //The connection is RESET or other errors when sending.
	{
		delete[] RecvBuffer;
		closesocket(TCPSocket);
		return RecvLen;
	}
	if (RecvLen == 0)
	{
		PrintError(4, _T("TCP request error"), NULL, NULL);

		delete[] RecvBuffer;
		closesocket(TCPSocket);
		return RETURN_ERROR;
	}
	memcpy(Recv, RecvBuffer + sizeof(USHORT), RecvLen);

	delete[] RecvBuffer;
	closesocket(TCPSocket);
	return RecvLen;
}

//Transmission of UDP protocol
SSIZE_T __stdcall UDPRequest(const PSTR Send, const size_t Length, const SOCKET_DATA TargetData, const SSIZE_T Index)
{
//Initialization
	PSTR SendBuffer = nullptr;
	try {
		SendBuffer = new char[PACKET_MAXSIZE]();
	}
	catch (std::bad_alloc)
	{
		PrintError(1, _T("Memory allocation failed"), NULL, NULL);

		TerminateService();
		return RETURN_ERROR;
	}
	memset(SendBuffer, 0, PACKET_MAXSIZE);
	sockaddr_storage Addr = {0};
	SYSTEM_SOCKET UDPSocket = 0;
	memcpy(SendBuffer, Send, Length);

//Socket initialization
	if (TargetData.AddrLen == sizeof(sockaddr_in6)) //IPv6
		{
			UDPSocket = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
			((PSOCKADDR_IN6)&Addr)->sin6_addr = Parameter.DNSTarget.IPv6Target;
			((PSOCKADDR_IN6)&Addr)->sin6_family = AF_INET6;
			((PSOCKADDR_IN6)&Addr)->sin6_port = htons(DNS_Port);
		}
	else { //IPv4
		UDPSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		((PSOCKADDR_IN)&Addr)->sin_addr = Parameter.DNSTarget.IPv4Target;
		((PSOCKADDR_IN)&Addr)->sin_family = AF_INET;
		((PSOCKADDR_IN)&Addr)->sin_port = htons(DNS_Port);
	}

	if (UDPSocket == INVALID_SOCKET)
	{
		PrintError(4, _T("UDP request initialization failed"), WSAGetLastError(), NULL);

		delete[] SendBuffer;
		closesocket(UDPSocket);
		return RETURN_ERROR;
	}

//Send request
	if (sendto(UDPSocket, SendBuffer, (int)Length, 0, (PSOCKADDR)&Addr, TargetData.AddrLen) == SOCKET_ERROR)
	{
		PrintError(4, _T("UDP request error"), WSAGetLastError(), NULL);

		delete[] SendBuffer;
		closesocket(UDPSocket);
		return RETURN_ERROR;
	}

//Sign in port list
	if (Index >= 0)
	{
		getsockname(UDPSocket, (PSOCKADDR)&Addr, (PINT)&(TargetData.AddrLen));
		if (TargetData.AddrLen == sizeof(sockaddr_in6)) //IPv6
			PortList.SendPort[Index] = ((PSOCKADDR_IN6)&Addr)->sin6_port;
		else //IPv4
			PortList.SendPort[Index] = ((PSOCKADDR_IN)&Addr)->sin_port;
	}

	delete[] SendBuffer;
	closesocket(UDPSocket);
	return 0;
}