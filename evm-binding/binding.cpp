#define NAPI_VERSION 3

#include <napi-macros.h>
#include <node_api.h>

#include <libethereum/State.h>
#include <libethereum/Executive.h>
#include <libethereum/Transaction.h>
#include <libethereum/ChainParams.h>
#include <libethereum/LastBlockHashesFace.h>

#include <libethcore/SealEngine.h>
#include <libethcore/TransactionBase.h>

#include <libdevcore/RLP.h>
#include <libdevcore/OverlayDB.h>
#include <libdevcore/DBFactory.h>

#include <libethashseal/GenesisInfo.h>

using namespace dev;
using namespace dev::db;
using namespace dev::eth;

class MockLastBlockHashesFace : public LastBlockHashesFace
{
public:
    virtual h256s precedingHashes(h256 const& _mostRecentHash) const final
    {
        // TODO: return empty...
        return h256s{256, h256()};
    }

    virtual void clear() final
    {
        // do nothing...
    }
};

BlockHeader createMockBlockHeader() {
    RLPStream header;

    header.appendList(13);
    // parent hash
    header << h256(0);
    // sha3 uncles
    header << h256(0);
    // author
    header << h160(0);
    // state root
    header << h256(0);
    // transactions root
    header << fromHex("0x1dcc4de8dec75d7aab85b567b6ccd41ad312451b948a7413f0a142fd40d49347");
    // receipts root
    header << fromHex("0x1dcc4de8dec75d7aab85b567b6ccd41ad312451b948a7413f0a142fd40d49347");
    // log bloom
    header << h2048(0);
    // difficulty
    header << 0;
    // number
    header << 0;
    // gas limit
    header << 0x1312d00;
    // gas used
    header << 0;
    // timestamp
    header << 0;
    // extra data
    header << h256(0);

    return BlockHeader{header.out(), BlockDataType::HeaderData};
}

Transaction createMockTransaction() {
    TransactionSkeleton tx;
    tx.from = Address("0x3289621709F5B35D09B4335E129907aC367A0593");
    tx.to = Address("0xD1E52F6EACBb95f5F8512Ff129CbD6360E549B0B");
    tx.value = 1;
    tx.nonce = 0;
    tx.gas = 21000;
    tx.gasPrice = 1;
    return Transaction{tx, Secret("0xd8ca4883bbf62202904e402750d593a297b5640dea80b6d5b239c5a9902662c0")};
}

void hellow_evmone() {
    ChainParams params(genesisInfo(eth::Network::REIDevNetwork), genesisStateRoot(eth::Network::REIDevNetwork));
    std::unique_ptr<SealEngineFace> engine(params.createSealEngine());
    MockLastBlockHashesFace lbh;
    auto header = createMockBlockHeader();
    EnvInfo info(header, lbh, u256(0), params.chainID);
    OverlayDB db(DBFactory::create(DatabaseKind::MemoryDB));
    State state(0, db, BaseState::Empty);
    state.addBalance(Address("0x3289621709F5B35D09B4335E129907aC367A0593"), 100000);
    Executive executor(state, info, *engine);
    auto tx = createMockTransaction();
    executor.initialize(tx);
    executor.execute();
    executor.finalize();
}

NAPI_METHOD(init)
{
    // register seal engines
    NoProof::init();
    NoReward::init();
    return 0;
}

NAPI_METHOD(run)
{
    try {
        hellow_evmone();
    } catch(const std::exception& err) {
        std::cout << "error: " << err.what() << std::endl;
    } catch(...) {
        std::cout << "error: unknown" << std::endl;
    }
    return 0;
}

NAPI_INIT()
{
    NAPI_EXPORT_FUNCTION(init)
    NAPI_EXPORT_FUNCTION(run)
}