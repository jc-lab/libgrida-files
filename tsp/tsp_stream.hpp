/**
 * @file	tsp_stream.hpp
 * @author	Jichan (development@jc-lab.net / http://ablog.jc-lab.net/ )
 * @date	2019/09/04
 * @copyright Copyright (C) 2019 jichan.\n
 *            This software may be modified and distributed under the terms
 *            of the Apache License 2.0.  See the LICENSE file for details.
 */

#ifndef __GRIDA_TSP_TSPSTREAM_HPP__
#define __GRIDA_TSP_TSPSTREAM_HPP__

#include <memory>
#include <vector>

#include <functional>

#include <map>
#include <string>
#include <list>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>

#include "tsp_recv_context.hpp"

namespace grida {
	
	namespace tsp {

		class Payload;
		class Protocol;

		class TspStream {
		private:
			uint8_t endpayload_sp_type_;
			std::map<uint8_t, Protocol*> protocols_;

			TspRecvContext recv_ctx_;

			bool use_custom_layer_;

		public:
			virtual int processRecvPacket(std::unique_ptr<const char[]>& packet_data, int packet_len);
			int processRecvSubProtocol(const std::string& remote_ip, int remote_port, uint8_t sp_type, const unsigned char *sp_buffer, int sp_length, void* user_ctx);

		protected:
			std::string remote_ip_;
			int remote_port_;

			int processRecvPacketImpl(TspRecvContext* context);

			virtual int onRecvEndPayload(const std::vector<std::unique_ptr<Payload>>& ancestors, std::unique_ptr<Payload> &payload) = 0;
			virtual int onRecvCustomPayload(TspRecvContext* recv_ctx, const unsigned char* data, int length) { return 1; };

		public:
			TspStream();
			virtual ~TspStream();
			void addProtocol(Protocol *protocol);
			void setEndpayloadType(uint8_t sp_type);

			void useCustomLayer(bool state) { use_custom_layer_ = state; }
		};

	} // namespace tsp

} // namespace grida

#endif // __GRIDA_TSP_MSTCP_HPP__
