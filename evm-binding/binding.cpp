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
        auto gasUsed = fromHex(info[3].As<Napi::String>());

        // load chain params
        ChainParams params(genesisInfo(eth::Network::REIDevNetwork), genesisStateRoot(eth::Network::REIDevNetwork));
        // create seal engine instance
        std::unique_ptr<SealEngineFace> engine(params.createSealEngine());
        // TODO: create lbh object
        MockLastBlockHashesFace lbh;
        // create env info object
        EnvInfo envInfo(header, lbh, u256(gasUsed), params.chainID);
        // TODO: get leveldb instance
        OverlayDB db(DBFactory::create(DatabaseKind::MemoryDB));
        // create state manager object
        State state(0, db, BaseState::Empty);
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
    return exports;
}

NODE_API_MODULE(binding, Init)