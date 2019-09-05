/**
 * @file	tsp_stream.cpp
 * @author	Jichan (development@jc-lab.net / http://ablog.jc-lab.net/ )
 * @date	2019/09/04
 * @copyright Copyright (C) 2019 jichan.\n
 *            This software may be modified and distributed under the terms
 *            of the Apache License 2.0.  See the LICENSE file for details.
 */

#include "tsp_stream.hpp"

#include "protocol.hpp"

#include <jcu/bytebuffer/byte_buffer.hpp>

namespace grida {

	namespace tsp {

		TspStream::TspStream() : use_custom_layer_(false)
		{
		}

		TspStream::~TspStream()
		{
		}

		void TspStream::setEndpayloadType(uint8_t sp_type) {
			endpayload_sp_type_ = sp_type;
		}
		void TspStream::addProtocol(Protocol* protocol) {
			protocols_[protocol->get_sp_type()] = protocol;
		}

		int TspStream::processRecvPacket(std::unique_ptr<const char[]>& packet_data, int packet_len)
		{
			recv_ctx_.buffer_.insert(recv_ctx_.buffer_.end(), packet_data.get(), packet_data.get() + packet_len);
			return processRecvPacketImpl(&recv_ctx_);
		}

		int TspStream::processRecvPacketImpl(TspRecvContext* recv_ctx) {
			int retval = 1;

			while (recv_ctx->buffer_.size() > 0) {
				if (!use_custom_layer_) {
					std::string remote_ip = recv_ctx->remote_ip();
					int remote_port = recv_ctx->remote_port();
					jcu::ByteBuffer packet_reader(jcu::ByteBuffer::BIG_ENDIAN);
					packet_reader.wrapReadMode(recv_ctx->buffer_.data(), recv_ctx->buffer_.size());

					uint16_t header = packet_reader.getUint16();
					uint8_t sp_type = (uint8_t)((header >> 12) & 0x0f);
					uint16_t sp_length = header & 0x0FFF;

					if (packet_reader.flowed() || (packet_reader.remaining() < sp_length)) {
						break;
					}

					printf("processRecvPacket-found %s:%d  %u, %u < %u\n", remote_ip.c_str(),remote_port_, sp_type, packet_reader.remaining(), sp_length);

					retval = processRecvSubProtocol(remote_ip, remote_port, sp_type, packet_reader.readPtr(), sp_length, recv_ctx);
					if (retval < 0) {
						recv_ctx->errored_ = true;
						printf("processRecvPacket-error %s:%d  %u, %u < %u\n", remote_ip.c_str(), remote_port, sp_type, packet_reader.remaining(), sp_length);
						break;
					}
					else if (retval == 0) {
						printf("processRecvPacket-loopback %s:%d  %u, %u < %u\n", remote_ip.c_str(), remote_port, sp_type, packet_reader.remaining(), sp_length);
					}
					packet_reader.increasePosition(sp_length);

					memmove(&recv_ctx->buffer_[0], packet_reader.readPtr(), packet_reader.remaining());
					recv_ctx->buffer_.resize(packet_reader.remaining());
				} else {
					retval = onRecvCustomPayload(recv_ctx, recv_ctx->buffer_.data(), recv_ctx->buffer_.size());
					recv_ctx->buffer_.clear();
				}
			}

			return retval;
		}

		int TspStream::processRecvSubProtocol(const std::string& remote_ip, int remote_port, uint8_t sp_type, const unsigned char* sp_buffer, int sp_length, void* user_ctx)
		{
			int retval;

			std::vector<std::unique_ptr<Payload>> ancestors;

			uint8_t cur_sp_type = sp_type;
			std::unique_ptr<char[]> cur_packet;
			const unsigned char* cur_packet_buf = sp_buffer;
			int cur_packet_len = sp_length;

			ancestors.reserve(4);

			ancestors.push_back(std::move(std::make_unique<SocketPayload>(remote_ip, remote_port)));

			do {
				auto protocol_iter = protocols_.find(cur_sp_type);
				if (protocol_iter == protocols_.end()) {
					// Not support protocol
					printf("Not support protocol\n");
					return -1;
				}

				Protocol* protocol = protocol_iter->second;
				std::unique_ptr<Payload> cur_payload;

				retval = protocol->parsePayload(cur_payload, cur_packet_buf, cur_packet_len, user_ctx);
				if (retval < 0) {
					printf("Parse failed\n");
					return -1;
				}
				else if (retval == 0) {
					// Loopback packet
					return 0;
				}
				if (protocol->get_sp_type() == endpayload_sp_type_) {
					printf("Found end payload\n");
					retval = onRecvEndPayload(ancestors, cur_payload);
					break;
				}

				cur_packet_len = protocol->unwrap(cur_packet, cur_payload.get());
				if (cur_packet_len <= 0) {
					printf("unwrap failed\n");
					return cur_packet_len;
				}

				ancestors.push_back(std::move(cur_payload));

				jcu::ByteBuffer packet_reader(jcu::ByteBuffer::BIG_ENDIAN);
				packet_reader.wrapReadMode(cur_packet.get(), cur_packet_len);

				uint16_t header = packet_reader.getUint16();
				cur_sp_type = (uint8_t)((header >> 12) & 0x0f);
				cur_packet_len = header & 0x0FFF;
				if (packet_reader.flowed() || (packet_reader.remaining() < cur_packet_len)) {
					// Invalid packet
					printf("Invalid packet\n");
					return -1;
				}
				cur_packet_buf = (const unsigned char*)packet_reader.readPtr();
			} while (true);

			return retval;
		}


	} // namespace tsp

} // namespace grida
