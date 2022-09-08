#include <napi.h>

#include <libethereum/ChainParams.h>
#include <libethereum/Executive.h>
#include <libethereum/LastBlockHashesFace.h>
#include <libethereum/State.h>
#include <libethereum/Transaction.h>

#include <libethcore/SealEngine.h>
#include <libethcore/TransactionBase.h>

#include <libdevcore/DBFactory.h>
#include <libdevcore/OverlayDB.h>
#include <libdevcore/RLP.h>

#include <libethashseal/GenesisInfo.h>

using namespace dev;
using namespace dev::db;
using namespace dev::eth;

using Buffer = Napi::Buffer<unsigned char>;

OverlayDB g_mockDB = OverlayDB(DBFactory::create(DatabaseKind::MemoryDB));

class MockLastBlockHashesFace : public LastBlockHashesFace
{
  public:
    virtual h256s precedingHashes(h256 const &_mostRecentHash) const final
    {
        // TODO: return empty...
        return h256s{256, h256()};
    }

    virtual void clear() final
    {
        // do nothing...
    }
};

Napi::Value init(const Napi::CallbackInfo &info)
{
    // register seal engines
    NoProof::init();
    NoReward::init();
    return info.Env().Undefined();
}

/**
 * Init genesis state to database
 * @param addresses - Address array
 * @param balances - Balance array
 * @return Genesis state root(if succeed)
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
        auto address = addresses.Get(index);
        auto balance = balances.Get(index);
        if (!address.IsString() || !balance.IsString())
        {
            Napi::TypeError::New(env, "Wrong arguments").ThrowAsJavaScriptException();
            return env.Undefined();
        }
        genesisInfo.push_back({Address(address.As<Napi::String>()), u256(fromHex(balance.As<Napi::String>()))});
    }

    State state(0, g_mockDB, BaseState::Empty);
    auto sp = state.savepoint();
    try
    {
        for (const auto &pair : genesisInfo)
        {
            state.addBalance(pair.first, pair.second);
        }
        state.commit(State::CommitBehaviour::KeepEmptyAccounts);
        return Napi::String::From(env, toHex(state.rootHash()));
    }
    catch (const std::exception &err)
    {
        state.rollback(sp);
        Napi::Error::New(env, err.what()).ThrowAsJavaScriptException();
        return env.Undefined();
    }
    catch (...)
    {
        state.rollback(sp);
        Napi::Error::New(env, "Unknown error").ThrowAsJavaScriptException();
        return env.Undefined();
    }
}

/**
 * Run transaction
 * @param stateRoot
 * @param headerRLP
 * @param txRLP
 * @param gasUsed - Hex encoded gas used
 * @return Napi::Value
 */
Napi::Value runTx(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (info.Length() != 4)
    {
        Napi::TypeError::New(env, "Wrong number of arguments").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    if (!info[0].IsBuffer() || !info[1].IsBuffer() || !info[2].IsBuffer() || !info[3].IsString())
    {
        Napi::TypeError::New(env, "Wrong arguments").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    try
    {
        // parse input params
        auto stateRootBuf = info[0].As<Buffer>();
        h256 stateRoot(bytesConstRef(stateRootBuf.Data(), stateRootBuf.Length()));
        auto headerBuf = info[1].As<Buffer>();
        BlockHeader header(bytesConstRef(headerBuf.Data(), headerBuf.Length()), BlockDataType::HeaderData);
        auto txBuf = info[2].As<Buffer>();
        Transaction tx(bytesConstRef(txBuf.Data(), txBuf.Length()), CheckTransaction::Everything);
        u256 gasUsed(fromHex(info[3].As<Napi::String>()));

        // load chain params
        ChainParams params(genesisInfo(eth::Network::REIDevNetwork), genesisStateRoot(eth::Network::REIDevNetwork));
        // create seal engine instance
        std::unique_ptr<SealEngineFace> engine(params.createSealEngine());
        // TODO: create lbh object
        MockLastBlockHashesFace lbh;
        // create env info object
        EnvInfo envInfo(header, lbh, gasUsed, params.chainID);
        // TODO: get leveldb instance
        // TODO: BaseState
        // create state manager object
        State state(0, g_mockDB, BaseState::PreExisting);
        state.setRoot(stateRoot);
        // execute transaction
        auto [result, receipt] = state.execute(envInfo, *engine, tx, Permanence::Committed);

        // TODO: return receipt
        return env.Undefined();
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

Napi::Object Init(Napi::Env env, Napi::Object exports)
{
    exports.Set(Napi::String::New(env, "init"), Napi::Function::New(env, init));
    exports.Set(Napi::String::New(env, "runTx"), Napi::Function::New(env, runTx));
    exports.Set(Napi::String::New(env, "genesis"), Napi::Function::New(env, genesis));
    return exports;
}

NODE_API_MODULE(binding, Init)