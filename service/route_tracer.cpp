/**
 * @file	route_tracer.cpp
 * @author	Jichan (development@jc-lab.net / http://ablog.jc-lab.net/ )
 * @date	2019/07/08
 * @copyright Copyright (C) 2019 jichan.\n
 *            This software may be modified and distributed under the terms
 *            of the Apache License 2.0.  See the LICENSE file for details.
 * @brief     Find routes from peer to the server by ICMP and TTL.
 */

#include "route_tracer.hpp"

#include <uvw/udp.hpp>

#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <ipexport.h>
#include <icmpapi.h>
#include <tchar.h>

#include <stdio.h>
#include <stdlib.h>

#include <iostream>

namespace grida {
	namespace service {
		RouteTracer::RouteTracer(const internal::LoopProvider* provider)
			: LoopUse(provider)
		{

		}

		int RouteTracer::tracert(std::unique_ptr< std::list<TestResultItem> >& result_list, const std::string& ip)
		{
			std::basic_string<TCHAR> tstr_ip(ip.begin(), ip.end());
			HANDLE hIcmpFile;
			unsigned char reply_buffer[sizeof(ICMP_ECHO_REPLY) + 16 + 8];
			char send_buffer[16] = { 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15 };
			DWORD dwResult;
			IPAddr ipaddr = INADDR_NONE;
			int depth;
			bool done = false;
			int inet_type = AF_INET;
			PICMP_ECHO_REPLY pEchoReply = (PICMP_ECHO_REPLY)reply_buffer;

			hIcmpFile = ::IcmpCreateFile();
			if (!hIcmpFile || (hIcmpFile == INVALID_HANDLE_VALUE))
			{
				return ::GetLastError();
			}

			InetPton(inet_type, tstr_ip.c_str(), &ipaddr);

			result_list.reset(new std::list<TestResultItem>());

			for (depth = 1; !done && depth < 64; depth++)
			{
				int trycount;
				TestResultItem result_item;
				IP_OPTION_INFORMATION option = { 0 };
				DWORD dwTimeout = 125 + 4 * depth;
				if (dwTimeout > 500) dwTimeout = 500;
				result_item.depth = depth;
				result_item.rtt = -1;
				option.Ttl = depth;
				for (trycount = 0; trycount < 2; trycount++) {
					IPAddr gw_ipaddr;
					dwResult = IcmpSendEcho2(hIcmpFile, NULL, NULL, NULL,
						ipaddr, send_buffer, sizeof(send_buffer), &option,
						reply_buffer, sizeof(reply_buffer), dwTimeout);
					if (dwResult != 0) {
						TCHAR textbuf[128] = { 0 };
						InetNtop(inet_type, &pEchoReply->Address, textbuf, sizeof(textbuf));
						std::basic_string<TCHAR> strbuf(textbuf);
						result_item.ip = std::string(strbuf.begin(), strbuf.end());
						result_item.result = pEchoReply->Status;
						if (pEchoReply->Status == 0) {
							result_item.rtt = pEchoReply->RoundTripTime;
							done = true;
						}
						break;
					}
					else {
						dwResult = GetLastError();
						result_item.result = dwResult;
					}
				}
				result_list->push_back(result_item);
			}

			::IcmpCloseHandle(hIcmpFile);

			return 0;
		}

	} // namespace service
} // namespace grida
