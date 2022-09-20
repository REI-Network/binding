add_library(
    ethereum
    Account.cpp
    Account.h
    BasicGasPricer.cpp
    BasicGasPricer.h
    Block.cpp
    Block.h
    BlockChain.cpp
    BlockChain.h
    BlockChainImporter.cpp
    BlockChainImporter.h
    # BlockChainSync.cpp
    # BlockChainSync.h
    BlockDetails.cpp
    BlockDetails.h
    BlockQueue.cpp
    BlockQueue.h
    ChainParams.cpp
    ChainParams.h
    # Client.cpp
    # Client.h
    # ClientBase.cpp
    # ClientBase.h
    # ClientTest.cpp
    # ClientTest.h
    CodeSizeCache.h
    # CommonNet.cpp
    # CommonNet.h
    DatabasePaths.cpp
    DatabasePaths.h
    # EthereumCapability.cpp
    # EthereumCapability.h
    # EthereumPeer.cpp
    # EthereumPeer.h
    Executive.cpp
    Executive.h
    ExtVM.cpp
    ExtVM.h
    GasPricer.cpp
    GasPricer.h
    GenericFarm.h
    GenericMiner.cpp
    GenericMiner.h
    GenesisInfo.cpp
    GenesisInfo.h
    ImportPerformanceLogger.cpp
    ImportPerformanceLogger.h
    Interface.cpp
    Interface.h
    LastBlockHashesFace.h
    LogFilter.cpp
    LogFilter.h
    SecureTrieDB.h
    # SnapshotImporter.cpp
    # SnapshotImporter.h
    # SnapshotStorage.cpp
    # SnapshotStorage.h
    # StandardTrace.cpp
    # StandardTrace.h
    State.cpp
    State.h
    StateImporter.cpp
    StateImporter.h
    Transaction.cpp
    Transaction.h
    TransactionQueue.cpp
    TransactionQueue.h
    TransactionReceipt.cpp
    TransactionReceipt.h
    ValidationSchemes.cpp
    ValidationSchemes.h
    VerifiedBlock.h
    # WarpCapability.cpp
    # WarpCapability.h
)

target_include_directories(ethereum PRIVATE "${UTILS_INCLUDE_DIR}")
target_link_directories(ethereum PUBLIC ${JSONCPP_ROOT}/lib)
target_link_libraries(ethereum PUBLIC evm ethcore devcrypto devcore PRIVATE jsoncpp snappy Boost::fiber Boost::context)
