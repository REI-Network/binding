// Aleth: Ethereum C++ client, tools and libraries.
// Copyright 2014-2019 Aleth Authors.
// Licensed under the GNU General Public License, Version 3.

#include "Executive.h"
#include "Block.h"
#include "BlockChain.h"
#include "ExtVM.h"
#include "Interface.h"
#include "State.h"
#include <libdevcore/CommonIO.h>
#include <libethcore/CommonJS.h>
#include <libevm/VMFactory.h>

using namespace std;
using namespace dev;
using namespace dev::eth;

namespace
{
Address const c_RipemdPrecompiledAddress{0x03};
h256 const c_Keccak256RLP = h256(fromHex("56e81f171bcc55a6ff8345e692c0f86e5b48e01b996cadc001622fb5e363b421"));

std::string dumpStorage(ExtVM const& _ext)
{
    ostringstream o;
    o << "    STORAGE\n";
    for (auto const& i : _ext.state().storage(_ext.myAddress))
        o << showbase << hex << i.second.first << ": " << i.second.second << "\n";
    return o.str();
};
}  // namespace

Executive::Executive(Block& _s, BlockChain const& _bc, unsigned _level)
  : m_s(_s.mutableState()),
    m_envInfo(_s.info(), _bc.lastBlockHashes(), 0, _bc.chainID()),
    m_depth(_level),
    m_sealEngine(*_bc.sealEngine())
{
}

Executive::Executive(Block& _s, LastBlockHashesFace const& _lh, unsigned _level)
  : m_s(_s.mutableState()),
    m_envInfo(_s.info(), _lh, 0, _s.sealEngine()->chainParams().chainID),
    m_depth(_level),
    m_sealEngine(*_s.sealEngine())
{
}

Executive::Executive(
    State& io_s, Block const& _block, unsigned _txIndex, BlockChain const& _bc, unsigned _level)
  : m_s(createIntermediateState(io_s, _block, _txIndex, _bc)),
    m_envInfo(_block.info(), _bc.lastBlockHashes(),
        _txIndex ? _block.receipt(_txIndex - 1).cumulativeGasUsed() : 0, _bc.chainID()),
    m_depth(_level),
    m_sealEngine(*_bc.sealEngine())
{
}

u256 Executive::gasUsed() const
{
    if (m_t)
        return m_t.gas() - m_gas;
    else if (m_msg.has_value())
        return m_msg->cp.gas - m_gas - m_msg->baseFee;
    else
        BOOST_THROW_EXCEPTION(ExecutionFailed() << errinfo_comment("missing tx or msg"));
}

void Executive::accrueSubState(SubState& _parentContext)
{
    if (m_ext)
        _parentContext += m_ext->sub;
}

void Executive::initialize(Transaction const& _transaction)
{
    m_t = _transaction;
    const auto& schedule = m_sealEngine.evmSchedule(m_envInfo.number());
    m_baseGasRequired = m_t.baseGasRequired(schedule);
    try
    {
        m_sealEngine.verifyTransaction(ImportRequirements::Everything, m_t, m_envInfo.header(), m_envInfo.gasUsed());

        // TODO: This doesn't belong here!
        // Check sender address for EIP-3607
        if (schedule.eip3607Mode && m_s.addressHasCode(m_t.sender()))
             BOOST_THROW_EXCEPTION(EIP3607InvalidSender());
    }
    catch (Exception const& ex)
    {
        m_excepted = toTransactionException(ex);
        throw;
    }

    if (!m_t.hasZeroSignature())
    {
        // Avoid invalid transactions.
        u256 nonceReq;
        try
        {
            nonceReq = m_s.getNonce(m_t.sender());
        }
        catch (InvalidSignature const&)
        {
            LOG(m_execLogger) << "Invalid Signature";
            m_excepted = TransactionException::InvalidSignature;
            throw;
        }
        if (m_t.nonce() != nonceReq)
        {
            LOG(m_execLogger) << "Sender: " << m_t.sender().hex() << " Invalid Nonce: Required "
                              << nonceReq << ", received " << m_t.nonce();
            m_excepted = TransactionException::InvalidNonce;
            BOOST_THROW_EXCEPTION(
                InvalidNonce() << RequirementError((bigint)nonceReq, (bigint)m_t.nonce()));
        }

        // Avoid unaffordable transactions.
        bigint gasCost = (bigint)m_t.gas() * m_t.gasPrice();
        bigint totalCost = m_t.value() + gasCost;
        if (m_s.balance(m_t.sender()) < totalCost)
        {
            LOG(m_execLogger) << "Not enough cash: Require > " << totalCost << " = " << m_t.gas()
                              << " * " << m_t.gasPrice() << " + " << m_t.value() << " Got"
                              << m_s.balance(m_t.sender()) << " for sender: " << m_t.sender();
            m_excepted = TransactionException::NotEnoughCash;
            m_excepted = TransactionException::NotEnoughCash;
            BOOST_THROW_EXCEPTION(NotEnoughCash() << RequirementError(totalCost, (bigint)m_s.balance(m_t.sender())) << errinfo_comment(m_t.sender().hex()));
        }
        m_gasCost = (u256)gasCost;  // Convert back to 256-bit, safe now.
    }

    initializeAccessList(m_t.accessList(), m_t.sender(), m_t.to(), m_t.isCreation());
}

void Executive::initialize(Message const& _msg)
{
    m_msg = _msg;
    m_baseGasRequired = _msg.baseFee.convert_to<int64_t>();

    boost::optional<AccessList> accessList;
    if (_msg.accessList.has_value())
        accessList = AccessList{*_msg.accessList};
    initializeAccessList(accessList, _msg.cp.senderAddress, _msg.cp.receiveAddress, _msg.isCreation);
}

void Executive::initializeAccessList(boost::optional<AccessList> const& _accessList, Address const& _from, Address const& _to, bool _isCreation)
{
    const auto& schedule = m_sealEngine.evmSchedule(m_envInfo.number());
    // EIP-2929 logic:
    // Mark tx.sender, tx.to and the set of all precompiles as warmed.
    if (schedule.eip2929Mode)
    {
        for (const auto& pair : schedule.supportedPrecompiled)
            if (pair.second)
                m_s.accessAddress(pair.first);
        m_s.accessAddress(_from);
        if (!_isCreation)
            m_s.accessAddress(_to);
    }

    // EIP-2930 logic:
    // Mark all addresses and storage in access list as warmed.
    if (_accessList.has_value())
        _accessList->forEach([this](const auto& _addr, const auto& _keys)
        {
            m_s.accessAddress(_addr);
            for (const auto& key : _keys)
                m_s.accessStorage(_addr, key);
        });
}

bool Executive::execute()
{
    if (m_t)
    {
        // Entry point for a user-executed transaction.

        // Pay...
        LOG(m_detailsLogger) << "Paying " << formatBalance(m_gasCost) << " from sender for gas ("
                            << m_t.gas() << " gas at " << formatBalance(m_t.gasPrice()) << ")";
        m_s.subBalance(m_t.sender(), m_gasCost);

        assert(m_t.gas() >= (u256)m_baseGasRequired);
        if (m_t.isCreation())
            return create(m_t.sender(), m_t.value(), m_t.gasPrice(), m_t.gas() - (u256)m_baseGasRequired, &m_t.data(), m_t.sender());
        else
            return call(m_t.receiveAddress(), m_t.sender(), m_t.value(), m_t.gasPrice(), bytesConstRef(&m_t.data()), m_t.gas() - (u256)m_baseGasRequired);
    }
    else if (m_msg.has_value())
    {
        auto const& _msg = *m_msg;
        
        assert(_msg.cp.gas >= (u256)m_baseGasRequired);
        if (_msg.isCreation)
            return create(_msg.cp.senderAddress, _msg.cp.valueTransfer, _msg.gasPrice, _msg.cp.gas - (u256)m_baseGasRequired, _msg.cp.data, _msg.cp.senderAddress);
        else if (_msg.isUpgrade)
            return upgrade(_msg.cp.senderAddress, _msg.cp.receiveAddress, _msg.cp.valueTransfer, _msg.gasPrice, _msg.cp.gas - (u256)m_baseGasRequired, _msg.cp.data, _msg.cp.senderAddress, _msg.clearStorage);
        else
            return call(_msg.cp.receiveAddress, _msg.cp.senderAddress, _msg.cp.valueTransfer, _msg.gasPrice, _msg.cp.data, _msg.cp.gas - (u256)m_baseGasRequired);
    }
    else
        BOOST_THROW_EXCEPTION(ExecutionFailed() << errinfo_comment("missing tx or msg"));
}

bool Executive::call(Address const& _receiveAddress, Address const& _senderAddress, u256 const& _value, u256 const& _gasPrice, bytesConstRef _data, u256 const& _gas)
{
    CallParameters params{_senderAddress, _receiveAddress, _receiveAddress, _value, _value, _gas, _data, {}};
    return call(params, _gasPrice, _senderAddress);
}

bool Executive::call(CallParameters const& _p, u256 const& _gasPrice, Address const& _origin)
{
    // If external transaction.
    if (m_t)
    {
        // FIXME: changelog contains unrevertable balance change that paid
        //        for the transaction.
        // Increment associated nonce for sender.
        if (_p.senderAddress != MaxAddress ||
            m_envInfo.number() < m_sealEngine.chainParams().experimentalForkBlock)  // EIP86
            m_s.incNonce(_p.senderAddress);
    }

    m_savepoint = m_s.savepoint();

    if (m_sealEngine.isPrecompiled(_p.codeAddress, m_envInfo.number()))
    {
        // Empty RIPEMD contract needs to be deleted even in case of OOG
        // because of the anomaly on the main net caused by buggy behavior by both Geth and Parity
        // https://github.com/ethereum/go-ethereum/pull/3341/files#diff-2433aa143ee4772026454b8abd76b9dd
        // https://github.com/ethereum/EIPs/issues/716
        // https://github.com/ethereum/aleth/pull/5664
        // We mark the account as touched here, so that is can be removed among other touched empty
        // accounts (after tx finalization)
        if (_p.receiveAddress == c_RipemdPrecompiledAddress)
            m_s.unrevertableTouch(_p.codeAddress);

        bigint g = m_sealEngine.costOfPrecompiled(_p.codeAddress, _p.data, m_envInfo.number());
        if (_p.gas < g)
        {
            m_excepted = TransactionException::OutOfGasBase;
            // Bail from exception.
            return true;	// true actually means "all finished - nothing more to be done regarding go().
        }
        else
        {
            m_gas = (u256)(_p.gas - g);
            bytes output;
            bool success;
            tie(success, output) = m_sealEngine.executePrecompiled(
                _p.codeAddress,
                _p.data, 
                [this](Address const& _addr, uint64_t timestamp) -> u256
                {
                    const auto& schedule = m_sealEngine.evmSchedule(m_envInfo.number());
                    if (!schedule.enableFreeStaking)
                        BOOST_THROW_EXCEPTION(ExecutionFailed() << errinfo_comment("free staking hardfork is not enabled"));

                    if (m_s.accountNonemptyAndExisting(_addr))
                    {
                        const auto& stakeInfo = m_s.stakeInfo(_addr);
                        if (stakeInfo)
                        {
                            u256 dailyFee;
                            if (schedule.enableDAO)
                                // load daily fee from contract storge
                                dailyFee = m_s.storage(ConfigAddress, u256{"0x0000000000000000000000000000000000000000000000000000000000000015"});
                            else
                                // load daily fee from schedule
                                dailyFee = schedule.dailyFee;
                            return stakeInfo->estimateFee(timestamp, m_s.balance(FeeManagerAddress), dailyFee);
                        }
                    }

                    return 0;
                },
                m_envInfo.number()
            );
            size_t outputSize = output.size();
            m_output = owning_bytes_ref{std::move(output), 0, outputSize};
            if (!success)
            {
                m_gas = 0;
                m_excepted = TransactionException::OutOfGas;
                return true;	// true means no need to run go().
            }
        }
    }
    else
    {
        m_gas = _p.gas;
        if (m_s.addressHasCode(_p.codeAddress))
        {
            bytes const& c = m_s.code(_p.codeAddress);
            h256 codeHash = m_s.codeHash(_p.codeAddress);
            // Contract will be executed with the version stored in account
            auto const version = m_s.version(_p.codeAddress);
            m_ext = make_shared<ExtVM>(m_s, m_envInfo, m_sealEngine, _p.receiveAddress,
                _p.senderAddress, _origin, _p.apparentValue, _gasPrice, _p.data, &c, codeHash,
                version, m_depth, false, _p.staticCall);
        }
    }

    // Transfer ether.
    m_s.transferBalance(_p.senderAddress, _p.receiveAddress, _p.valueTransfer);
    return !m_ext;
}

bool Executive::create(Address const& _txSender, u256 const& _endowment, u256 const& _gasPrice, u256 const& _gas, bytesConstRef _init, Address const& _origin)
{
    // Contract will be created with the version corresponding to latest hard fork
    auto const latestVersion = m_sealEngine.evmSchedule(m_envInfo.number()).accountVersion;
    return createWithAddressFromNonceAndSender(
        _txSender, _endowment, _gasPrice, _gas, _init, _origin, latestVersion);
}

bool Executive::upgrade(Address const& _sender, Address const& _receiver, u256 const& _endowment, u256 const& _gasPrice, u256 const& _gas, bytesConstRef _init, Address const& _origin, bool _clearStorage)
{
    // Contract will be created with the version corresponding to latest hard fork
    auto const latestVersion = m_sealEngine.evmSchedule(m_envInfo.number()).accountVersion;
    return upgradeWithAddressFromReceiveAddress(
        _sender, _receiver, _endowment, _gasPrice, _gas, _init, _origin, _clearStorage, latestVersion);
}

bool Executive::createOpcode(Address const& _sender, u256 const& _endowment, u256 const& _gasPrice, u256 const& _gas, bytesConstRef _init, Address const& _origin)
{
    // Contract will be created with the version equal to parent's version
    return createWithAddressFromNonceAndSender(
        _sender, _endowment, _gasPrice, _gas, _init, _origin, m_s.version(_sender));
}

bool Executive::createWithAddressFromNonceAndSender(Address const& _sender, u256 const& _endowment,
    u256 const& _gasPrice, u256 const& _gas, bytesConstRef _init, Address const& _origin,
    u256 const& _version)
{
    u256 nonce = m_s.getNonce(_sender);
    // To be compatible with ethereumjs,
    // when this is a message call,
    // the nonce needs to be decremented by 1
    if (m_msg.has_value())
    {
        assert(nonce > 0);
        nonce--;
    }
    m_newAddress = right160(sha3(rlpList(_sender, nonce)));
    return executeCreate(_sender, _endowment, _gasPrice, _gas, _init, _origin, _version);
}

bool Executive::upgradeWithAddressFromReceiveAddress(Address const& _sender, Address const& _receiver, u256 const& _endowment,
    u256 const& _gasPrice, u256 const& _gas, bytesConstRef _init, Address const& _origin, bool _clearStorage, u256 const& _version)
{
    m_newAddress = _receiver;
    return executeUpgrade(_sender, _endowment, _gasPrice, _gas, _init, _origin, _clearStorage, _version);
}

bool Executive::create2Opcode(Address const& _sender, u256 const& _endowment, u256 const& _gasPrice, u256 const& _gas, bytesConstRef _init, Address const& _origin, u256 const& _salt)
{
    m_newAddress = right160(sha3(bytes{0xff} +_sender.asBytes() + toBigEndian(_salt) + sha3(_init)));
    // Contract will be created with the version equal to parent's version
    return executeCreate(
        _sender, _endowment, _gasPrice, _gas, _init, _origin, m_s.version(_sender));
}

bool Executive::executeCreate(Address const& _sender, u256 const& _endowment, u256 const& _gasPrice,
    u256 const& _gas, bytesConstRef _init, Address const& _origin, u256 const& _version)
{
    // If this is a message call,
    // the nonce will be handled externally and does not need to be incremented
    if (!m_msg.has_value() && (_sender != MaxAddress ||
        m_envInfo.number() < m_sealEngine.chainParams().experimentalForkBlock))  // EIP86
        m_s.incNonce(_sender);

    const auto& schedule = m_sealEngine.evmSchedule(m_envInfo.number());
    if (schedule.eip2929Mode) 
        m_s.accessAddress(m_newAddress);

    m_savepoint = m_s.savepoint();

    m_isCreation = true;

    // We can allow for the reverted state (i.e. that with which m_ext is constructed) to contain the m_orig.address, since
    // we delete it explicitly if we decide we need to revert.

    m_gas = _gas;
    bool accountAlreadyExist = (m_s.addressHasCode(m_newAddress) || m_s.getNonce(m_newAddress) > 0);
    if (accountAlreadyExist)
    {
        LOG(m_detailsLogger) << "Address already used: " << m_newAddress;
        m_gas = 0;
        m_excepted = TransactionException::AddressAlreadyUsed;
        revert();
        m_ext = {}; // cancel the _init execution if there are any scheduled.
        return !m_ext;
    }

    // Transfer ether before deploying the code. This will also create new
    // account if it does not exist yet.
    m_s.transferBalance(_sender, m_newAddress, _endowment);

    u256 newNonce = m_s.requireAccountStartNonce();
    if (schedule.eip158Mode)
        newNonce += 1;
    m_s.setNonce(m_newAddress, newNonce);

    m_s.clearStorage(m_newAddress);

    // Schedule _init execution if not empty.
    if (!_init.empty())
        m_ext = make_shared<ExtVM>(m_s, m_envInfo, m_sealEngine, m_newAddress, _sender, _origin,
            _endowment, _gasPrice, bytesConstRef(), _init, sha3(_init), _version, m_depth, true,
            false);
    else
        // code stays empty, but we set the version
        m_s.setCode(m_newAddress, {}, _version);

    return !m_ext;
}

bool Executive::executeUpgrade(Address const& _sender, u256 const& _endowment, u256 const& _gasPrice,
    u256 const& _gas, bytesConstRef _init, Address const& _origin, bool _clearStorage, u256 const& _version)
{
    if (m_t || !m_msg.has_value())
        BOOST_THROW_EXCEPTION(ExecutionFailed() << errinfo_comment("tx cannot upgrade contract or missing msg"));

    const auto& schedule = m_sealEngine.evmSchedule(m_envInfo.number());
    if (schedule.eip2929Mode)
        m_s.accessAddress(m_newAddress);

    m_savepoint = m_s.savepoint();

    bool exist = m_s.storageRoot(m_newAddress) != c_Keccak256RLP;

    if (!exist)
    {
        m_isCreation = true;

        // We can allow for the reverted state (i.e. that with which m_ext is constructed) to contain the m_orig.address, since
        // we delete it explicitly if we decide we need to revert.

        m_gas = _gas;

        // Transfer ether before deploying the code. This will also create new
        // account if it does not exist yet.
        m_s.transferBalance(_sender, m_newAddress, _endowment);

        if (schedule.eip158Mode)
            m_s.setNonce(m_newAddress, m_s.getNonce(m_newAddress) + 1);

        m_s.clearStorage(m_newAddress);

        // Schedule _init execution if not empty.
        if (!_init.empty())
            m_ext = make_shared<ExtVM>(m_s, m_envInfo, m_sealEngine, m_newAddress, _sender, _origin,
                _endowment, _gasPrice, bytesConstRef(), _init, sha3(_init), _version, m_depth, true,
                false);
        else
            // code stays empty, but we set the version
            m_s.setCode(m_newAddress, {}, _version);
    }
    else
    {
        if (!m_s.addressHasCode(m_newAddress))
        {
            LOG(m_detailsLogger) << "Invalid upgrade: " << m_newAddress;
            m_gas = 0;
            m_excepted = TransactionException::AddressAlreadyUsed;
            revert();
            m_ext = {}; // cancel the _init execution if there are any scheduled.
            return !m_ext;
        }

        if (_clearStorage)
            m_s.clearStorage(m_newAddress);

        m_s.setCode(m_newAddress, _init.toBytes(), _version);
    }

    return !m_ext;
}

OnOpFunc Executive::simpleTrace()
{
    Logger& traceLogger = m_vmTraceLogger;

    return [&traceLogger](uint64_t steps, uint64_t PC, Instruction inst, bigint newMemSize,
               bigint gasCost, bigint gas, VMFace const* _vm, ExtVMFace const* voidExt) {
        ExtVM const& ext = *static_cast<ExtVM const*>(voidExt);
        // auto vm = dynamic_cast<LegacyVM const*>(_vm);

        // if (vm)
        //     LOG(traceLogger) << dumpStackAndMemory(*vm);
        LOG(traceLogger) << dumpStorage(ext);
        LOG(traceLogger) << " < " << dec << ext.depth << " : " << ext.myAddress << " : #" << steps
                         << " : " << hex << setw(4) << setfill('0') << PC << " : "
                         << instructionInfo(inst).name << " : " << dec << gas << " : -" << dec
                         << gasCost << " : " << newMemSize << "x32"
                         << " >";
    };
}

bool Executive::go(OnOpFunc const& _onOp)
{
    if (m_ext)
    {
#if ETH_TIMED_EXECUTIONS
        Timer t;
#endif
        try
        {
            // Create VM instance. Force Interpreter if tracing requested.
            auto vm = VMFactory::create();
            if (m_isCreation)
            {
                auto result = vm->exec(m_gas, *m_ext, _onOp);
                if (m_res)
                {
                    m_res->gasForDeposit = m_gas;
                    m_res->depositSize = result.output.size();
                }
                if (result.output.size() > m_ext->evmSchedule().maxCodeSize)
                    BOOST_THROW_EXCEPTION(OutOfGas());
                else if (result.output.size() * m_ext->evmSchedule().createDataGas <= m_gas)
                {
                    if (m_res)
                        m_res->codeDeposit = CodeDeposit::Success;
                    m_gas -= result.output.size() * m_ext->evmSchedule().createDataGas;
                }
                else
                {
                    if (m_ext->evmSchedule().exceptionalFailedCodeDeposit)
                        BOOST_THROW_EXCEPTION(OutOfGas());
                    else
                    {
                        if (m_res)
                            m_res->codeDeposit = CodeDeposit::Failed;
                        result.output = {};
                    }
                }
                if (m_res)
                    m_res->output = result.output.toVector(); // copy output to execution result
                m_s.setCode(m_ext->myAddress, result.output.toVector(), m_ext->version);
            }
            else
            {
                auto result = vm->exec(m_gas, *m_ext, _onOp);
                m_output = std::move(result.output);
            }
        }
        catch (RevertInstruction& _e)
        {
            revert();
            m_output = _e.output();
            m_excepted = TransactionException::RevertInstruction;
        }
        catch (VMException const& _e)
        {
            LOG(m_detailsLogger) << "Safe VM Exception. " << diagnostic_information(_e);
            m_gas = 0;
            m_excepted = toTransactionException(_e);
            revert();
        }
        catch (InternalVMError const& _e)
        {
            cerror << "Internal VM Error (EVMC status code: "
                 << *boost::get_error_info<errinfo_evmcStatusCode>(_e) << ")";
            revert();
            throw;
        }
        catch (Exception const& _e)
        {
            // TODO: AUDIT: check that this can never reasonably happen. Consider what to do if it does.
            cerror << "Unexpected exception in VM. There may be a bug in this implementation. "
                 << diagnostic_information(_e);
            exit(1);
            // Another solution would be to reject this transaction, but that also
            // has drawbacks. Essentially, the amount of ram has to be increased here.
        }
        catch (std::exception const& _e)
        {
            // TODO: AUDIT: check that this can never reasonably happen. Consider what to do if it does.
            cerror << "Unexpected std::exception in VM. Not enough RAM? " << _e.what();
            exit(1);
            // Another solution would be to reject this transaction, but that also
            // has drawbacks. Essentially, the amount of ram has to be increased here.
        }

        if (m_res && m_output)
            // Copy full output:
            m_res->output = m_output.toVector();

#if ETH_TIMED_EXECUTIONS
        cnote << "VM took:" << t.elapsed() << "; gas used: " << (sgas - m_endGas);
#endif
    }
    return true;
}

bool Executive::finalize()
{
    if (m_ext)
    {
        // Accumulate refunds for selfdestructs.
        m_ext->sub.refunds +=
            m_ext->evmSchedule().selfdestructRefundGas * m_ext->sub.selfdestructs.size();

        // Refunds must be applied before the miner gets the fees.
        assert(m_ext->sub.refunds >= 0);

        // debug log
        // std::cout << "refund: " << maxRefund << " " << m_ext->sub.refunds << " " << m_ext->sub.refunds << " size: " << m_ext->sub.selfdestructs.size() << std::endl;

        if (m_t)
        {
            int64_t maxRefund = (static_cast<int64_t>(m_t.gas()) - static_cast<int64_t>(m_gas)) / 2;
            m_gas += min(maxRefund, m_ext->sub.refunds);
        }

        // If this is a message call,
        // the refund will be processed externally and no processing is required
    }

    if (m_t)
    {
        m_s.addBalance(m_t.sender(), m_gas * m_t.gasPrice());

        u256 feesEarned = (m_t.gas() - m_gas) * m_t.gasPrice();
        m_s.addBalance(m_envInfo.author(), feesEarned);
    }

    // Selfdestructs...
    if (m_ext)
        for (auto a: m_ext->sub.selfdestructs)
            m_s.kill(a);

    // Logs...
    if (m_ext)
        m_logs = m_ext->sub.logs;

    if (m_res) // Collect results
    {
        m_res->gasUsed = gasUsed();
        m_res->excepted = m_excepted; // TODO: m_except is used only in ExtVM::call
        m_res->newAddress = m_newAddress;
        m_res->gasRefunded = m_ext ? m_ext->sub.refunds : 0;
    }
    return (m_excepted == TransactionException::None);
}

void Executive::revert()
{
    if (m_ext)
        m_ext->sub.clear();

    // Set result address to the null one.
    m_newAddress = {};
    m_s.rollback(m_savepoint);
}
