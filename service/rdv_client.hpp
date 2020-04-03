/**
 * @file	rdv_client.hpp
 * @author	Jichan (development@jc-lab.net / http://ablog.jc-lab.net/ )
 * @copyright Copyright (C) 2019 jichan.\n
 *            This software may be modified and distributed under the terms
 *            of the Apache License 2.0.  See the LICENSE file for details.
 */

#ifndef __GRIDA_SERVICE_RDV_CLIENT_HPP__
#define __GRIDA_SERVICE_RDV_CLIENT_HPP__

#include "../internal/use_loop.hpp"
#include "../peer_context.hpp"
#include "../native_loop.hpp"

namespace grida {
	namespace service {

		class RouteTracer;

		class RdvClientWs;

		class RdvClient : public internal::LoopUse {
		private:
			class Impl;

			std::unique_ptr<Impl> impl_;

		public:
			RdvClient(const internal::LoopProvider* provider, std::shared_ptr<NativeLoop> native_loop, PeerContext* peer_context);
			~RdvClient();

			int start(RouteTracer* route_tracer);
			int stop();

			virtual bool checkUseableWebsockets() { return true; }
		};

	} // namespace service
} // namespace grida

#endif // __GRIDA_SERVICE_RDV_CLIENT_HPP__
