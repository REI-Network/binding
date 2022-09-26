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

using AccessListStruct = std::vector<std::pair<Address, std::vector<u256>>>;

class AccessList
{
  public:
    AccessList(): m_keys(0) {}
    AccessList(AccessListStruct&& _list);
    AccessList(RLP const &_rlp);

    /// Calculate access list base gas usage.
    int64_t calculateBaseGas(EVMSchedule const &_schedule) const;

    /// RLP encode access list.
    void streamRLP(RLPStream& _s) const;
    
private:
    int64_t m_keys;
     AccessListStruct m_list;
};

} // namespace eth
} // namespace dev