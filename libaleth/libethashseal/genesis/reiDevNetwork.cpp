// Aleth: Ethereum C++ client, tools and libraries.
// Copyright 2015-2019 Aleth Authors.
// Licensed under the GNU General Public License, Version 3.

#include "../GenesisInfo.h"

static dev::h256 const c_genesisStateRootREIDevNetwork("b7b7c6d4fe03e627d8f51a68cacf12b30d6182b59f3c97eb013517b6436b48a0");
static std::string const c_genesisInfoREIDevNetwork = std::string() +
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
        "freeStakingForkBlock": "0x00",
        "betterPOSForkBlock": "0x00",
        "reiDAOForkBlock": "0x32",
        "networkID" : "0x5c1b",
        "chainID": "0x5c1b",
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
