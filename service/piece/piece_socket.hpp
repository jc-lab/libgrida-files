/**
 * @file	piece_socket.hpp
 * @author	Jichan (development@jc-lab.net / http://ablog.jc-lab.net/ )
 * @copyright Copyright (C) 2019 jichan.\n
 *            This software may be modified and distributed under the terms
 *            of the Apache License 2.0.  See the LICENSE file for details.
 */

#ifndef __GRIDA_SERVICE_PIECE_PIECE_SOCKET_HPP__
#define __GRIDA_SERVICE_PIECE_PIECE_SOCKET_HPP__

#include <memory>

#include <vector>

#include <atomic>

#include <uvw/tcp.hpp>

#include "../../tsp/tsp_stream.hpp"
#include "../../tsp/protocol.hpp"

#include "../../file_handle.hpp"
#include "../../limited_memory_pool.hpp"

#include "../../piece_download_context.hpp"

#include "piece_protocol.hpp"

namespace grida {

	class ThreadPool;
	class PieceDownloadContext;

	namespace service {

		class PieceService;

		namespace piece {
			
			class PieceSocket : public tsp::TspStream, public PieceDownloaderContext {
			public:
				enum ConnectionState {
					CONN_HANDSHAKE = 1,
					CONN_TLS = 2,
					CONN_DATA = 3,
				};

			private:
				std::weak_ptr<PieceSocket> self_;

				PieceService* piece_service_;
				std::shared_ptr<uvw::Loop> loop_;
				ThreadPool* thread_pool_;

				std::weak_ptr<uvw::TCPHandle> handle_;

				// Communication
				ConnectionState conn_state_;
				PieceProtocol piece_protocol_;
				std::atomic_int task_count_;

				// For download
				std::shared_ptr<PieceDownloadContext> piece_download_ctx_;

				// For upload
				std::shared_ptr<FileHandle> file_handle_;
				std::unique_ptr<LimitedMemoryPool::PooledPack> pooled_buffer_;

				std::chrono::time_point<std::chrono::steady_clock> upload_begin_time_;

				int sendHandshakePayload(const tsp::Payload* payload);

				void onRecvPacket(std::unique_ptr<char[]> data, size_t size);
				void closeWithQueue();

				void uploadPiece();

				PieceSocket(PieceService* piece_service, std::shared_ptr<uvw::Loop> loop, ThreadPool* thread_pool);

			public:
				static std::shared_ptr<PieceSocket> create(PieceService* piece_service, std::shared_ptr<uvw::Loop> loop, ThreadPool* thread_pool);

				virtual ~PieceSocket();

				bool acceptFrom(std::shared_ptr<uvw::TCPHandle> handle);
				void connectTo(const std::string& remote_ip, int port, std::shared_ptr<PieceDownloadContext> piece_download_ctx);

			protected:
				int onRecvEndPayload(const std::vector<std::unique_ptr<tsp::Payload>>& ancestors, std::unique_ptr<tsp::Payload>& payload) override;
				int onRecvCustomPayload(tsp::TspRecvContext* recv_ctx, const unsigned char* data, int length) override;
				void onDownloadCancel() override;
			};

		} // namespace
	} // namespace service
} // namespace grida

#endif // __GRIDA_SERVICE_PIECE_PIECE_SOCKET_HPP__
