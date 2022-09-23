#pragma once

#include <utility>
#include <vector>

#include <libdevcore/Address.h>
#include <libdevcore/RLP.h>
#include <libethcore/EVMSchedule.h>

namespace dev
{
namespace eth
{

class AccessList
{
  public:
    AccessList(RLP const &_rlp);

    int64_t calculateBaseGas(EVMSchedule const &_schedule);

    std::vector<std::pair<Address, std::vector<u256>>> list;
    
private:
    int64_t m_keys;
};

} // namespace eth
} // namespace dev