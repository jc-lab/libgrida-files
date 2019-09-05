/**
 * @file	route_tracer.hpp
 * @author	Jichan (development@jc-lab.net / http://ablog.jc-lab.net/ )
 * @date	2019/07/08
 * @copyright Copyright (C) 2019 jichan.\n
 *            This software may be modified and distributed under the terms
 *            of the Apache License 2.0.  See the LICENSE file for details.
 */

#ifndef __GRIDA_SERVICE_ROUTE_TRACER_HPP__
#define __GRIDA_SERVICE_ROUTE_TRACER_HPP__

#include "../internal/use_loop.hpp"

namespace grida {
	namespace service {

		class RouteTracer : public internal::LoopUse {
		private:
			class TraceContext;

		public:
			struct TestResultItem {
				int depth;
				int result;
				std::string ip;
				int rtt;
			};

			RouteTracer(const internal::LoopProvider* provider);

			int tracert(std::unique_ptr< std::list<TestResultItem> > &result_list, const std::string& ip);
		};

	} // namespace service
} // namespace grida

#endif // __GRIDA_SERVICE_ROUTE_TRACER_HPP__
