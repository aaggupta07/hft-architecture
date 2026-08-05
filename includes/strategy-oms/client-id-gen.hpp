#ifndef CLIENT_ID_GEN_HPP
#define CLIENT_ID_GEN_HPP

#include "order.hpp"

namespace strategy {
class ClientOrderIDGenerator {
private:
	static Order::ID next_client_id_;
public:
	ClientOrderIDGenerator() = delete;
	ClientOrderIDGenerator(const ClientOrderIDGenerator&) = delete;
	ClientOrderIDGenerator(ClientOrderIDGenerator&&) = delete;
	ClientOrderIDGenerator& operator=(const ClientOrderIDGenerator&) = delete;
	ClientOrderIDGenerator& operator=(ClientOrderIDGenerator&&) = delete;

	static Order::ID generate();
};
}

#endif