/**
 * @file	piece_service.cpp
 * @author	Jichan (development@jc-lab.net / http://ablog.jc-lab.net/ )
 * @copyright Copyright (C) 2019 jichan.\n
 *            This software may be modified and distributed under the terms
 *            of the Apache License 2.0.  See the LICENSE file for details.
 */

#include "piece_service.hpp"

#include "../peer_service.hpp"

#include "piece/piece_socket.hpp"

namespace grida {
	namespace service {

		PieceService::PieceService(PeerService* peer_service)
			: peer_service_(peer_service), upload_socket_count_(0)
		{
		}

		PieceService::~PieceService()
		{
			if (local_tcp_listener_)
			{
				local_tcp_listener_->stop();
				local_tcp_listener_->close();
				local_tcp_listener_.reset();
			}
		}

		void PieceService::init(LimitedMemoryPool* memory_pool) {
			memory_pool_ = memory_pool;
		}

		void PieceService::listen(int port)
		{
			local_tcp_listener_ = peer_service_->get_loop()->resource<uvw::TCPHandle>();
			local_tcp_listener_->on<uvw::ListenEvent>([this](const uvw::ListenEvent& evt, uvw::TCPHandle& handle) {
				std::shared_ptr<uvw::TCPHandle> client = handle.loop().resource<uvw::TCPHandle>();
				std::shared_ptr<piece::PieceSocket> piece_socket(new piece::PieceSocket(this, peer_service_->get_loop(), peer_service_->thread_pool()));

				handle.accept(*client);
				if(piece_socket->acceptFrom(piece_socket, client))
				{
					std::unique_lock<std::mutex> lock(piece_sockets_.mutex);
					piece_sockets_.map[piece_socket.get()] = piece_socket;
					upload_socket_count_++;
				}
			});

			local_tcp_listener_->bind("0.0.0.0", port);
			local_tcp_listener_->listen();
		}

		piece::PieceSocket* PieceService::requestPieceToPeer(std::shared_ptr<PeerPieceDownloadContext> piece_download_ctx)
		{
			const DownloadContext::PeerInfo* peer_info = piece_download_ctx->peer_info();

			std::shared_ptr< piece::PieceSocket> piece_socket(new piece::PieceSocket(this, peer_service_->get_loop(), peer_service_->thread_pool()));

			piece_socket->connectTo(piece_socket, peer_info->remote_ip, 19901, piece_download_ctx);

			{
				std::unique_lock<std::mutex> lock(piece_sockets_.mutex);
				piece_sockets_.map[piece_socket.get()] = piece_socket;
			}

			return piece_socket.get();
		}
		void PieceService::doneRequest(piece::PieceSocket* self) {
			{
				std::unique_lock<std::mutex> lock(piece_sockets_.mutex);
				auto iter = piece_sockets_.map.find(self);
				if (iter != piece_sockets_.map.end()) {
					piece_sockets_.map.erase(iter);
					upload_socket_count_--;
				}
			}
		}

		std::unique_ptr<FileHandle> PieceService::openPieceFile(const std::string& object_id, const std::string& piece_id)
		{
			return peer_service_->openPieceFile(object_id, piece_id);
		}

		int64_t PieceService::getSpeedLimitBitrate() {
			return peer_service_->getSpeedLimitBitratePeer();
		}

		int PieceService::getUploadSocketCount() const {
			return upload_socket_count_;
		}

		int64_t PieceService::computedSocketSpeedLimitBitrate() {
			int count = getUploadSocketCount();
			if (count <= 0) count = 1;
			return getSpeedLimitBitrate() / count;
		}

	} // namespace service
} // namespace grida
