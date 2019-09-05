/**
 * @file	mcd_peer_handler.cpp
 * @author	Jichan (development@jc-lab.net / http://ablog.jc-lab.net/ )
 * @date	2019/08/16
 * @copyright Copyright (C) 2019 jichan.\n
 *            This software may be modified and distributed under the terms
 *            of the Apache License 2.0.  See the LICENSE file for details.
 */

#include "mcd_peer_handler.hpp"
#include "mcd_service_impl.hpp"

namespace grida {
    namespace service {

		void McdPeerHandler::sendMcdPayload(mcd::McdPayload* payload, tsp::PskeFlags pske_flags, const std::vector<std::unique_ptr<grida::tsp::Payload>>* ancestors) {
			McdService::Impl* impl = (McdService::Impl*)impl_;
			impl->sendMcdPayload(payload, pske_flags, ancestors);
		}

    }
}
