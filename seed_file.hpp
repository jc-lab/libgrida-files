/**
 * @file	seed_file.hpp
 * @author	Jichan (development@jc-lab.net / http://ablog.jc-lab.net/ )
 * @copyright Copyright (C) 2019 jichan.\n
 *            This software may be modified and distributed under the terms
 *            of the Apache License 2.0.  See the LICENSE file for details.
 */

#ifndef __GRIDA_SEED_FILE_HPP__
#define __GRIDA_SEED_FILE_HPP__

#include <JsBsonRPCSerializable/Serializable.h>

namespace grida {

    class SeedFile : public JsBsonRPC::Serializable {
    public:
		JsBsonRPC::SType<std::string> directory;
		JsBsonRPC::SType<std::string> file_name;
		JsBsonRPC::SType<int64_t> file_size;
		JsBsonRPC::SType<std::string> hash_algo;
		JsBsonRPC::SType<std::vector<unsigned char>> file_hash;
		JsBsonRPC::SType<int> piece_length;
		JsBsonRPC::SType<std::list<std::vector<unsigned char>>> pieces;

		SeedFile() :
			Serializable("kr.jclab.grida.data.SeedFile", 101) {
			serializableMapMember("directory", directory);
			serializableMapMember("file_name", file_name);
			serializableMapMember("file_size", file_size);
			serializableMapMember("hash_algo", hash_algo);
			serializableMapMember("file_hash", file_hash);
			serializableMapMember("piece_length", piece_length);
			serializableMapMember("pieces", pieces);
		}
    };

} // namespace grida

#endif // __GRIDA_SEED_FILE_HPP__
