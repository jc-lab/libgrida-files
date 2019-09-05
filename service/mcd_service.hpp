/**
 * @file	mcd_service.hpp
 * @author	Jichan (development@jc-lab.net / http://ablog.jc-lab.net/ )
 * @copyright Copyright (C) 2019 jichan.\n
 *            This software may be modified and distributed under the terms
 *            of the Apache License 2.0.  See the LICENSE file for details.
 */

#ifndef __GRIDA_SERVICE_MCD_SERVICE_HPP__
#define __GRIDA_SERVICE_MCD_SERVICE_HPP__

#include "../internal/use_loop.hpp"
#include "../peer_context.hpp"
#include "mcd_peer_handler.hpp"

#include "../object_discovery_context.hpp"

namespace grida {

	class ThreadPool;

	namespace service {

		class McdService : public internal::LoopUse {
		public:
			class Impl;

		private:
			class TspLayer;

			std::unique_ptr<Impl> impl_;

		public:
			McdService(const internal::LoopProvider* provider, PeerContext *peer_context);
			~McdService();

			int start(ThreadPool* thread_pool, McdPeerHandler* peer_handler, const std::string& multicast_addr, const std::string& interface_addr);
			int stop();

			int startDiscoveryObject(std::shared_ptr<ObjectDiscoveryContext> discovery_context);
			int stopDiscoveryObject(ObjectDiscoveryContext* download_context);
		};

	} // namespace service
} // namespace grida

#endif // __GRIDA_SERVICE_MCD_SERVICE_HPP__
