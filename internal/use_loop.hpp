/**
 * @file	use_loop.hpp
 * @author	Jichan (development@jc-lab.net / http://ablog.jc-lab.net/ )
 * @copyright Copyright (C) 2019 jichan.\n
 *            This software may be modified and distributed under the terms
 *            of the Apache License 2.0.  See the LICENSE file for details.
 */

#ifndef __GRIDA_INTERNAL_USE_LOOP_HPP__
#define __GRIDA_INTERNAL_USE_LOOP_HPP__

#include <uvw/loop.hpp>

namespace grida {
	namespace internal {

		class LoopProvider {
		public:
			virtual std::shared_ptr<::uvw::Loop> get_loop() const = 0;
		};

		class LoopUse {
		protected:
			std::shared_ptr<::uvw::Loop> loop_;

		protected:
			LoopUse(const std::shared_ptr<::uvw::Loop>& loop)
				: loop_(loop)
			{}

			LoopUse(const LoopProvider& provider)
				: loop_(provider.get_loop())
			{}

			LoopUse(const LoopProvider* provider)
				: loop_(provider->get_loop())
			{}

		};

	} // namespace internal
} // namespace grida

#endif // __GRIDA_INTERNAL_USE_LOOP_HPP__
