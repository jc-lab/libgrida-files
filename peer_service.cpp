/**
 * @file	peer_service.cpp
 * @author	Jichan (development@jc-lab.net / http://ablog.jc-lab.net/ )
 * @copyright Copyright (C) 2019 jichan.\n
 *            This software may be modified and distributed under the terms
 *            of the Apache License 2.0.  See the LICENSE file for details.
 */

#include "peer_service.hpp"

#include "shared_file_handle.hpp"

#include "service/rdv_client.hpp"
#include "service/route_tracer.hpp"
#include "service/mcd_service.hpp"

#include "seed_file.hpp"

#include <jcp/message_digest.hpp>
#include <jcp/message_digest_algo.hpp>
#include <jcp/key_factory.hpp>
#include <jcp/key_factory_algo.hpp>
#include <jcp/x509_encoded_key_spec.hpp>
#include <jcp/pkcs8_encoded_key_spec.hpp>

#include "internal/hex_util.hpp"

#include "data/bitstream.hpp"

#include "service/piece_service.hpp"

#include "service/piece/piece_socket.hpp"

namespace grida {

	class PeerService::PieceFileHandle : public FileHandle {
	private:
		int64_t offset_;
		int64_t piece_size_;
		int64_t remaining_;
		std::unique_ptr<FileHandle> real_;

	public:
		PieceFileHandle(std::unique_ptr<FileHandle> real, int64_t offset, int64_t size) {
			real_ = std::move(real);
			offset_ = offset;
			piece_size_ = size;
			remaining_ = size;
		}

		virtual ~PieceFileHandle() {
			close();
		}

		int readLock() override {
			return 0;
		}
		int readUnlock() override {
			return 0;
		}
		int writeLock() override {
			return 0;
		}
		int writeUnlock() override {
			return 0;
		}
		int setFileSize(int64_t size) override {
			return 0;
		}
		int getFileSize(int64_t* psize) override {
			*psize = piece_size_;
			return 1;
		}
		int seek(int64_t offset, SeekMethod method) override {
			return 0;
		}
		int64_t read(void* buf, size_t length) override {
			if (real_) {
				size_t avail = (remaining_ > length) ? length : remaining_;
				int64_t rc;
				if (avail <= 0)
					return 0;
				FileHandle::UniqueReadLock lock(*real_);
				real_->seek(offset_, FileHandle::SEEK_FILE_BEGIN);
				rc = real_->read(buf, avail);
				if (rc > 0) {
					offset_ += rc;
					remaining_ -= rc;
				}
				return rc;
			}
			return 0;
		}
		int64_t write(const void* buf, size_t length) override {
			return 0;
		}
		int close() override {
			if (real_) {
				int rc = real_->close();
				real_.reset();
				return rc;
			}
			return 2;
		}
	};

	PeerService::PeerService(const std::shared_ptr<::uvw::Loop>& loop)
		: loop_(loop), piece_service_(std::make_unique<service::PieceService>(this)), piece_download_handler_(this)
	{
	}

	PeerService::~PeerService() {
		stop();
	}

	int PeerService::start(const Config& config)
	{
		config_ = config;
		route_tracer_.reset(new service::RouteTracer(this));
		mcd_service_.reset(new service::McdService(this, config.peer_context));
		
		piece_service_->init(config.memory_pool);
		piece_service_->listen(19901);
		
		if (config.use_rdv) {
			rdv_client_.reset(new service::RdvClient(this, config.peer_context));
			rdv_client_->start(route_tracer_.get());
		}
		
		mcd_service_->start(thread_pool(), config.multicast_addr, config.interface_addr);

		manage_thread_run_.store(true);
		manage_thread_ = std::thread(std::bind(&PeerService::downloadManageThreadProc, this));

		thread_pool_.start(1);

		return 0;
	}

	int PeerService::stop()
	{
		manage_thread_run_.store(false);
		thread_pool_.stop();
		if (manage_thread_.joinable()) {
			manage_thread_.join();
		}
		if (route_tracer_.get()) {
			route_tracer_.reset();
		}
		if (mcd_service_.get()) {
			mcd_service_->stop();
			mcd_service_.reset();
		}
		if (rdv_client_.get()) {
			rdv_client_->stop();
			rdv_client_.reset();
		}
		return 0;
	}

	jcp::Result<void> PeerService::setClientTLS(const std::vector<unsigned char>& cert_der, const std::vector<unsigned char>& key_der)
	{
		std::unique_ptr<jcp::KeyFactory> x509_key_factory = jcp::KeyFactory::getInstance(jcp::KeyFactoryAlgorithm::X509PublicKey.algo_id());
		std::unique_ptr<jcp::KeyFactory> pkcs8_key_factory = jcp::KeyFactory::getInstance(jcp::KeyFactoryAlgorithm::Pkcs8PrivateKey.algo_id());

		jcp::Result<std::unique_ptr<jcp::X509EncodedKeySpec>> cert_spec(jcp::X509EncodedKeySpec::decode(cert_der.data(), cert_der.size()));
		jcp::Result<std::unique_ptr<jcp::PKCS8EncodedKeySpec>> key_spec(jcp::PKCS8EncodedKeySpec::decode(key_der.data(), key_der.size()));

		if (!cert_spec) {
			return jcp::ResultBuilder<void, std::exception>().withOtherException(cert_spec.move_exception()).build();
		}
		if (!key_spec) {
			return jcp::ResultBuilder<void, std::exception>().withOtherException(key_spec.move_exception()).build();
		}

		return jcp::ResultBuilder<void, void>().build();
	}

	void PeerService::downloadManageThreadProc() {
		while (manage_thread_run_) {
			{
				std::unique_lock<std::mutex> lock(running_downloads_.mutex);
				for (auto work = running_downloads_.map.begin(); work != running_downloads_.map.end(); ) {
					bool work_done = false;
					DownloadContext* download_context = work->second.get();

					// ====================================================================================================
					// Try download by other peer
					// ====================================================================================================
					{
						std::unique_lock<std::recursive_mutex> pieces_lock(download_context->pieces_.mutex);
						std::unique_lock<std::recursive_mutex> peers_lock(download_context->peers_info_.mutex);

						for (auto peer_iter = download_context->peers_info_.map.begin(); peer_iter != download_context->peers_info_.map.end(); peer_iter++) {
							if (peer_iter->second->valided && peer_iter->second->get_use_count() == 0) {
								DownloadContext::PeerInfo* peer_info = peer_iter->second.get();
								std::vector<DownloadContext::PieceState*> avail_list;
								avail_list.reserve(download_context->num_of_pieces_);
								for (int i = 0; i < download_context->pieces_.indexed.size(); i++) {
									if (peer_info->pieces_bitmap.get(i)) {
										DownloadContext::PieceState* item = download_context->pieces_.indexed[i];
										if (item->status_ == DownloadContext::PieceState::STATUS_NOT_DOWNLOADED)
										{
											avail_list.push_back(item);
										}
									}
								}

								if (avail_list.size() > 0) {
									int target_index = download_context->random_->nextInt(avail_list.size() - 1);
									DownloadContext::PieceState* target_piece = avail_list[target_index];

									target_piece->status_ = DownloadContext::PieceState::STATUS_DOWNLOADING;

									// GO DOWNLOAD!
									std::shared_ptr<PeerPieceDownloadContext> piece_download_context(PeerPieceDownloadContext::create(&piece_download_handler_, loop_.get(), download_context->self(), target_piece, peer_info, 1));
									if (piece_download_context) {
										printf("Begin download by Peer : piece=%s, peer=%s\n", target_piece->piece_id_.c_str(), peer_info->remote_ip.c_str());
										beginPieceDownloadByPeer(piece_download_context);
									}
								}
							}
						}
					}
					// ====================================================================================================

					// ====================================================================================================
					// Try download by custom-downloader
					// ====================================================================================================
					{
						DownloadContext::PieceState* piece = download_context->nextPiece();
						if (piece) {
							int avail_custom_downloader = config_.peer_context->availCustomDownloader();
							int custom_downloading_count = ++download_context->scd_useing();
							if (custom_downloading_count > avail_custom_downloader) {
								download_context->scd_useing()--;
								download_context->cancelPiece(piece);
							}
							else {
								// GO DOWNLOAD!
								std::shared_ptr<PieceDownloadContext> piece_download_context(CustomerPieceDownloadContext::create(&piece_download_handler_, loop_.get(), download_context->self(), piece));
								printf("Begin download by CustomDownloader : piece=%s\n", piece->piece_id_.c_str());
								config_.peer_context->beginCustomDownload(download_context->db_object_info(), piece->piece_id_, piece_download_context, download_context->user_object_ctx());
							}
						}
					}
					// ====================================================================================================

					if (download_context->checkState()) {
						work_done = true;
					}

					if (work_done) {
						work = running_downloads_.map.erase(work);
					} else {
						work++;
					}
				}
			}
			{
				std::unique_lock<std::mutex> lock(object_file_handles_.mutex);
				for (auto iter = object_file_handles_.map.begin(); iter != object_file_handles_.map.end(); ) {
					if (iter->second.expired()) {
						iter = object_file_handles_.map.erase(iter);
					} else {
						iter++;
					}
				}
			}
			Sleep(50);
		}
	}

	bool PeerService::checkPieceFromFile(FileHandle* file_handle, const SeedFile& seed_file, int piece_index) {
		std::unique_ptr<jcp::MessageDigest> message_digest = jcp::MessageDigest::getInstance(jcp::MessageDigestAlgorithm::SHA_256.algo_id());

		const std::list<std::vector<unsigned char>>& pieces = seed_file.pieces.get();
		std::vector<unsigned char> file_buf(seed_file.piece_length.get());

		unsigned char digest_buf[64];
		int digest_size = message_digest->digest_size();
		int64_t read_size;

		if ((piece_index < 0) || (piece_index >= pieces.size()))
			return false;

		file_handle->seek((int64_t)seed_file.piece_length.get() * (int64_t)piece_index, FileHandle::SEEK_FILE_BEGIN);
		read_size = file_handle->read(&file_buf[0], file_buf.size());

		message_digest->update(file_buf.data(), read_size);
		message_digest->digest(digest_buf);

		auto iter = pieces.cbegin();
		for (int count = piece_index; count; count--) {
			iter++;
		}

		if (digest_size != iter->size())
			return false;

		return (memcmp(iter->data(), digest_buf, iter->size()) == 0);
	}

	void PeerService::beginDownloadBySeedFileImpl(std::shared_ptr<DownloadContext> download_context, const std::vector<unsigned char>& seed_file, std::shared_ptr<void> user_object_ctx)
	{
		int result;

		SeedFile seed_file_vo;
		unsigned char seed_file_hash[32];
		std::string object_id;

		int db_res;
		std::unique_ptr<DBObjectInformation> db_obj_info;

		{
			std::unique_ptr<jcp::MessageDigest> seed_md = jcp::MessageDigest::getInstance(jcp::MessageDigestAlgorithm::SHA_256.algo_id());
			seed_md->update(seed_file.data(), seed_file.size());
			seed_md->digest(seed_file_hash);
			object_id = internal::HexUtil::bytesToHexString(seed_file_hash, sizeof(seed_file_hash));
		}

		seed_file_vo.deserialize(seed_file, 0);

		db_res = config_.peer_context->dbGetObjectInformation(db_obj_info, object_id, user_object_ctx.get());
		if (db_res == 0) {
			if (db_obj_info->status == 1)
			{
				std::unique_ptr<FileHandle> file_handle;
				result = config_.peer_context->fileOpen(file_handle, download_context->db_object_info(), FileHandle::MODE_READ, &seed_file_vo, user_object_ctx);
				if ((result > 0) && file_handle) {
					int64_t file_size = 0;
					if (file_handle->getFileSize(&file_size) && (file_size >= seed_file_vo.file_size.get())) {
						if (checkPieceFromFile(file_handle.get(), seed_file_vo, 0) && checkPieceFromFile(file_handle.get(), seed_file_vo, seed_file_vo.pieces.get().size() - 1))
						{
                            file_handle->close();
							download_context->setDone(1);
							return;
						}
					}
                    file_handle->close();
				}
			}
		}

		if (!db_obj_info) {
			db_obj_info = std::make_unique<DBObjectInformation>();
			db_obj_info->object_id = object_id;
			db_obj_info->status = 0;
			db_obj_info->raw_seed_file = seed_file;

			db_res = config_.peer_context->dbSetObjectInformation(db_obj_info, seed_file_vo, user_object_ctx.get());
			if (db_res != 0) {
				return;
			}
		}
		
		download_context->init(this, object_id, seed_file_vo, std::move(db_obj_info));

		{
			std::unique_ptr<FileHandle> file_handle;
			result = config_.peer_context->fileOpen(file_handle, download_context->db_object_info(), FileHandle::MODE_RW, &seed_file_vo, user_object_ctx);
			if (result <= 0)
			{
				download_context->setDone(result);
				return;
			}
			else if (!file_handle.get()) {
				download_context->setDone(-1);
				return;
			}

			{
				SharedFileHandle* shared_file_handle = dynamic_cast<SharedFileHandle*>(file_handle.get());
				if (shared_file_handle) {
					std::unique_lock<std::mutex> lock(object_file_handles_.mutex);
					object_file_handles_.map[object_id] = shared_file_handle->rootHandle();
				}
			}

			result = file_handle->setFileSize(seed_file_vo.file_size.get());
			if (result <= 0)
			{
				file_handle->close();
				download_context->setDone(result);
				return;
			}

			download_context->setFileHandle(std::move(file_handle));
		}

		mcd_service_->startDiscoveryObject(download_context);

		{
			std::unique_lock<std::mutex> lock(running_downloads_.mutex);
			running_downloads_.map[download_context.get()] = download_context;
		}
	}

	void PeerService::doneDownload(DownloadContext* download_context, bool success) {
		mcd_service_->stopDiscoveryObject(download_context);

		if (success)
		{
			int db_res;
			download_context->db_obj_info_->status = 1;
			db_res = config_.peer_context->dbSetObjectInformation(download_context->db_obj_info_, *download_context->seed_file(), download_context->user_object_ctx_.get());
		}
	}

	void PeerService::beginPieceDownloadByPeer(std::shared_ptr<PeerPieceDownloadContext> piece_download_context)
	{
		service::piece::PieceSocket *piece_socket = piece_service_->requestPieceToPeer(piece_download_context);
	}

	std::unique_ptr<FileHandle> PeerService::openPieceFile(const std::string& object_id, const std::string& piece_id)
	{
		std::unique_ptr<FileHandle> file_handle;
		std::unique_ptr<DBObjectInformation> object_info;
		std::unique_ptr<DBPieceInformation> piece_info;
		int rc;
		SeedFile seed_file;
		
		rc = config_.peer_context->dbGetObjectInformation(object_info, object_id, NULL);
		if (rc != 0) {
			return nullptr;
		}
		
		rc = config_.peer_context->dbGetPieceInformation(piece_info, object_id, piece_id, NULL);
		if (rc != 0) {
			return nullptr;
		}

		try {
			seed_file.deserialize(object_info->raw_seed_file, 0);
		}
		catch (JsBsonRPC::Serializable::ParseException& ex) {
			return nullptr;
		}

		do {
			std::unique_lock<std::mutex> lock(object_file_handles_.mutex);

			{
				auto iter = object_file_handles_.map.find(object_id);
				if (iter != object_file_handles_.map.end()) {
					std::shared_ptr<SharedFileHandle::RootHandle> shared_file_root = iter->second.lock();
					if (shared_file_root) {
						file_handle = std::move(shared_file_root->subReadModeOpen());
					} else {
						object_file_handles_.map.erase(iter);
					}
				}
			}

			if (!file_handle) {
				rc = config_.peer_context->fileOpen(file_handle, object_info.get(), FileHandle::MODE_READ, &seed_file, NULL);
				if (rc != 1) {
					return nullptr;
				}

				SharedFileHandle* shared_file_handle = dynamic_cast<SharedFileHandle*>(file_handle.get());
				if (shared_file_handle) {
					object_file_handles_.map[object_id] = shared_file_handle->rootHandle();
				}
			}
		} while (0);

		return std::make_unique<PieceFileHandle>(std::move(file_handle), piece_info->file_offset, piece_info->piece_size);
	}

	int64_t PeerService::getSpeedLimitBitratePeer() {
		return config_.bitrate_limit_peer;
	}

	void PeerService::PieceDownloadHandler::add(PieceDownloadContext *id, std::shared_ptr<PieceDownloadContext> ctx) {
		std::unique_lock<std::mutex> lock(service_->piece_downloads_.mutex);
		service_->piece_downloads_.map[ctx->self()] = ctx;
	}
	void PeerService::PieceDownloadHandler::remove(PieceDownloadContext* id) {
		std::unique_lock<std::mutex> lock(service_->piece_downloads_.mutex);
		auto iter = service_->piece_downloads_.map.find(id);
		if (iter == service_->piece_downloads_.map.end()) {
			printf("ERROR!");
			return;
		}

		service_->piece_downloads_.map.erase(iter);
	}
	
} // namespace grida
