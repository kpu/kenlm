#include "bounded/client.hh"

namespace bounded {

Client::Client(Manager &manager) : manager_(manager) {}

Client::~Client() {}

SpillClient::SpillClient(Manager &manager) : Client(manager) {}

SpillClient::~SpillClient() {}

} // namespace bounded
