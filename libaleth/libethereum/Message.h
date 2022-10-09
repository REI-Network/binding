#pragma once

#include <libdevcore/Common.h>
#include <libevm/ExtVMFace.h>

#include <boost/optional.hpp>

namespace dev
{

namespace eth
{

struct Message
{
    CallParameters cp;
    u256 baseFee;
    u256 gasPrice;
    bool isCreation;
    bool isUpgrade;
    bool clearStorage;
    bool clearEmptyAccount;
    boost::optional<AccessListStruct> accessList;
};

}
}
