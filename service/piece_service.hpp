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
			PeerService* peer_service_;

			LimitedMemoryPool piece_buf_pool_;

			std::shared_ptr<uvw::TCPHandle> local_tcp_listener_;

			struct {
				std::mutex mutex;
				std::map<piece::PieceSocket*, std::weak_ptr<piece::PieceSocket>> map;
			} piece_sockets_;

		public:
			PieceService(PeerService* peer_service);
			~PieceService();
			
			void listen(int port);

			piece::PieceSocket* requestPieceToPeer(std::shared_ptr<PeerPieceDownloadContext> piece_download_ctx);
			void doneRequest(piece::PieceSocket* self);

			std::unique_ptr<FileHandle> openPieceFile(const std::string& object_id, const std::string& piece_id);

			LimitedMemoryPool* piece_buf_pool() {
				return &piece_buf_pool_;
			}
		};

	} // namespace service
} // namespace grida

#endif // __GRIDA_SERVICE_PIECE_SERVER_SERVICE_HPP__
