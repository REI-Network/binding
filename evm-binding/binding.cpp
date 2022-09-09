#include <optional>

#include <napi.h>

#include <libethereum/ChainParams.h>
#include <libethereum/Executive.h>
#include <libethereum/LastBlockHashesFace.h>
#include <libethereum/State.h>
#include <libethereum/Transaction.h>
#include <libethereum/TransactionReceipt.h>

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
using External = Napi::External<void>;

using LastBlockHashesLoader = std::function<h256s()>;
using GenesisInfo = std::vector<std::pair<Address, u256>>;

template <class T> Napi::Value toNapiValue(Napi::Env env, const T &t);

Napi::Value toNapiValue(Napi::Env env, const h256 &t)
{
    return Napi::String::New(env, "0x" + toHex(t));
}

Napi::Value toNapiValue(Napi::Env env, const bytes &t)
{
    return Napi::String::New(env, "0x" + toHex(t));
}

template <class R, class T> R fromNapiValue(const T &t);

LastBlockHashesLoader fromNapiValue(const Napi::Function &func)
{
    return [&func]() {
        auto result = func.Call({});

        if (!result.IsArray())
        {
            Napi::TypeError::New(func.Env(), "Wrong number of arguments").ThrowAsJavaScriptException();
            return h256s{};
        }

        auto array = result.As<Napi::Array>();

        h256s hashes;
        for (std::size_t i = 0; i < array.Length(); i++)
        {
            auto value = array.Get(Napi::Number::New(func.Env(), i));

            if (!value.IsBuffer())
            {
                Napi::TypeError::New(func.Env(), "Wrong number of arguments").ThrowAsJavaScriptException();
                return h256s{};
            }

            auto buffer = value.As<Buffer>();

            hashes.emplace_back(bytesConstRef(buffer.Data(), buffer.Length()));
        }

        return hashes;
    };
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

class EVMBinding
{
  public:
    EVMBinding(void *db, eth::Network network)
        : m_db(DBFactory::create(db)), m_params(genesisInfo(network), genesisStateRoot(network)),
          m_engine(m_params.createSealEngine())
    {
    }

    int chainID()
    {
        return m_params.chainID;
    }

    h256 genesis(const GenesisInfo &info)
    {
        if (m_state.get() != nullptr)
        {
            throw std::runtime_error("state already exists");
        }

        createStateIfNotExsits(&info);

        return m_state->rootHash();
    }

    auto runTx(const h256 &stateRoot, const BlockHeader &header, const Transaction &tx, const u256 &gasUsed,
               LastBlockHashes loader)
    {
        return run(stateRoot, header, tx, gasUsed, loader, Permanence::Committed);
    }

    auto runCall(const h256 &stateRoot, const BlockHeader &header, const Transaction &tx, const u256 &gasUsed,
                 LastBlockHashes loader)
    {
        auto [newStateRoot, result, receipt] = run(stateRoot, header, tx, gasUsed, loader, Permanence::Reverted);
        return result.output;
    }

  private:
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
    ChainParams m_params;
    std::unique_ptr<SealEngineFace> m_engine;
    std::shared_ptr<State> m_state;
};

class JSEVMBinding : public Napi::ObjectWrap<JSEVMBinding>
{
  public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports)
    {
        // This method is used to hook the accessor and method callbacks
        Napi::Function func = DefineClass(env, "JSEVMBinding",
                                          {
                                              InstanceMethod("chainID", &JSEVMBinding::chainID),
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

    JSEVMBinding(const Napi::CallbackInfo &info) : Napi::ObjectWrap<JSEVMBinding>(info)
    {
        Napi::Env env = info.Env();

        if (info.Length() != 2)
        {
            Napi::TypeError::New(env, "Wrong number of arguments").ThrowAsJavaScriptException();
            return;
        }

        if (!info[0].IsExternal() || !info[1].IsNumber())
        {
            Napi::TypeError::New(env, "Wrong arguments").ThrowAsJavaScriptException();
            return;
        }

        m_binding = std::make_shared<EVMBinding>(info[0].As<External>().Data(),
                                                 eth::Network(info[1].As<Napi::Number>().Uint32Value()));
    }

    Napi::Value chainID(const Napi::CallbackInfo &info)
    {
        checkBinding(info);

        return Napi::Number::New(info.Env(), m_binding->chainID());
    }

    Napi::Value genesis(const Napi::CallbackInfo &info)
    {
        checkBinding(info);

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

        return toNapiValue(env, m_binding->genesis(genesisInfo));
    }

    Napi::Value runTx(const Napi::CallbackInfo &info)
    {
        // parse input params
        auto [stateRoot, header, tx, gasUsed, loader] = parseRunParams(info);

        // invoke cpp impl
        auto [newStateRoot, result, receipt] = m_binding->runTx(stateRoot, header, tx, gasUsed, loader);

        // TODO: return receipt and result
        return toNapiValue(info.Env(), newStateRoot);
    }

    Napi::Value runCall(const Napi::CallbackInfo &info)
    {
        // parse input params
        auto [stateRoot, header, tx, gasUsed, loader] = parseRunParams(info);

        // invoke cpp impl
        auto output = m_binding->runCall(stateRoot, header, tx, gasUsed, loader);

        // TODO: return receipt and result
        return toNapiValue(info.Env(), output);
    }

  private:
    void checkBinding(const Napi::CallbackInfo &info)
    {
        if (m_binding.get() == nullptr)
        {
            Napi::TypeError::New(info.Env(), "Missing binding instance").ThrowAsJavaScriptException();
        }
    }

    std::tuple<h256, BlockHeader, Transaction, u256, LastBlockHashesLoader> parseRunParams(
        const Napi::CallbackInfo &info)
    {
        Napi::Env env = info.Env();

        if (info.Length() != 5)
        {
            Napi::TypeError::New(env, "Wrong number of arguments").ThrowAsJavaScriptException();
        }

        if (!info[0].IsBuffer() || !info[1].IsBuffer() || !info[2].IsBuffer() || !info[3].IsString() ||
            !info[4].IsFunction())
        {
            Napi::TypeError::New(env, "Wrong arguments").ThrowAsJavaScriptException();
        }

        // parse input params
        auto stateRootBuf = info[0].As<Buffer>();
        h256 stateRoot(bytesConstRef(stateRootBuf.Data(), stateRootBuf.Length()));

        auto headerBuf = info[1].As<Buffer>();
        BlockHeader header(bytesConstRef(headerBuf.Data(), headerBuf.Length()), BlockDataType::HeaderData);

        auto txBuf = info[2].As<Buffer>();
        Transaction tx(bytesConstRef(txBuf.Data(), txBuf.Length()), CheckTransaction::Everything);

        u256 gasUsed(fromHex(info[3].As<Napi::String>()));

        auto loader = fromNapiValue(info[4].As<Napi::Function>());

        return std::make_tuple(stateRoot, header, tx, gasUsed, loader);
    }

    std::shared_ptr<EVMBinding> m_binding;
};

Napi::Value init(const Napi::CallbackInfo &info)
{
    // register seal engines
    NoProof::init();
    NoReward::init();
    return info.Env().Undefined();
}

Napi::Object Init(Napi::Env env, Napi::Object exports)
{
    JSEVMBinding::Init(env, exports);
    exports.Set(Napi::String::New(env, "init"), Napi::Function::New(env, init));
    return exports;
}

NODE_API_MODULE(binding, Init)