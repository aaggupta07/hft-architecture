#include "client-id-gen.hpp"

namespace strategy {
Order::ID ClientOrderIDGenerator::next_client_id_ = 1;

Order::ID ClientOrderIDGenerator::generate() {
	return next_client_id_++;
}
}