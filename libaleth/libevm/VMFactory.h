// Aleth: Ethereum C++ client, tools and libraries.
// Copyright 2014-2019 Aleth Authors.
// Licensed under the GNU General Public License, Version 3.
#pragma once

#include "VMFace.h"

namespace dev
{
namespace eth
{
enum class VMKind
{
    Interpreter,
    Legacy,
    DLL,
    One
};

using VMPtr = std::unique_ptr<VMFace, void (*)(VMFace*)>;

class VMFactory
{
public:
    VMFactory() = delete;
    ~VMFactory() = delete;

    /// Creates a VM instance of the global kind (controlled by the --vm command line option).
    static VMPtr create();

    /// Creates a VM instance of the kind provided.
    static VMPtr create(VMKind _kind);
};
}  // namespace eth
}  // namespace dev
