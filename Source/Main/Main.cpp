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

Configuration Parameter;
std::string LocalhostPTR[THREAD_PARTNUM] = {("?"), ("?"), (""), ("")};

extern std::wstring Path;

//The Main function of program
int main(int argc, char *argv[])
{
/*
//Handle the system signal
	SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE);
*/

//Get Path
	if (GetServiceInfo() == RETURN_ERROR)
		return RETURN_ERROR;

//Read configuration file and WinPcap initialization
	Parameter.PrintError = true;
	if (Parameter.ReadParameter() == RETURN_ERROR || CaptureInitialization() == RETURN_ERROR)
		return RETURN_ERROR;

//Get Localhost DNS PTR Records
	std::thread IPv6LocalAddressThread(LocalAddressToPTR, LocalhostPTR[0], AF_INET6);
	std::thread IPv4LocalAddressThread(LocalAddressToPTR, LocalhostPTR[1], AF_INET);
	IPv6LocalAddressThread.detach();
	IPv4LocalAddressThread.detach();

//Read Hosts
	std::thread HostsThread(&Configuration::ReadHosts, std::ref(Parameter));
	HostsThread.detach();

//Windows Firewall Test
	if (FirewallTest() == RETURN_ERROR)
	{
		PrintError(4, _T("Windows Firewall Test failed"), NULL, NULL);
		return RETURN_ERROR;
	}

//Service initialization
	SERVICE_TABLE_ENTRY ServiceTable[] = 
	{
		{LOCALSERVERNAME, (LPSERVICE_MAIN_FUNCTION)ServiceMain},
		{NULL, NULL}
	};

//Start service
	if (!StartServiceCtrlDispatcher(ServiceTable))
	{
		PrintError(1, _T("Service start failed(It's probably a Firewall Test, please restart service and check once again)"), GetLastError(), NULL);

		WSACleanup();
		return RETURN_ERROR;
	}

	WSACleanup();
	return 0;
}

//Winsock initialization and Windows Firewall Test
inline SSIZE_T __stdcall FirewallTest()
{
//Winsock and socket initialization
	WSADATA WSAData = {0};
	if (WSAStartup(MAKEWORD(2, 2), &WSAData) != 0 || LOBYTE(WSAData.wVersion) != 2 || HIBYTE(WSAData.wVersion) != 2)
    {
		PrintError(4, _T("Winsock initialization failed"), WSAGetLastError(), NULL);

		WSACleanup();
		return RETURN_ERROR;
	}

	srand((UINT)time((time_t *)NULL));
//Socket initialization(IPv4)
	SYSTEM_SOCKET LocalFirewall = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	sockaddr_in FirewallAddr = {0};
	FirewallAddr.sin_family = AF_INET;
	FirewallAddr.sin_addr.S_un.S_addr = INADDR_ANY;
	FirewallAddr.sin_port = htons((USHORT)(rand() % 65535 + 1));

/*
	srand((UINT)time((time_t *)NULL));
//Socket initialization(IPv6)
	SYSTEM_SOCKET LocalFirewall = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
	sockaddr_in6 FirewallAddr = {0};
	FirewallAddr.sin6_family = AF_INET6;
	FirewallAddr.sin6_addr = in6addr_any;
	FirewallAddr.sin6_port = htons((USHORT)(rand() % 65535 + 1));
*/

//Bind local socket
	if (LocalFirewall == INVALID_SOCKET || bind(LocalFirewall, (PSOCKADDR)&FirewallAddr, sizeof(sockaddr_in)) == SOCKET_ERROR)
	{
		closesocket(LocalFirewall);
		WSACleanup();
		return RETURN_ERROR;
	}

	closesocket(LocalFirewall);
	return 0;
}