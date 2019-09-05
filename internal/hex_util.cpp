/**
 * @file	hex_util.cpp
 * @author	Jichan (development@jc-lab.net / http://ablog.jc-lab.net/ )
 * @copyright Copyright (C) 2019 jichan.\n
 *            This software may be modified and distributed under the terms
 *            of the Apache License 2.0.  See the LICENSE file for details.
 */

#include "hex_util.hpp"

namespace grida {
	namespace internal {

		std::string HexUtil::bytesToHexString(const void* buf, size_t len)
		{
			static char *HEX_STRING = "0123456789abcdef";

			std::string result;
			const unsigned char* ptr = (const unsigned char* )buf;

			while (len--) {
				result.push_back(HEX_STRING[(*ptr >> 4) & 0x0F]);
				result.push_back(HEX_STRING[(*ptr >> 0) & 0x0F]);
				ptr++;
			}

			return result;
		}

		int HexUtil::hexStringToBytes(unsigned char* buf, size_t buf_size, const char* hex_string) {
			int data_len = strlen(hex_string);
			int remaining;
			if (data_len % 2)
				return -1;
			data_len /= 2;
			if (buf_size < data_len)
				return -1;
			remaining = data_len;
			while (remaining--) {
				unsigned char b = 0;
				char d = hexToDec(*(hex_string++));
				if (d < 0)
					return -1;
				b = (d << 8) & 0xF0;
				d = hexToDec(*(hex_string++));
				if (d < 0)
					return -1;
				b |= d;
				*(buf++) = b;
			}
			return data_len;
		}

		int HexUtil::hexStringToBytes(std::vector<unsigned char>& buffer, const char* hex_string) {
			int data_len = strlen(hex_string);
			int remaining;
			if (data_len % 2)
				return -1;
			data_len /= 2;
			remaining = data_len;
			while (remaining--) {
				unsigned char b = 0;
				char d = hexToDec(*(hex_string++));
				if (d < 0)
					return -1;
				b = (d << 8) & 0xF0;
				d = hexToDec(*(hex_string++));
				if (d < 0)
					return -1;
				b |= d;
				buffer.push_back(b);
			}
			return data_len;
		}

	} // namespace internal
} // namespace grida

