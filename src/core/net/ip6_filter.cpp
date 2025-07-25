/*
 *  Copyright (c) 2016, The OpenThread Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 *   This file implements IPv6 datagram filtering.
 */

#include "ip6_filter.hpp"

#include "instance/instance.hpp"

namespace ot {
namespace Ip6 {

RegisterLogModule("Ip6Filter");

Error Filter::Apply(const Message &aMessage) const
{
    Error    error = kErrorDrop;
    Headers  headers;
    uint16_t dstPort;

    // Allow all received IPv6 datagrams with link security enabled
    if (aMessage.IsLinkSecurityEnabled())
    {
        ExitNow(error = kErrorNone);
    }

    SuccessOrExit(headers.ParseFrom(aMessage));

    VerifyOrExit(headers.GetDestinationAddress().IsLinkLocalUnicastOrMulticast());

    // Allow all link-local IPv6 datagrams when Thread is not enabled
    if (Get<Mle::Mle>().GetRole() == Mle::kRoleDisabled)
    {
        ExitNow(error = kErrorNone);
    }

    dstPort = headers.GetDestinationPort();

    switch (headers.GetIpProto())
    {
    case kProtoUdp:
        // Allow MLE traffic
        if (dstPort == Mle::kUdpPort)
        {
            ExitNow(error = kErrorNone);
        }

#if OPENTHREAD_CONFIG_BORDER_AGENT_ENABLE
        // Allow native commissioner traffic
        if (Get<KeyManager>().GetSecurityPolicy().mNativeCommissioningEnabled &&
            dstPort == Get<MeshCoP::BorderAgent>().GetUdpPort())
        {
            ExitNow(error = kErrorNone);
        }
#endif
        break;

    case kProtoTcp:
        break;

    default:
        // Allow UDP or TCP traffic only
        ExitNow();
    }

    // Check against allowed unsecure port list
    if (mUnsecurePorts.Contains(dstPort))
    {
        error = kErrorNone;
    }

exit:
    return error;
}

Error Filter::UpdateUnsecurePorts(Action aAction, uint16_t aPort)
{
    Error     error = kErrorNone;
    uint16_t *entry;

    VerifyOrExit(aPort != 0, error = kErrorInvalidArgs);

    entry = mUnsecurePorts.Find(aPort);

    if (aAction == kAdd)
    {
        VerifyOrExit(entry == nullptr);
        SuccessOrExit(error = mUnsecurePorts.PushBack(aPort));
    }
    else
    {
        VerifyOrExit(entry != nullptr, error = kErrorNotFound);
        mUnsecurePorts.Remove(*entry);
    }

    LogInfo("%s unsecure port %d", (aAction == kAdd) ? "Added" : "Removed", aPort);

exit:
    return error;
}

} // namespace Ip6
} // namespace ot
