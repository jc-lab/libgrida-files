/**
 * @file	download_context.cpp
 * @author	Jichan (development@jc-lab.net / http://ablog.jc-lab.net/ )
 * @copyright Copyright (C) 2019 jichan.\n
 *            This software may be modified and distributed under the terms
 *            of the Apache License 2.0.  See the LICENSE file for details.
 */

#include "download_context.hpp"

#include "piece_download_context.hpp"

#include <libgrida/internal/hex_util.hpp>

#include "peer_service.hpp"

namespace grida {

	const int DownloadContext::PROGRESS_RATIO = 1000;

	DownloadContext::DownloadContext(std::shared_ptr<void> user_object_ctx) : user_object_ctx_(user_object_ctx), done_flag_(false), result_(0), scd_useing_(0), peer_service_(NULL), progress_(0) {
		random_ = jcp::SecureRandom::getInstance();
	}
	DownloadContext::~DownloadContext() {
	}

	void DownloadContext::init(PeerService *peer_service, const std::string& object_id, const SeedFile& seed_file, std::unique_ptr<DBObjectInformation> db_obj_info) {
		if (!object_id_.empty())
			return;
		peer_service_ = peer_service;
		object_id_ = object_id;
		seed_file_ = seed_file;
		db_obj_info_ = std::move(db_obj_info);

		const std::list<std::vector<unsigned char>>& pieces = seed_file.pieces.get();
		int piece_index = 0;
		pieces_.indexed.reserve(pieces.size());
		num_of_pieces_ = pieces.size();
		for (auto iter = pieces.cbegin(); iter != pieces.cend(); iter++) {
			std::string piece_id = ::grida::internal::HexUtil::bytesToHexString(iter->data(), iter->size());
			std::unique_ptr<PieceState> piece = std::make_unique<PieceState>(piece_id, piece_index++);
			memcpy(piece->digest_buf_, iter->data(), iter->size());
			piece->digest_len_ = iter->size();
			pieces_.indexed.push_back(piece.get());
			pieces_.map[piece_id] = std::move(piece);
		}
	}

	void DownloadContext::setFileHandle(std::unique_ptr<FileHandle> file_handle) {
		file_handle_ = std::move(file_handle);
	}

	void DownloadContext::pieceDownloadUpdate(PieceDownloadContext* piece_download_ctx) {
		PieceState &piece_state = pieces_.get(piece_download_ctx->piece_id());
		piece_state.progress_.store((int)(piece_download_ctx->progress() * 1000));
	}
	void DownloadContext::pieceDownloadCancel(PieceDownloadContext* piece_download_ctx)
	{
		PieceState& piece_state = pieces_.get(piece_download_ctx->piece_id());
		piece_state.restarted_count_++;
		piece_state.status_ = PieceState::STATUS_NOT_DOWNLOADED;

		if (piece_download_ctx->downloader_type() == DOWNLOADER_CUSTOMER) {
			scd_useing_--;
		}

		return;
	}
	void DownloadContext::pieceDownloadDone(PieceDownloadContext* piece_download_ctx)
	{
		PieceState &piece_state = pieces_.get(piece_download_ctx->piece_id());

		if (memcmp(piece_state.digest_buf_, piece_download_ctx->digest_buf(), piece_state.digest_len_))
		{
			// Failed
			piece_state.restarted_count_++;
			piece_state.progress_ = 0;
			piece_state.status_ = PieceState::STATUS_NOT_DOWNLOADED;
		} else {
			piece_state.progress_ = 1.0f;
			piece_state.status_ = PieceState::STATUS_DOWNLOADED;
			{
				int piece_size = seed_file_.piece_length.get();
				grida::DBPieceInformation db_piece_row;
				db_piece_row.object_id = object_id();
				db_piece_row.piece_id = piece_download_ctx->piece_id();
				db_piece_row.file_offset = (int64_t)piece_state.piece_index() * (int64_t)piece_size;
				db_piece_row.piece_size = piece_size;
				peer_service_->config()->peer_context->dbSetPieceInformation(db_piece_row, user_object_ctx_.get());
			}
		}

		if (piece_download_ctx->downloader_type() == DOWNLOADER_CUSTOMER) {
			scd_useing_--;
		}
	}

	bool DownloadContext::checkState() {
		int completed_count = 0;
		int all_count = 0;
		float running_progress_sum = 0;

		{
			std::unique_lock<std::recursive_mutex> lock(pieces_.mutex);
			all_count = pieces_.map.size();
			for (auto iter = pieces_.map.begin(); iter != pieces_.map.end(); iter++) {
				switch (iter->second->status_) {
				case PieceState::STATUS_DOWNLOADING:
					running_progress_sum += iter->second->progress_;
					break;
				case PieceState::STATUS_DOWNLOADED:
					completed_count++;
					break;
				}
			}

			if (completed_count == pieces_.map.size()) {
				progress_.store(PROGRESS_RATIO);
				setDone(1);
				return true;
			}
		}

		running_progress_sum += completed_count;
		progress_.store((int)(running_progress_sum * PROGRESS_RATIO / all_count));

		return false;
	}

	void DownloadContext::setDone(int result) {
		if (peer_service_)
			peer_service_->doneDownload(this, result > 0);

		{
			std::lock_guard<std::mutex> lock(done_mutex_);
			result_ = result;
			done_flag_ = true;
		}
		done_cond_.notify_all();
	}

	bool DownloadContext::isDone(int timeout_ms) {
		std::unique_lock<std::mutex> lock(done_mutex_);
		if (done_flag_)
			return true;

		if (timeout_ms > 0)
			done_cond_.wait_for(lock, std::chrono::milliseconds(timeout_ms));
		else if (timeout_ms < 0)
			done_cond_.wait(lock);

		return done_flag_;
	}

	int DownloadContext::generateWaitingTable() {
		std::vector< PieceState* > temp_table;
		int i;
		
		temp_table.reserve(pieces_.map.size());

		for (auto iter = pieces_.map.begin(); iter != pieces_.map.end(); iter++) {
			if (iter->second->status_ == PieceState::STATUS_NOT_DOWNLOADED) {
				temp_table.push_back(iter->second.get());
			}
		}

		if (!temp_table.empty()) {
			for (i = temp_table.size() - 1; i > 0; i--) {
				int j = random_->nextInt(i);
				PieceState* temp_swap = temp_table[j];
				temp_table[j] = temp_table[i];
				temp_table[i] = temp_swap;
			}
		}

		waiting_table_.buf = temp_table;
		waiting_table_.rear = temp_table.size() - 1;
		waiting_table_.front = -1;

		// rear = (rear + 1) % SIZE
		// buf[rear] = NEW

		// So, (rear + 1) == front : FULL

		// front = (front + 1) % SIZE
		// BUF[front] -> POP

		// So, rear == front : Empty

		return temp_table.size();
	}

	DownloadContext::PieceState* DownloadContext::nextPiece() {
		std::unique_lock<std::mutex> lock(waiting_table_.mutex);

		int loop_count = 2;

		do {
			if (waiting_table_.front == waiting_table_.rear)
			{
				int count = generateWaitingTable();
				if (count <= 0)
					return NULL;
				loop_count--;
			}

			waiting_table_.front = (waiting_table_.front + 1) % waiting_table_.buf.size();
			DownloadContext::PieceState* item = waiting_table_.buf[waiting_table_.front];
			if (item->status_ == DownloadContext::PieceState::STATUS_NOT_DOWNLOADED) {
				return item;
			}
		} while (loop_count);

		return NULL;
	}

	void DownloadContext::cancelPiece(PieceState* item) {
		std::unique_lock<std::mutex> lock(waiting_table_.mutex);
		waiting_table_.rear = (waiting_table_.rear + 1) % waiting_table_.buf.size();
		waiting_table_.buf[waiting_table_.rear] = item;
	}

	void DownloadContext::onObjectDiscovered(tsp::SocketPayload* socket_payload, service::mcd::McdObjectDiscoveryResponsePayload* response_payload)
	{
		std::unique_lock<std::recursive_mutex> lock(peers_info_.mutex);
		PeerInfo& peer_info = peers_info_.get(socket_payload->remote_ip());
		data::BitStream bit_stream;

		if (response_payload->object_id.get() != this->object_id_)
			return;

		{
			const std::vector<unsigned char>& temp = response_payload->pieces_bitmap.get();
			bit_stream.from(temp.data(), temp.size());
		}

		if (bit_stream.bits_size() == num_of_pieces_) {
			peer_info.pieces_bitmap = bit_stream;
			peer_info.valided = true;
		}

		peer_info.last_valid_time = std::chrono::steady_clock::now().time_since_epoch().count();
	}

} // namespace grida
