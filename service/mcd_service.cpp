/**
 * @file	mcd_service.cpp
 * @author	Jichan (development@jc-lab.net / http://ablog.jc-lab.net/ )
 * @copyright Copyright (C) 2019 jichan.\n
 *            This software may be modified and distributed under the terms
 *            of the Apache License 2.0.  See the LICENSE file for details.
 */

#include "mcd_service.hpp"
#include "mcd_service_impl.hpp"

namespace grida {

	namespace service {

		McdService::McdService(const internal::LoopProvider* provider, PeerContext* peer_context)
			: LoopUse(provider), impl_(Impl::create(peer_context, provider))
		{
		}

		McdService::~McdService()
		{

		}

		int McdService::start(ThreadPool* thread_pool, const std::string& multicast_addr, const std::string& interface_addr)
		{
			return impl_->start(thread_pool, multicast_addr, interface_addr);
		}

		int McdService::stop()
		{
			return impl_->stop();
		}

		int McdService::startDiscoveryObject(std::shared_ptr<ObjectDiscoveryContext> discovery_context)
		{
			return impl_->startDiscoveryObject(discovery_context);
		}
		int McdService::stopDiscoveryObject(ObjectDiscoveryContext* discovery_context)
		{
			return impl_->stopDiscoveryObject(discovery_context);
		}


	} // namespace service
} // namespace grida

