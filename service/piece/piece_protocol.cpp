/**
 * @file	piece_protocol.cpp
 * @author	Jichan (development@jc-lab.net / http://ablog.jc-lab.net/ )
 * @copyright Copyright (C) 2019 jichan.\n
 *            This software may be modified and distributed under the terms
 *            of the Apache License 2.0.  See the LICENSE file for details.
 */

#include "piece_protocol.hpp"

namespace grida {
	namespace service {
		namespace piece {

			const uint16_t PieceRequestPayload::PAYLOAD_TYPE = 0x0101;
			const uint16_t PieceStartDataPayload::PAYLOAD_TYPE = 0x01fa;

			PieceProtocol::PieceProtocol()
			{
				payload_factories_[PieceRequestPayload::PAYLOAD_TYPE] = []() -> std::unique_ptr<PiecePayload> { return std::unique_ptr<PieceRequestPayload>(new PieceRequestPayload()); };
				payload_factories_[PieceStartDataPayload::PAYLOAD_TYPE] = []() -> std::unique_ptr<PiecePayload> { return std::unique_ptr<PieceStartDataPayload>(new PieceStartDataPayload()); };
			}

			void PieceProtocol::addCustomPayload(uint16_t payload_type, PayloadFactory factory)
			{
				payload_factories_[payload_type] = factory;
			}

			int PieceProtocol::makePacket(std::unique_ptr<char[]>& out_packet, const tsp::Payload* in_payload, void* user_ctx)
			{
				const PiecePayload* real_payload = dynamic_cast<const PiecePayload*>(in_payload);
				uint16_t payload_type = real_payload->payload_type();
				std::vector<unsigned char> payload_buf;
				size_t payload_size = real_payload->serialize(payload_buf);

				size_t need_size = 2 + payload_size;
				size_t out_size = packet_header_size() + need_size;

				unsigned char* pcur;

				out_packet.reset(new char[out_size]);
				make_packet_header(out_packet.get(), need_size);

				pcur = (unsigned char*)(out_packet.get() + packet_header_size());

				*(pcur++) = (uint8_t)(payload_type >> 8);
				*(pcur++) = (uint8_t)(payload_type);

				memcpy(pcur, payload_buf.data(), payload_buf.size());
				
				return out_size;
			}

			int PieceProtocol::parsePayload(std::unique_ptr<tsp::Payload>& out_payload, const unsigned char* in_packet, int in_length, void* user_ctx)
			{
				uint16_t payload_type = 0;
				payload_type |= (((uint16_t)in_packet[0]) & 0xFF) << 8;
				payload_type |= (((uint16_t)in_packet[1]) & 0xFF);

				std::map<uint16_t, PayloadFactory>::const_iterator factory_iter = payload_factories_.find(payload_type);
				if (factory_iter == payload_factories_.end()) {
					return 0;
				}

				std::unique_ptr<PiecePayload> payload = (factory_iter->second)();
				
				std::vector<unsigned char> in_packet_vector(in_packet + 2, in_packet + in_length);
				try {
					payload->deserialize(in_packet_vector);
					out_payload = std::move(payload);
				}
				catch (JsBsonRPC::Serializable::ParseException e) {
					return 0;
				}

				return 1;
			}

		} // namespace mcd
	} // namespace service
} // namespace grida

