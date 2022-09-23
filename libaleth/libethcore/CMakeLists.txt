add_library(
    ethcore
    AccessList.h
    AccessList.cpp
    BasicAuthority.cpp
    BasicAuthority.h
    BlockHeader.cpp
    BlockHeader.h
    ChainOperationParams.cpp
    ChainOperationParams.h
    Common.cpp
    Common.h
    CommonJS.cpp
    CommonJS.h
    EVMSchedule.cpp
    EVMSchedule.h
    Exceptions.h
    # KeyManager.cpp
    # KeyManager.h
    LogEntry.cpp
    LogEntry.h
    Precompiled.cpp
    Precompiled.h
    SealEngine.cpp
    SealEngine.h
    TransactionBase.cpp
    TransactionBase.h
)

target_link_libraries(ethcore PUBLIC devcrypto devcore)
