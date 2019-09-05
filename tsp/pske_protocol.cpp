/**
 * @file	pske_protocol.cpp
 * @author	Jichan (development@jc-lab.net / http://ablog.jc-lab.net/ )
 * @copyright Copyright (C) 2019 jichan.\n
 *            This software may be modified and distributed under the terms
 *            of the Apache License 2.0.  See the LICENSE file for details.
 */

#include "pske_protocol.hpp"

#include <jcu/bytebuffer/byte_buffer.hpp>

#include <jcp/cipher_algo.hpp>
#include <jcp/secret_key_factory.hpp>
#include <jcp/secret_key_factory_algo.hpp>
#include <jcp/hkdf_key_spec.hpp>
#include <jcp/gcm_param_spec.hpp>
#include <jcp/key_agreement.hpp>
#include <jcp/key_agreement_algo.hpp>
#include <jcp/key_factory.hpp>
#include <jcp/x509_encoded_key_spec.hpp>

namespace grida {

	namespace tsp {

		PskeProtocol::PskeProtocol() {
			random_ = jcp::SecureRandom::getInstance();
			identification_number_ = random_->nextInt() ^ ((random_->nextInt() & 0x00FF0000) << 8);
			sequence_number_ = 0;
			session_key_priv_ = NULL;

			ipe_key_factory_ = jcp::KeyFactory::getInstance("X509");
		}

		std::unique_ptr<jcp::Cipher> PskeProtocol::createCipher(jcp::Cipher::Mode mode, uint32_t identification_number, uint32_t sequence_number, const unsigned char* ipe_pubkey_data, int ipe_pubkey_len) {
			std::unique_ptr<jcp::Cipher> cipher(jcp::Cipher::getInstance(jcp::CipherAlgorithm::AesGcmNoPadding.algo_id()));

			jcu::ByteBuffer auth_data_writer(jcu::ByteBuffer::BIG_ENDIAN);
			std::vector<unsigned char> temp_buffer;

			jcp::Result<jcp::Buffer> result_sk;

			if (ipe_pubkey_data && ipe_pubkey_len) {
				std::unique_ptr<jcp::KeyAgreement> ka = jcp::KeyAgreement::getInstance(jcp::KeyAgreementAlgorithm::ECDH.algo_id());
				jcp::Result<std::unique_ptr<jcp::X509EncodedKeySpec>> pub_key_spec = jcp::X509EncodedKeySpec::decode(ipe_pubkey_data, ipe_pubkey_len);
				jcp::Result<std::unique_ptr<jcp::AsymKey>> result_dest_pub_key = ipe_key_factory_->generatePublicKey(pub_key_spec->get());
				ka->init(session_key_priv_, random_.get());
				ka->doPhase(result_dest_pub_key->get());
				result_sk = ka->generateSecret();
				temp_buffer.resize(8 + result_sk->size());
			} else {
				temp_buffer.resize(8);
			}

			auth_data_writer.wrapWriteMode(&temp_buffer[0], temp_buffer.size());

			auth_data_writer.putUint32(identification_number);
			auth_data_writer.putUint32(sequence_number);
			if (ipe_pubkey_data && ipe_pubkey_len) {
				auth_data_writer.put(result_sk->data(), result_sk->size());
			}

			const jcp::SecretKeyFactory* skf = jcp::SecretKeyFactory::getInstance(jcp::SecretKeyFactoryAlgorithm::HKDFWithSHA256.algo_id());
			jcp::HKDFKeySpec key_spec((const char*)password_.data(), password_.size(), 64, (const unsigned char*)temp_buffer.data(), auth_data_writer.position());
			jcp::Result<jcp::SecretKey> res_secret_key = skf->generateSecret(&key_spec);
			std::vector<unsigned char> secret_key = res_secret_key->getEncoded();

			jcp::SecretKey enc_key(secret_key.data(), 32);
			cipher->init(mode, &enc_key, jcp::GCMParameterSpec::create(32, secret_key.data() + 48, 16).get());
			cipher->updateAAD(secret_key.data() + 32, 16);
			
			return cipher;
		}

		int PskeProtocol::wrap(std::unique_ptr<Payload>& out_payload, const char* data, int length, grida::tsp::PskeFlags pske_flags, const std::vector<std::unique_ptr<tsp::Payload>>* recv_ancestors) const
		{
			std::unique_ptr<PskePayload> payload(new PskePayload());
			payload->version = 1;
			payload->flags = pske_flags;
			payload->src_identification_number = get_identification_number();
			payload->dest_identification_number = 0;
			if (recv_ancestors) {
				for (auto iter = recv_ancestors->cbegin(); iter != recv_ancestors->cend(); iter++) {
					const tsp::Payload* cur_payload = iter->get();
					const tsp::PskePayload * cur_pske_payload = dynamic_cast<const tsp::PskePayload*>(cur_payload);
					if (cur_pske_payload) {
						payload->dest_identification_number = cur_pske_payload->src_identification_number;
						payload->ipe_pubkey = cur_pske_payload->ipe_pubkey;
						break;
					}
				}
			}
			payload->sequence_number = 0;
			payload->sub_payload_len = length;
			payload->sub_payload_data.reset(new char[length]);
			memcpy(payload->sub_payload_data.get(), data, length);
			out_payload = std::move(payload);
			return 1;
		}

		int PskeProtocol::makePacket(std::unique_ptr<char[]>& out_packet, const Payload* in_payload, void *user_ctx)
		{
			const PskePayload* real_payload = dynamic_cast<const PskePayload*>(in_payload);
			if (!real_payload)
				return 0;

			uint32_t identification_number = get_identification_number();
			uint32_t sequence_number = next_sequence_number();

			unsigned char auth_data[8];
			jcu::ByteBuffer payload_buffer(jcu::ByteBuffer::BIG_ENDIAN);
			size_t need_size = 19 + real_payload->sub_payload_len;
			size_t buf_size;

			std::unique_ptr<jcp::Cipher> cipher;

			if (real_payload->flags & PSKE_FLAG_IPE)
			{
				need_size += 2 + session_key_pub_.size();
			}
			if ((real_payload->flags & (PSKE_FLAG_IPE | PSKE_FLAG_IPE_RESPONSE)) == (PSKE_FLAG_IPE | PSKE_FLAG_IPE_RESPONSE)) {
				cipher = createCipher(jcp::Cipher::ENCRYPT_MODE, identification_number, sequence_number, real_payload->ipe_pubkey.data(), real_payload->ipe_pubkey.size());
			} else {
				cipher = createCipher(jcp::Cipher::ENCRYPT_MODE, identification_number, sequence_number);
			}


			buf_size = packet_header_size() + need_size + 1024;
			out_packet.reset(new char[buf_size]);
			payload_buffer.wrapWriteMode(out_packet.get() + packet_header_size(), need_size);
			payload_buffer.putUint8(real_payload->version);
			payload_buffer.putUint8(real_payload->flags);
			payload_buffer.putUint32(identification_number);
			payload_buffer.putUint32(real_payload->dest_identification_number);
			payload_buffer.putUint32(sequence_number);

			if (real_payload->flags & PSKE_FLAG_IPE) {
				payload_buffer.putUint8(PSKE_OPT_IPE_PUBLIC_KEY);
				payload_buffer.putUint8(session_key_pub_.size());
				payload_buffer.put(session_key_pub_.data(), session_key_pub_.size());
			}

			payload_buffer.putUint8(0);

			{
				jcp::Result<jcp::Buffer> res_data = cipher->doFinal(real_payload->sub_payload_data.get(), real_payload->sub_payload_len);
				if (!res_data) {
					return -1;
				}
				if (res_data->size() > 0)
					payload_buffer.put(res_data->data(), res_data->size());
			}

			if (real_payload->flags & PSKE_FLAG_SIGNED) {
				std::unique_ptr<jcp::Signature> signature = getSignature(user_ctx, real_payload, true);
				signature->update(out_packet.get() + packet_header_size(), payload_buffer.position());
				jcp::Result<jcp::Buffer> result_sign = signature->sign();
				if (result_sign)
				{
					return -1;
				}
				if (!payload_buffer.put(result_sign->data(), result_sign->size()))
				{
					return -1;
				}
				payload_buffer.putUint16(result_sign->size());
			}
			
			make_packet_header(out_packet.get(), payload_buffer.position());
			return packet_header_size() + payload_buffer.position();
		}

		int PskeProtocol::parsePayload(std::unique_ptr<Payload>& out_payload, const unsigned char* in_packet, int in_length, void* user_ctx)
		{
			int rc;

			uint16_t sign_length = 0;
			const unsigned char *sign_ptr = NULL;

			uint8_t header_opt_id;

			std::unique_ptr<PskePayload> payload(new PskePayload());
			jcu::ByteBuffer payload_buffer(jcu::ByteBuffer::BIG_ENDIAN);
			payload_buffer.wrapReadMode(in_packet, in_length);

			payload->version = payload_buffer.getUint8();
			payload->flags = payload_buffer.getUint8();
			payload->src_identification_number = payload_buffer.getUint32();
			payload->dest_identification_number = payload_buffer.getUint32();
			payload->sequence_number = payload_buffer.getUint32();

			if (payload->src_identification_number == get_identification_number())
			{
				// Loopback packet -> Drop
				return 0;
			}
			if ((payload->dest_identification_number != get_identification_number()) && ((payload->flags & PSKE_FLAG_BROADCAST) == 0))
			{
				// Loopback packet -> Drop
				return 0;
			}

			do {
				header_opt_id = payload_buffer.getUint8();
				if (header_opt_id) {
					uint8_t header_opt_size = payload_buffer.getUint8();
					if (payload_buffer.flowed())
					{
						return -1;
					}
					if (header_opt_size > 0) {
						std::vector<unsigned char> header_opt_data(header_opt_size);
						payload_buffer.get(&header_opt_data[0], header_opt_size);
						if (payload_buffer.flowed())
						{
							return -1;
						}
						if (header_opt_id == PSKE_OPT_IPE_PUBLIC_KEY) {
							payload->ipe_pubkey = header_opt_data;
						}
					}
				}
			} while (header_opt_id);

			if (payload->flags & PSKE_FLAG_SIGNED)
			{
				int pos = in_length;
				sign_length = (uint16_t)in_packet[--pos];
				sign_length |= ((uint16_t)in_packet[--pos]) << 8;
				pos -= sign_length;
				sign_ptr = (const unsigned char*)&in_packet[pos];
			}

			std::unique_ptr<jcp::Cipher> cipher;

			if (payload->flags & PSKE_FLAG_IPE_RESPONSE)
				cipher = createCipher(jcp::Cipher::DECRYPT_MODE, payload->src_identification_number, payload->sequence_number, payload->ipe_pubkey.data(), payload->ipe_pubkey.size());
			else
				cipher = createCipher(jcp::Cipher::DECRYPT_MODE, payload->src_identification_number, payload->sequence_number);

			jcp::Result<jcp::Buffer> res_buf = cipher->doFinal(payload_buffer.readPtr(), payload_buffer.remaining() - sign_length);
			if (!res_buf) {
				return -1;
			}

			payload->sub_payload_data.reset(new char[res_buf->size()]);
			payload->sub_payload_len = res_buf->size();
			memcpy(payload->sub_payload_data.get(), res_buf->data(), payload->sub_payload_len);

			if (payload->flags & PSKE_FLAG_SIGNED)
			{
				std::unique_ptr<jcp::Signature> signature = getSignature(user_ctx, payload.get(), false);
				if (signature) {
					signature->update(in_packet, (size_t)(sign_ptr - in_packet));
					jcp::Result<bool> result = signature->verify(sign_ptr, sign_length);
					if (!result)
					{
						// Some error -> Drop
						return 0;
					}
					if (!result)
					{
						// Validation failed -> Drop
						return 0;
					}
				}
			}

			rc = verifySequenceNumber(user_ctx, payload.get());
			if (rc <= 0)
				return rc;

			out_payload = std::move(payload);
			return 1;
		}

		int PskeProtocol::unwrap(std::unique_ptr<char[]>& out_packet, const Payload* in_payload)
		{
			const PskePayload *real_payload = dynamic_cast<const PskePayload*>(in_payload);
			if (!real_payload)
				return 0;

			out_packet.reset(new char[real_payload->sub_payload_len]);
			memcpy(out_packet.get(), real_payload->sub_payload_data.get(), real_payload->sub_payload_len);

			return real_payload->sub_payload_len;
		}

		int PskeProtocol::setSessionKey(jcp::AsymKey* key, const unsigned char* pubkey_x509_data, int pubkey_x509_size)
		{
			session_key_priv_ = key;
			session_key_pub_.clear();
			session_key_pub_.insert(session_key_pub_.end(), pubkey_x509_data, pubkey_x509_data + pubkey_x509_size);
			return 0;
		}
		
	} // namespace tsp

} // namespace grida
