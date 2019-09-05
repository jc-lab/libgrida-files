/**
 * @file	mcd_protocol.hpp
 * @author	Jichan (development@jc-lab.net / http://ablog.jc-lab.net/ )
 * @copyright Copyright (C) 2019 jichan.\n
 *            This software may be modified and distributed under the terms
 *            of the Apache License 2.0.  See the LICENSE file for details.
 */

#ifndef __GRIDA_SERVICE_MCD_MCD_PROTOCOL_HPP__
#define __GRIDA_SERVICE_MCD_MCD_PROTOCOL_HPP__

#include "../../tsp/protocol.hpp"

#include <JsBsonRPCSerializable/Serializable.h>

#include <map>
#include <functional>

namespace grida {
	namespace service {
		namespace mcd {
			class McdPayload : public tsp::Payload, public JsBsonRPC::Serializable {
			public:
				McdPayload(const char *name, int64_t serialVersionUID) : Serializable(name, serialVersionUID) {
				}

				virtual uint16_t payload_type() const = 0;
			};

			class McdObjectDiscoveryRequestPayload : public McdPayload {
			public:
				static const uint16_t PAYLOAD_TYPE;

				JsBsonRPC::SType<std::string> object_id;

				McdObjectDiscoveryRequestPayload() : McdPayload("grida:mcd:ObjectDiscoveryRequestPayload", 101) {
					serializableMapMember("object_id", object_id);
				}
				uint16_t payload_type() const override {
					return PAYLOAD_TYPE;
				}
			};

			class McdObjectDiscoveryResponsePayload : public McdPayload {
			public:
				static const uint16_t PAYLOAD_TYPE;

				JsBsonRPC::SType<std::string> object_id;
				JsBsonRPC::SType<std::vector<unsigned char>> pieces_bitmap;

				McdObjectDiscoveryResponsePayload() : McdPayload("grida:mcd:McdObjectDiscoveryResponsePayload", 101) {
					serializableMapMember("object_id", object_id);
					serializableMapMember("pieces_bitmap", pieces_bitmap);
				}
				uint16_t payload_type() const override {
					return PAYLOAD_TYPE;
				}
			};

			class McdProtocol : public tsp::Protocol {
			public:
				typedef std::function<std::unique_ptr<McdPayload>()> PayloadFactory;

				uint8_t get_sp_type() const {
					return 10;
				}

			private:
				std::map<uint16_t, PayloadFactory> payload_factories_;

			public:
				McdProtocol();
				int makePacket(std::unique_ptr<char[]>& out_packet, const tsp::Payload* in_payload, void* user_ctx) override;
				int parsePayload(std::unique_ptr<tsp::Payload>& out_payload, const unsigned char* in_packet, int in_length, void* user_ctx) override;

				void addCustomPayload(uint16_t payload_type, PayloadFactory factory);
			};

		} // namespace
	} // namespace service
} // namespace grida

#endif // __GRIDA_SERVICE_MCD_MCD_PROTOCOL_HPP__
