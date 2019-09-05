/**
 * @file	protocol.hpp
 * @author	Jichan (development@jc-lab.net / http://ablog.jc-lab.net/ )
 * @copyright Copyright (C) 2019 jichan.\n
 *            This software may be modified and distributed under the terms
 *            of the Apache License 2.0.  See the LICENSE file for details.
 */

#ifndef __GRIDA_TSP_PROTOCOL_HPP__
#define __GRIDA_TSP_PROTOCOL_HPP__

#include <memory>
#include <vector>
#include <string>

namespace grida {
	
	namespace tsp {

		class Payload {
		public:
			Payload() {}
			virtual ~Payload() {}
		};

		class SocketPayload : public Payload {
		private:
			std::string remote_ip_;
			int remote_port_;

		public:
			SocketPayload(const SocketPayload& obj) {
				remote_ip_ = obj.remote_ip();
				remote_port_ = obj.remote_port();
			}

			SocketPayload(const std::string& remote_ip, int remote_port) : remote_ip_(remote_ip), remote_port_(remote_port) {}

			const std::string remote_ip() const {
				return remote_ip_;
			}

			int remote_port() const {
				return remote_port_;
			}
		};

		class Protocol {
		public:
			virtual uint8_t get_sp_type() const = 0;

			virtual int makePacket(std::unique_ptr<char[]>& out_packet, const Payload* in_payload, void* user_ctx) = 0;

			virtual int parsePayload(std::unique_ptr<Payload>& out_payload, const unsigned char* in_packet, int in_length, void* user_ctx) = 0;
			virtual int unwrap(std::unique_ptr<char[]>& out_packet, const Payload* in_payload) { return 0; }

		protected:
			int packet_header_size() const {
				return 2;
			}

			void make_packet_header(void *buf, uint16_t sp_length) const {
				unsigned char* pbuf = (unsigned char*)buf;
				pbuf[0] = (get_sp_type() << 4) | ((sp_length >> 8) & 0x0F);
				pbuf[1] = (uint8_t)sp_length;
			}
		};

	} // namespace tsp

} // namespace grida

#endif // __GRIDA_TSP_MSTCP_HPP__
