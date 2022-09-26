#include <libethcore/AccessList.h>
#include <libethcore/Exceptions.h>

using namespace dev;
using namespace dev::eth;

AccessList::AccessList(RLP const &_rlp): m_keys(0)
{
    if (!_rlp.isList())
        BOOST_THROW_EXCEPTION(InvalidAccessList() << errinfo_comment("access list RLP must be a list"));

    for (std::size_t i = 0; i < _rlp.itemCount(); i++)
    {
        auto element = _rlp[i];
        if (!element.isList() || element.itemCount() != 2)
            BOOST_THROW_EXCEPTION(InvalidAccessList() << errinfo_comment("access list element RLP must be a list"));

        auto addressBytes = element[0].toBytes(RLP::VeryStrict);
        if (addressBytes.size() != 20)
            BOOST_THROW_EXCEPTION(InvalidAccessList() << errinfo_comment("address length must be 20"));

        Address address{addressBytes};

        auto storageList = element[1];
        if (!storageList.isList())
            BOOST_THROW_EXCEPTION(InvalidAccessList() << errinfo_comment("storage list RLP must be a list"));

        std::vector<u256> keys;
        for (std::size_t j = 0; j < storageList.itemCount(); j++)
        {
            auto storageBytes = storageList[j].toBytes(RLP::VeryStrict);
            if (storageBytes.size() != 32)
                BOOST_THROW_EXCEPTION(InvalidAccessList() << errinfo_comment("storage length must be 32"));

            keys.emplace_back(storageBytes);
        }

        m_keys += keys.size();
        m_list.emplace_back(std::make_pair(address, keys));
    }
}

int64_t AccessList::calculateBaseGas(EVMSchedule const &_schedule) const
{
    return _schedule.accessListAddressCost * m_list.size() + _schedule.accessListStroageKeyCost * m_keys;
}

void AccessList::streamRLP(RLPStream& _s) const
{
    _s.appendList(m_list.size());
    for (const auto& pair : m_list)
    {
        _s << pair.first;
        _s.appendList(pair.second.size());
        for (const auto& key : pair.second)
            _s << key;
    }
}