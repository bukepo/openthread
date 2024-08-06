/*
 *  Copyright (c) 2024, The OpenThread Authors.
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

#include "mock_coprocessor_platform.hpp"
#include "mock_platform.hpp"

#include <openthread/ncp.h>

#include "lib/hdlc/hdlc.hpp"
#include "lib/spinel/spinel.h"
#include "lib/spinel/spinel_interface.hpp"

namespace ot {

otError DirectSpinelInterface::SendFrame(const uint8_t *aFrame, uint16_t aLength)
{
    otError                            error = OT_ERROR_NONE;
    Spinel::FrameBuffer<kMaxFrameSize> encoderBuffer;
    Hdlc::Encoder                      hdlcEncoder(encoderBuffer);

    SuccessOrExit(error = hdlcEncoder.BeginFrame());
    SuccessOrExit(error = hdlcEncoder.Encode(aFrame, aLength));
    SuccessOrExit(error = hdlcEncoder.EndFrame());

    otNcpHdlcReceive(encoderBuffer.GetFrame(), encoderBuffer.GetLength());

exit:
    return error;
}

otError DirectSpinelInterface::WaitForFrame(uint64_t aTimeoutUs)
{
    do
    {
        if (mReceived)
            break;
    } while ((aTimeoutUs = PlatformBase::CurrentPlatform().Run(aTimeoutUs)));

    Error error = mReceived ? kErrorNone : kErrorResponseTimeout;
    mReceived   = false;
    return error;
}

int DirectSpinelInterface::Receive(const uint8_t *aBuffer, uint16_t aLength)
{
    Hdlc::Decoder hdlcDecoder;

    hdlcDecoder.Init(*mDecoderBuffer, &DirectSpinelInterface::OnReceived, this);
    hdlcDecoder.Decode(aBuffer, aLength);

    return aLength;
}

CoprocessorPlatform::CoprocessorPlatform()
{
    otNcpHdlcInit(mInstance, [](const uint8_t *aBuf, uint16_t aLength) -> int {
        int rval =
            static_cast<CoprocessorPlatform &>(PlatformBase::CurrentPlatform()).mSpinelInterface.Receive(aBuf, aLength);
        otNcpHdlcSendDone();
        return rval;
    });

    spinel_iid_t iids[]{
        0,
    };

    mSpinelDriver.Init(mSpinelInterface, false, iids, sizeof(iids) / sizeof(iids[0]));

    mRadioSpinel.Init(true, false, &mSpinelDriver, 0, false);
}

} // namespace ot
