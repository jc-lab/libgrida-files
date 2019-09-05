/**
 * @file	mcd_peer_handler.hpp
 * @author	Jichan (development@jc-lab.net / http://ablog.jc-lab.net/ )
 * @date	2019/08/16
 * @copyright Copyright (C) 2019 jichan.\n
 *            This software may be modified and distributed under the terms
 *            of the Apache License 2.0.  See the LICENSE file for details.
 */

#ifndef __GRIDA_SERVICE_MCD_PEER_HANDLER_HPP__
#define __GRIDA_SERVICE_MCD_PEER_HANDLER_HPP__

#include <memory>

#include "../tsp/tsp_recv_context.hpp"
#include "../tsp/pske_protocol.hpp"

#include "mcd/mcd_protocol.hpp"

namespace grida {
    namespace service {

        class McdPeerHandler {
        private:
			friend class McdService;

            void *impl_;

            void attachServiceImpl(void *impl) {
                impl_ = impl;
            }

		public:
			void sendMcdPayload(mcd::McdPayload *payload, tsp::PskeFlags pske_flags, const std::vector<std::unique_ptr<grida::tsp::Payload>>* ancestors);

        public:
            virtual std::unique_ptr<tsp::TspRecvContext> createTspRecvContext(const std::string& remote_ip, int remote_port) = 0;
			virtual std::unique_ptr<tsp::PskeProtocol> createPskeProtocol() = 0;
        };

    }
}

#endif // __GRIDA_SERVICE_MCD_PEER_HANDLER_HPP__
