/**
 * @file	hex_util.hpp
 * @author	Jichan (development@jc-lab.net / http://ablog.jc-lab.net/ )
 * @copyright Copyright (C) 2019 jichan.\n
 *            This software may be modified and distributed under the terms
 *            of the Apache License 2.0.  See the LICENSE file for details.
 */

#ifndef __GRIDA_INTERNAL_HEX_UTIL_HPP__
#define __GRIDA_INTERNAL_HEX_UTIL_HPP__

#include <string>
#include <vector>

namespace grida {
	namespace internal {

		class HexUtil {
		public:
			static char hexToDec(char c) {
				if ((c >= '0') && (c <= '9')) {
					return (c - '0');
				}else if ((c >= 'a') && (c <= 'f')) {
					return (c - 'a') + 10;
				}else if ((c >= 'A') && (c <= 'F')) {
					return (c - 'A') + 10;
				}
				return -1;
			}
			static std::string bytesToHexString(const void* buf, size_t len);
			static int hexStringToBytes(unsigned char* buf, size_t buf_size, const char* hex_string);
			static int hexStringToBytes(std::vector<unsigned char> &buffer, const char* hex_string);
		};

	} // namespace internal
} // namespace grida

#endif // __GRIDA_INTERNAL_HEX_UTIL_HPP__
