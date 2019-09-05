/**
 * @file	bitstream.hpp
 * @author	Jichan (development@jc-lab.net / http://ablog.jc-lab.net/ )
 * @copyright Copyright (C) 2019 jichan.\n
 *            This software may be modified and distributed under the terms
 *            of the Apache License 2.0.  See the LICENSE file for details.
 */

#ifndef __GRIDA_DATA_BITSTREAM_HPP__
#define __GRIDA_DATA_BITSTREAM_HPP__

#include <string>
#include <vector>

namespace grida {

	namespace data {

		class BitStream {
		private:
			/*
			 * buf[0] = version(5bit) << 3 | pad_bits(3bit)

			 // If size=14 & pad_bits = (16 - 14) = 2
			 * buf[1] = (msb) d0 d1 d2 d3 d4 d5 d6 d7 (lsb)
			 * buf[2] = (msb) d8 d9 d10 d11 d12 d13 0(pad) 0(pad) (lsb)
			 */

			std::vector<unsigned char> buffer_;
			int bits_size_;

			int position_;

		public:
			BitStream();
			BitStream(const BitStream& ref);

			void from(const unsigned char* data, int length);
			void init(int bits_size);

			bool push(bool value);
			bool get(int position);

			int bits_size() const;

			const std::vector<unsigned char>& ref_buffer() const {
				return buffer_;
			}
		};

	} // namespace data

} // namespace grida

#endif // __GRIDA_DATA_BITSTREAM_HPP__
