/**
 * @file	piece_download_context.hpp
 * @author	Jichan (development@jc-lab.net / http://ablog.jc-lab.net/ )
 * @copyright Copyright (C) 2019 jichan.\n
 *            This software may be modified and distributed under the terms
 *            of the Apache License 2.0.  See the LICENSE file for details.
 */

#ifndef __GRIDA_PIECE_DOWNLOAD_CONTEXT_HPP__
#define __GRIDA_PIECE_DOWNLOAD_CONTEXT_HPP__

#include <string>

#include <memory>

#include <uvw/loop.hpp>

#include <uvw/timer.hpp>

#include <jcp/message_digest.hpp>

#include "file_handle.hpp"

#include "download_context.hpp"

#include "peer_service.hpp"

namespace grida {

	class PeerService;
	class PieceDownloadContext;

	class PieceDownloaderContext {
	protected:
		friend class PieceDownloadContext;
		virtual void onDownloadCancel() {}
	};

    class PieceDownloadContext {
	private:
		uvw::Loop* loop_;
		PeerService::PieceDownloadHandler* peer_service_handler_;

		DownloadContext::PieceState* piece_;

		std::string object_id_;
		std::string piece_id_;
		int piece_size_;
		std::unique_ptr<jcp::MessageDigest> md_;
		std::shared_ptr<DownloadContext> download_ctx_;

		std::mutex mutex_;
		std::shared_ptr<uvw::TimerHandle> timer_timeout_;

		bool alive_;

		FileHandle* file_handle_;

		int64_t file_offset_;
		int written_bytes_;

		std::atomic<int64_t> latest_recv_time_;
		unsigned char digest_buf_[64];
		int digest_len_;

		DownloadContext::DownloaderType downloader_type_;

		std::weak_ptr<PieceDownloaderContext> piece_downloader_ctx_;
		
	protected:
		PieceDownloadContext(PeerService::PieceDownloadHandler* peer_service_handler, std::shared_ptr<DownloadContext> download_ctx, DownloadContext::DownloaderType downloader_type, DownloadContext::PieceState* piece);
		void init(uvw::Loop* loop, std::shared_ptr<PieceDownloadContext> self);

    public:
		virtual ~PieceDownloadContext();

		PieceDownloadContext* self() {
			return this;
		}

		const std::string& object_id() const {
			return object_id_;
		}

		const std::string& piece_id() const {
			return piece_id_;
		}

		void appendData(const void* buf, size_t len);

		void done();

		bool alive() const {
			return alive_;
		}

		const unsigned char* digest_buf() const {
			return digest_buf_;
		}

		int digest_len() const {
			return digest_len_;
		}

		float progress() const {
			return (float)written_bytes_ / (float)piece_size_;
		}

		DownloadContext::DownloaderType downloader_type() const {
			return downloader_type_;
		}

		void setPieceDownloaderContext(std::shared_ptr<PieceDownloaderContext> obj) {
			piece_downloader_ctx_ = obj;
		}

		std::shared_ptr<PieceDownloaderContext> getPieceDownloaderContext() {
			return piece_downloader_ctx_.lock();
		}
    };

	class CustomerPieceDownloadContext : public PieceDownloadContext {
	private:
		CustomerPieceDownloadContext(PeerService::PieceDownloadHandler* peer_service_handler, std::shared_ptr<DownloadContext> download_ctx, DownloadContext::PieceState* piece)
			: PieceDownloadContext(peer_service_handler, download_ctx, DownloadContext::DOWNLOADER_CUSTOMER, piece)
		{}
	public:
		static std::shared_ptr<CustomerPieceDownloadContext> create(PeerService::PieceDownloadHandler* peer_service_handler, uvw::Loop* loop, std::shared_ptr<DownloadContext> download_ctx, DownloadContext::PieceState* piece);
	};

} // namespace grida

#endif // __GRIDA_PIECE_DOWNLOAD_CONTEXT_HPP__
