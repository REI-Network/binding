#include <functional>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

#include <boost/optional.hpp>

#include <napi.h>

#include <libethereum/ChainParams.h>
#include <libethereum/Executive.h>
#include <libethereum/LastBlockHashesFace.h>
#include <libethereum/State.h>
#include <libethereum/Transaction.h>
#include <libethereum/TransactionReceipt.h>

#include <libethcore/LogEntry.h>
#include <libethcore/SealEngine.h>
#include <libethcore/TransactionBase.h>

#include <libdevcore/DBFactory.h>
#include <libdevcore/Log.h>
#include <libdevcore/OverlayDB.h>
#include <libdevcore/RLP.h>

#include <libethashseal/Ethash.h>
#include <libethashseal/GenesisInfo.h>

using namespace dev;
using namespace dev::db;
using namespace dev::eth;

using Buffer = Napi::Buffer<unsigned char>;
using External = Napi::External<void>;

using LastBlockHashesLoader = std::function<h256s()>;
using GenesisInfo = std::vector<std::pair<Address, u256>>;

template <class T> Napi::Value toNapiValue(Napi::Env env, const T &t);

Napi::Value toNapiValue(Napi::Env env, const h256 &h)
{
    return Napi::String::New(env, "0x" + toHex(h));
}

Napi::Value toNapiValue(Napi::Env env, const h2048 &h)
{
    return Napi::String::New(env, "0x" + toHex(h));
}

Napi::Value toNapiValue(Napi::Env env, const bytes &bs)
{
    return Napi::String::New(env, "0x" + toHex(bs));
}

Napi::Value toNapiValue(Napi::Env env, const u256 &u)
{
    return Napi::String::New(env, "0x" + u.str(16));
}

Napi::Value toNapiValue(Napi::Env env, const uint8_t &i)
{
    return Napi::Number::New(env, i);
}

Napi::Value toNapiValue(Napi::Env env, const Address &a)
{
    return Napi::String::New(env, "0x" + a.hex());
}

Napi::Value toNapiValue(Napi::Env env, const h256s &_hs)
{
    auto hs = Napi::Array::New(env, _hs.size());
    for (std::size_t i = 0; i < _hs.size(); i++)
    {
        hs.Set(i, toNapiValue(env, _hs[i]));
    }
    return hs;
}

Napi::Value toNapiValue(Napi::Env env, const TransactionException &te)
{
    if (te == TransactionException::None)
    {
        return env.Undefined();
    }

    auto error = Napi::Object::New(env);
    switch (te)
    {
    case TransactionException::BadRLP:
        error.Set("error", Napi::String::New(env, "bad RLP"));
        break;
    case TransactionException::InvalidFormat:
        error.Set("error", Napi::String::New(env, "invalid format"));
        break;
    case TransactionException::OutOfGasIntrinsic:
        error.Set("error", Napi::String::New(env, "out of gas intrinsic"));
        break;
    case TransactionException::InvalidSignature:
        error.Set("error", Napi::String::New(env, "invalid signature"));
        break;
    case TransactionException::InvalidNonce:
        error.Set("error", Napi::String::New(env, "invalid nonce"));
        break;
    case TransactionException::NotEnoughCash:
        error.Set("error", Napi::String::New(env, "not enough cash"));
        break;
    case TransactionException::OutOfGasBase:
        error.Set("error", Napi::String::New(env, "out of gas base"));
        break;
    case TransactionException::BlockGasLimitReached:
        error.Set("error", Napi::String::New(env, "block gas limit reached"));
        break;
    case TransactionException::BadInstruction:
        error.Set("error", Napi::String::New(env, "bad instruction"));
        break;
    case TransactionException::BadJumpDestination:
        error.Set("error", Napi::String::New(env, "bad jump destination"));
        break;
    case TransactionException::OutOfGas:
        error.Set("error", Napi::String::New(env, "out of gas"));
        break;
    case TransactionException::OutOfStack:
        error.Set("error", Napi::String::New(env, "out of stack"));
        break;
    case TransactionException::StackUnderflow:
        error.Set("error", Napi::String::New(env, "stack under flow"));
        break;
    case TransactionException::RevertInstruction:
        error.Set("error", Napi::String::New(env, "revert instruction"));
        break;
    case TransactionException::InvalidZeroSignatureFormat:
        error.Set("error", Napi::String::New(env, "invalid zero signature format"));
        break;
    case TransactionException::AddressAlreadyUsed:
        error.Set("error", Napi::String::New(env, "address already used"));
        break;
    case TransactionException::Unknown:
    default:
        error.Set("error", Napi::String::New(env, "unknown"));
        break;
    }
    return error;
}

Napi::Value toNapiValue(Napi::Env env, const ExecutionResult &er)
{
    auto result = Napi::Object::New(env);
    result.Set("gasUsed", toNapiValue(env, er.gasUsed));
    result.Set("excepted", toNapiValue(env, er.excepted));
    result.Set("newAddress", er.newAddress != ZeroAddress ? toNapiValue(env, er.newAddress) : env.Undefined());
    result.Set("output", toNapiValue(env, er.output));
    result.Set("gasRefunded", toNapiValue(env, er.gasRefunded));
    return result;
}

Napi::Value toNapiValue(Napi::Env env, const LogEntry &_log)
{
    auto log = Napi::Object::New(env);
    log.Set("address", toNapiValue(env, _log.address));
    log.Set("topics", toNapiValue(env, _log.topics));
    log.Set("data", toNapiValue(env, _log.data));
    return log;
}

Napi::Value toNapiValue(Napi::Env env, const LogEntries &_logs)
{
    auto logs = Napi::Array::New(env, _logs.size());
    for (std::size_t i = 0; i < _logs.size(); i++)
    {
        logs.Set(i, toNapiValue(env, _logs[i]));
    }
    return logs;
}

Napi::Value toNapiValue(Napi::Env env, const TransactionReceipt &_receipt)
{
    auto receipt = Napi::Object::New(env);
    receipt.Set("logs", toNapiValue(env, _receipt.log()));
    receipt.Set("bloom", toNapiValue(env, _receipt.bloom()));
    receipt.Set("cumulativeGasUsed", toNapiValue(env, _receipt.cumulativeGasUsed()));
    if (_receipt.hasStatusCode())
    {
        receipt.Set("status", toNapiValue(env, _receipt.statusCode()));
    }
    else
    {
        receipt.Set("stateRoot", toNapiValue(env, _receipt.stateRoot()));
    }
    return receipt;
}

std::string toString(const Napi::Value &value)
{
    if (!value.IsString())
    {
        Napi::TypeError::New(value.Env(), "Wrong arguments").ThrowAsJavaScriptException();
        return std::string{};
    }

    return value.As<Napi::String>();
}

u256 toU256(const Napi::Value &value, std::optional<u256> defaultValue = {})
{
    if (value.IsString())
    {
        return u256{fromHex(value.As<Napi::String>())};
    }
    else if (value.IsNumber())
    {
        return u256(value.As<Napi::Number>().Uint32Value());
    }
    else if ((value.IsUndefined() || value.IsNull()) && defaultValue.has_value())
    {
        return *defaultValue;
    }
    else
    {
        Napi::TypeError::New(value.Env(), "Wrong arguments").ThrowAsJavaScriptException();
        return 0;
    }
}

u256 toFixed32U256(const Napi::Value &value)
{
    if (!value.IsString())
    {
        Napi::TypeError::New(value.Env(), "Wrong arguments").ThrowAsJavaScriptException();
        return 0;
    }

    auto bytes = fromHex(value.As<Napi::String>());

    if (bytes.size() != 32)
    {
        Napi::TypeError::New(value.Env(), "Wrong arguments").ThrowAsJavaScriptException();
        return 0;
    }

    return u256{bytes};
}

u256s toFixed32U256s(const Napi::Value &value)
{
    if (!value.IsArray())
    {
        Napi::TypeError::New(value.Env(), "Wrong arguments").ThrowAsJavaScriptException();
        return u256s{};
    }

    auto array = value.As<Napi::Array>();

    u256s result;
    for (std::size_t i = 0; i < array.Length(); i++)
    {
        result.emplace_back(toFixed32U256(array.Get(i)));
    }
    return result;
}

h256 toH256(const Napi::Value &value, std::optional<h256> defaultValue = {})
{
    if (value.IsString())
    {
        return h256(fromHex(value.As<Napi::String>()));
    }
    else if (value.IsBuffer())
    {
        auto buffer = value.As<Buffer>();
        return h256(bytesConstRef(buffer.Data(), buffer.Length()));
    }
    else if ((value.IsUndefined() || value.IsNull()) && defaultValue.has_value())
    {
        return *defaultValue;
    }
    else
    {
        Napi::TypeError::New(value.Env(), "Wrong arguments").ThrowAsJavaScriptException();
        return h256{};
    }
}

h2048 toH2048(const Napi::Value &value, std::optional<h2048> defaultValue = {})
{
    if (value.IsString())
    {
        return h2048(fromHex(value.As<Napi::String>()));
    }
    else if (value.IsBuffer())
    {
        auto buffer = value.As<Buffer>();
        return h2048(bytesConstRef(buffer.Data(), buffer.Length()));
    }
    else if ((value.IsUndefined() || value.IsNull()) && defaultValue.has_value())
    {
        return *defaultValue;
    }
    else
    {
        Napi::TypeError::New(value.Env(), "Wrong arguments").ThrowAsJavaScriptException();
        return h2048{};
    }
}

bytes toBytes(const Napi::Value &value, std::optional<bytes> defaultValue = {})
{
    if (value.IsString())
    {
        return fromHex(value.As<Napi::String>());
    }
    else if (value.IsBuffer())
    {
        auto buffer = value.As<Buffer>();
        return bytes(buffer.Data(), buffer.Data() + buffer.Length());
    }
    else if ((value.IsUndefined() || value.IsNull()) && defaultValue.has_value())
    {
        return *defaultValue;
    }
    else
    {
        Napi::TypeError::New(value.Env(), "Wrong arguments").ThrowAsJavaScriptException();
        return bytes{};
    }
}

bytesConstRef toBytesConstRef(const Napi::Value &value, std::optional<bytesConstRef> defaultValue = {})
{
    if (value.IsBuffer())
    {
        auto buffer = value.As<Buffer>();
        return bytesConstRef(buffer.Data(), buffer.Length());
    }
    else if ((value.IsUndefined() || value.IsNull()) && defaultValue.has_value())
    {
        return *defaultValue;
    }
    else
    {
        Napi::TypeError::New(value.Env(), "Wrong arguments").ThrowAsJavaScriptException();
        return bytesConstRef{};
    }
}

Address toAddress(const Napi::Value &value, std::optional<Address> defaultValue = {})
{
    if (value.IsString())
    {
        return Address(value.As<Napi::String>());
    }
    else if ((value.IsUndefined() || value.IsNull()) && defaultValue.has_value())
    {
        return *defaultValue;
    }
    else
    {
        Napi::TypeError::New(value.Env(), "Wrong arguments").ThrowAsJavaScriptException();
        return Address{};
    }
}

uint32_t toUint32(const Napi::Value &value, std::optional<uint32_t> defaultValue = {})
{
    if (value.IsNumber())
    {
        return value.As<Napi::Number>().Uint32Value();
    }
    else if ((value.IsUndefined() || value.IsNull()) && defaultValue.has_value())
    {
        return *defaultValue;
    }
    else
    {
        Napi::TypeError::New(value.Env(), "Wrong arguments").ThrowAsJavaScriptException();
        return 0;
    }
}

void *toExternalPointer(const Napi::Value &value)
{
    if (!value.IsExternal())
    {
        Napi::TypeError::New(value.Env(), "Wrong arguments").ThrowAsJavaScriptException();
        return nullptr;
    }

    return value.As<External>().Data();
}

h256s toH256s(const Napi::Value &value)
{
    h256s hashes;

    if (!value.IsArray())
    {
        Napi::TypeError::New(value.Env(), "Wrong arguments").ThrowAsJavaScriptException();
        return hashes;
    }

    auto array = value.As<Napi::Array>();

    for (std::size_t i = 0; i < array.Length(); i++)
    {
        hashes.emplace_back(toH256(array.Get(i)));
    }

    return hashes;
}

LastBlockHashesLoader toLoader(const Napi::Value &value)
{
    if (!value.IsFunction())
    {
        Napi::TypeError::New(value.Env(), "Wrong arguments").ThrowAsJavaScriptException();
        return 0;
    }

    return [func = value.As<Napi::Function>()]() { return toH256s(func.Call({})); };
}

AccessListStruct toAccessList(const Napi::Value &value)
{
    AccessListStruct accessList;
    if (!value.IsArray())
    {
        Napi::TypeError::New(value.Env(), "Wrong arguments").ThrowAsJavaScriptException();
        return accessList;
    }

    auto array = value.As<Napi::Array>();

    for (std::size_t i = 0; i < array.Length(); i++)
    {
        auto items = array.Get(i);
        if (!items.IsArray())
        {
            Napi::TypeError::New(value.Env(), "Wrong arguments").ThrowAsJavaScriptException();
            return accessList;
        }

        auto itemsArray = items.As<Napi::Array>();
        if (itemsArray.Length() != 2)
        {
            Napi::TypeError::New(value.Env(), "Wrong arguments").ThrowAsJavaScriptException();
            return accessList;
        }

        accessList.emplace_back(
            std::make_pair(toAddress(itemsArray.Get((uint32_t)0)), toFixed32U256s(itemsArray.Get((uint32_t)1))));
    }

    return accessList;
}

Transaction toTx(const Napi::Value &input)
{
    if (input.IsBuffer())
    {
        // decode as raw tx
        return Transaction(toBytesConstRef(input), CheckTransaction::Everything);
    }
    else if (input.IsObject())
    {
        // decode as object
        boost::optional<AccessListStruct> accessList;
        boost::optional<uint64_t> chainID;
        auto obj = input.As<Napi::Object>();
        auto value = toU256(obj.Get("value"), 0);
        auto gasPrice = toU256(obj.Get("gasPrice"), 0);
        auto gas = toU256(obj.Get("gas"), 0xffffff);
        auto data = toBytes(obj.Get("data"), bytes{});
        auto nonce = toU256(obj.Get("nonce"), 0);
        auto from = toAddress(obj.Get("from"), ZeroAddress);
        auto accessListValue = obj.Get("accessList");
        if (!accessListValue.IsUndefined() && !accessListValue.IsNull())
            accessList = toAccessList(accessListValue);
        auto chainIDValue = obj.Get("chainID");
        if (!chainIDValue.IsUndefined() && !chainIDValue.IsNull())
            chainID = toUint32(chainIDValue);
        auto destValue = obj.Get("to");
        if (!destValue.IsUndefined() && !destValue.IsNull())
        {
            auto dest = toAddress(destValue);
            Transaction tx(value, gasPrice, gas, dest, data, nonce, accessList, chainID);
            tx.forceSender(from);
            return tx;
        }
        else
        {
            Transaction tx(value, gasPrice, gas, data, nonce, accessList, chainID);
            tx.forceSender(from);
            return tx;
        }
    }
    else
    {
        Napi::TypeError::New(input.Env(), "Wrong arguments").ThrowAsJavaScriptException();
        return Transaction{};
    }
}

BlockHeader toHeader(const Napi::Value &value)
{
    if (value.IsBuffer())
    {
        // decode as raw header
        return BlockHeader(toBytesConstRef(value), BlockDataType::HeaderData);
    }
    else if (value.IsObject())
    {
        // decode as object
        auto obj = value.As<Napi::Object>();
        BlockHeader header;
        header.setParentHash(toH256(obj.Get("parentHash"), h256{}));
        header.setTimestamp(toUint32(obj.Get("timestamp"), 0));
        header.setAuthor(toAddress(obj.Get("author"), ZeroAddress));
        header.setRoots(toH256(obj.Get("transactionsRoot"), EmptyListSHA3),
                        toH256(obj.Get("receiptsRoot"), EmptyListSHA3), toH256(obj.Get("sha3Uncles"), EmptyListSHA3),
                        toH256(obj.Get("stateRoot"), EmptyTrie));
        header.setGasUsed(toU256(obj.Get("gasUsed"), 0));
        header.setNumber(toUint32(obj.Get("number"), 0));
        header.setGasLimit(toU256(obj.Get("gasLimit"), 0xffffff));
        header.setExtraData(toBytes(obj.Get("extraData"), bytes{}));
        header.setLogBloom(toH2048(obj.Get("logBloom"), h2048{}));
        header.setDifficulty(toU256(obj.Get("difficulty"), 0));
        return header;
    }
    else
    {
        Napi::TypeError::New(value.Env(), "Wrong arguments").ThrowAsJavaScriptException();
        return BlockHeader{};
    }
}

class LastBlockHashes : public LastBlockHashesFace
{
  public:
    LastBlockHashes(LastBlockHashesLoader loader) : m_loader(loader)
    {
    }

    virtual h256s precedingHashes(h256 const &_mostRecentHash) const final
    {
        return m_loader();
    }

    virtual void clear() final
    {
        // do nothing...
    }

  private:
    LastBlockHashesLoader m_loader;
};

// global chain params cache
std::unordered_map<Network, ChainParams> g_chainParams;

/**
 * Load chain params for network
 * @param network - Network id
 * @return Chain params
 */
ChainParams &loadChainParam(Network network)
{
    auto itr = g_chainParams.find(network);
    if (itr != g_chainParams.end())
    {
        return itr->second;
    }

    auto [insertedItr, tookPlace] =
        g_chainParams.insert(std::make_pair(network, ChainParams(genesisInfo(network), genesisStateRoot(network))));
    return insertedItr->second;
}

/**
 * C++ implementation of EVM binding.
 */
class EVMBinding
{
  public:
    /**
     * Construct a new EVMBinding object.
     * @param db - Level db object
     * @param network - Network id
     */
    EVMBinding(void *db, Network network)
        : m_db(DBFactory::create(db)), m_params(loadChainParam(network)), m_engine(m_params.createSealEngine())
    {
    }

    /**
     * Get chain id.
     * @return Chain id
     */
    int chainID()
    {
        return m_params.chainID;
    }

    /**
     * Force set hardfork by name
     * @param hardfork - Hardfork name
     */
    void setHardfork(const std::string &hardfork)
    {
        m_engine->setEvmSchedule(hardfork);
    }

    /**
     * Reset hardfork
     */
    void resetHardfork()
    {
        m_engine->resetEvmSchedule();
    }

    /**
     * Initialize genesis state.
     * @param info - Genesis information
     * @return State root hash
     */
    h256 genesis(const GenesisInfo &info)
    {
        if (m_state.get() != nullptr)
        {
            throw std::runtime_error("state already exists");
        }

        createStateIfNotExsits(&info);

        return m_state->rootHash();
    }

    /**
     * Execute transaction.
     * @param stateRoot - Previous state root hash
     * @param header - Block header
     * @param tx - Transaction
     * @param gasUsed - Gas used
     * @param loader - A function used to load block hash
     * @return New state root, execution result and transaction receipt
     */
    auto runTx(const h256 &stateRoot, const BlockHeader &header, const Transaction &tx, const u256 &gasUsed,
               LastBlockHashes loader)
    {
        return run(stateRoot, header, tx, gasUsed, loader, Permanence::Committed);
    }

    /**
     * Execute call.
     * @param stateRoot - Previous state root hash
     * @param header - Block header
     * @param tx - Transaction
     * @param gasUsed - Gas used
     * @param loader - A function used to load block hash
     * @return Contract output
     */
    auto runCall(const h256 &stateRoot, const BlockHeader &header, const Transaction &tx, const u256 &gasUsed,
                 LastBlockHashes loader)
    {
        auto [newStateRoot, result, receipt] = run(stateRoot, header, tx, gasUsed, loader, Permanence::Reverted);
        return result.output;
    }

  private:
    /**
     * Create a state if it doesn't exsit.
     * @param info - Genesis info
     */
    void createStateIfNotExsits(const GenesisInfo *info = nullptr)
    {
        if (m_state.get() != nullptr)
        {
            return;
        }

        if (info != nullptr)
        {
            // init genesis state
            m_state = std::make_shared<State>(0, m_db, BaseState::Empty);
            for (const auto &pair : *info)
            {
                m_state->addBalance(pair.first, pair.second);
            }
            m_state->commit(State::CommitBehaviour::KeepEmptyAccounts);
            // commit data to db
            m_state->db().commit();
        }
        else
        {
            m_state = std::make_shared<State>(0, m_db, BaseState::PreExisting);
        }
    }

    /**
     * Run vm.
     * @param stateRoot - Previous state root hash
     * @param header - Block header
     * @param tx - Transaction
     * @param gasUsed - Gas used
     * @param loader - A function used to load block hash
     * @param permanence - Whether to persist the database
     * @return New state root, execution result and transaction receipt
     */
    std::tuple<h256, ExecutionResult, TransactionReceipt> run(const h256 &stateRoot, const BlockHeader &header,
                                                              const Transaction &tx, const u256 &gasUsed,
                                                              LastBlockHashes loader, Permanence permanence)
    {
        createStateIfNotExsits();

        // create env info object
        EnvInfo envInfo(header, LastBlockHashes(loader), gasUsed, m_params.chainID);
        // reset state root
        m_state->setRoot(stateRoot);
        // execute transaction
        auto [result, receipt] = m_state->execute(envInfo, *m_engine, tx, permanence);
        // commit data to db
        m_state->db().commit();

        return std::make_tuple(m_state->rootHash(), result, receipt);
    }

    OverlayDB m_db;
    ChainParams &m_params;
    std::unique_ptr<SealEngineFace> m_engine;
    std::shared_ptr<State> m_state;
};

/**
 * JS wrapper for EVM binding.
 */
class JSEVMBinding : public Napi::ObjectWrap<JSEVMBinding>
{
  public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports)
    {
        // This method is used to hook the accessor and method callbacks
        Napi::Function func = DefineClass(env, "JSEVMBinding",
                                          {
                                              InstanceMethod("chainID", &JSEVMBinding::chainID),
                                              InstanceMethod("setHardfork", &JSEVMBinding::setHardfork),
                                              InstanceMethod("resetHardfork", &JSEVMBinding::resetHardfork),
                                              InstanceMethod("genesis", &JSEVMBinding::genesis),
                                              InstanceMethod("runTx", &JSEVMBinding::runTx),
                                              InstanceMethod("runCall", &JSEVMBinding::runCall),
                                          });

        Napi::FunctionReference *constructor = new Napi::FunctionReference();
        *constructor = Napi::Persistent(func);
        env.SetInstanceData(constructor);

        exports.Set("JSEVMBinding", func);
        return exports;
    }

    /**
     * Construct a new JSEVMBinding object.
     * @param info - Napi callback info
     * @param info_0 - External level db object
     * @param info_1 - Network id
     */
    JSEVMBinding(const Napi::CallbackInfo &info)
        : Napi::ObjectWrap<JSEVMBinding>(info),
          m_binding(std::make_shared<EVMBinding>(toExternalPointer(info[0]), Network(toUint32(info[1]))))
    {
    }

    /**
     * Get chain id.
     * @param info - Napi callback info
     * @return Chain id
     */
    Napi::Value chainID(const Napi::CallbackInfo &info)
    {
        return Napi::Number::New(info.Env(), m_binding->chainID());
    }

    /**
     * Force set hardfork by name
     * @param info - Napi callback info
     * @param info_0 - Hardfork name
     */
    Napi::Value setHardfork(const Napi::CallbackInfo &info)
    {
        m_binding->setHardfork(toString(info[0]));

        return info.Env().Undefined();
    }

    /**
     * Reset hardfork
     * @param info - Napi callback info
     */
    Napi::Value resetHardfork(const Napi::CallbackInfo &info)
    {
        m_binding->resetHardfork();

        return info.Env().Undefined();
    }

    /**
     * Initialize genesis state.
     * @param info - Napi callback info
     * @param info_0 - An array containing all addresses
     * @param info_1 - An array containing the balances of all addresses
     * @return New state root hash
     */
    Napi::Value genesis(const Napi::CallbackInfo &info)
    {
        Napi::Env env = info.Env();

        if (info.Length() != 2)
        {
            Napi::TypeError::New(env, "Wrong number of arguments").ThrowAsJavaScriptException();
            return env.Undefined();
        }

        if (!info[0].IsArray() || !info[1].IsArray())
        {
            Napi::TypeError::New(env, "Wrong arguments").ThrowAsJavaScriptException();
            return env.Undefined();
        }

        auto addresses = info[0].As<Napi::Array>();
        auto balances = info[1].As<Napi::Array>();

        if (addresses.Length() != balances.Length())
        {
            Napi::TypeError::New(env, "Wrong arguments").ThrowAsJavaScriptException();
            return env.Undefined();
        }

        std::vector<std::pair<Address, u256>> genesisInfo;
        for (std::size_t i = 0; i < addresses.Length(); i++)
        {
            auto index = Napi::Number::New(env, i);
            genesisInfo.push_back({toAddress(addresses.Get(index)), toU256(balances.Get(index))});
        }

        return toNapiValue(env, m_binding->genesis(genesisInfo));
    }

    /**
     * Execute transaction.
     * @param info - Napi callback info
     * @param info_0 - Previous state root hash
     * @param info_1 - RLP encoded block header or header object
     * @param info_2 - RLP encoded transaction or transaction object
     * @param info_3 - Gas used
     * @param info_4 - A function used to load block hash
     * @return New state root hash, execution result and receipt
     */
    Napi::Value runTx(const Napi::CallbackInfo &info)
    {
        // parse input params
        auto params = parseRunParams(info);

        // invoke cpp impl
        return executeUnderTryCatch(info.Env(), [&, this]() {
            auto [stateRoot, header, tx, gasUsed, loader] = params;
            auto [newStateRoot, executionResult, receipt] = m_binding->runTx(stateRoot, header, tx, gasUsed, loader);
            auto result = Napi::Object::New(info.Env());
            result.Set("stateRoot", toNapiValue(info.Env(), newStateRoot));
            result.Set("result", toNapiValue(info.Env(), executionResult));
            result.Set("receipt", toNapiValue(info.Env(), receipt));
            return result;
        });
    }

    /**
     * Execute call.
     * @param info - Napi callback info
     * @param info_0 - Previous state root hash
     * @param info_1 - RLP encoded block header or header object
     * @param info_2 - RLP encoded transaction or transaction object
     * @param info_3 - Gas used
     * @param info_4 - A function used to load block hash
     * @return Contract output
     */
    Napi::Value runCall(const Napi::CallbackInfo &info)
    {
        // parse input params
        auto params = parseRunParams(info);

        // invoke cpp impl
        return executeUnderTryCatch(info.Env(), [&, this]() {
            auto [stateRoot, header, tx, gasUsed, loader] = params;
            auto output = m_binding->runCall(stateRoot, header, tx, gasUsed, loader);
            return toNapiValue(info.Env(), output);
        });
    }

  private:
    /**
     * Parse napi value for vm.
     * @param info - Napi callback info
     * @return Input params
     */
    std::tuple<h256, BlockHeader, Transaction, u256, LastBlockHashesLoader> parseRunParams(
        const Napi::CallbackInfo &info)
    {
        // parse input params
        auto stateRoot = toH256(info[0]);
        auto header = toHeader(info[1]);
        auto tx = toTx(info[2]);
        auto gasUsed = toU256(info[3]);
        auto loader = toLoader(info[4]);

        return std::make_tuple(stateRoot, header, tx, gasUsed, loader);
    }

    /**
     * Execute function under try/catch
     * and throw a napi error if there is a problem
     * @param env - Napi env
     * @param func - Logic function
     * @return Execution result
     */
    Napi::Value executeUnderTryCatch(Napi::Env env, std::function<Napi::Value()> func)
    {
        try
        {
            return func();
        }
        catch (const std::exception &err)
        {
            Napi::Error::New(env, err.what()).ThrowAsJavaScriptException();
            return env.Undefined();
        }
        catch (...)
        {
            Napi::Error::New(env, "Unknown error").ThrowAsJavaScriptException();
            return env.Undefined();
        }
    }

    std::shared_ptr<EVMBinding> m_binding;
};

/**
 * Register all seal engines and set log level
 * @param info - Napi callback info
 */
Napi::Value init(const Napi::CallbackInfo &info)
{
    NoProof::init();
    NoReward::init();
    Ethash::init();

    LoggingOptions options;
    options.verbosity = Verbosity::VerbosityError;
    setupLogging(options);

    return info.Env().Undefined();
}

Napi::Object initExports(Napi::Env env, Napi::Object exports)
{
    JSEVMBinding::Init(env, exports);
    exports.Set(Napi::String::New(env, "init"), Napi::Function::New(env, init));
    return exports;
}

NODE_API_MODULE(binding, initExports)