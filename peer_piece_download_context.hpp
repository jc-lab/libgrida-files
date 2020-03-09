/**
 * @file	peer_piece_download_context.hpp
 * @author	Jichan (development@jc-lab.net / http://ablog.jc-lab.net/ )
 * @copyright Copyright (C) 2019 jichan.\n
 *            This software may be modified and distributed under the terms
 *            of the Apache License 2.0.  See the LICENSE file for details.
 */

#ifndef __GRIDA_PEER_PIECE_DOWNLOAD_CONTEXT_HPP__
#define __GRIDA_PEER_PIECE_DOWNLOAD_CONTEXT_HPP__

#include "piece_download_context.hpp"

namespace grida {

	class PeerPieceDownloadContext : public PieceDownloadContext {
	private:
		DownloadContext::PeerInfo* peer_info_;
	
	private:
		PeerPieceDownloadContext(PeerService::PieceDownloadHandler* peer_service_handler, std::shared_ptr<DownloadContext> download_ctx, DownloadContext::PieceState* piece, DownloadContext::PeerInfo *peer_info)
			: PieceDownloadContext(peer_service_handler, download_ctx, DownloadContext::DOWNLOADER_PEER, piece), peer_info_(peer_info)
		{
		}

	public:
		static std::shared_ptr<PeerPieceDownloadContext> create(PeerService::PieceDownloadHandler* peer_service_handler, uvw::Loop* loop, std::shared_ptr<DownloadContext> download_ctx, DownloadContext::PieceState* piece, DownloadContext::PeerInfo* peer_info, int max_count);

		virtual ~PeerPieceDownloadContext();

		DownloadContext::PeerInfo* peer_info() {
			return peer_info_;
		}
	};

} // namespace grida

#endif // __GRIDA_PEER_PIECE_DOWNLOAD_CONTEXT_HPP__
