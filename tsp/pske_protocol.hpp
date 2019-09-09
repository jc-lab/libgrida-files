/**
 * @file	pske_protocol.hpp
 * @author	Jichan (development@jc-lab.net / http://ablog.jc-lab.net/ )
 * @copyright Copyright (C) 2019 jichan.\n
 *            This software may be modified and distributed under the terms
 *            of the Apache License 2.0.  See the LICENSE file for details.
 */

#ifndef __GRIDA_TSP_PSKE_PROTOCOL_HPP__
#define __GRIDA_TSP_PSKE_PROTOCOL_HPP__

#include "protocol.hpp"

#include <jcp/cipher.hpp>
#include <jcp/secure_random.hpp>

namespace grida {
	
	namespace tsp {

		enum PskeFlags {
			PSKE_FLAG_NONE = 0x00,
			PSKE_FLAG_SIGNED = 0x01,
			PSKE_FLAG_IPE_RESPONSE = 0x10,
			PSKE_FLAG_IPE = 0x20,
			PSKE_FLAG_ENCRYPTION = 0x40,
			PSKE_FLAG_BROADCAST = 0x80,
		};

		enum PskeOptionId {
			PSKE_OPT_IPE_PUBLIC_KEY = 0x41
		};

		struct PskeOption {
			uint8_t id;
			uint8_t size;
			std::vector<unsigned char> data;
		};

		class PskePayload : public Payload {
		public:
			uint8_t version;
			uint8_t flags;
			uint32_t src_identification_number;
			uint32_t dest_identification_number;
			uint32_t sequence_number;
			std::unique_ptr<char[]> sub_payload_data;
			int sub_payload_len;
			std::map<uint8_t, std::unique_ptr<PskeOption>> options;

			std::vector<unsigned char> ipe_pubkey;
		};

		class PskeProtocol : public Protocol {
		private:
			std::vector<unsigned char> password_;

			std::unique_ptr<jcp::Cipher> createCipher(jcp::Cipher::Mode mode, uint32_t identification_number, uint32_t sequence_number, const unsigned char* ipe_pubkey_data = NULL, int ipe_pubkey_len = 0);

			std::unique_ptr<jcp::SecureRandom> random_;
			std::unique_ptr<jcp::KeyFactory> ipe_key_factory_;

			uint32_t identification_number_;
			uint32_t sequence_number_;

			jcp::AsymKey* session_key_priv_;
			std::vector<unsigned char> session_key_pub_;

		public:
			uint8_t get_sp_type() const override {
				return 0x03;
			}

			virtual uint32_t get_identification_number() const {
				return identification_number_;
			}

			virtual uint32_t next_sequence_number() {
				return (++sequence_number_);
			}

			PskeProtocol();
			virtual ~PskeProtocol() {}

			int makePacket(std::unique_ptr<char[]>& out_packet, const Payload* in_payload, void* user_ctx) override;
			virtual int wrap(std::unique_ptr<Payload>& out_payload, const char* data, int length, grida::tsp::PskeFlags pske_flags, const std::vector<std::unique_ptr<tsp::Payload>>* recv_ancestors = NULL) const;

			int parsePayload(std::unique_ptr<Payload>& out_payload, const unsigned char* in_packet, int in_length, void* user_ctx) override;
			int unwrap(std::unique_ptr<char[]>& out_packet, const Payload* in_payload) override;

			int setPassword(const unsigned char* password, int length);
			int setSessionKey(jcp::AsymKey *key, const unsigned char * pubkey_x509_data, int pubkey_x509_size);

		protected:
			/**
			 * If sign=false & return NULL then, Pass verify payload.
			 */
			virtual std::unique_ptr<jcp::Signature> getSignature(void* user_ctx, const PskePayload* payload, bool sign) { return NULL; }

			virtual int verifySequenceNumber(void* user_ctx, const PskePayload* payload) { return 1; }
		};

	} // namespace tsp

} // namespace grida

#endif // __GRIDA_TSP_MSTCP_HPP__
