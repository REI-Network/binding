// Aleth: Ethereum C++ client, tools and libraries.
// Copyright 2014-2019 Aleth Authors.
// Licensed under the GNU General Public License, Version 3.

#include "VMFactory.h"
#include "EVMC.h"

#include <evmone/evmone.h>

#include <evmc/loader.h>

namespace dev
{
namespace eth
{

VMPtr VMFactory::create()
{
    return create(VMKind::One);
}

VMPtr VMFactory::create(VMKind _kind)
{
    static const auto default_delete = [](VMFace * _vm) noexcept { delete _vm; };
    static const auto null_delete = [](VMFace*) noexcept {};

    switch (_kind)
    {
    case VMKind::One:
        // TODO: set options
        return {new EVMC{evmc_create_evmone(), {}}, default_delete};
    case VMKind::Legacy:
    case VMKind::Interpreter:
    case VMKind::DLL:
    default:
        BOOST_THROW_EXCEPTION(std::system_error(std::make_error_code(std::errc::invalid_argument),
            "unsupported vm kind"));
    }
}
}  // namespace eth
}  // namespace dev
