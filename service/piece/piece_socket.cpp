/**
 * @file	piece_socket.cpp
 * @author	Jichan (development@jc-lab.net / http://ablog.jc-lab.net/ )
 * @copyright Copyright (C) 2019 jichan.\n
 *            This software may be modified and distributed under the terms
 *            of the Apache License 2.0.  See the LICENSE file for details.
 */

#include "piece_socket.hpp"

#include <uvw/async.hpp>

#include "../piece_service.hpp"

#include <jcu/bytebuffer/byte_buffer.hpp>

#include "../../thread_pool.hpp"

#include "../../piece_download_context.hpp"

namespace grida {
	namespace service {
		namespace piece {

			PieceSocket::PieceSocket(PieceService* piece_service, std::shared_ptr<uvw::Loop> loop, ThreadPool *thread_pool) : conn_state_(CONN_HANDSHAKE), piece_service_(piece_service), loop_(loop), thread_pool_(thread_pool), task_count_(0)
			{
				addProtocol(&piece_protocol_);
				setEndpayloadType(piece_protocol_.get_sp_type());
			}

			PieceSocket::~PieceSocket()
			{
				std::shared_ptr<uvw::TCPHandle> handle = handle_.lock();
				if (handle) {
					handle->close();
				}
			}

			void PieceSocket::onRecvPacket(std::unique_ptr<char[]> data, size_t packet_len) {
				std::unique_ptr<const char[]> temp(const_cast<char const*>(data.release()));
				task_count_++;
				thread_pool_->send([this](std::unique_ptr<const char[]> packet_data, size_t packet_len) {
					std::unique_ptr<const char[]> copied_packet_data(std::move(packet_data));
					processRecvPacket(copied_packet_data, packet_len);
					task_count_--;
				}, std::move(temp), packet_len);
			}

			void PieceSocket::closeWithQueue() {
				if (task_count_ > 0) {
					std::shared_ptr<uvw::TCPHandle> handle = handle_.lock();
					thread_pool_->send([this](std::shared_ptr<uvw::TCPHandle> handle) {
						closeWithQueue();
					}, handle);
				} else {
					if (piece_download_ctx_) {
						piece_download_ctx_->done();
						piece_download_ctx_.reset();
					}
					piece_service_->doneRequest(this);
				}
				
			}

			bool PieceSocket::acceptFrom(std::shared_ptr<PieceSocket> self, std::shared_ptr<uvw::TCPHandle> shared_handle)
			{
				handle_ = shared_handle;
				shared_handle->data(self);

				shared_handle->on<uvw::DataEvent>([this](uvw::DataEvent& evt, uvw::TCPHandle& handle) {
					onRecvPacket(std::move(evt.data), evt.length);
				});
				shared_handle->on<uvw::ErrorEvent>([this](const uvw::ErrorEvent&, uvw::TCPHandle& handle) {
					handle.close();
				});
				shared_handle->on<uvw::EndEvent>([](const uvw::EndEvent&, uvw::TCPHandle& handle) {
					handle.close();
				});
				shared_handle->once<uvw::CloseEvent>([this, shared_handle](const uvw::CloseEvent&, uvw::TCPHandle& handle) {
					// Capture shared_handle for keep reference
					{
						auto peer = handle.peer();
						printf("PieceSocket closeFrom : %s:%d\n", peer.ip.c_str(), peer.port);
					}

					closeWithQueue();
				});

				{
					auto peer = shared_handle->peer();
					printf("PieceSocket acceptFrom : %s:%d\n", peer.ip.c_str(), peer.port);
				}

				pooled_buffer_ = piece_service_->piece_buf_pool()->allocatePack(4194304);
				if (!pooled_buffer_) {
					printf("***************MEMORY ALLOCATE FAILED****************");
					shared_handle->close();
					return false;
				}

				shared_handle->read();

				return true;
			}

			void PieceSocket::connectTo(std::shared_ptr<PieceSocket> self, const std::string& remote_ip, int port, std::shared_ptr<PieceDownloadContext> piece_download_ctx)
			{
				std::shared_ptr<uvw::TCPHandle> shared_handle = loop_->resource<uvw::TCPHandle>();
				handle_ = shared_handle;
				shared_handle->data(self);

				piece_download_ctx_ = piece_download_ctx;
				shared_handle->once<uvw::ConnectEvent>([this, piece_download_ctx, remote_ip, port](const uvw::ConnectEvent& evt, uvw::TCPHandle& handle) {
					piece::PieceRequestPayload req_payload;

					handle.read();

					remote_ip_ = remote_ip;
					remote_port_ = port;

					req_payload.object_id.set(piece_download_ctx->object_id());
					req_payload.piece_id.set(piece_download_ctx->piece_id());

					sendHandshakePayload(&req_payload);
				});
				shared_handle->on<uvw::DataEvent>([this](uvw::DataEvent& evt, uvw::TCPHandle& handle) {
					onRecvPacket(std::move(evt.data), evt.length);
				});
				shared_handle->on<uvw::ErrorEvent>([this](const uvw::ErrorEvent&, uvw::TCPHandle& handle) {
					handle.close();
				});
				shared_handle->on<uvw::EndEvent>([](const uvw::EndEvent&, uvw::TCPHandle& handle) {
					handle.close();
				});
				shared_handle->on<uvw::CloseEvent>([this](const uvw::CloseEvent&, uvw::TCPHandle& handle) {
					{
						auto peer = handle.peer();
						printf("PieceSocket closeFrom : %s:%d\n", peer.ip.c_str(), peer.port);
					}

					closeWithQueue();
					handle_.reset();
				});
				shared_handle->connect(remote_ip, port);
			}

			void PieceSocket::uploadPiece()
			{
				std::shared_ptr<uvw::TCPHandle> handle = handle_.lock();
				LimitedMemoryPool::PooledPack *pack = (LimitedMemoryPool::PooledPack *)pooled_buffer_.get();
				int64_t read_bytes = file_handle_->read(pack->raw(), pack->size());
				if (read_bytes > 0) {
					handle->once<uvw::WriteEvent>([this](uvw::WriteEvent& evt, uvw::TCPHandle& handle) {
						uploadPiece();
						});
					handle->write((char*)pack->raw(), pack->size());
				} else {
					handle->close();
				}
			}

			int PieceSocket::onRecvEndPayload(const std::vector<std::unique_ptr<tsp::Payload>>& ancestors, std::unique_ptr<tsp::Payload>& payload)
			{
				std::shared_ptr<uvw::TCPHandle> handle = handle_.lock();
				const PiecePayload* piece_payload = dynamic_cast<const PiecePayload*>(payload.get());
				if (piece_payload->payload_type() == PieceRequestPayload::PAYLOAD_TYPE)
				{
					const PieceRequestPayload* real_payload = dynamic_cast<const PieceRequestPayload*>(piece_payload);
					PieceStartDataPayload start_payload;
					size_t piece_size = 0;
					file_handle_ = piece_service_->openPieceFile(real_payload->object_id.get(), real_payload->piece_id.get());
					if (!file_handle_) {
						handle->close();
						return 1;
					}
					file_handle_->getFileSize(&piece_size);
					start_payload.piece_size.set(piece_size);
					sendHandshakePayload(&start_payload);
					conn_state_ = CONN_DATA;

					std::shared_ptr<uvw::AsyncHandle> task = handle->loop().resource<uvw::AsyncHandle>();
					task->on<uvw::AsyncEvent>([this](const uvw::AsyncEvent& evt, uvw::AsyncHandle& handle) {
						uploadPiece();
					});
					task->send();
				}else if (piece_payload->payload_type() == PieceStartDataPayload::PAYLOAD_TYPE)
				{
					const PieceStartDataPayload* real_payload = dynamic_cast<const PieceStartDataPayload*>(piece_payload);
					useCustomLayer(true);
				}
				return 1;
			}

			int PieceSocket::onRecvCustomPayload(tsp::TspRecvContext* recv_ctx, const unsigned char* data, int length)
			{
				piece_download_ctx_->appendData(data, length);
				return 1;
			}

			int PieceSocket::sendHandshakePayload(const tsp::Payload *payload)
			{
				std::shared_ptr<uvw::TCPHandle> handle = handle_.lock();
				std::unique_ptr<char[]> out_packet;
				std::unique_ptr<tsp::Payload> out_payload;
				int rc;

				rc = piece_protocol_.makePacket(out_packet, payload, NULL);
				if (rc <= 0)
					return rc;

				handle->write(std::move(out_packet), rc);

				return rc;
			}

		} // namespace mcd
	} // namespace service
} // namespace grida

