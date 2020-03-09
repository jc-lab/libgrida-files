/**
 * @file	peer_piece_download_context.cpp
 * @author	Jichan (development@jc-lab.net / http://ablog.jc-lab.net/ )
 * @copyright Copyright (C) 2019 jichan.\n
 *            This software may be modified and distributed under the terms
 *            of the Apache License 2.0.  See the LICENSE file for details.
 */

#include "peer_piece_download_context.hpp"

namespace grida {
	
	PeerPieceDownloadContext::~PeerPieceDownloadContext() {
		if (status() != PieceDownloadContext::DOWNLOAD_STATUS_SUCCESS) {
			int count = peer_info_->fail_count_inc_fetch();
			printf("fail_count_inc_fetch : [count=%d, status=%d]\n", count, status());
		}

		peer_info_->use_count_fetch_dec();
	}

	std::shared_ptr<PeerPieceDownloadContext> PeerPieceDownloadContext::create(PeerService::PieceDownloadHandler* peer_service_handler, uvw::Loop* loop, std::shared_ptr<DownloadContext> download_ctx, DownloadContext::PieceState* piece, DownloadContext::PeerInfo* peer_info, int max_count) {
		int count = peer_info->use_count_fetch_inc();
		if (count >= max_count)
		{
			peer_info->use_count_fetch_dec();
			return nullptr;
		}

		std::shared_ptr<PeerPieceDownloadContext> obj(new PeerPieceDownloadContext(peer_service_handler, download_ctx, piece, peer_info));
		obj->init(loop, obj);
		return obj;
	}

} // namespace grida
