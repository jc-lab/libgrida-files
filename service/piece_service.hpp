/**
 * @file	piece_service.hpp
 * @author	Jichan (development@jc-lab.net / http://ablog.jc-lab.net/ )
 * @copyright Copyright (C) 2019 jichan.\n
 *            This software may be modified and distributed under the terms
 *            of the Apache License 2.0.  See the LICENSE file for details.
 */

#ifndef __GRIDA_SERVICE_PIECE_SERVER_SERVICE_HPP__
#define __GRIDA_SERVICE_PIECE_SERVER_SERVICE_HPP__

#include <uvw/tcp.hpp>

#include <map>
#include <mutex>
#include <atomic>

#include "../peer_piece_download_context.hpp"
#include "../limited_memory_pool.hpp"

namespace grida {

	class PeerService;

	namespace service {

		namespace piece {
			class PieceSocket;
		};

		class PieceService {
		private:
			std::weak_ptr<PieceService> self_;
			std::atomic_bool started_;

			PeerService* peer_service_;

			LimitedMemoryPool* memory_pool_;

			std::shared_ptr<uvw::TCPHandle> local_tcp_listener_;

			struct {
				std::mutex mutex;
				std::map<piece::PieceSocket*, std::weak_ptr<piece::PieceSocket>> map;
			} piece_sockets_;

			std::atomic_int upload_socket_count_;

			PieceService(PeerService* peer_service);

		public:
			static std::shared_ptr<PieceService> create(PeerService* peer_service);
			~PieceService();
			
			void init(LimitedMemoryPool* memory_pool);
			void stop();
			void listen(int port);

			piece::PieceSocket* requestPieceToPeer(std::shared_ptr<PeerPieceDownloadContext> piece_download_ctx);
			void doneRequest(piece::PieceSocket* self);

			std::unique_ptr<FileHandle> openPieceFile(const std::string& object_id, const std::string& piece_id);

			LimitedMemoryPool* piece_buf_pool() {
				return memory_pool_;
			}

			int64_t getSpeedLimitBitrate();
			int getUploadSocketCount() const;

			int64_t computedSocketSpeedLimitBitrate();
		};

	} // namespace service
} // namespace grida

#endif // __GRIDA_SERVICE_PIECE_SERVER_SERVICE_HPP__
