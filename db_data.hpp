/**
 * @file	db_data.hpp
 * @author	Jichan (development@jc-lab.net / http://ablog.jc-lab.net/ )
 * @copyright Copyright (C) 2019 jichan.\n
 *            This software may be modified and distributed under the terms
 *            of the Apache License 2.0.  See the LICENSE file for details.
 */

#ifndef __GRIDA_DB_DATA_HPP__
#define __GRIDA_DB_DATA_HPP__

#include <string>
#include <vector>

namespace grida {

	struct DBObjectInformation {
		std::string object_id; // Primary Key
		int status;
		std::vector<unsigned char> raw_seed_file;

		DBObjectInformation() {
			status = 0;
		}

		virtual ~DBObjectInformation() {}
	};

	struct DBPieceInformation {
		std::string object_id;
		std::string piece_id;
		int64_t file_offset;
		int piece_size;

		virtual ~DBPieceInformation() {}
	};

} // namespace grida

#endif // __GRIDA_DB_DATA_HPP__
