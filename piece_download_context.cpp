/**
 * @file	piece_download_context.cpp
 * @author	Jichan (development@jc-lab.net / http://ablog.jc-lab.net/ )
 * @copyright Copyright (C) 2019 jichan.\n
 *            This software may be modified and distributed under the terms
 *            of the Apache License 2.0.  See the LICENSE file for details.
 */

#include "piece_download_context.hpp"

#include <uvw/async.hpp>

#include <jcp/message_digest_algo.hpp>

namespace grida {

	std::shared_ptr<CustomerPieceDownloadContext> CustomerPieceDownloadContext::create(PeerService::PieceDownloadHandler* peer_service_handler, uvw::Loop* loop, std::shared_ptr<DownloadContext> download_ctx, DownloadContext::PieceState* piece)
	{
		std::shared_ptr<CustomerPieceDownloadContext> obj(new CustomerPieceDownloadContext(peer_service_handler, download_ctx, piece));
		obj->init(loop, obj);
		return obj;
	}

	PieceDownloadContext::PieceDownloadContext(PeerService::PieceDownloadHandler* peer_service_handler, std::shared_ptr<DownloadContext> download_ctx, DownloadContext::DownloaderType downloader_type, DownloadContext::PieceState* piece)
		: peer_service_handler_(peer_service_handler), download_ctx_(download_ctx), downloader_type_(downloader_type), written_bytes_(0), alive_(true), loop_(NULL)
	{
		object_id_ = download_ctx->object_id();
		piece_id_ = piece->piece_id();
		piece_size_ = download_ctx->piece_size();
		file_handle_ = download_ctx->file_handle();
		file_offset_ = (int64_t)piece_size_ * (int64_t)piece->piece_index();

		md_ = jcp::MessageDigest::getInstance(jcp::MessageDigestAlgorithm::SHA_256.algo_id());
	}

	void PieceDownloadContext::init(uvw::Loop* loop, std::shared_ptr<PieceDownloadContext> shared_self)
	{
		std::weak_ptr<PieceDownloadContext> weak_self(shared_self);
		std::shared_ptr<uvw::AsyncHandle> loop_task = loop->resource<uvw::AsyncHandle>();
		
		loop_ = loop;

		peer_service_handler_->add(self(), shared_self);

		loop_task->on<uvw::AsyncEvent>([weak_self](const uvw::AsyncEvent& task_evt, uvw::AsyncHandle& task_handle) {
			std::shared_ptr<PieceDownloadContext> self(weak_self.lock());
			if (self) {
				self->timer_timeout_ = task_handle.loop().resource<uvw::TimerHandle>();
				self->timer_timeout_->on<uvw::TimerEvent>([weak_self](const uvw::TimerEvent& timer_evt, uvw::TimerHandle& timer_handle) {
					std::shared_ptr<PieceDownloadContext> self(weak_self.lock());
					if (self) {
						if (self->alive_) {
							self->alive_ = false;
							self->download_ctx_->pieceDownloadCancel(self.get());
						}
						timer_handle.stop();
						timer_handle.close();
						self->timer_timeout_.reset();
					}
				});
				self->timer_timeout_->start(uvw::TimerHandle::Time{ 330000 }, uvw::TimerHandle::Time{ 0 }); // 3min 30sec
			}
			task_handle.close();
		});
		
		loop_task->send();
	}

	PieceDownloadContext::~PieceDownloadContext()
	{
		peer_service_handler_->remove(self());

		if (loop_ && timer_timeout_)
		{
			std::shared_ptr<uvw::AsyncHandle> loop_task = loop_->resource<uvw::AsyncHandle>();
			std::shared_ptr<uvw::TimerHandle> timer_timeout(timer_timeout_);
			timer_timeout_.reset();
			loop_task->on<uvw::AsyncEvent>([timer_timeout](const uvw::AsyncEvent& task_evt, uvw::AsyncHandle& task_handle) {
				if (timer_timeout->active()) {
					timer_timeout->stop();
				}
				timer_timeout->close();
				task_handle.close();
			});
			loop_task->send();
		}

		if (alive_)
		{
			download_ctx_->pieceDownloadCancel(this);
		}
	}

	void PieceDownloadContext::appendData(const void* buf, size_t len)
	{
		if (!alive_)
			return;

		int64_t remaining = piece_size_ - written_bytes_;
		size_t avail_len = (remaining < len) ? remaining : len;

		if (avail_len != len) {
			// Some problem
			printf("**ERR[PieceDownloadContext::appendData]\n\n");
		}

		{
			FileHandle::UniqueWriteLock lock(*file_handle_);
			file_handle_->seek(file_offset_ + written_bytes_, FileHandle::SEEK_FILE_BEGIN);
			file_handle_->write(buf, avail_len);
		}
		written_bytes_ += avail_len;
		md_->update(buf, avail_len);
	}

	void PieceDownloadContext::done() {
		if (!alive_)
			return;

		if (written_bytes_ != piece_size_)
		{
			// Error
			printf("written_bytes_ != piece_size_\n");
		}
		md_->digest(digest_buf_);
		digest_len_ = md_->digest_size();

		alive_ = false;
		download_ctx_->pieceDownloadDone(this);
	}

} // namespace grida
