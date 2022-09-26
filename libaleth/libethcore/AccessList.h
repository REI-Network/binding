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
    AccessList(): m_keys(0) {}
    AccessList(AccessListStruct const &_list);
    AccessList(RLP const &_rlp);

    /// Calculate access list base gas usage.
    int64_t calculateBaseGas(EVMSchedule const &_schedule) const;

    /// RLP encode access list.
    void streamRLP(RLPStream& _s) const;

    /// Traverse access list.
    void forEach(std::function<void(Address const&, u256s const&)> const &_cb) const;
    
private:
    int64_t m_keys;
     AccessListStruct m_list;
};

} // namespace eth
} // namespace dev