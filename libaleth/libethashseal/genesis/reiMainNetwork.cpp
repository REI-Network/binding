// Aleth: Ethereum C++ client, tools and libraries.
// Copyright 2015-2019 Aleth Authors.
// Licensed under the GNU General Public License, Version 3.

#include "../GenesisInfo.h"

static dev::h256 const c_genesisStateRootREIMainNetwork("2fa2d04b5f646c95f481ae147b4188b9294d9f6b2f97eaa3509186159acde524");
static std::string const c_genesisInfoREIMainNetwork = std::string() +
                                                    R"E(
{
    "sealEngine": "NoProof",
    "params": {
        "accountStartNonce": "0x00",
        "homesteadForkBlock": "0x00",
        "daoHardforkBlock": "0x00",
        "EIP150ForkBlock": "0x00",
        "EIP158ForkBlock": "0x00",
        "byzantiumForkBlock": "0x00",
        "constantinopleForkBlock": "0x00",
        "constantinopleFixForkBlock": "0x00",
        "istanbulForkBlock": "0x00",
        "muirGlacierForkBlock": "0x00",
        "berlinForkBlock": "0x00",
        "freeStakingForkBlock": "0x558c9d",
        "betterPOSForkBlock": "0x92ed85",
        "networkID" : "0xbabd",
        "chainID": "0xbabd",
        "maximumExtraDataSize": "0x2000",
        "tieBreakingGas": false,
        "minGasLimit": "0x1388",
        "maxGasLimit": "7fffffffffffffff",
        "gasLimitBoundDivisor": "0x0400",
        "minimumDifficulty": "0x00",
        "difficultyBoundDivisor": "0x00",
        "durationLimit": "0x0d",
        "blockReward": "0x00"
    },
    "genesis": {
        "nonce": "0x00",
        "difficulty": "0x00",
        "mixHash": "0x0000000000000000000000000000000000000000000000000000000000000000",
        "author": "0x0000000000000000000000000000000000000000",
        "timestamp": "0x00",
        "parentHash": "0x0000000000000000000000000000000000000000000000000000000000000000",
        "extraData": "0x0000000000000000000000000000000000000000000000000000000000000000",
        "gasLimit": "0x00"
    },
    "accounts": {
    }
}
)E";
