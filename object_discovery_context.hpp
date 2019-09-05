/**
 * @file	object_discovery_context.hpp
 * @author	Jichan (development@jc-lab.net / http://ablog.jc-lab.net/ )
 * @date	2019/08/29
 * @copyright Copyright (C) 2019 jichan.\n
 *            This software may be modified and distributed under the terms
 *            of the Apache License 2.0.  See the LICENSE file for details.
 */

#ifndef __GRIDA_DISCOVERY_CONTEXT_HPP__
#define __GRIDA_DISCOVERY_CONTEXT_HPP__

#include "service/mcd/mcd_protocol.hpp"

namespace grida {

    class ObjectDiscoveryContext {
	protected:
		std::string object_id_;

	public:
		std::string object_id() const {
			return object_id_;
		}

		virtual void onObjectDiscovered(tsp::SocketPayload* socket_payload, service::mcd::McdObjectDiscoveryResponsePayload *response_payload) = 0;
    };

}

#endif // __GRIDA_DISCOVERY_CONTEXT_HPP__
