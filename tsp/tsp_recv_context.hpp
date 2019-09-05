#ifndef __GRIDA_TSP_TSP_RECV_CONTEXT_HPP__
#define __GRIDA_TSP_TSP_RECV_CONTEXT_HPP__

#include <vector>

namespace grida {

	namespace tsp {

		class TspRecvContext {
		public:
			bool errored_;
			std::vector<unsigned char> buffer_;

		public:
			TspRecvContext() : errored_(false) {}
			virtual ~TspRecvContext() {}

			virtual std::string remote_ip() const {
				return "";
			}

			virtual int remote_port() const {
				return -1;
			}
		};

	} // namespace tsp

} // namespace grida

#endif // __GRIDA_TSP_TSP_RECV_CONTEXT_HPP__
