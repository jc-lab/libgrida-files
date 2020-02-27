/**
 * @file	download_context.hpp
 * @author	Jichan (development@jc-lab.net / http://ablog.jc-lab.net/ )
 * @copyright Copyright (C) 2019 jichan.\n
 *            This software may be modified and distributed under the terms
 *            of the Apache License 2.0.  See the LICENSE file for details.
 */

#ifndef __GRIDA_DOWNLOAD_CONTEXT_HPP__
#define __GRIDA_DOWNLOAD_CONTEXT_HPP__

#include <string>

#include <memory>
#include <map>
#include <list>
#include <vector>
#include <mutex>
#include <atomic>
#include <condition_variable>

#include "object_discovery_context.hpp"

#include <jcp/secure_random.hpp>

#include "seed_file.hpp"

#include "db_data.hpp"

#include "file_handle.hpp"

#include "data/bitstream.hpp"

namespace grida {

	class PeerService;

	class PieceDownloadContext;

	class DownloadContext : public ObjectDiscoveryContext {
	public:
		static const int PROGRESS_RATIO;

		enum DownloaderType {
			DOWNLOADER_PEER = 1,
			DOWNLOADER_CUSTOMER
		};
		struct PieceState {
			enum Status {
				STATUS_NOT_DOWNLOADED = 0,
				STATUS_DOWNLOADING = 1,
				STATUS_DOWNLOADED = 2,
			};

			std::string piece_id_;
			int piece_index_;
			volatile Status status_; // 0:Not downloaded, 1:Downloading, 2:Downloaded
			int restarted_count_;
			int dup_count_; // 0:Not inited
			std::atomic_int progress_; // 0.0~1.0 * 1000

			unsigned char digest_buf_[64];
			int digest_len_;

			std::string downloader_peer_id_;

			PieceState(const std::string& piece_id, int piece_index) : status_(STATUS_NOT_DOWNLOADED), piece_id_(piece_id), piece_index_(piece_index), progress_(0) {
				restarted_count_ = 0;
				dup_count_ = 0;
				digest_len_ = 0;
			}

			const std::string piece_id() const {
				return piece_id_;
			}

			int piece_index() const {
				return piece_index_;
			}

			const std::string downloader_peer_id() const {
				return downloader_peer_id_;
			}
		};

		struct PeerInfo {
			std::string remote_ip;
			data::BitStream pieces_bitmap;
			bool valided;
			std::atomic<int64_t> last_valid_time; // milliseconds

			std::atomic_int count_;

			PeerInfo() : valided(false), count_(0) { }

			int use_count_fetch_inc()
			{
				return count_.fetch_add(1);
			};

			int use_count_fetch_dec()
			{
				return count_.fetch_sub(1);
			};

			int get_use_count() const {
				return count_.load();
			}
		};

	private:
		friend class PeerService;

		std::weak_ptr<DownloadContext> self_;
		grida::PeerService* peer_service_;
		std::shared_ptr<void> user_object_ctx_;
		SeedFile seed_file_;

		std::unique_ptr<DBObjectInformation> db_obj_info_;

		std::mutex done_mutex_;
		std::condition_variable done_cond_;
		bool done_flag_;

		int result_;

		int num_of_pieces_;

		struct {
			std::recursive_mutex mutex;
			std::map<std::string, std::unique_ptr<PieceState> > map;
			std::vector< PieceState* > indexed;

			PieceState &get(const std::string& piece_id) {
				std::unique_lock<std::recursive_mutex> lock(mutex);
				return *map[piece_id];
			}
		} pieces_;

		struct {
			std::recursive_mutex mutex;
			std::map<std::string, std::unique_ptr<PeerInfo>> map;

			PeerInfo& get(const std::string& remote_ip) {
				std::unique_lock<std::recursive_mutex> lock(mutex);
				std::unique_ptr<PeerInfo>& ref = map[remote_ip];
				if(!ref) {
					ref.reset(new PeerInfo());
					ref->remote_ip = remote_ip;
				}
				return *ref;
			}
		} peers_info_;

		std::unique_ptr<jcp::SecureRandom> random_;

		// "Single Custom Download" Using Flag
		std::atomic_int scd_useing_;

		std::atomic_int progress_; // 0.0~1.0 * 1000

		// piece ordering
		struct WaitingTable {
			std::mutex mutex;
			std::vector<PieceState*> buf;
			int front;
			int rear;

			WaitingTable() : front(-1), rear(-1) {
			}

			int getCurIndex(int* position) {
				int old = *position;
				int pos = *position = (*position) % buf.size();
				if (old == pos)
					(*position)++;
				return pos;
			}
		} waiting_table_;

		std::unique_ptr<FileHandle> file_handle_;

		/**
		 * @return remainging pieces count
		 */
		int generateWaitingTable();
		
	public:
		DownloadContext(std::shared_ptr<void> user_object_ctx);
		virtual ~DownloadContext();

		static std::shared_ptr<DownloadContext> create(std::shared_ptr<void> user_object_ctx) {
			std::shared_ptr<DownloadContext> download_context = std::make_shared<DownloadContext>(user_object_ctx);
			download_context->self_ = download_context;
			return download_context;
		}

		void init(grida::PeerService *peer_service, const std::string& object_id, const SeedFile& seed_file, std::unique_ptr<DBObjectInformation> db_obj_info);

		void setFileHandle(std::unique_ptr<FileHandle> file_handle);

		std::shared_ptr<DownloadContext> self() {
			return std::shared_ptr<DownloadContext>(self_);
		}

		const SeedFile* seed_file() const {
			return &seed_file_;
		}

		const DBObjectInformation* db_object_info() const {
			return db_obj_info_.get();
		}

		std::shared_ptr<void> user_object_ctx() {
			return user_object_ctx_;
		}

		int piece_size() const {
			return seed_file_.piece_length.get();
		}

		FileHandle* file_handle() {
			return file_handle_.get();
		}

		void pieceDownloadUpdate(PieceDownloadContext* piece_download_ctx);
		void pieceDownloadCancel(PieceDownloadContext* piece_download_ctx);
		void pieceDownloadDone(PieceDownloadContext* piece_download_ctx);

		bool checkState();

		void setDone(int result);

		bool isDone(int timeout_ms);

		int result() const {
			return result_;
		}

		PieceState* nextPiece();

		void cancelPiece(PieceState* item);

		std::atomic_int& scd_useing() {
			return scd_useing_;
		}

		float getProgress() const {
			return (float)progress_.load() / (float)PROGRESS_RATIO;
		}

		protected:
			void onObjectDiscovered(tsp::SocketPayload* socket_payload, service::mcd::McdObjectDiscoveryResponsePayload* response_payload) override;
	};

} // namespace grida

#endif // __GRIDA_DOWNLOAD_CONTEXT_HPP__
