/**
 * @file	peer_service.hpp
 * @author	Jichan (development@jc-lab.net / http://ablog.jc-lab.net/ )
 * @copyright Copyright (C) 2019 jichan.\n
 *            This software may be modified and distributed under the terms
 *            of the Apache License 2.0.  See the LICENSE file for details.
 */

#ifndef __GRIDA_PEER_SERVICE_HPP__
#define __GRIDA_PEER_SERVICE_HPP__

#include <memory>
#include <mutex>
#include <condition_variable>

#include "limited_memory_pool.hpp"

#include "shared_file_handle.hpp"

#include "internal/use_loop.hpp"
#include "peer_context.hpp"

#include "download_context.hpp"

#include "thread_pool.hpp"

namespace grida {

	namespace service {
		class RdvClient;
		class RouteTracer;
		class McdService;
		class PieceService;
	} // service

	class PeerPieceDownloadContext;

	template<typename TSharedContext>
	class DownloadContextWithSharedContext : public DownloadContext {
	private:
		std::shared_ptr<TSharedContext> shared_context_;

	public:
		DownloadContextWithSharedContext(std::shared_ptr<TSharedContext> shared_context) : DownloadContext(), shared_context_(shared_context)
		{}

		static std::shared_ptr<DownloadContextWithSharedContext> create(std::shared_ptr<TSharedContext> shared_context) {
			std::shared_ptr<DownloadContextWithSharedContext> download_context = std::make_shared<DownloadContextWithSharedContext<TSharedContext>>(shared_context);
			download_context->self_ = download_context;
			return download_context;
		}
	};

	class PeerService : public virtual internal::LoopProvider {
	public:
		struct Config {
			PeerContext* peer_context;
			LimitedMemoryPool* memory_pool;
			std::string multicast_addr;
			std::string interface_addr;
			int64_t bitrate_limit_peer;

			int peer_ttl;

			bool use_rdv;

			Config() {
				peer_context = NULL;
				use_rdv = true;
				memory_pool = NULL;
				bitrate_limit_peer = 0;
				peer_ttl = 30;
			}
		};

		class PieceDownloadHandler {
		private:
			PeerService* service_;

		public:
			PieceDownloadHandler(PeerService* service) : service_(service) {}
			void add(PieceDownloadContext *id, std::shared_ptr<PieceDownloadContext> ctx);
			void remove(PieceDownloadContext* id);
		};

	private:
		class PieceFileHandle;

		std::shared_ptr<::uvw::Loop> loop_;

		ThreadPool thread_pool_;

		std::thread manage_thread_;
		std::atomic_bool manage_thread_run_;

		Config config_;

		std::shared_ptr<service::PieceService> piece_service_;

		PieceDownloadHandler piece_download_handler_;

		struct {
			std::mutex mutex;
			std::map< std::string, std::weak_ptr<SharedFileHandle::RootHandle>> map;
		} object_file_handles_;

		struct {
			std::mutex mutex;
			std::map< DownloadContext*, std::shared_ptr<DownloadContext>> map;
		} running_downloads_;

		struct {
			std::mutex mutex;
			std::map< PieceDownloadContext*, std::weak_ptr<PieceDownloadContext> > map;
		} piece_downloads_;

		void beginDownloadBySeedFileImpl(std::shared_ptr<DownloadContext> download_context, const std::vector<unsigned char>& seed_file, std::shared_ptr<void> user_object_ctx);

		bool checkPieceFromFile(FileHandle* file_handle, const SeedFile& seed_file, int piece_index);

		void downloadManageThreadProc();

		void beginPieceDownloadByPeer(std::shared_ptr<PeerPieceDownloadContext> piece_download_context);

	public:
		std::shared_ptr<service::RdvClient> rdv_client_;
		std::shared_ptr<service::RouteTracer> route_tracer_;
		std::shared_ptr<service::McdService> mcd_service_;

		const Config* config() const {
			return &config_;
		}

	public:
		PeerService(const std::shared_ptr<::uvw::Loop>& loop);
		~PeerService();

		std::shared_ptr<::uvw::Loop> get_loop() const override {
			return loop_;
		}

		ThreadPool* thread_pool() {
			return &thread_pool_;
		}

		int start(const Config &config);
		int stop();

		jcp::Result<void> setClientTLS(const std::vector<unsigned char>& cert_der, const std::vector<unsigned char>& key_der);

		std::shared_ptr<DownloadContext> beginDownloadBySeedFile(const std::vector<unsigned char>& seed_file, std::shared_ptr<void> user_object_ctx) {
			std::shared_ptr<DownloadContext> download_context = DownloadContext::create(user_object_ctx);
			beginDownloadBySeedFileImpl(download_context, seed_file, user_object_ctx);
			return download_context;
		}

		void doneDownload(DownloadContext* download_context, bool success);

		std::unique_ptr<FileHandle> openPieceFile(const std::string& object_id, const std::string& piece_id);

		int64_t getSpeedLimitBitratePeer();
	};

} // namespace grida

#endif // __GRIDA_PEER_SERVICE_HPP__
