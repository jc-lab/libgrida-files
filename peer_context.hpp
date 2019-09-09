/**
 * @file	peer_context.hpp
 * @author	Jichan (development@jc-lab.net / http://ablog.jc-lab.net/ )
 * @copyright Copyright (C) 2019 jichan.\n
 *            This software may be modified and distributed under the terms
 *            of the Apache License 2.0.  See the LICENSE file for details.
 */

#ifndef __GRIDA_PEER_CONTEXT_HPP__
#define __GRIDA_PEER_CONTEXT_HPP__

#include <string>

#include "tsp/tsp_recv_context.hpp"
#include "tsp/pske_protocol.hpp"

#include "seed_file.hpp"

#include "file_handle.hpp"

#include "db_data.hpp"

namespace grida {

	class PieceDownloadContext;

	class PeerContext {
	public:
		/* MultiCast Discovery Communication Layer */
		virtual std::unique_ptr<tsp::TspRecvContext> createMcdTspRecvContext(const std::string& remote_ip, int remote_port) = 0;
		virtual std::unique_ptr<tsp::PskeProtocol> createMcdPskeProtocol() = 0;

		/* Database Layer */
		/* Return code : Success(0), NotExists(2), Failed(otherwise) */
		virtual int dbSetObjectInformation(/* in/out */ std::unique_ptr<DBObjectInformation> &row, const SeedFile& seed_file, void *user_object_ctx) = 0;
		virtual int dbGetObjectInformation(/* out */ std::unique_ptr<DBObjectInformation> &row, const std::string& object_id, void *user_object_ctx) = 0;

		virtual int dbSetPieceInformation(/* in */ const DBPieceInformation& row, void *user_object_ctx) = 0;
		virtual int dbGetPieceInformation(/* out */ std::unique_ptr<DBPieceInformation>& row, const std::string& object_id, const std::string& piece_id, void* user_object_ctx) = 0;
		virtual int dbGetAllPieceInformation(std::list<std::unique_ptr<DBPieceInformation>> &rows, const std::string& object_id, void *user_object_ctx) = 0;

		/* Filesystem Layer */
		virtual int fileOpen(std::unique_ptr<FileHandle>& file_handle, const DBObjectInformation* row, FileHandle::OpenMode mode, const SeedFile* seed_file, std::shared_ptr<void> user_object_ctx) = 0;

		/**
		 * @return
		 *   0 : Not support
		 * > 0 : Number of instances available at the same time per object.
		 */
		virtual int availCustomDownloader() const = 0;

		virtual int beginCustomDownload(const grida::DBObjectInformation *object_row, const std::string& piece_id, std::shared_ptr<PieceDownloadContext> piece_download_ctx, std::shared_ptr<void> user_object_ctx) = 0;
	};

} // namespace grida

#endif // __GRIDA_PEER_CONTEXT_HPP__
