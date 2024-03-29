// Aleth: Ethereum C++ client, tools and libraries.
// Copyright 2014-2019 Aleth Authors.
// Licensed under the GNU General Public License, Version 3.


#include "Account.h"
#include "SecureTrieDB.h"
#include "ValidationSchemes.h"
#include <libdevcore/JsonUtils.h>
#include <libdevcore/OverlayDB.h>
#include <libethcore/ChainOperationParams.h>

using namespace std;
using namespace dev;
using namespace dev::eth;
using namespace dev::eth::validation;

namespace fs = boost::filesystem;

const uint64_t StakeInfo::recoverInterval = 86400;

StakeInfo::StakeInfo(RLP const& _rlp) :
    m_total(_rlp[0].toInt<u256>()),
    m_usage(_rlp[1].toInt<u256>()),
    m_timestampBytes(_rlp[2].toBytes())
{
    m_timestamp = u256(m_timestampBytes).convert_to<uint64_t>();
}

u256 StakeInfo::estimateFee(uint64_t timestamp, u256 totalAmount, u256 dailyFee) const
{
    auto const usage = estimateUsage(timestamp);
    auto const fee = estimateTotalFee(totalAmount, dailyFee);
    if (fee > usage)
        return fee - usage;
    else
        return 0;
}

u256 StakeInfo::estimateTotalFee(u256 totalAmount, u256 dailyFee) const
{
    if (totalAmount == 0)
        return 0;
    else
        return m_total * dailyFee / totalAmount;
}

u256 StakeInfo::estimateUsage(uint64_t timestamp) const
{
    if (timestamp <= m_timestamp)
        return m_usage;

    const uint64_t interval = timestamp - m_timestamp;
    if (m_usage > 0 && interval < StakeInfo::recoverInterval)
        return m_usage * (StakeInfo::recoverInterval - interval) / StakeInfo::recoverInterval;
    else
        return 0;
}

void Account::setCode(bytes&& _code, u256 const& _version)
{
    auto const newHash = sha3(_code);
    if (newHash != m_codeHash)
    {
        // code was updated
        if (m_codeHash != EmptySHA3)
            changed();

        m_codeCache = std::move(_code);
        m_hasNewCode = true;
        m_codeHash = newHash;
    }
    m_version = _version;
}

void Account::resetCode()
{
    m_codeCache.clear();
    m_hasNewCode = false;
    m_codeHash = EmptySHA3;
    // Reset the version, as it was set together with code
    m_version = 0;
}

u256 Account::originalStorageValue(u256 const& _key, OverlayDB const& _db) const
{
    auto it = m_storageOriginal.find(_key);
    if (it != m_storageOriginal.end())
        return it->second;

    // Not in the original values cache - go to the DB.
    SecureTrieDB<h256, OverlayDB> const memdb(const_cast<OverlayDB*>(&_db), m_storageRoot);
    std::string const payload = memdb.at(_key);
    auto const value = payload.size() ? RLP(payload).toInt<u256>() : 0;
    m_storageOriginal[_key] = value;
    return value;
}

namespace js = json_spirit;

// TODO move AccountMaskObj to libtesteth (it is used only in test logic)
AccountMap dev::eth::jsonToAccountMap(std::string const& _json, u256 const& _defaultNonce,
    AccountMaskMap* o_mask, const fs::path& _configPath)
{
    auto u256Safe = [](std::string const& s) -> u256 {
        bigint ret(s);
        if (ret >= bigint(1) << 256)
            BOOST_THROW_EXCEPTION(
                ValueTooLarge() << errinfo_comment("State value is equal or greater than 2**256"));
        return (u256)ret;
    };

    std::unordered_map<Address, Account> ret;

    js::mValue val;
    json_spirit::read_string_or_throw(_json, val);

    for (auto const& account : val.get_obj())
    {
        Address a(fromHex(account.first));
        auto const& accountMaskJson = account.second.get_obj();

        bool haveBalance = (accountMaskJson.count(c_wei) || accountMaskJson.count(c_finney) ||
                            accountMaskJson.count(c_balance));
        bool haveNonce = accountMaskJson.count(c_nonce);
        bool haveCode = accountMaskJson.count(c_code) || accountMaskJson.count(c_codeFromFile);
        bool haveStorage = accountMaskJson.count(c_storage);
        bool shouldNotExists = accountMaskJson.count(c_shouldnotexist);

        if (haveStorage || haveCode || haveNonce || haveBalance)
        {
            u256 balance = 0;
            if (accountMaskJson.count(c_wei))
                balance = u256Safe(accountMaskJson.at(c_wei).get_str());
            else if (accountMaskJson.count(c_finney))
                balance = u256Safe(accountMaskJson.at(c_finney).get_str()) * finney;
            else if (accountMaskJson.count(c_balance))
                balance = u256Safe(accountMaskJson.at(c_balance).get_str());

            u256 nonce =
                haveNonce ? u256Safe(accountMaskJson.at(c_nonce).get_str()) : _defaultNonce;

            ret[a] = Account(nonce, balance);
            auto codeIt = accountMaskJson.find(c_code);
            if (codeIt != accountMaskJson.end())
            {
                auto& codeObj = codeIt->second;
                if (codeObj.type() == json_spirit::str_type)
                {
                    auto& codeStr = codeObj.get_str();
                    if (codeStr.find("0x") != 0 && !codeStr.empty())
                        cerr << "Error importing code of account " << a
                             << "! Code needs to be hex bytecode prefixed by \"0x\".";
                    else
                        ret[a].setCode(fromHex(codeStr), 0);
                }
                else
                    cerr << "Error importing code of account " << a
                         << "! Code field needs to be a string";
            }

            auto codePathIt = accountMaskJson.find(c_codeFromFile);
            if (codePathIt != accountMaskJson.end())
            {
                auto& codePathObj = codePathIt->second;
                if (codePathObj.type() == json_spirit::str_type)
                {
                    fs::path codePath{codePathObj.get_str()};
                    if (codePath.is_relative())  // Append config dir if code file path is relative.
                        codePath = _configPath.parent_path() / codePath;
                    bytes code = contents(codePath);
                    if (code.empty())
                        cerr << "Error importing code of account " << a << "! Code file "
                             << codePath << " empty or does not exist.\n";
                    ret[a].setCode(std::move(code), 0);
                }
                else
                    cerr << "Error importing code of account " << a
                         << "! Code file path must be a string\n";
            }

            if (haveStorage)
                for (pair<string, js::mValue> const& j : accountMaskJson.at(c_storage).get_obj())
                    ret[a].setStorage(u256(j.first), u256(j.second.get_str()));
        }

        if (o_mask)
        {
            (*o_mask)[a] =
                AccountMask(haveBalance, haveNonce, haveCode, haveStorage, shouldNotExists);
            if (!haveStorage && !haveCode && !haveNonce && !haveBalance &&
                shouldNotExists)  // defined only shouldNotExists field
                ret[a] = Account(0, 0);
        }
    }

    return ret;
}
