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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <openthread/platform/radio.h>

#include "common/error.hpp"

#include "mock_coprocessor_platform.hpp"

using namespace ot;

using ::testing::Truly;

TEST(RadioSpinel, TransmitWithTxPower)
{
    class MockPlatform : public CoprocessorPlatform
    {
    public:
        MOCK_METHOD(otError, Transmit, (otRadioFrame * aFrame), (override));
    };

    MockPlatform platform;

    uint8_t      frameBuffer[OT_RADIO_FRAME_MAX_SIZE];
    Mac::TxFrame frame;
    frame.mPsdu = frameBuffer;

    Mac::Addresses addresses;
    Mac::PanIds    panIds;
    frame.InitMacHeader(Mac::Frame::kTypeData, Mac::Frame::kVersion2006, addresses, panIds, Mac::Frame::kSecurityNone);
    frame.mInfo.mTxInfo.mTxPower = -10;

    EXPECT_CALL(platform, Transmit(Truly([&frame](otRadioFrame *aFrame) -> bool {
                    return aFrame->mInfo.mTxInfo.mTxPower == -10;
                })))
        .Times(1);

    EXPECT_EQ(platform.mRadioSpinel.Enable(platform.mInstance), kErrorNone);
    EXPECT_EQ(platform.mRadioSpinel.Receive(11), kErrorNone);
    EXPECT_EQ(platform.mRadioSpinel.Transmit(frame), kErrorNone);

    platform.Go(1000000);
}
