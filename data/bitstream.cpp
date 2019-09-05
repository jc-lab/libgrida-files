/**
 * @file	bitstream.cpp
 * @author	Jichan (development@jc-lab.net / http://ablog.jc-lab.net/ )
 * @copyright Copyright (C) 2019 jichan.\n
 *            This software may be modified and distributed under the terms
 *            of the Apache License 2.0.  See the LICENSE file for details.
 */

#include "bitstream.hpp"

namespace grida {

	namespace data {

		BitStream::BitStream() : bits_size_(0), position_(0) {
		}

		BitStream::BitStream(const BitStream& ref) {
			buffer_ = ref.ref_buffer();
			bits_size_ = ref.bits_size();
			position_ = 0;
		}

		void BitStream::from(const unsigned char* data, int length)
		{
			buffer_.clear();
			buffer_.insert(buffer_.end(), data, data + length);

			if (!buffer_.empty()) {
				bits_size_ = (buffer_.size() - 1) * 8;
				bits_size_ -= buffer_[0] & 0x07;
			}
		}

		void BitStream::init(int bits_size)
		{
			int size_bytes = bits_size / 8;
			int pad_size = 0;
			int i;
			buffer_.clear();
			if (bits_size % 8) {
				size_bytes++;
				pad_size = 8 - (bits_size % 8);
			}
			buffer_.push_back(0 | pad_size);

			for (i = 0; i < size_bytes; i++) {
				buffer_.push_back(0);
			}

			bits_size_ = bits_size;
		}

		bool BitStream::push(bool value)
		{
			int pos_bytes = position_ / 8;
			int pos_bits = 7 - (position_ % 8);

			if (position_ >= bits_size_)
				return false;

			if (value) {
				buffer_[1 + pos_bytes] |= (1 << pos_bits);
			} else {
				buffer_[1 + pos_bytes] &= ~(1 << pos_bits);
			}

			position_++;

			return true;
		}

		bool BitStream::get(int position)
		{
			int pos_bytes = position / 8;
			int pos_bits = 7 - (position % 8);

			if (position < 0)
				return false;
			if (position >= bits_size_)
				return false;

			return (buffer_[1 + pos_bytes] & (1 << pos_bits)) ? true : false;
		}

		int BitStream::bits_size() const
		{
			return bits_size_;
		}

	} // namespace data

} // namespace grida

