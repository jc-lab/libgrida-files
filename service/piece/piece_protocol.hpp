/**
 * @file	piece_protocol.hpp
 * @author	Jichan (development@jc-lab.net / http://ablog.jc-lab.net/ )
 * @copyright Copyright (C) 2019 jichan.\n
 *            This software may be modified and distributed under the terms
 *            of the Apache License 2.0.  See the LICENSE file for details.
 */

#ifndef __GRIDA_SERVICE_PIECE_PIECE_PROTOCOL_HPP__
#define __GRIDA_SERVICE_PIECE_PIECE_PROTOCOL_HPP__

#include "../../tsp/protocol.hpp"

#include <JsBsonRPCSerializable/Serializable.h>

#include <map>
#include <functional>

namespace grida {
	namespace service {
		namespace piece {

			class PiecePayload : public tsp::Payload, public JsBsonRPC::Serializable {
			public:
				PiecePayload(const char *name, int64_t serialVersionUID) : Serializable(name, serialVersionUID) {
				}

				virtual uint16_t payload_type() const = 0;
			};

			class PieceRequestPayload : public PiecePayload {
			public:
				static const uint16_t PAYLOAD_TYPE;

				JsBsonRPC::SType<std::string> object_id;
				JsBsonRPC::SType<std::string> piece_id;

				PieceRequestPayload() : PiecePayload("grida:piece:PieceRequestPayload", 101) {
					serializableMapMember("object_id", object_id);
					serializableMapMember("piece_id", piece_id);
				}
				uint16_t payload_type() const override {
					return PAYLOAD_TYPE;
				}
			};

			class PieceStartDataPayload : public PiecePayload {
			public:
				static const uint16_t PAYLOAD_TYPE;

				JsBsonRPC::SType<int> piece_size;

				PieceStartDataPayload() : PiecePayload("grida:piece:PieceStartDataPayload", 101) {
					serializableMapMember("piece_size", piece_size);
				}
				uint16_t payload_type() const override {
					return PAYLOAD_TYPE;
				}
			};

			class PieceProtocol : public tsp::Protocol {
			public:
				typedef std::function<std::unique_ptr<PiecePayload>()> PayloadFactory;

				uint8_t get_sp_type() const {
					return 14;
				}

			private:
				std::map<uint16_t, PayloadFactory> payload_factories_;

			public:
				PieceProtocol();
				int makePacket(std::unique_ptr<char[]>& out_packet, const tsp::Payload* in_payload, void* user_ctx) override;
				int parsePayload(std::unique_ptr<tsp::Payload>& out_payload, const unsigned char* in_packet, int in_length, void* user_ctx) override;

				void addCustomPayload(uint16_t payload_type, PayloadFactory factory);
			};

		} // namespace piece
	} // namespace service
} // namespace grida

#endif // __GRIDA_SERVICE_PIECE_PIECE_PROTOCOL_HPP__
