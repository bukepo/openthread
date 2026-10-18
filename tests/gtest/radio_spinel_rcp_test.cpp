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

#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <openthread/platform/radio.h>

#include "common/error.hpp"
#include "lib/spinel/openthread-spinel-config.h"
#include "mac/mac_frame.hpp"
#include "mac/mac_types.hpp"

#include "fake_coprocessor_platform.hpp"
#include "fake_platform.hpp"

using namespace ot;

using ::testing::AnyNumber;
using ::testing::Truly;

TEST(RadioSpinelTransmit, shouldPassDesiredTxPowerToRadioPlatform)
{
    class MockPlatform : public FakeCoprocessorPlatform
    {
    public:
        MOCK_METHOD(otError, Transmit, (otRadioFrame * aFrame), (override));
    };

    MockPlatform platform;

    constexpr Mac::PanId kSrcPanId  = 0x1234;
    constexpr Mac::PanId kDstPanId  = 0x4321;
    constexpr uint8_t    kDstAddr[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
    constexpr uint16_t   kSrcAddr   = 0xac00;
    constexpr int8_t     kTxPower   = 100;

    uint8_t      frameBuffer[OT_RADIO_FRAME_MAX_SIZE];
    Mac::TxFrame txFrame{};

    txFrame.mPsdu = frameBuffer;

    {
        Mac::TxFrame::BuildInfo buildInfo;

        buildInfo.mType    = Mac::Frame::kTypeData;
        buildInfo.mVersion = Mac::Frame::kVersion2006;
        buildInfo.mAddrs.mSource.SetShort(kSrcAddr);
        buildInfo.mAddrs.mDestination.SetExtended(kDstAddr);
        buildInfo.mPanIds.SetSource(kSrcPanId);
        buildInfo.mPanIds.SetDestination(kDstPanId);
        buildInfo.mSecurityLevel = Mac::Frame::kSecurityEncMic32;

        txFrame.PrepareHeadersWithEmptyPayload(buildInfo);
    }

    txFrame.mInfo.mTxInfo.mTxPower = kTxPower;
    txFrame.mChannel               = 11;

    EXPECT_CALL(platform, Transmit(Truly([](otRadioFrame *aFrame) -> bool {
                    Mac::Frame &frame = *static_cast<Mac::Frame *>(aFrame);
                    return frame.mInfo.mTxInfo.mTxPower == kTxPower;
                })))
        .Times(1);

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ASSERT_EQ(platform.mRadioSpinel.Transmit(txFrame), kErrorNone);

    platform.GoInMs(1000);
}

TEST(RadioSpinelTransmit, shouldCauseSwitchingToRxChannelAfterTxDone)
{
    FakeCoprocessorPlatform platform;

    constexpr Mac::PanId kSrcPanId  = 0x1234;
    constexpr Mac::PanId kDstPanId  = 0x4321;
    constexpr uint8_t    kDstAddr[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
    constexpr uint16_t   kSrcAddr   = 0xac00;
    constexpr int8_t     kTxPower   = 100;

    uint8_t      frameBuffer[OT_RADIO_FRAME_MAX_SIZE];
    Mac::TxFrame txFrame;

    memset(&txFrame, 0, sizeof(txFrame));
    txFrame.mPsdu = frameBuffer;

    {
        Mac::TxFrame::BuildInfo buildInfo;

        buildInfo.mType    = Mac::Frame::kTypeData;
        buildInfo.mVersion = Mac::Frame::kVersion2006;
        buildInfo.mAddrs.mSource.SetShort(kSrcAddr);
        buildInfo.mAddrs.mDestination.SetExtended(kDstAddr);
        buildInfo.mPanIds.SetSource(kSrcPanId);
        buildInfo.mPanIds.SetDestination(kDstPanId);
        buildInfo.mSecurityLevel = Mac::Frame::kSecurityEncMic32;

        txFrame.PrepareHeadersWithEmptyPayload(buildInfo);
    }

    txFrame.mInfo.mTxInfo.mTxPower              = kTxPower;
    txFrame.mChannel                            = 11;
    txFrame.mInfo.mTxInfo.mRxChannelAfterTxDone = 25;

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ASSERT_EQ(platform.mRadioSpinel.Transmit(txFrame), kErrorNone);
    platform.GoInMs(1000);
    EXPECT_EQ(platform.GetReceiveChannel(), 25);
}

TEST(RadioSpinelTransmit, shouldSkipCsmaCaWhenDisabled)
{
    class MockPlatform : public FakeCoprocessorPlatform
    {
    public:
        MOCK_METHOD(otError, Transmit, (otRadioFrame * aFrame), (override));
        MOCK_METHOD(otError, Receive, (uint8_t aChannel), (override));
    };

    MockPlatform platform;

    constexpr Mac::PanId kSrcPanId  = 0x1234;
    constexpr Mac::PanId kDstPanId  = 0x4321;
    constexpr uint8_t    kDstAddr[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
    constexpr uint16_t   kSrcAddr   = 0xac00;
    constexpr int8_t     kTxPower   = 100;

    uint8_t      frameBuffer[OT_RADIO_FRAME_MAX_SIZE];
    Mac::TxFrame txFrame{};

    txFrame.mPsdu = frameBuffer;

    {
        Mac::TxFrame::BuildInfo buildInfo;

        buildInfo.mType    = Mac::Frame::kTypeData;
        buildInfo.mVersion = Mac::Frame::kVersion2006;
        buildInfo.mAddrs.mSource.SetShort(kSrcAddr);
        buildInfo.mAddrs.mDestination.SetExtended(kDstAddr);
        buildInfo.mPanIds.SetSource(kSrcPanId);
        buildInfo.mPanIds.SetDestination(kDstPanId);
        buildInfo.mSecurityLevel = Mac::Frame::kSecurityEncMic32;

        txFrame.PrepareHeadersWithEmptyPayload(buildInfo);
    }

    txFrame.mInfo.mTxInfo.mCsmaCaEnabled = false;
    txFrame.mChannel                     = 11;

    EXPECT_CALL(platform, Transmit(Truly([](otRadioFrame *aFrame) -> bool {
                    Mac::Frame &frame = *static_cast<Mac::Frame *>(aFrame);
                    return frame.mInfo.mTxInfo.mCsmaCaEnabled == false;
                })))
        .Times(1);

    EXPECT_CALL(platform, Receive).Times(AnyNumber());
    // Receive(11) will be called exactly once to prepare for TX because the fake platform doesn't support sleep-to-tx
    // capability.
    EXPECT_CALL(platform, Receive(11)).Times(1);

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ASSERT_EQ(platform.mRadioSpinel.Transmit(txFrame), kErrorNone);

    platform.GoInMs(1000);
}

TEST(RadioSpinelTransmit, shouldPerformCsmaCaWhenEnabled)
{
    class MockPlatform : public FakeCoprocessorPlatform
    {
    public:
        MOCK_METHOD(otError, Transmit, (otRadioFrame * aFrame), (override));
        MOCK_METHOD(otError, Receive, (uint8_t aChannel), (override));
    };

    MockPlatform platform;

    constexpr Mac::PanId kSrcPanId  = 0x1234;
    constexpr Mac::PanId kDstPanId  = 0x4321;
    constexpr uint8_t    kDstAddr[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
    constexpr uint16_t   kSrcAddr   = 0xac00;
    constexpr int8_t     kTxPower   = 100;

    uint8_t      frameBuffer[OT_RADIO_FRAME_MAX_SIZE];
    Mac::TxFrame txFrame{};

    txFrame.mPsdu = frameBuffer;

    {
        Mac::TxFrame::BuildInfo buildInfo;

        buildInfo.mType    = Mac::Frame::kTypeData;
        buildInfo.mVersion = Mac::Frame::kVersion2006;
        buildInfo.mAddrs.mSource.SetShort(kSrcAddr);
        buildInfo.mAddrs.mDestination.SetExtended(kDstAddr);
        buildInfo.mPanIds.SetSource(kSrcPanId);
        buildInfo.mPanIds.SetDestination(kDstPanId);
        buildInfo.mSecurityLevel = Mac::Frame::kSecurityEncMic32;

        txFrame.PrepareHeadersWithEmptyPayload(buildInfo);
    }

    txFrame.mInfo.mTxInfo.mCsmaCaEnabled   = true;
    txFrame.mInfo.mTxInfo.mMaxCsmaBackoffs = 1;
    txFrame.mChannel                       = 11;

    EXPECT_CALL(platform, Transmit(Truly([](otRadioFrame *aFrame) -> bool {
                    Mac::Frame &frame = *static_cast<Mac::Frame *>(aFrame);
                    return frame.mInfo.mTxInfo.mCsmaCaEnabled == true;
                })))
        .Times(1);

    // Receive(11) will be called exactly twice:
    // 1. one time to prepare for TX because the fake platform doesn't support sleep-to-tx capability.
    // 2. one time in CSMA backoff because rx-on-when-idle is true.
    EXPECT_CALL(platform, Receive(11)).Times(2);

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ASSERT_EQ(platform.mRadioSpinel.Transmit(txFrame), kErrorNone);

    platform.GoInMs(1000);
}

TEST(RadioSpinelTransmit, shouldNotCauseSwitchingToRxAfterTxDoneIfNotRxOnWhenIdle)
{
    FakeCoprocessorPlatform platform;

    constexpr Mac::PanId kSrcPanId  = 0x1234;
    constexpr Mac::PanId kDstPanId  = 0x4321;
    constexpr uint8_t    kDstAddr[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
    constexpr uint16_t   kSrcAddr   = 0xac00;
    constexpr int8_t     kTxPower   = 100;

    uint8_t      frameBuffer[OT_RADIO_FRAME_MAX_SIZE];
    Mac::TxFrame txFrame;

    txFrame.mPsdu = frameBuffer;

    {
        Mac::TxFrame::BuildInfo buildInfo;

        buildInfo.mType    = Mac::Frame::kTypeData;
        buildInfo.mVersion = Mac::Frame::kVersion2006;
        buildInfo.mAddrs.mSource.SetShort(kSrcAddr);
        buildInfo.mAddrs.mDestination.SetExtended(kDstAddr);
        buildInfo.mPanIds.SetSource(kSrcPanId);
        buildInfo.mPanIds.SetDestination(kDstPanId);
        buildInfo.mSecurityLevel = Mac::Frame::kSecurityEncMic32;

        txFrame.PrepareHeadersWithEmptyPayload(buildInfo);
    }

    txFrame.mInfo.mTxInfo.mTxPower              = kTxPower;
    txFrame.mChannel                            = 11;
    txFrame.mInfo.mTxInfo.mRxChannelAfterTxDone = 25;

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ASSERT_EQ(platform.mRadioSpinel.Receive(11), kErrorNone);
    ASSERT_EQ(platform.mRadioSpinel.SetRxOnWhenIdle(false), kErrorNone);
    ASSERT_EQ(platform.mRadioSpinel.Transmit(txFrame), kErrorNone);
    platform.GoInMs(1000);
    EXPECT_EQ(platform.GetReceiveChannel(), 11);
}

TEST(RadioSpinelReceiveAt, shouldReceiveAtGiveRadioTime)
{
    class MockPlatform : public FakeCoprocessorPlatform
    {
    public:
        MOCK_METHOD(otError, ReceiveAt, (uint8_t aChannel, uint32_t aStart, uint32_t aDuration), (override));
    };

    MockPlatform platform;

    ON_CALL(platform, ReceiveAt)
        .WillByDefault([&platform](uint8_t aChannel, uint32_t aStart, uint32_t aDuration) -> otError {
            return platform.FakePlatform::ReceiveAt(aChannel, aStart, aDuration);
        });

    EXPECT_CALL(platform, ReceiveAt).Times(1);

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ASSERT_EQ(platform.mRadioSpinel.SetRxOnWhenIdle(false), kErrorNone);
    ASSERT_EQ(platform.mRadioSpinel.ReceiveAt(100000, 10000, 11), kErrorNone);
    platform.GoInUs(100000);
    EXPECT_EQ(platform.GetReceiveChannel(), 0);
    platform.GoInUs(1);
    EXPECT_EQ(platform.GetReceiveChannel(), 11);
    platform.GoInUs(10000);
    EXPECT_EQ(platform.GetReceiveChannel(), 0);
}

TEST(RadioSpinelTransmit, shouldSkipCsmaBackoffWhenCsmaCaIsEnabledAndMaxBackoffsIsZero)
{
    class MockPlatform : public FakeCoprocessorPlatform
    {
    public:
        MOCK_METHOD(otError, Transmit, (otRadioFrame * aFrame), (override));
        MOCK_METHOD(otError, Receive, (uint8_t aChannel), (override));
    };

    MockPlatform platform;

    constexpr Mac::PanId kSrcPanId  = 0x1234;
    constexpr Mac::PanId kDstPanId  = 0x4321;
    constexpr uint8_t    kDstAddr[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
    constexpr uint16_t   kSrcAddr   = 0xac00;
    constexpr int8_t     kTxPower   = 100;

    uint8_t      frameBuffer[OT_RADIO_FRAME_MAX_SIZE];
    Mac::TxFrame txFrame{};

    txFrame.mPsdu = frameBuffer;

    {
        Mac::TxFrame::BuildInfo buildInfo;

        buildInfo.mType    = Mac::Frame::kTypeData;
        buildInfo.mVersion = Mac::Frame::kVersion2006;
        buildInfo.mAddrs.mSource.SetShort(kSrcAddr);
        buildInfo.mAddrs.mDestination.SetExtended(kDstAddr);
        buildInfo.mPanIds.SetSource(kSrcPanId);
        buildInfo.mPanIds.SetDestination(kDstPanId);
        buildInfo.mSecurityLevel = Mac::Frame::kSecurityEncMic32;

        txFrame.PrepareHeadersWithEmptyPayload(buildInfo);
    }

    txFrame.mInfo.mTxInfo.mCsmaCaEnabled   = true;
    txFrame.mInfo.mTxInfo.mMaxCsmaBackoffs = 0;
    txFrame.mChannel                       = 11;

    EXPECT_CALL(platform, Transmit(Truly([](otRadioFrame *aFrame) -> bool {
                    Mac::Frame &frame = *static_cast<Mac::Frame *>(aFrame);
                    return frame.mInfo.mTxInfo.mCsmaCaEnabled == true && frame.mInfo.mTxInfo.mMaxCsmaBackoffs == 0;
                })))
        .Times(1);

    EXPECT_CALL(platform, Receive).Times(AnyNumber());
    // Receive(11) will be called exactly once to prepare for TX because the fake platform doesn't support sleep-to-tx
    // capability.
    EXPECT_CALL(platform, Receive(11)).Times(1);

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ASSERT_EQ(platform.mRadioSpinel.Transmit(txFrame), kErrorNone);

    platform.GoInMs(1000);
}

TEST(RadioSpinelSrcMatch, shouldBeAbleToEnableRadioSrcMatch)
{
    FakeCoprocessorPlatform platform;

    platform.SrcMatchEnable(false);
    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ASSERT_EQ(platform.mRadioSpinel.EnableSrcMatch(true), kErrorNone);
    ASSERT_EQ(platform.SrcMatchIsEnabled(), true);
}

TEST(RadioSpinelSrcMatch, shouldBeAbleToDisableRadioSrcMatch)
{
    FakeCoprocessorPlatform platform;

    platform.SrcMatchEnable(true);
    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ASSERT_EQ(platform.mRadioSpinel.EnableSrcMatch(false), kErrorNone);
    ASSERT_EQ(platform.SrcMatchIsEnabled(), false);
}

TEST(RadioSpinelSrcMatch, shouldBeAbleToAddRadioSrcMatchShortEntry)
{
    constexpr uint16_t      kTestShortAddr = 0x1234;
    FakeCoprocessorPlatform platform;

    platform.SrcMatchEnable(true);

    ASSERT_EQ(platform.SrcMatchHasShortEntry(kTestShortAddr), 0);

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ASSERT_EQ(platform.mRadioSpinel.AddSrcMatchShortEntry(kTestShortAddr), kErrorNone);

    ASSERT_EQ(platform.SrcMatchHasShortEntry(kTestShortAddr), 1);
}

TEST(RadioSpinelSrcMatch, shouldBeAbleToClearRadioSrcMatchShortEntry)
{
    constexpr uint16_t      kTestShortAddr = 0x1234;
    FakeCoprocessorPlatform platform;

    platform.SrcMatchEnable(true);
    platform.SrcMatchAddShortEntry(kTestShortAddr);

    ASSERT_EQ(platform.SrcMatchHasShortEntry(kTestShortAddr), 1);

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ASSERT_EQ(platform.mRadioSpinel.ClearSrcMatchShortEntry(kTestShortAddr), kErrorNone);

    ASSERT_EQ(platform.SrcMatchHasShortEntry(kTestShortAddr), 0);
}

TEST(RadioSpinelSrcMatch, shouldBeAbleToAddRadioSrcMatchExtEntry)
{
    constexpr otExtAddress  kTestExtAddr{0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
    constexpr otExtAddress  kTestExtAddrReversed{0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11};
    FakeCoprocessorPlatform platform;

    platform.SrcMatchEnable(true);
    platform.SrcMatchClearExtEntries();

    ASSERT_EQ(platform.SrcMatchHasExtEntry(kTestExtAddr), 0);
    ASSERT_EQ(platform.SrcMatchHasExtEntry(kTestExtAddrReversed), 0);

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ASSERT_EQ(platform.mRadioSpinel.AddSrcMatchExtEntry(kTestExtAddr), kErrorNone);

    ASSERT_EQ(platform.SrcMatchHasExtEntry(kTestExtAddr), 0);
    ASSERT_EQ(platform.SrcMatchHasExtEntry(kTestExtAddrReversed), 1);
}

TEST(RadioSpinelSrcMatch, shouldBeAbleToClearRadioSrcMatchExtEntry)
{
    constexpr otExtAddress  kTestExtAddr{0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
    constexpr otExtAddress  kTestExtAddrReversed{0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11};
    FakeCoprocessorPlatform platform;

    platform.SrcMatchEnable(true);
    platform.SrcMatchAddExtEntry(kTestExtAddrReversed);

    ASSERT_EQ(platform.SrcMatchHasExtEntry(kTestExtAddrReversed), 1);

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ASSERT_EQ(platform.mRadioSpinel.ClearSrcMatchExtEntry(kTestExtAddr), kErrorNone);

    ASSERT_EQ(platform.SrcMatchHasExtEntry(kTestExtAddrReversed), 0);
}

TEST(RadioSpinelSrcMatch, shouldBeAbleToClearAllRadioSrcMatchShortEntres)
{
    constexpr uint16_t      kTestShortAddr = 0x1234;
    FakeCoprocessorPlatform platform;

    platform.SrcMatchEnable(true);
    platform.SrcMatchAddShortEntry(kTestShortAddr);

    ASSERT_EQ(platform.SrcMatchHasShortEntry(kTestShortAddr), 1);

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ASSERT_EQ(platform.mRadioSpinel.ClearSrcMatchShortEntries(), kErrorNone);

    ASSERT_EQ(platform.SrcMatchCountShortEntries(), 0);
}

TEST(RadioSpinelSrcMatch, shouldBeAbleToClearAllRadioSrcMatchExtEntres)
{
    constexpr otExtAddress  kTestExtAddrReversed{0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11};
    FakeCoprocessorPlatform platform;

    platform.SrcMatchEnable(true);
    platform.SrcMatchAddExtEntry(kTestExtAddrReversed);

    ASSERT_EQ(platform.SrcMatchHasExtEntry(kTestExtAddrReversed), 1);

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ASSERT_EQ(platform.mRadioSpinel.ClearSrcMatchExtEntries(), kErrorNone);

    ASSERT_EQ(platform.SrcMatchCountExtEntries(), 0);
}

#if OPENTHREAD_SPINEL_CONFIG_RCP_RESTORATION_MAX_COUNT > 0
TEST(RadioSpinelMaxPowerTable, shouldRestoreEachChannelWithItsOwnPower)
{
    constexpr uint8_t kChannelA = 11;
    constexpr uint8_t kChannelB = 15;
    constexpr uint8_t kChannelC = 26;
    constexpr int8_t  kPowerA   = 5;
    constexpr int8_t  kPowerB   = 14;
    constexpr int8_t  kPowerC   = 20;

    FakeCoprocessorPlatform platform;

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);

    ASSERT_EQ(platform.mRadioSpinel.SetChannelMaxTransmitPower(kChannelA, kPowerA), kErrorNone);
    ASSERT_EQ(platform.mRadioSpinel.SetChannelMaxTransmitPower(kChannelB, kPowerB), kErrorNone);
    ASSERT_EQ(platform.mRadioSpinel.SetChannelMaxTransmitPower(kChannelC, kPowerC), kErrorNone);

    ASSERT_EQ(platform.ChannelMaxTxPowerGet(kChannelA), kPowerA);
    ASSERT_EQ(platform.ChannelMaxTxPowerGet(kChannelB), kPowerB);
    ASSERT_EQ(platform.ChannelMaxTxPowerGet(kChannelC), kPowerC);

    // Forget what the RCP was told, so what follows can only come from the
    // restore itself.
    platform.ChannelMaxTxPowerClear();
    ASSERT_EQ(platform.ChannelMaxTxPowerCount(), 0u);

    platform.mRadioSpinel.RestoreProperties();

    // Each configured channel comes back with its own power, not with a
    // neighbour's and not with the table default.
    EXPECT_EQ(platform.ChannelMaxTxPowerGet(kChannelA), kPowerA);
    EXPECT_EQ(platform.ChannelMaxTxPowerGet(kChannelB), kPowerB);
    EXPECT_EQ(platform.ChannelMaxTxPowerGet(kChannelC), kPowerC);

    // Channels never configured keep the table default, which is what the
    // restore sends for them. Copied into a local first: `kPowerDefault` is a
    // `static constexpr` with no out-of-line definition, and EXPECT_EQ binds
    // its arguments by reference, which would odr-use it.
    constexpr int8_t kDefaultPower = MaxPowerTable::kPowerDefault;
    EXPECT_EQ(platform.ChannelMaxTxPowerGet(12), kDefaultPower);
}

TEST(RadioSpinelMaxPowerTable, shouldNotTouchTheRcpWhenNoChannelWasConfigured)
{
    FakeCoprocessorPlatform platform;

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);

    platform.ChannelMaxTxPowerClear();
    platform.mRadioSpinel.RestoreProperties();

    // Nothing was configured, so the restore must not send the table's
    // defaults to the RCP -- 16 blocking transactions on every recovery for a
    // feature nobody asked for.
    EXPECT_EQ(platform.ChannelMaxTxPowerCount(), 0u);
}

TEST(RadioSpinelMaxPowerTable, shouldSurviveAnRcpThatDoesNotImplementIt)
{
    constexpr uint8_t kChannel = 11;
    constexpr int8_t  kPower   = 5;

    FakeCoprocessorPlatform platform;

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ASSERT_EQ(platform.mRadioSpinel.SetChannelMaxTransmitPower(kChannel, kPower), kErrorNone);

    // From here the RCP refuses the property. A firmware that lacks it answers
    // `SPINEL_STATUS_PROP_NOT_FOUND` while this returns
    // `SPINEL_STATUS_UNIMPLEMENTED`, but both reach the host as
    // `OT_ERROR_NOT_IMPLEMENTED`, which is the path under test.
    platform.ChannelMaxTxPowerFailWith(OT_ERROR_NOT_IMPLEMENTED);
    platform.ChannelMaxTxPowerClear();

    // Must not abort: this used to be a `DieNow()`, so a regression takes the
    // whole test binary down rather than failing an assertion.
    platform.mRadioSpinel.RestoreProperties();

    EXPECT_EQ(platform.ChannelMaxTxPowerCount(), 0u);

    // And it must give up rather than ask again on every later recovery, even
    // once the RCP would accept it.
    platform.ChannelMaxTxPowerFailWith(OT_ERROR_NONE);
    platform.mRadioSpinel.RestoreProperties();

    EXPECT_EQ(platform.ChannelMaxTxPowerCount(), 0u);
}

TEST(RadioSpinelSrcMatch, shouldNotDuplicateSrcMatchEntriesOnRestoreProperties)
{
    constexpr uint16_t      kTestShortAddr = 0x1234;
    constexpr otExtAddress  kTestExtAddr{0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
    constexpr otExtAddress  kTestExtAddrReversed{0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11};
    FakeCoprocessorPlatform platform;

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);

    ASSERT_EQ(platform.mRadioSpinel.AddSrcMatchShortEntry(kTestShortAddr), kErrorNone);
    ASSERT_EQ(platform.mRadioSpinel.AddSrcMatchExtEntry(kTestExtAddr), kErrorNone);

    ASSERT_EQ(platform.SrcMatchCountShortEntries(), 1);
    ASSERT_EQ(platform.SrcMatchCountExtEntries(), 1);
    ASSERT_EQ(platform.SrcMatchHasShortEntry(kTestShortAddr), 1);
    ASSERT_EQ(platform.SrcMatchHasExtEntry(kTestExtAddrReversed), 1);

    // Simulate RCP recovery by calling RestoreProperties multiple times.
    // Without clearing entries first, this would create duplicates.
    platform.mRadioSpinel.RestoreProperties();
    platform.mRadioSpinel.RestoreProperties();

    ASSERT_EQ(platform.SrcMatchCountShortEntries(), 1);
    ASSERT_EQ(platform.SrcMatchCountExtEntries(), 1);
    ASSERT_EQ(platform.SrcMatchHasShortEntry(kTestShortAddr), 1);
    ASSERT_EQ(platform.SrcMatchHasExtEntry(kTestExtAddrReversed), 1);
}
#endif // OPENTHREAD_SPINEL_CONFIG_RCP_RESTORATION_MAX_COUNT > 0

TEST(RadioSpinelProperty, shouldSetEmptyPropertyValue)
{
    constexpr uint16_t      kTestShortAddr = 0x1234;
    const uint8_t          *empty          = nullptr;
    FakeCoprocessorPlatform platform;

    platform.SrcMatchEnable(true);
    platform.SrcMatchAddShortEntry(kTestShortAddr);
    ASSERT_EQ(platform.SrcMatchCountShortEntries(), 1);

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    EXPECT_EQ(platform.mRadioSpinel.Set(SPINEL_PROP_MAC_SRC_MATCH_SHORT_ADDRESSES, SPINEL_DATATYPE_DATA_S, empty, 0),
              kErrorNone);
    EXPECT_EQ(platform.SrcMatchCountShortEntries(), 0);
}

TEST(RadioSpinelProperty, shouldGetEmptyPropertyValue)
{
    FakeCoprocessorPlatform platform;
    uint8_t                 buffer[32];
    spinel_size_t           length = sizeof(buffer);

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);

    ASSERT_EQ(platform.mRadioSpinel.Get(SPINEL_PROP_MAC_SCAN_MASK, SPINEL_DATATYPE_DATA_S, buffer, &length),
              kErrorNone);
    ASSERT_GT(length, 0u);

    ASSERT_EQ(platform.mRadioSpinel.Set(SPINEL_PROP_MAC_SCAN_MASK, SPINEL_DATATYPE_DATA_S, nullptr, 0), kErrorNone);

    length = sizeof(buffer);
    EXPECT_EQ(platform.mRadioSpinel.Get(SPINEL_PROP_MAC_SCAN_MASK, SPINEL_DATATYPE_DATA_S, buffer, &length),
              kErrorNone);
    EXPECT_EQ(length, 0u);
    EXPECT_EQ(platform.mRadioSpinel.GetRadioSpinelMetrics().mSpinelParseErrorCount, 0u);
}

TEST(RadioSpinelRadioAvailability, shouldSendRadioAvailabilityPropertyWhenUpdated)
{
    struct FrameCapture
    {
        bool                 mReceived       = false;
        spinel_status_t      mLastStatus     = SPINEL_STATUS_OK;
        uint64_t             mStartTime      = 0;
        uint8_t              mSlotDuration   = 0;
        uint8_t              mNumSlots       = 0;
        std::vector<uint8_t> mAvailableSlots = {};
        std::vector<uint8_t> mPreferredSlots = {};
    };

    FakeCoprocessorPlatform platform;
    FrameCapture            capture;

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);

    platform.mSpinelDriver.SetFrameHandler(
        [](const uint8_t *aFrame, uint16_t aLength, uint8_t aHeader, bool &aSave, void *aContext) {
            OT_UNUSED_VARIABLE(aHeader);

            FrameCapture     *ctx          = static_cast<FrameCapture *>(aContext);
            uint8_t           header       = 0;
            unsigned int      cmd          = 0;
            spinel_prop_key_t key          = 0;
            const uint8_t    *valuePtr     = nullptr;
            spinel_size_t     valueLen     = 0;
            const uint8_t    *availPtr     = nullptr;
            spinel_size_t     availLen     = 0;
            const uint8_t    *preferredPtr = nullptr;
            spinel_size_t     preferredLen = 0;

            aSave = false;

            ASSERT_GT(spinel_datatype_unpack(aFrame, aLength, "CiiD", &header, &cmd, &key, &valuePtr, &valueLen), 0);

            if ((cmd == SPINEL_CMD_PROP_VALUE_IS) && (key == SPINEL_PROP_LAST_STATUS))
            {
                ASSERT_GT(spinel_datatype_unpack(valuePtr, valueLen, "i", &ctx->mLastStatus), 0);
            }

            if ((cmd == SPINEL_CMD_PROP_VALUE_IS) && (key == SPINEL_PROP_RCP_RADIO_AVAILABILITY))
            {
                ctx->mReceived = true;
                ASSERT_GT(spinel_datatype_unpack(valuePtr, valueLen, "XCCdD", &ctx->mStartTime, &ctx->mSlotDuration,
                                                 &ctx->mNumSlots, &availPtr, &availLen, &preferredPtr, &preferredLen),
                          0);
                ctx->mAvailableSlots.assign(availPtr, availPtr + availLen);

                if (preferredLen > 0)
                {
                    const uint8_t *prefDataPtr = nullptr;
                    spinel_size_t  prefDataLen = 0;

                    ASSERT_GT(spinel_datatype_unpack(preferredPtr, preferredLen, "d", &prefDataPtr, &prefDataLen), 0);
                    ctx->mPreferredSlots.assign(prefDataPtr, prefDataPtr + prefDataLen);
                }
                else
                {
                    ctx->mPreferredSlots.clear();
                }
            }
        },
        nullptr, &capture);

    // 1. Update with both available slots and preferred slots (12 slots -> 2 bytes)
    {
        const uint8_t kAvailSlots[]     = {0xa5, 0xf0};
        const uint8_t kPreferredSlots[] = {0x05, 0x30};

        capture = FrameCapture{};
        platform.UpdateRadioAvailability(123456789ULL, OT_RADIO_AVAILABILITY_SLOT_DURATION_625_US, kAvailSlots,
                                         kPreferredSlots, 12);
        platform.GoInMs(1);

        EXPECT_TRUE(capture.mReceived);
        EXPECT_EQ(capture.mStartTime, 123456789ULL);
        EXPECT_EQ(capture.mSlotDuration, OT_RADIO_AVAILABILITY_SLOT_DURATION_625_US);
        EXPECT_EQ(capture.mNumSlots, 12);
        EXPECT_EQ(capture.mAvailableSlots, std::vector<uint8_t>({0xa5, 0xf0}));
        EXPECT_EQ(capture.mPreferredSlots, std::vector<uint8_t>({0x05, 0x30}));
    }

    // 2. Update without preferred slots (nullptr) (8 slots -> 1 byte)
    {
        const uint8_t kAvailSlots[] = {0x5a};

        capture = FrameCapture{};
        platform.UpdateRadioAvailability(987654321ULL, OT_RADIO_AVAILABILITY_SLOT_DURATION_1250_US, kAvailSlots,
                                         nullptr, 8);
        platform.GoInMs(1);

        EXPECT_TRUE(capture.mReceived);
        EXPECT_EQ(capture.mStartTime, 987654321ULL);
        EXPECT_EQ(capture.mSlotDuration, OT_RADIO_AVAILABILITY_SLOT_DURATION_1250_US);
        EXPECT_EQ(capture.mNumSlots, 8);
        EXPECT_EQ(capture.mAvailableSlots, std::vector<uint8_t>({0x5a}));
        EXPECT_TRUE(capture.mPreferredSlots.empty());
    }

    // 3. Update with 0 slots (radio always available)
    {
        const uint8_t kPreferredSlots[] = {0x05};

        capture = FrameCapture{};
        platform.UpdateRadioAvailability(555666777ULL, OT_RADIO_AVAILABILITY_SLOT_DURATION_625_US, nullptr,
                                         kPreferredSlots, 0);
        platform.GoInMs(1);

        EXPECT_TRUE(capture.mReceived);
        EXPECT_EQ(capture.mStartTime, 555666777ULL);
        EXPECT_EQ(capture.mNumSlots, 0);
        EXPECT_TRUE(capture.mAvailableSlots.empty());
        EXPECT_TRUE(capture.mPreferredSlots.empty());
    }

    // 4. Multiple updates before the tasklet runs are coalesced and the latest schedule is sent.
    {
        const uint8_t kOldAvailSlots[] = {0x11};
        const uint8_t kNewAvailSlots[] = {0x22};

        capture = FrameCapture{};
        platform.UpdateRadioAvailability(1000ULL, OT_RADIO_AVAILABILITY_SLOT_DURATION_625_US, kOldAvailSlots, nullptr,
                                         8);
        platform.UpdateRadioAvailability(2000ULL, OT_RADIO_AVAILABILITY_SLOT_DURATION_1250_US, kNewAvailSlots, nullptr,
                                         8);
        platform.GoInMs(1);

        EXPECT_TRUE(capture.mReceived);
        EXPECT_EQ(capture.mStartTime, 2000ULL);
        EXPECT_EQ(capture.mSlotDuration, OT_RADIO_AVAILABILITY_SLOT_DURATION_1250_US);
        EXPECT_EQ(capture.mNumSlots, 8);
        EXPECT_EQ(capture.mAvailableSlots, std::vector<uint8_t>({0x22}));
        EXPECT_TRUE(capture.mPreferredSlots.empty());

        capture = FrameCapture{};
        platform.GoInMs(1);

        EXPECT_FALSE(capture.mReceived);
    }

    // 5. The preferred slots bitmap is omitted if it equals the available slots bitmap.
    {
        const uint8_t kSlots[] = {0x3c, 0xf0};

        capture = FrameCapture{};
        platform.UpdateRadioAvailability(3000ULL, OT_RADIO_AVAILABILITY_SLOT_DURATION_625_US, kSlots, kSlots, 12);
        platform.GoInMs(1);

        EXPECT_TRUE(capture.mReceived);
        EXPECT_EQ(capture.mNumSlots, 12);
        EXPECT_EQ(capture.mAvailableSlots, std::vector<uint8_t>({0x3c, 0xf0}));
        EXPECT_TRUE(capture.mPreferredSlots.empty());
    }

    // 6. A schedule longer than `OT_RADIO_AVAILABILITY_MAX_DURATION_US` is reported as an error.
    {
        uint8_t availSlots[OT_RADIO_AVAILABILITY_MAX_BITMAP_SIZE];

        memset(availSlots, 0xff, sizeof(availSlots));

        // 97 slots of 625 us (60.625 ms), which would not fit into the bitmap arrays.
        capture = FrameCapture{};
        platform.UpdateRadioAvailability(4000ULL, OT_RADIO_AVAILABILITY_SLOT_DURATION_625_US, availSlots, nullptr, 97);
        platform.GoInMs(1);

        EXPECT_FALSE(capture.mReceived);
        EXPECT_EQ(capture.mLastStatus, SPINEL_STATUS_INTERNAL_ERROR);

        // 49 slots of 1.25 ms (61.25 ms), which would fit into the bitmap arrays.
        capture = FrameCapture{};
        platform.UpdateRadioAvailability(5000ULL, OT_RADIO_AVAILABILITY_SLOT_DURATION_1250_US, availSlots, nullptr, 49);
        platform.GoInMs(1);

        EXPECT_FALSE(capture.mReceived);
        EXPECT_EQ(capture.mLastStatus, SPINEL_STATUS_INTERNAL_ERROR);

        // 96 slots of 625 us (60 ms) is the maximum.
        capture = FrameCapture{};
        platform.UpdateRadioAvailability(6000ULL, OT_RADIO_AVAILABILITY_SLOT_DURATION_625_US, availSlots, nullptr, 96);
        platform.GoInMs(1);

        EXPECT_TRUE(capture.mReceived);
        EXPECT_EQ(capture.mNumSlots, 96);
        EXPECT_EQ(capture.mAvailableSlots.size(), static_cast<size_t>(OT_RADIO_AVAILABILITY_MAX_BITMAP_SIZE));
    }
}

TEST(RadioSpinelRadioAvailability, shouldGetRadioAvailabilityProperty)
{
    FakeCoprocessorPlatform platform;
    uint64_t                startTime    = 0;
    uint8_t                 slotDuration = 0;
    uint8_t                 numSlots     = 0;
    uint8_t                 availSlots[OT_RADIO_AVAILABILITY_MAX_BITMAP_SIZE];
    spinel_size_t           availLen = sizeof(availSlots);
    uint8_t                 preferredSlots[OT_RADIO_AVAILABILITY_MAX_BITMAP_SIZE];
    spinel_size_t           preferredLen = sizeof(preferredSlots);

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);

    // 1. The radio driver has not reported radio availability.
    EXPECT_EQ(
        platform.mRadioSpinel.Get(SPINEL_PROP_RCP_RADIO_AVAILABILITY, SPINEL_DATATYPE_DATA_S, availSlots, &availLen),
        kErrorNotImplemented);
    availLen = sizeof(availSlots);

    // 2. The radio driver has reported radio availability. The unused bits in the last byte are cleared.
    {
        const uint8_t kAvailSlots[]     = {0xa5, 0x0f};
        const uint8_t kPreferredSlots[] = {0x05, 0x03};

        platform.UpdateRadioAvailability(123456789ULL, OT_RADIO_AVAILABILITY_SLOT_DURATION_1250_US, kAvailSlots,
                                         kPreferredSlots, 12);
        platform.GoInMs(1);

        ASSERT_EQ(platform.mRadioSpinel.Get(SPINEL_PROP_RCP_RADIO_AVAILABILITY,
                                            SPINEL_DATATYPE_UINT64_S SPINEL_DATATYPE_UINT8_S SPINEL_DATATYPE_UINT8_S
                                                SPINEL_DATATYPE_DATA_WLEN_S SPINEL_DATATYPE_DATA_WLEN_S,
                                            &startTime, &slotDuration, &numSlots, availSlots, &availLen, preferredSlots,
                                            &preferredLen),
                  kErrorNone);

        EXPECT_EQ(startTime, 123456789ULL);
        EXPECT_EQ(slotDuration, OT_RADIO_AVAILABILITY_SLOT_DURATION_1250_US);
        EXPECT_EQ(numSlots, 12);
        EXPECT_EQ(std::vector<uint8_t>(availSlots, availSlots + availLen), std::vector<uint8_t>({0xa5, 0x00}));
        EXPECT_EQ(std::vector<uint8_t>(preferredSlots, preferredSlots + preferredLen),
                  std::vector<uint8_t>({0x05, 0x00}));
    }
}

TEST(RadioSpinelRadioAvailability, shouldSendRadioAvailabilityPropertyAfterRadioStackReset)
{
    FakeCoprocessorPlatform platform;
    bool                    received = false;

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);

    platform.mSpinelDriver.SetFrameHandler(
        [](const uint8_t *aFrame, uint16_t aLength, uint8_t aHeader, bool &aSave, void *aContext) {
            unsigned int      cmd = 0;
            spinel_prop_key_t key = 0;

            OT_UNUSED_VARIABLE(aHeader);
            aSave = false;

            if ((spinel_datatype_unpack(aFrame, aLength, "Cii", nullptr, &cmd, &key) > 0) &&
                (cmd == SPINEL_CMD_PROP_VALUE_IS) && (key == SPINEL_PROP_RCP_RADIO_AVAILABILITY))
            {
                *static_cast<bool *>(aContext) = true;
            }
        },
        nullptr, &received);

    // The radio availability callback is kept across `otInstanceResetRadioStack()`.
    otInstanceResetRadioStack(FakePlatform::CurrentInstance());

    {
        const uint8_t kAvailSlots[] = {0x5a};

        platform.UpdateRadioAvailability(1000ULL, OT_RADIO_AVAILABILITY_SLOT_DURATION_625_US, kAvailSlots, nullptr, 8);
        platform.GoInMs(1);
    }

    EXPECT_TRUE(received);
}

#if OPENTHREAD_CONFIG_LINK_EXT_ENABLE
namespace {

void PrepareTestFrame(Mac::TxFrame &aFrame, uint8_t (&aPsduBuffer)[OT_RADIO_FRAME_MAX_SIZE], uint8_t aChannel)
{
    constexpr Mac::PanId kSrcPanId  = 0x1234;
    constexpr Mac::PanId kDstPanId  = 0x4321;
    constexpr uint8_t    kDstAddr[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
    constexpr uint16_t   kSrcAddr   = 0xac00;

    Mac::TxFrame::BuildInfo buildInfo;

    memset(&aFrame, 0, sizeof(aFrame));
    aFrame.mPsdu = aPsduBuffer;

    buildInfo.mType    = Mac::Frame::kTypeData;
    buildInfo.mVersion = Mac::Frame::kVersion2006;
    buildInfo.mAddrs.mSource.SetShort(kSrcAddr);
    buildInfo.mAddrs.mDestination.SetExtended(kDstAddr);
    buildInfo.mPanIds.SetSource(kSrcPanId);
    buildInfo.mPanIds.SetDestination(kDstPanId);
    buildInfo.mSecurityLevel = Mac::Frame::kSecurityNone;

    aFrame.PrepareHeadersWithEmptyPayload(buildInfo);
    aFrame.SetAckRequest(false);

    aFrame.mChannel                            = aChannel;
    aFrame.mInfo.mTxInfo.mRxChannelAfterTxDone = aChannel;
    aFrame.mInfo.mTxInfo.mCsmaCaEnabled        = false;
    aFrame.mInfo.mTxInfo.mMaxCsmaBackoffs      = 0;
    aFrame.mInfo.mTxInfo.mMaxFrameRetries      = 0;
    aFrame.mInfo.mTxInfo.mTxPower              = 0;
}

otError SendSpinelCommand(FakeCoprocessorPlatform &aPlatform,
                          uint32_t                 aCommand,
                          spinel_prop_key_t        aKey,
                          spinel_tid_t             aTid,
                          const char              *aFormat,
                          ...)
{
    va_list args;
    otError error;

    va_start(args, aFormat);
    error = aPlatform.mSpinelDriver.SendCommand(aCommand, aKey, aTid, aFormat, args);
    va_end(args);

    return error;
}

otError SendStreamRaw(FakeCoprocessorPlatform &aPlatform, spinel_tid_t aTid, const Mac::TxFrame &aFrame)
{
    return SendSpinelCommand(aPlatform, SPINEL_CMD_PROP_VALUE_SET, SPINEL_PROP_STREAM_RAW, aTid,
                             SPINEL_DATATYPE_DATA_WLEN_S SPINEL_DATATYPE_UINT8_S, aFrame.mPsdu, aFrame.mLength,
                             aFrame.mChannel);
}

otError SendTimedStreamRaw(FakeCoprocessorPlatform &aPlatform,
                           spinel_tid_t             aTid,
                           const Mac::TxFrame      &aFrame,
                           uint32_t                 aTxDelay)
{
    // Requests a transmission that waits in `SubMac` for its target TX time, `aTxDelay` from now.
    return SendSpinelCommand(
        aPlatform, SPINEL_CMD_PROP_VALUE_SET, SPINEL_PROP_STREAM_RAW, aTid,
        SPINEL_DATATYPE_DATA_WLEN_S SPINEL_DATATYPE_UINT8_S SPINEL_DATATYPE_UINT8_S SPINEL_DATATYPE_UINT8_S
            SPINEL_DATATYPE_BOOL_S SPINEL_DATATYPE_BOOL_S SPINEL_DATATYPE_BOOL_S SPINEL_DATATYPE_BOOL_S
                SPINEL_DATATYPE_UINT32_S SPINEL_DATATYPE_UINT32_S,
        aFrame.mPsdu, aFrame.mLength, aFrame.mChannel, /* aMaxCsmaBackoffs */ 0, /* aMaxFrameRetries */ 0,
        /* aCsmaCaEnabled */ false, /* aIsHeaderUpdated */ false, /* aIsARetx */ false,
        /* aIsSecurityProcessed */ false, aTxDelay, static_cast<uint32_t>(aPlatform.GetNow()));
}

otError SendTxBurst(FakeCoprocessorPlatform &aPlatform,
                    spinel_tid_t             aTid,
                    const Mac::TxFrame      &aFrame,
                    uint16_t                 aBurstCount,
                    uint32_t                 aBurstPeriodUs,
                    uint32_t                 aBurstChannelMask)
{
    return SendSpinelCommand(
        aPlatform, SPINEL_CMD_PROP_VALUE_SET, SPINEL_PROP_RCP_TX_BURST, aTid,
        SPINEL_DATATYPE_STRUCT_S(SPINEL_DATATYPE_UINT16_S SPINEL_DATATYPE_UINT32_S SPINEL_DATATYPE_UINT32_S)
            SPINEL_DATATYPE_DATA_WLEN_S SPINEL_DATATYPE_UINT8_S SPINEL_DATATYPE_UINT8_S SPINEL_DATATYPE_UINT8_S
                SPINEL_DATATYPE_BOOL_S SPINEL_DATATYPE_BOOL_S SPINEL_DATATYPE_BOOL_S SPINEL_DATATYPE_BOOL_S
                    SPINEL_DATATYPE_UINT32_S SPINEL_DATATYPE_UINT32_S SPINEL_DATATYPE_UINT8_S SPINEL_DATATYPE_INT8_S,
        aBurstCount, aBurstPeriodUs, aBurstChannelMask, aFrame.mPsdu, aFrame.mLength, aFrame.mChannel,
        aFrame.mInfo.mTxInfo.mMaxCsmaBackoffs, aFrame.mInfo.mTxInfo.mMaxFrameRetries,
        aFrame.mInfo.mTxInfo.mCsmaCaEnabled, aFrame.mInfo.mTxInfo.mIsHeaderUpdated, aFrame.mInfo.mTxInfo.mIsARetx,
        aFrame.mInfo.mTxInfo.mIsSecurityProcessed, aFrame.mInfo.mTxInfo.mTxDelay, aFrame.mInfo.mTxInfo.mTxDelayBaseTime,
        aFrame.mInfo.mTxInfo.mRxChannelAfterTxDone, aFrame.mInfo.mTxInfo.mTxPower);
}

otError SendStopBurst(FakeCoprocessorPlatform &aPlatform, spinel_tid_t aTid)
{
    return SendSpinelCommand(aPlatform, SPINEL_CMD_PROP_VALUE_SET, SPINEL_PROP_RCP_TX_BURST, aTid,
                             SPINEL_DATATYPE_STRUCT_S(SPINEL_DATATYPE_UINT16_S), 0);
}

struct RcpFrame
{
    spinel_tid_t mTid;
    unsigned int mCommand;
    unsigned int mKey;
    unsigned int mStatus;      // Status of `LAST_STATUS`.
    uint16_t     mBurstCount;  // `Count` of `RCP_TX_BURST`.
    uint32_t     mBurstPeriod; // `PeriodUs` of `RCP_TX_BURST`.
    uint32_t     mBurstMask;   // `ChannelMask` of `RCP_TX_BURST`.
    bool         mBoolValue;   // Value of `MAC_RAW_STREAM_ENABLED` or `PHY_ENABLED`.
    uint64_t     mTimeUs;
};

void ObserveRcpFrames(FakeCoprocessorPlatform &aPlatform, std::vector<RcpFrame> &aFrames)
{
    aPlatform.mSpinelInterface.mFrameObserver = [&aPlatform, &aFrames](const uint8_t *aFrame, uint16_t aLength) {
        uint8_t  header;
        RcpFrame frame{};

        if (spinel_datatype_unpack(aFrame, aLength,
                                   SPINEL_DATATYPE_UINT8_S SPINEL_DATATYPE_UINT_PACKED_S SPINEL_DATATYPE_UINT_PACKED_S,
                                   &header, &frame.mCommand, &frame.mKey) <= 0)
        {
            return;
        }

        if (frame.mKey == SPINEL_PROP_LAST_STATUS)
        {
            IgnoreReturnValue(spinel_datatype_unpack(aFrame, aLength,
                                                     SPINEL_DATATYPE_UINT8_S SPINEL_DATATYPE_UINT_PACKED_S
                                                         SPINEL_DATATYPE_UINT_PACKED_S SPINEL_DATATYPE_UINT_PACKED_S,
                                                     &header, &frame.mCommand, &frame.mKey, &frame.mStatus));
        }
        else if (frame.mKey == SPINEL_PROP_RCP_TX_BURST)
        {
            IgnoreReturnValue(spinel_datatype_unpack(
                aFrame, aLength,
                SPINEL_DATATYPE_UINT8_S SPINEL_DATATYPE_UINT_PACKED_S SPINEL_DATATYPE_UINT_PACKED_S
                    SPINEL_DATATYPE_STRUCT_S(
                        SPINEL_DATATYPE_UINT16_S SPINEL_DATATYPE_UINT32_S SPINEL_DATATYPE_UINT32_S),
                &header, &frame.mCommand, &frame.mKey, &frame.mBurstCount, &frame.mBurstPeriod, &frame.mBurstMask));
        }
        else if ((frame.mKey == SPINEL_PROP_MAC_RAW_STREAM_ENABLED) || (frame.mKey == SPINEL_PROP_PHY_ENABLED))
        {
            IgnoreReturnValue(spinel_datatype_unpack(aFrame, aLength,
                                                     SPINEL_DATATYPE_UINT8_S SPINEL_DATATYPE_UINT_PACKED_S
                                                         SPINEL_DATATYPE_UINT_PACKED_S SPINEL_DATATYPE_BOOL_S,
                                                     &header, &frame.mCommand, &frame.mKey, &frame.mBoolValue));
        }
        else if (frame.mKey == SPINEL_PROP_MAC_ENERGY_SCAN_RESULT)
        {
            // Only the key is needed.
        }
        else
        {
            return;
        }

        frame.mTid    = SPINEL_HEADER_GET_TID(header);
        frame.mTimeUs = aPlatform.GetNow();
        aFrames.push_back(frame);
    };
}

int FindRcpFrame(const std::vector<RcpFrame> &aFrames, spinel_tid_t aTid, spinel_prop_key_t aKey)
{
    for (size_t i = 0; i < aFrames.size(); i++)
    {
        if ((aFrames[i].mTid == aTid) && (aFrames[i].mKey == aKey))
        {
            return static_cast<int>(i);
        }
    }

    return -1;
}

int CountRcpFrames(const std::vector<RcpFrame> &aFrames, spinel_tid_t aTid, spinel_prop_key_t aKey)
{
    int count = 0;

    for (const RcpFrame &frame : aFrames)
    {
        if ((frame.mTid == aTid) && (frame.mKey == aKey))
        {
            count++;
        }
    }

    return count;
}

} // namespace

TEST(RadioSpinelTransmit, shouldTransmitPeriodicWakeBurst)
{
    class BurstMockPlatform : public FakeCoprocessorPlatform
    {
    public:
        otError Transmit(otRadioFrame *aFrame) override
        {
            mTxChannels.push_back(aFrame->mChannel);
            mTxTimesUs.push_back(GetNow());
            mTxDelays.push_back(aFrame->mInfo.mTxInfo.mTxDelay);
            mTxDelayBaseTimes.push_back(aFrame->mInfo.mTxInfo.mTxDelayBaseTime);
            otPlatRadioTxStarted(CurrentInstance(), aFrame);
            otPlatRadioTxDone(CurrentInstance(), aFrame, nullptr, OT_ERROR_NONE);
            return OT_ERROR_NONE;
        }

        std::vector<uint8_t>  mTxChannels;
        std::vector<uint64_t> mTxTimesUs;
        std::vector<uint32_t> mTxDelays;
        std::vector<uint32_t> mTxDelayBaseTimes;
    };

    BurstMockPlatform platform;
    uint8_t           frameBuffer[OT_RADIO_FRAME_MAX_SIZE];
    Mac::TxFrame      txFrame;

    constexpr uint8_t  kTxChannel        = 15;
    constexpr uint8_t  kRxChannel        = 20;
    constexpr uint8_t  kRxChannelAfterTx = 21; // Ignored for bursts.
    constexpr uint16_t kBurstCount       = 4;
    constexpr uint32_t kBurstPeriod      = 7500; // us
    constexpr uint64_t kExpectedPeriodUs = 7500;

    PrepareTestFrame(txFrame, frameBuffer, kTxChannel);
    txFrame.mInfo.mTxInfo.mRxChannelAfterTxDone = kRxChannelAfterTx;

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ASSERT_EQ(platform.mRadioSpinel.Receive(kRxChannel), kErrorNone);
    ASSERT_EQ(SendTxBurst(platform, /* aTid */ 1, txFrame, kBurstCount, kBurstPeriod, /* aBurstChannelMask */ 0),
              kErrorNone);

    platform.GoInMs(50);

    ASSERT_EQ(platform.mTxChannels.size(), kBurstCount);
    for (size_t i = 0; i < kBurstCount; ++i)
    {
        EXPECT_EQ(platform.mTxChannels[i], kTxChannel);
        EXPECT_EQ(platform.mTxTimesUs[i] - platform.mTxTimesUs[0], i * kExpectedPeriodUs);
        EXPECT_EQ(platform.mTxDelays[i], 0u);
        EXPECT_EQ(platform.mTxDelayBaseTimes[i], 0u);
    }

    // The radio receives on the host's receive channel after the burst.
    EXPECT_EQ(platform.GetReceiveChannel(), kRxChannel);
    EXPECT_FALSE(platform.IsSleeping());
}

TEST(RadioSpinelTransmit, shouldTransmitBackToBackBurst)
{
    FakeCoprocessorPlatform platform;
    uint8_t                 frameBuffer[OT_RADIO_FRAME_MAX_SIZE];
    Mac::TxFrame            txFrame;

    constexpr uint8_t  kTxChannel  = 15;
    constexpr uint16_t kBurstCount = 4;

    PrepareTestFrame(txFrame, frameBuffer, kTxChannel);
    platform.mTxTiming.mEnabled = true;

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);

    // With a zero period, each tick starts as soon as the previous one ends.
    ASSERT_EQ(
        SendTxBurst(platform, /* aTid */ 1, txFrame, kBurstCount, /* aBurstPeriodUs */ 0, /* aBurstChannelMask */ 0),
        kErrorNone);
    platform.GoInMs(10);
    ASSERT_EQ(platform.mTxRecords.size(), kBurstCount);
    for (size_t i = 0; i < kBurstCount; ++i)
    {
        const FakePlatform::TxRecord &record = platform.mTxRecords[i];

        EXPECT_EQ(record.mChannel, kTxChannel);
        EXPECT_EQ(record.mCcaEnd, record.mCalledAt);
        EXPECT_EQ(record.mSfdEnd, record.mCalledAt + platform.mTxTiming.mRampUpUs + FakePlatform::kShrUs);
        EXPECT_EQ(record.mEnd,
                  record.mCalledAt + platform.mTxTiming.mRampUpUs + FakePlatform::GetAirtimeUs(txFrame.mLength));

        if (i > 0)
        {
            EXPECT_EQ(record.mCalledAt, platform.mTxRecords[i - 1].mEnd);
        }
    }
}
TEST(RadioSpinelTransmit, shouldUpdateTargetTxTimeDuringPeriodicBurst)
{
    class TimedBurstMockPlatform : public FakeCoprocessorPlatform
    {
    public:
        otError Transmit(otRadioFrame *aFrame) override
        {
            mTxChannels.push_back(aFrame->mChannel);
            mTxTimesUs.push_back(GetNow());
            mTxDelays.push_back(aFrame->mInfo.mTxInfo.mTxDelay);
            mTxDelayBaseTimes.push_back(aFrame->mInfo.mTxInfo.mTxDelayBaseTime);
            otPlatRadioTxStarted(CurrentInstance(), aFrame);
            otPlatRadioTxDone(CurrentInstance(), aFrame, nullptr, OT_ERROR_NONE);
            return OT_ERROR_NONE;
        }

        std::vector<uint8_t>  mTxChannels;
        std::vector<uint64_t> mTxTimesUs;
        std::vector<uint32_t> mTxDelays;
        std::vector<uint32_t> mTxDelayBaseTimes;
    };

    TimedBurstMockPlatform platform;
    uint8_t                frameBuffer[OT_RADIO_FRAME_MAX_SIZE];
    Mac::TxFrame           txFrame;

    constexpr uint8_t  kTxChannel        = 15;
    constexpr uint16_t kBurstCount       = 4;
    constexpr uint32_t kBurstPeriod      = 7500; // us
    constexpr uint32_t kInitialBaseTime  = 1000;
    constexpr uint32_t kInitialTxDelay   = 5000;
    constexpr uint32_t kExpectedPeriodUs = 7500;

    PrepareTestFrame(txFrame, frameBuffer, kTxChannel);
    txFrame.mInfo.mTxInfo.mTxDelayBaseTime = kInitialBaseTime;
    txFrame.mInfo.mTxInfo.mTxDelay         = kInitialTxDelay;

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    platform.GoInUs(kInitialBaseTime);

    ASSERT_EQ(SendTxBurst(platform, /* aTid */ 1, txFrame, kBurstCount, kBurstPeriod, /* aBurstChannelMask */ 0),
              kErrorNone);

    platform.GoInMs(50);

    ASSERT_EQ(platform.mTxChannels.size(), kBurstCount);
    EXPECT_GT(platform.mTxTimesUs[0], kInitialBaseTime);
    for (size_t i = 0; i < kBurstCount; ++i)
    {
        EXPECT_EQ(platform.mTxChannels[i], kTxChannel);
        EXPECT_EQ(platform.mTxTimesUs[i] - platform.mTxTimesUs[0], i * kExpectedPeriodUs);
        EXPECT_EQ(platform.mTxDelayBaseTimes[i] + platform.mTxDelays[i],
                  kInitialBaseTime + kInitialTxDelay + i * kExpectedPeriodUs);
    }
}

TEST(RadioSpinelTransmit, shouldTransmitSingleMultiChannelSweep)
{
    FakeCoprocessorPlatform platform;
    uint8_t                 frameBuffer[OT_RADIO_FRAME_MAX_SIZE];
    Mac::TxFrame            txFrame;

    constexpr uint8_t  kRxChannel   = 22;
    constexpr uint32_t kChannelMask = (1UL << 11) | (1UL << 18) | (1UL << 26);
    constexpr uint8_t  kChannels[]  = {11, 18, 26};

    PrepareTestFrame(txFrame, frameBuffer, /* aChannel */ 20);
    txFrame.mInfo.mTxInfo.mRxChannelAfterTxDone = 21; // Ignored for bursts.
    platform.mTxTiming.mEnabled                 = true;

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ASSERT_EQ(platform.mRadioSpinel.Receive(kRxChannel), kErrorNone);
    ASSERT_EQ(SendTxBurst(platform, /* aTid */ 1, txFrame, /* aBurstCount */ 1, /* aBurstPeriodUs */ 0, kChannelMask),
              kErrorNone);

    platform.GoInMs(10);

    // Channels are swept back-to-back in ascending order.
    ASSERT_EQ(platform.mTxRecords.size(), 3u);
    for (size_t i = 0; i < OT_ARRAY_LENGTH(kChannels); ++i)
    {
        EXPECT_EQ(platform.mTxRecords[i].mChannel, kChannels[i]);

        if (i > 0)
        {
            EXPECT_EQ(platform.mTxRecords[i].mCalledAt, platform.mTxRecords[i - 1].mEnd);
        }
    }
    EXPECT_EQ(platform.GetReceiveChannel(), kRxChannel);
}
TEST(RadioSpinelTransmit, shouldTransmitPeriodicMultiChannelBurst)
{
    FakeCoprocessorPlatform platform;
    uint8_t                 frameBuffer[OT_RADIO_FRAME_MAX_SIZE];
    Mac::TxFrame            txFrame;

    constexpr uint16_t kBurstCount       = 3;
    constexpr uint32_t kBurstPeriod      = 10000; // us
    constexpr uint32_t kChannelMask      = (1UL << 12) | (1UL << 24);
    constexpr uint64_t kExpectedPeriodUs = 10000;

    PrepareTestFrame(txFrame, frameBuffer, /* aChannel */ 11);
    platform.mTxTiming.mEnabled = true;

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ASSERT_EQ(SendTxBurst(platform, /* aTid */ 1, txFrame, kBurstCount, kBurstPeriod, kChannelMask), kErrorNone);

    platform.GoInMs(50);

    // Each tick starts at its slot, and its second channel follows its first channel back-to-back.
    ASSERT_EQ(platform.mTxRecords.size(), 6u);
    for (size_t tick = 0; tick < kBurstCount; ++tick)
    {
        const FakePlatform::TxRecord &first  = platform.mTxRecords[tick * 2];
        const FakePlatform::TxRecord &second = platform.mTxRecords[tick * 2 + 1];

        EXPECT_EQ(first.mChannel, 12);
        EXPECT_EQ(second.mChannel, 24);
        EXPECT_EQ(first.mCalledAt - platform.mTxRecords[0].mCalledAt, tick * kExpectedPeriodUs);
        EXPECT_EQ(second.mCalledAt, first.mEnd);
    }
}
TEST(RadioSpinelTransmit, shouldNotStopBurstOnReceivedFrame)
{
    class RecorderMockPlatform : public FakeCoprocessorPlatform
    {
    public:
        otError Transmit(otRadioFrame *aFrame) override
        {
            mTxChannels.push_back(aFrame->mChannel);
            otPlatRadioTxStarted(CurrentInstance(), aFrame);
            otPlatRadioTxDone(CurrentInstance(), aFrame, nullptr, OT_ERROR_NONE);
            return OT_ERROR_NONE;
        }

        std::vector<uint8_t> mTxChannels;
    };

    RecorderMockPlatform platform;
    uint8_t              frameBuffer[OT_RADIO_FRAME_MAX_SIZE];
    Mac::TxFrame         txFrame;

    constexpr uint8_t  kBurstChannel = 15;
    constexpr uint16_t kBurstCount   = 10;
    constexpr uint32_t kBurstPeriod  = 10000; // us

    PrepareTestFrame(txFrame, frameBuffer, kBurstChannel);

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ASSERT_EQ(SendTxBurst(platform, /* aTid */ 1, txFrame, kBurstCount, kBurstPeriod,
                          /* aBurstChannelMask */ 0),
              kErrorNone);

    // Run 15 ms: ticks at 0 ms and 10 ms should have fired (2 transmissions).
    platform.GoInUs(15000);
    ASSERT_EQ(platform.mTxChannels.size(), 2u);

    // Simulate receiving a frame on the radio mid-burst; burst must continue unaffected.
    otRadioFrame rxFrame{};
    uint8_t      rxPsdu[OT_RADIO_FRAME_MAX_SIZE]{};
    rxFrame.mPsdu    = rxPsdu;
    rxFrame.mLength  = 10;
    rxFrame.mChannel = kBurstChannel;
    otPlatRadioReceiveDone(FakePlatform::CurrentInstance(), &rxFrame, OT_ERROR_NONE);

    // Advance another 10 ms (to 25 ms): tick at 20 ms should fire (3rd transmission).
    platform.GoInUs(10000);
    ASSERT_EQ(platform.mTxChannels.size(), 3u);
    EXPECT_EQ(platform.mTxChannels[2], kBurstChannel);

    // Run past the burst window; all ticks are sent on the burst channel.
    platform.GoInMs(100);
    ASSERT_EQ(platform.mTxChannels.size(), kBurstCount);

    for (uint8_t channel : platform.mTxChannels)
    {
        EXPECT_EQ(channel, kBurstChannel);
    }
}

TEST(RadioSpinelTransmit, shouldRejectBurstWithAckRequestOrUnprocessedSecurity)
{
    class ValidationMockPlatform : public FakeCoprocessorPlatform
    {
    public:
        otError Transmit(otRadioFrame *aFrame) override
        {
            mTxChannels.push_back(aFrame->mChannel);
            otPlatRadioTxStarted(CurrentInstance(), aFrame);
            otPlatRadioTxDone(CurrentInstance(), aFrame, nullptr, OT_ERROR_NONE);
            return OT_ERROR_NONE;
        }

        std::vector<uint8_t> mTxChannels;
    };

    ValidationMockPlatform platform;
    uint8_t                frameBuffer[OT_RADIO_FRAME_MAX_SIZE];
    Mac::TxFrame           txFrame;

    constexpr uint8_t  kTxChannel   = 15;
    constexpr uint16_t kBurstCount  = 3;
    constexpr uint32_t kBurstPeriod = 7500; // us

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);

    // 1. Burst frame with AckRequest == true must be rejected.
    PrepareTestFrame(txFrame, frameBuffer, kTxChannel);
    txFrame.SetAckRequest(true);
    ASSERT_EQ(SendTxBurst(platform, /* aTid */ 1, txFrame, kBurstCount, kBurstPeriod, /* aBurstChannelMask */ 0),
              kErrorNone);
    platform.GoInMs(30);
    EXPECT_EQ(platform.mTxChannels.size(), 0u);

    // 2. Burst frame with SecurityEnabled == true and mIsSecurityProcessed == false must be rejected.
    {
        Mac::TxFrame::BuildInfo buildInfo;
        constexpr uint8_t       kDstAddr[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};

        memset(&txFrame, 0, sizeof(txFrame));
        txFrame.mPsdu = frameBuffer;

        buildInfo.mType    = Mac::Frame::kTypeData;
        buildInfo.mVersion = Mac::Frame::kVersion2006;
        buildInfo.mAddrs.mSource.SetShort(0xac00);
        buildInfo.mAddrs.mDestination.SetExtended(kDstAddr);
        buildInfo.mPanIds.SetSource(0x1234);
        buildInfo.mPanIds.SetDestination(0x4321);
        buildInfo.mSecurityLevel = Mac::Frame::kSecurityEncMic32;
        buildInfo.mKeyIdMode     = Mac::Frame::kKeyIdMode1;

        txFrame.PrepareHeadersWithEmptyPayload(buildInfo);
        txFrame.SetAckRequest(false);
        txFrame.mChannel                            = kTxChannel;
        txFrame.mInfo.mTxInfo.mRxChannelAfterTxDone = kTxChannel;
        txFrame.mInfo.mTxInfo.mCsmaCaEnabled        = false;
        txFrame.mInfo.mTxInfo.mIsSecurityProcessed  = false;
    }

    ASSERT_EQ(SendTxBurst(platform, /* aTid */ 2, txFrame, kBurstCount, kBurstPeriod, /* aBurstChannelMask */ 0),
              kErrorNone);
    platform.GoInMs(30);
    EXPECT_EQ(platform.mTxChannels.size(), 0u);

    // 3. Same secured frame with mIsSecurityProcessed == true must succeed.
    txFrame.mInfo.mTxInfo.mIsSecurityProcessed = true;
    ASSERT_EQ(SendTxBurst(platform, /* aTid */ 3, txFrame, kBurstCount, kBurstPeriod, /* aBurstChannelMask */ 0),
              kErrorNone);
    platform.GoInMs(30);
    EXPECT_EQ(platform.mTxChannels.size(), kBurstCount);
}

TEST(RadioSpinelTransmit, shouldContinueBurstWhenSingleTransmitFails)
{
    class ErrorResilienceMockPlatform : public FakeCoprocessorPlatform
    {
    public:
        otError Transmit(otRadioFrame *aFrame) override
        {
            mTxChannels.push_back(aFrame->mChannel);
            otPlatRadioTxStarted(CurrentInstance(), aFrame);

            otError txDoneError = (mTxChannels.size() == 2) ? OT_ERROR_CHANNEL_ACCESS_FAILURE : OT_ERROR_NONE;
            otPlatRadioTxDone(CurrentInstance(), aFrame, nullptr, txDoneError);
            return OT_ERROR_NONE;
        }

        std::vector<uint8_t> mTxChannels;
    };

    ErrorResilienceMockPlatform platform;
    uint8_t                     frameBuffer[OT_RADIO_FRAME_MAX_SIZE];
    Mac::TxFrame                txFrame;

    constexpr uint16_t kBurstCount  = 3;
    constexpr uint32_t kBurstPeriod = 10000; // us
    constexpr uint32_t kChannelMask = (1UL << 11) | (1UL << 15);

    PrepareTestFrame(txFrame, frameBuffer, /* aChannel */ 11);

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ASSERT_EQ(SendTxBurst(platform, /* aTid */ 1, txFrame, kBurstCount, kBurstPeriod, kChannelMask), kErrorNone);

    platform.GoInMs(50);

    ASSERT_EQ(platform.mTxChannels.size(), 6u);
    for (size_t tick = 0; tick < kBurstCount; ++tick)
    {
        EXPECT_EQ(platform.mTxChannels[tick * 2], 11);
        EXPECT_EQ(platform.mTxChannels[tick * 2 + 1], 15);
    }
}

TEST(RadioSpinelTransmit, shouldClearTxDelayAfterFirstChannelInTimedSweep)
{
    class TimedMultiChannelMockPlatform : public FakeCoprocessorPlatform
    {
    public:
        otError Transmit(otRadioFrame *aFrame) override
        {
            mTxChannels.push_back(aFrame->mChannel);
            mTxDelays.push_back(aFrame->mInfo.mTxInfo.mTxDelay);
            mTxDelayBaseTimes.push_back(aFrame->mInfo.mTxInfo.mTxDelayBaseTime);
            mCsmaEnabledFlags.push_back(aFrame->mInfo.mTxInfo.mCsmaCaEnabled);
            otPlatRadioTxStarted(CurrentInstance(), aFrame);
            otPlatRadioTxDone(CurrentInstance(), aFrame, nullptr, OT_ERROR_NONE);
            return OT_ERROR_NONE;
        }

        std::vector<uint8_t>  mTxChannels;
        std::vector<uint32_t> mTxDelays;
        std::vector<uint32_t> mTxDelayBaseTimes;
        std::vector<bool>     mCsmaEnabledFlags;
    };

    TimedMultiChannelMockPlatform platform;
    uint8_t                       frameBuffer[OT_RADIO_FRAME_MAX_SIZE];
    Mac::TxFrame                  txFrame;

    constexpr uint16_t kBurstCount       = 3;
    constexpr uint32_t kBurstPeriod      = 10000; // us
    constexpr uint32_t kExpectedPeriodUs = 10000;
    constexpr uint32_t kInitialBaseTime  = 5000;
    constexpr uint32_t kInitialTxDelay   = 0; // Scheduled via non-zero mTxDelayBaseTime with zero mTxDelay
    constexpr uint32_t kChannelMask      = (1UL << 11) | (1UL << 18) | (1UL << 26);

    PrepareTestFrame(txFrame, frameBuffer, /* aChannel */ 20);
    txFrame.mInfo.mTxInfo.mTxDelayBaseTime = kInitialBaseTime;
    txFrame.mInfo.mTxInfo.mTxDelay         = kInitialTxDelay;
    txFrame.mInfo.mTxInfo.mCsmaCaEnabled   = true;

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    platform.GoInUs(1000);

    ASSERT_EQ(SendTxBurst(platform, /* aTid */ 1, txFrame, kBurstCount, kBurstPeriod, kChannelMask), kErrorNone);

    platform.GoInMs(50);

    ASSERT_EQ(platform.mTxChannels.size(), 9u);
    for (size_t tick = 0; tick < kBurstCount; ++tick)
    {
        size_t base = tick * 3;

        EXPECT_EQ(platform.mTxChannels[base + 0], 11);
        EXPECT_EQ(platform.mTxChannels[base + 1], 18);
        EXPECT_EQ(platform.mTxChannels[base + 2], 26);

        // First channel of tick k uses TargetTxTime = T_0 + k * periodUs; subsequent channels clear both mTxDelay and
        // mTxDelayBaseTime to 0 so IsTargetTxTimeSpecified() is false.
        EXPECT_EQ(platform.mTxDelayBaseTimes[base + 0] + platform.mTxDelays[base + 0],
                  kInitialBaseTime + kInitialTxDelay + tick * kExpectedPeriodUs);
        EXPECT_EQ(platform.mTxDelays[base + 1], 0u);
        EXPECT_EQ(platform.mTxDelayBaseTimes[base + 1], 0u);
        EXPECT_EQ(platform.mTxDelays[base + 2], 0u);
        EXPECT_EQ(platform.mTxDelayBaseTimes[base + 2], 0u);

        // CSMA-CA enable setting remains preserved across all channels in every tick.
        EXPECT_TRUE(platform.mCsmaEnabledFlags[base + 0]);
        EXPECT_TRUE(platform.mCsmaEnabledFlags[base + 1]);
        EXPECT_TRUE(platform.mCsmaEnabledFlags[base + 2]);
    }
}

TEST(RadioSpinelTransmit, shouldSkipOverrunTicksDuringPeriodicBurst)
{
    FakeCoprocessorPlatform platform;
    uint8_t                 frameBuffer[OT_RADIO_FRAME_MAX_SIZE];
    Mac::TxFrame            txFrame;

    // A 127-byte frame takes 40 us ramp-up + 4256 us airtime, so a 2-channel sweep takes 8592 us, longer than the
    // 5000 us period. With slots at 0, 5, 10 and 15 ms, the ticks at 0 and 10 ms are sent, and the slots at 5 and
    // 15 ms are skipped since they have passed when the previous tick ends.
    constexpr uint16_t kBurstCount  = 4;
    constexpr uint32_t kBurstPeriod = 5000; // us
    constexpr uint32_t kChannelMask = (1UL << 11) | (1UL << 15);
    constexpr uint16_t kLength      = OT_RADIO_FRAME_MAX_SIZE;

    PrepareTestFrame(txFrame, frameBuffer, /* aChannel */ 20);
    txFrame.mLength             = kLength;
    platform.mTxTiming.mEnabled = true;

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ASSERT_EQ(SendTxBurst(platform, /* aTid */ 1, txFrame, kBurstCount, kBurstPeriod, kChannelMask), kErrorNone);

    platform.GoInMs(60);

    ASSERT_EQ(platform.mTxRecords.size(), 4u);

    const otRadioTime64 kStart    = platform.mTxRecords[0].mCalledAt;
    const uint64_t      kTxTimeUs = platform.mTxTiming.mRampUpUs + FakePlatform::GetAirtimeUs(kLength);

    EXPECT_EQ(kTxTimeUs, 4296u);
    EXPECT_EQ(platform.mTxRecords[0].mChannel, 11);
    EXPECT_EQ(platform.mTxRecords[1].mChannel, 15);
    EXPECT_EQ(platform.mTxRecords[1].mCalledAt, kStart + kTxTimeUs);
    EXPECT_EQ(platform.mTxRecords[2].mChannel, 11);
    EXPECT_EQ(platform.mTxRecords[2].mCalledAt, kStart + 10000);
    EXPECT_EQ(platform.mTxRecords[3].mChannel, 15);
    EXPECT_EQ(platform.mTxRecords[3].mCalledAt, kStart + 10000 + kTxTimeUs);
}

TEST(RadioSpinelTransmit, shouldEndSfdAtTargetTxTimeForTimedTicks)
{
    FakeCoprocessorPlatform platform;
    uint8_t                 frameBuffer[OT_RADIO_FRAME_MAX_SIZE];
    Mac::TxFrame            txFrame;

    constexpr uint16_t kBurstCount       = 3;
    constexpr uint32_t kBurstPeriod      = 7500; // us
    constexpr uint32_t kExpectedPeriodUs = 7500;
    constexpr uint32_t kTxDelay          = 5000;
    constexpr uint32_t kChannelMask      = (1UL << 11) | (1UL << 15);

    PrepareTestFrame(txFrame, frameBuffer, /* aChannel */ 20);
    txFrame.mInfo.mTxInfo.mCsmaCaEnabled = true;
    platform.mTxTiming.mEnabled          = true;

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);

    otRadioTime64 targetTime = platform.GetRadioNow() + kTxDelay;

    txFrame.mInfo.mTxInfo.mTxDelayBaseTime = static_cast<uint32_t>(targetTime - kTxDelay);
    txFrame.mInfo.mTxInfo.mTxDelay         = kTxDelay;

    ASSERT_EQ(SendTxBurst(platform, /* aTid */ 1, txFrame, kBurstCount, kBurstPeriod, kChannelMask), kErrorNone);
    platform.GoInMs(50);

    ASSERT_EQ(platform.mTxRecords.size(), 2u * kBurstCount);
    for (size_t tick = 0; tick < kBurstCount; ++tick)
    {
        const FakePlatform::TxRecord &first  = platform.mTxRecords[tick * 2];
        const FakePlatform::TxRecord &second = platform.mTxRecords[tick * 2 + 1];

        // The first channel of a tick performs CCA and its SFD ends exactly at the target TX time of the tick.
        EXPECT_TRUE(first.mIsTimed);
        EXPECT_FALSE(first.mAborted);
        EXPECT_EQ(first.mTargetTime, targetTime + tick * kExpectedPeriodUs);
        EXPECT_EQ(first.mSfdEnd, first.mTargetTime);

        // The second channel is not timed and starts right after the first one, performing its own CCA.
        EXPECT_FALSE(second.mIsTimed);
        EXPECT_EQ(second.mCalledAt, first.mEnd);
        EXPECT_EQ(second.mCcaEnd, second.mCalledAt + platform.mTxTiming.mCcaUs);
        EXPECT_EQ(second.mSfdEnd, second.mCcaEnd + platform.mTxTiming.mRampUpUs + FakePlatform::kShrUs);
    }
}
namespace {

class TimedBurstRecorderPlatform : public FakeCoprocessorPlatform
{
public:
    otError Transmit(otRadioFrame *aFrame) override
    {
        mTxChannels.push_back(aFrame->mChannel);
        mTxTimesUs.push_back(GetNow());
        mTxTargetTimes.push_back(aFrame->mInfo.mTxInfo.mTxDelayBaseTime + aFrame->mInfo.mTxInfo.mTxDelay);
        mIsTimed.push_back(aFrame->mInfo.mTxInfo.mTxDelayBaseTime != 0 || aFrame->mInfo.mTxInfo.mTxDelay != 0);
        otPlatRadioTxStarted(CurrentInstance(), aFrame);
        otPlatRadioTxDone(CurrentInstance(), aFrame, nullptr, mTxDoneError);
        return OT_ERROR_NONE;
    }

    otError               mTxDoneError = OT_ERROR_NONE;
    std::vector<uint8_t>  mTxChannels;
    std::vector<uint64_t> mTxTimesUs;
    std::vector<uint32_t> mTxTargetTimes;
    std::vector<bool>     mIsTimed;
};

} // namespace

TEST(RadioSpinelTransmit, shouldSkipTimedBurstSlotsAlreadyInThePast)
{
    TimedBurstRecorderPlatform platform;
    uint8_t                    frameBuffer[OT_RADIO_FRAME_MAX_SIZE];
    Mac::TxFrame               txFrame;

    constexpr uint16_t kBurstCount  = 4;
    constexpr uint32_t kBurstPeriod = 10000; // us
    constexpr uint32_t kTargetTime  = 5000;

    PrepareTestFrame(txFrame, frameBuffer, /* aChannel */ 15);
    txFrame.mInfo.mTxInfo.mTxDelayBaseTime = 1000;
    txFrame.mInfo.mTxInfo.mTxDelay         = kTargetTime - 1000;

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);

    // Slots are at 5 ms, 15 ms, 25 ms and 35 ms. At 20 ms, the first two slots are missed.
    platform.GoInUs(20000 - platform.GetNow());

    ASSERT_EQ(SendTxBurst(platform, /* aTid */ 1, txFrame, kBurstCount, kBurstPeriod, /* aBurstChannelMask */ 0),
              kErrorNone);
    platform.GoInMs(50);

    ASSERT_EQ(platform.mTxChannels.size(), 2u);
    EXPECT_TRUE(platform.mIsTimed[0]);
    EXPECT_TRUE(platform.mIsTimed[1]);
    EXPECT_EQ(platform.mTxTargetTimes[0], kTargetTime + 2 * 10000);
    EXPECT_EQ(platform.mTxTargetTimes[1], kTargetTime + 3 * 10000);
    EXPECT_LE(platform.mTxTimesUs[0], kTargetTime + 2 * 10000);
    EXPECT_EQ(platform.mTxTimesUs[1] - platform.mTxTimesUs[0], 10000u);
}

TEST(RadioSpinelTransmit, shouldRejectTimedBurstWhenAllSlotsAreInThePast)
{
    TimedBurstRecorderPlatform platform;
    uint8_t                    frameBuffer[OT_RADIO_FRAME_MAX_SIZE];
    Mac::TxFrame               txFrame;

    PrepareTestFrame(txFrame, frameBuffer, /* aChannel */ 15);
    txFrame.mInfo.mTxInfo.mTxDelayBaseTime = 1000;
    txFrame.mInfo.mTxInfo.mTxDelay         = 4000;

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    platform.GoInUs(50000 - platform.GetNow());

    // 1. Periodic burst whose slots (5, 15, 25 ms) are all in the past.
    ASSERT_EQ(SendTxBurst(platform, /* aTid */ 1, txFrame, /* aBurstCount */ 3, /* aBurstPeriodUs */ 10000,
                          /* aBurstChannelMask */ 0),
              kErrorNone);
    platform.GoInMs(50);
    EXPECT_EQ(platform.mTxChannels.size(), 0u);

    // 2. Single-tick burst whose target time is in the past.
    ASSERT_EQ(SendTxBurst(platform, /* aTid */ 2, txFrame, /* aBurstCount */ 1, /* aBurstPeriodUs */ 0,
                          /* aBurstChannelMask */ 0),
              kErrorNone);
    platform.GoInMs(50);
    EXPECT_EQ(platform.mTxChannels.size(), 0u);

    // 3. Back-to-back burst whose first (timed) tick is missed sends the remaining ticks immediately.
    ASSERT_EQ(SendTxBurst(platform, /* aTid */ 3, txFrame, /* aBurstCount */ 3, /* aBurstPeriodUs */ 0,
                          /* aBurstChannelMask */ 0),
              kErrorNone);
    platform.GoInMs(50);
    ASSERT_EQ(platform.mTxChannels.size(), 2u);
    EXPECT_FALSE(platform.mIsTimed[0]);
    EXPECT_FALSE(platform.mIsTimed[1]);
}

TEST(RadioSpinelTransmit, shouldSkipTimedBurstSlotsWithinTimedTxLeadTime)
{
    TimedBurstRecorderPlatform platform;
    uint8_t                    frameBuffer[OT_RADIO_FRAME_MAX_SIZE];
    Mac::TxFrame               txFrame;
    uint32_t                   targetTime;

    // Closer than `SubMac::kTimedTxLeadTime`, so `SubMac` could not start the transmission on time.
    constexpr uint32_t kTooCloseUs  = 200;
    constexpr uint32_t kBurstPeriod = 10000; // us

    PrepareTestFrame(txFrame, frameBuffer, /* aChannel */ 15);

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);

    // 1. Periodic burst: the first slot is skipped, and the next slots are sent on time.
    targetTime                             = static_cast<uint32_t>(platform.GetNow()) + kTooCloseUs;
    txFrame.mInfo.mTxInfo.mTxDelayBaseTime = targetTime - kTooCloseUs;
    txFrame.mInfo.mTxInfo.mTxDelay         = kTooCloseUs;

    ASSERT_EQ(SendTxBurst(platform, /* aTid */ 1, txFrame, /* aBurstCount */ 3, kBurstPeriod,
                          /* aBurstChannelMask */ 0),
              kErrorNone);
    platform.GoInMs(50);

    ASSERT_EQ(platform.mTxChannels.size(), 2u);
    EXPECT_TRUE(platform.mIsTimed[0]);
    EXPECT_TRUE(platform.mIsTimed[1]);
    EXPECT_EQ(platform.mTxTargetTimes[0], targetTime + kBurstPeriod);
    EXPECT_EQ(platform.mTxTargetTimes[1], targetTime + 2 * kBurstPeriod);

    // 2. Back-to-back burst: the first (timed) tick is skipped, and the remaining tick is sent right away.
    platform.mTxChannels.clear();
    platform.mTxTimesUs.clear();
    platform.mTxTargetTimes.clear();
    platform.mIsTimed.clear();

    targetTime                             = static_cast<uint32_t>(platform.GetNow()) + kTooCloseUs;
    txFrame.mInfo.mTxInfo.mTxDelayBaseTime = targetTime - kTooCloseUs;
    txFrame.mInfo.mTxInfo.mTxDelay         = kTooCloseUs;

    ASSERT_EQ(SendTxBurst(platform, /* aTid */ 2, txFrame, /* aBurstCount */ 2, /* aBurstPeriodUs */ 0,
                          /* aBurstChannelMask */ 0),
              kErrorNone);
    platform.GoInMs(50);

    ASSERT_EQ(platform.mTxChannels.size(), 1u);
    EXPECT_FALSE(platform.mIsTimed[0]);
}

TEST(RadioSpinelTransmit, shouldNotRetryTimedBurstTickOnCcaFailure)
{
    TimedBurstRecorderPlatform platform;
    uint8_t                    frameBuffer[OT_RADIO_FRAME_MAX_SIZE];
    Mac::TxFrame               txFrame;

    constexpr uint16_t kBurstCount = 3;

    PrepareTestFrame(txFrame, frameBuffer, /* aChannel */ 15);
    txFrame.mInfo.mTxInfo.mCsmaCaEnabled   = true;
    txFrame.mInfo.mTxInfo.mMaxCsmaBackoffs = 4;
    txFrame.mInfo.mTxInfo.mMaxFrameRetries = 3;

    platform.mTxDoneError = OT_ERROR_CHANNEL_ACCESS_FAILURE;

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);

    // 1. Untimed single-tick burst honors `mMaxCsmaBackoffs` and `mMaxFrameRetries`.
    ASSERT_EQ(SendTxBurst(platform, /* aTid */ 1, txFrame, /* aBurstCount */ 1, /* aBurstPeriodUs */ 0,
                          /* aBurstChannelMask */ 0),
              kErrorNone);
    platform.GoInMs(500);
    EXPECT_GT(platform.mTxChannels.size(), 1u);

    // 2. Timed burst ticks perform a single CCA before the target TX time. The host sets no CSMA backoffs and no
    //    frame retries, since they would be late.
    platform.mTxChannels.clear();
    platform.mIsTimed.clear();
    txFrame.mInfo.mTxInfo.mMaxCsmaBackoffs = 0;
    txFrame.mInfo.mTxInfo.mMaxFrameRetries = 0;
    txFrame.mInfo.mTxInfo.mTxDelayBaseTime = static_cast<uint32_t>(platform.GetNow());
    txFrame.mInfo.mTxInfo.mTxDelay         = 5000;

    ASSERT_EQ(SendTxBurst(platform, /* aTid */ 2, txFrame, kBurstCount, /* aBurstPeriodUs */ 10000,
                          /* aBurstChannelMask */ 0),
              kErrorNone);
    platform.GoInMs(100);
    ASSERT_EQ(platform.mTxChannels.size(), kBurstCount);
    for (bool isTimed : platform.mIsTimed)
    {
        EXPECT_TRUE(isTimed);
    }
}

TEST(RadioSpinelTransmit, shouldFollowReceiveAndSleepBetweenTicksWithoutEndingBurst)
{
    TimedBurstRecorderPlatform platform;
    uint8_t                    frameBuffer[OT_RADIO_FRAME_MAX_SIZE];
    Mac::TxFrame               txFrame;

    constexpr uint8_t  kBurstChannel = 15;
    constexpr uint8_t  kOtherChannel = 20;
    constexpr uint16_t kBurstCount   = 4;
    constexpr uint32_t kBurstPeriod  = 10000; // us

    PrepareTestFrame(txFrame, frameBuffer, kBurstChannel);

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ASSERT_EQ(platform.mRadioSpinel.Receive(kBurstChannel), kErrorNone);

    // 1. A receive request on another channel (`SPINEL_PROP_PHY_CHAN`) moves the radio to that channel between
    //    ticks, and the burst continues.
    ASSERT_EQ(SendTxBurst(platform, /* aTid */ 1, txFrame, kBurstCount, kBurstPeriod, /* aBurstChannelMask */ 0),
              kErrorNone);
    platform.GoInUs(15000);
    ASSERT_EQ(platform.mTxChannels.size(), 2u);

    ASSERT_EQ(platform.mRadioSpinel.Receive(kOtherChannel), kErrorNone);
    EXPECT_EQ(platform.GetReceiveChannel(), kOtherChannel);
    platform.GoInMs(100);
    EXPECT_EQ(platform.mTxChannels.size(), kBurstCount);
    EXPECT_EQ(platform.GetReceiveChannel(), kOtherChannel);
    EXPECT_FALSE(platform.IsSleeping());

    // 2. A timed burst with ticks at 5, 15, 25 and 35 ms. Each tick is handed to `SubMac` 1 ms before its target TX
    //    time (`OPENTHREAD_CONFIG_LINK_EXT_BURST_TX_AHEAD_US`).
    platform.mTxChannels.clear();
    txFrame.mInfo.mTxInfo.mTxDelayBaseTime = static_cast<uint32_t>(platform.GetNow());
    txFrame.mInfo.mTxInfo.mTxDelay         = 5000;

    ASSERT_EQ(SendTxBurst(platform, /* aTid */ 2, txFrame, kBurstCount, kBurstPeriod, /* aBurstChannelMask */ 0),
              kErrorNone);
    platform.GoInUs(10000);
    ASSERT_EQ(platform.mTxChannels.size(), 1u);

    // A sleep request (`SPINEL_PROP_MAC_RAW_STREAM_ENABLED` = false) between ticks takes effect right away.
    ASSERT_EQ(platform.mRadioSpinel.Sleep(), kErrorNone);
    EXPECT_TRUE(platform.IsSleeping());

    // A receive request while the next tick waits in `SubMac` takes effect after that tick.
    platform.GoInUs(4500);
    ASSERT_EQ(platform.mTxChannels.size(), 1u);
    ASSERT_EQ(platform.mRadioSpinel.Receive(kOtherChannel), kErrorNone);
    EXPECT_TRUE(platform.IsSleeping());

    platform.GoInUs(1000);
    ASSERT_EQ(platform.mTxChannels.size(), 2u);
    EXPECT_FALSE(platform.IsSleeping());
    EXPECT_EQ(platform.GetReceiveChannel(), kOtherChannel);

    platform.GoInMs(100);
    EXPECT_EQ(platform.mTxChannels.size(), kBurstCount);
    EXPECT_FALSE(platform.IsSleeping());
}

TEST(RadioSpinelTransmit, shouldScheduleTimedBurstOnRadioClock)
{
    TimedBurstRecorderPlatform platform;
    uint8_t                    frameBuffer[OT_RADIO_FRAME_MAX_SIZE];
    Mac::TxFrame               txFrame;

    constexpr uint16_t kBurstCount       = 4;
    constexpr uint32_t kBurstPeriod      = 7500; // us
    constexpr uint32_t kExpectedPeriodUs = 7500;
    constexpr uint32_t kTxDelay          = 5000;

    // Use a radio clock that differs from the alarm clock and wraps its 32-bit value during the burst.
    platform.SetRadioClockOffset(0xfffff000u);

    PrepareTestFrame(txFrame, frameBuffer, /* aChannel */ 15);

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);

    uint64_t startUs   = platform.GetNow();
    uint32_t radioBase = static_cast<uint32_t>(platform.GetRadioNow());

    txFrame.mInfo.mTxInfo.mTxDelayBaseTime = radioBase;
    txFrame.mInfo.mTxInfo.mTxDelay         = kTxDelay;

    ASSERT_EQ(SendTxBurst(platform, /* aTid */ 1, txFrame, kBurstCount, kBurstPeriod, /* aBurstChannelMask */ 0),
              kErrorNone);
    platform.GoInMs(50);

    ASSERT_EQ(platform.mTxChannels.size(), kBurstCount);
    EXPECT_GT(platform.mTxTimesUs[0], startUs);
    EXPECT_LE(platform.mTxTimesUs[0], startUs + kTxDelay);
    for (size_t i = 0; i < kBurstCount; ++i)
    {
        EXPECT_EQ(platform.mTxTargetTimes[i], static_cast<uint32_t>(radioBase + kTxDelay + i * kExpectedPeriodUs));
        EXPECT_EQ(platform.mTxTimesUs[i] - platform.mTxTimesUs[0], i * kExpectedPeriodUs);
    }
}

TEST(RadioSpinelTransmit, shouldReplyRightAwayAndNotifyWhenBurstEnds)
{
    TimedBurstRecorderPlatform platform;
    uint8_t                    frameBuffer[OT_RADIO_FRAME_MAX_SIZE];
    Mac::TxFrame               txFrame;
    std::vector<RcpFrame>      frames;

    constexpr spinel_tid_t kTid         = 1;
    constexpr spinel_tid_t kGetTid      = 2;
    constexpr uint16_t     kBurstCount  = 3;
    constexpr uint32_t     kBurstPeriod = 10000; // us
    constexpr uint32_t     kChannelMask = (1UL << 11) | (1UL << 15);

    PrepareTestFrame(txFrame, frameBuffer, /* aChannel */ 15);

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ObserveRcpFrames(platform, frames);

    uint64_t startUs = platform.GetNow();

    ASSERT_EQ(SendTxBurst(platform, kTid, txFrame, kBurstCount, kBurstPeriod, kChannelMask), kErrorNone);
    platform.GoInUs(1);

    // The request is answered right away with the configuration of the burst.
    ASSERT_EQ(frames.size(), 1u);
    EXPECT_EQ(frames[0].mTid, kTid);
    EXPECT_EQ(frames[0].mCommand, static_cast<unsigned int>(SPINEL_CMD_PROP_VALUE_IS));
    EXPECT_EQ(frames[0].mKey, static_cast<unsigned int>(SPINEL_PROP_RCP_TX_BURST));
    EXPECT_EQ(frames[0].mBurstCount, kBurstCount);
    EXPECT_EQ(frames[0].mBurstPeriod, kBurstPeriod);
    EXPECT_EQ(frames[0].mBurstMask, kChannelMask);

    // Ticks are at 0, 10 and 20 ms. Reading the property gives the remaining ticks.
    platform.GoInUs(15000);
    ASSERT_EQ(platform.mTxChannels.size(), 4u);
    ASSERT_EQ(platform.mSpinelDriver.SendCommand(SPINEL_CMD_PROP_VALUE_GET, SPINEL_PROP_RCP_TX_BURST, kGetTid),
              kErrorNone);
    platform.GoInUs(1);
    ASSERT_GE(FindRcpFrame(frames, kGetTid, SPINEL_PROP_RCP_TX_BURST), 0);
    EXPECT_EQ(frames[FindRcpFrame(frames, kGetTid, SPINEL_PROP_RCP_TX_BURST)].mBurstCount, 1u);
    EXPECT_EQ(CountRcpFrames(frames, /* aTid */ 0, SPINEL_PROP_RCP_TX_BURST), 0);

    // When the last tick is done, the end of the burst is notified once.
    platform.GoInMs(50);
    ASSERT_EQ(platform.mTxChannels.size(), 2u * kBurstCount);
    ASSERT_EQ(CountRcpFrames(frames, /* aTid */ 0, SPINEL_PROP_RCP_TX_BURST), 1);

    const RcpFrame &done = frames[FindRcpFrame(frames, /* aTid */ 0, SPINEL_PROP_RCP_TX_BURST)];

    EXPECT_EQ(done.mCommand, static_cast<unsigned int>(SPINEL_CMD_PROP_VALUE_IS));
    EXPECT_EQ(done.mBurstCount, 0u);
    EXPECT_EQ(done.mBurstPeriod, 0u);
    EXPECT_EQ(done.mBurstMask, 0u);
    EXPECT_GE(done.mTimeUs, startUs + 2 * 10000);
}

TEST(RadioSpinelTransmit, shouldNotifyEndOfBurstWhenAllTransmissionsFail)
{
    TimedBurstRecorderPlatform platform;
    uint8_t                    frameBuffer[OT_RADIO_FRAME_MAX_SIZE];
    Mac::TxFrame               txFrame;
    std::vector<RcpFrame>      frames;

    PrepareTestFrame(txFrame, frameBuffer, /* aChannel */ 15);
    platform.mTxDoneError = OT_ERROR_CHANNEL_ACCESS_FAILURE;

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ObserveRcpFrames(platform, frames);
    ASSERT_EQ(SendTxBurst(platform, /* aTid */ 1, txFrame, /* aBurstCount */ 3, /* aBurstPeriodUs */ 10000,
                          /* aBurstChannelMask */ 0),
              kErrorNone);
    platform.GoInMs(50);

    EXPECT_EQ(platform.mTxChannels.size(), 3u);
    EXPECT_EQ(CountRcpFrames(frames, /* aTid */ 0, SPINEL_PROP_RCP_TX_BURST), 1);
}

TEST(RadioSpinelTransmit, shouldStopBurstRightAwayWhenNoFrameIsOnAir)
{
    TimedBurstRecorderPlatform platform;
    uint8_t                    frameBuffer[OT_RADIO_FRAME_MAX_SIZE];
    Mac::TxFrame               txFrame;
    std::vector<RcpFrame>      frames;

    constexpr spinel_tid_t kBurstTid = 1;
    constexpr spinel_tid_t kStopTid  = 2;

    PrepareTestFrame(txFrame, frameBuffer, /* aChannel */ 15);

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ObserveRcpFrames(platform, frames);

    // A timed burst with ticks at 5, 15, 25 ms, ..., each handed to `SubMac` 1 ms before its target TX time. At
    // 14.5 ms, the second tick waits in `SubMac` for its target TX time.
    txFrame.mInfo.mTxInfo.mTxDelayBaseTime = static_cast<uint32_t>(platform.GetNow());
    txFrame.mInfo.mTxInfo.mTxDelay         = 5000;
    ASSERT_EQ(SendTxBurst(platform, kBurstTid, txFrame, /* aBurstCount */ 10, /* aBurstPeriodUs */ 10000,
                          /* aBurstChannelMask */ 0),
              kErrorNone);
    platform.GoInUs(14500);
    ASSERT_EQ(platform.mTxChannels.size(), 1u);

    ASSERT_EQ(SendStopBurst(platform, kStopTid), kErrorNone);
    platform.GoInUs(1);

    // The burst ends right away: the reply reads no ongoing burst, and no notification is sent.
    int replyIndex = FindRcpFrame(frames, kStopTid, SPINEL_PROP_RCP_TX_BURST);

    ASSERT_GE(replyIndex, 0);
    EXPECT_EQ(frames[replyIndex].mBurstCount, 0u);
    EXPECT_EQ(CountRcpFrames(frames, /* aTid */ 0, SPINEL_PROP_RCP_TX_BURST), 0);

    // No more ticks are sent, including the one that was waiting in `SubMac`.
    platform.GoInMs(100);
    EXPECT_EQ(platform.mTxChannels.size(), 1u);
    EXPECT_EQ(CountRcpFrames(frames, /* aTid */ 0, SPINEL_PROP_RCP_TX_BURST), 0);

    // Stopping again, with no ongoing burst, also succeeds and notifies nothing.
    ASSERT_EQ(SendStopBurst(platform, /* aTid */ 3), kErrorNone);
    platform.GoInUs(1);
    ASSERT_GE(FindRcpFrame(frames, /* aTid */ 3, SPINEL_PROP_RCP_TX_BURST), 0);
    EXPECT_EQ(frames[FindRcpFrame(frames, /* aTid */ 3, SPINEL_PROP_RCP_TX_BURST)].mBurstCount, 0u);
    EXPECT_EQ(CountRcpFrames(frames, /* aTid */ 0, SPINEL_PROP_RCP_TX_BURST), 0);
}

TEST(RadioSpinelTransmit, shouldStopBurstWhenFrameOnAirIsDone)
{
    FakeCoprocessorPlatform platform;
    uint8_t                 frameBuffer[OT_RADIO_FRAME_MAX_SIZE];
    Mac::TxFrame            txFrame;
    std::vector<RcpFrame>   frames;

    constexpr spinel_tid_t kBurstTid    = 1;
    constexpr spinel_tid_t kStopTid     = 2;
    constexpr uint32_t     kChannelMask = (1UL << 11) | (1UL << 15);

    PrepareTestFrame(txFrame, frameBuffer, /* aChannel */ 15);
    txFrame.mLength             = OT_RADIO_FRAME_MAX_SIZE; // 4296 us on air, including ramp-up.
    platform.mTxTiming.mEnabled = true;

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ObserveRcpFrames(platform, frames);
    ASSERT_EQ(SendTxBurst(platform, kBurstTid, txFrame, /* aBurstCount */ 10, /* aBurstPeriodUs */ 20000, kChannelMask),
              kErrorNone);

    // The first channel of the first tick is on air.
    platform.GoInUs(1000);
    ASSERT_EQ(platform.mTxRecords.size(), 1u);

    const otRadioTime64 frameEnd = platform.mTxRecords[0].mEnd;

    ASSERT_LT(platform.ToRadioTime(platform.GetNow()), frameEnd);

    // The reply reads the burst as still ongoing with only the tick in progress, and nothing is notified yet.
    ASSERT_EQ(SendStopBurst(platform, kStopTid), kErrorNone);
    platform.GoInUs(1);
    ASSERT_GE(FindRcpFrame(frames, kStopTid, SPINEL_PROP_RCP_TX_BURST), 0);
    EXPECT_EQ(frames[FindRcpFrame(frames, kStopTid, SPINEL_PROP_RCP_TX_BURST)].mBurstCount, 1u);
    EXPECT_EQ(CountRcpFrames(frames, /* aTid */ 0, SPINEL_PROP_RCP_TX_BURST), 0);

    // The burst ends when the frame on air is done, without sending the other channel of the tick.
    platform.GoInMs(100);
    ASSERT_EQ(CountRcpFrames(frames, /* aTid */ 0, SPINEL_PROP_RCP_TX_BURST), 1);
    EXPECT_EQ(platform.ToRadioTime(frames[FindRcpFrame(frames, /* aTid */ 0, SPINEL_PROP_RCP_TX_BURST)].mTimeUs),
              frameEnd);
    EXPECT_EQ(platform.mTxRecords.size(), 1u);
}

TEST(RadioSpinelTransmit, shouldAbortTimedTicksThatCannotBeSentOnTime)
{
    FakeCoprocessorPlatform platform;
    uint8_t                 frameBuffer[OT_RADIO_FRAME_MAX_SIZE];
    Mac::TxFrame            txFrame;
    std::vector<RcpFrame>   frames;

    constexpr uint16_t kBurstCount = 3;
    constexpr uint32_t kTxDelay    = 5000;

    PrepareTestFrame(txFrame, frameBuffer, /* aChannel */ 20);
    txFrame.mInfo.mTxInfo.mCsmaCaEnabled = true;

    // A ramp-up time longer than `SubMac` budgets makes the end of SFD 40 us later than the target TX time.
    platform.mTxTiming.mEnabled  = true;
    platform.mTxTiming.mRampUpUs = 80;

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ObserveRcpFrames(platform, frames);

    txFrame.mInfo.mTxInfo.mTxDelayBaseTime = static_cast<uint32_t>(platform.GetRadioNow());
    txFrame.mInfo.mTxInfo.mTxDelay         = kTxDelay;

    ASSERT_EQ(SendTxBurst(platform, /* aTid */ 1, txFrame, kBurstCount, /* aBurstPeriodUs */ 7500,
                          /* aBurstChannelMask */ 0),
              kErrorNone);
    platform.GoInMs(50);

    // Every tick is aborted (no retry), and the burst still ends.
    ASSERT_EQ(platform.mTxRecords.size(), kBurstCount);
    for (const FakePlatform::TxRecord &record : platform.mTxRecords)
    {
        EXPECT_TRUE(record.mIsTimed);
        EXPECT_TRUE(record.mAborted);
    }

    EXPECT_EQ(CountRcpFrames(frames, /* aTid */ 0, SPINEL_PROP_RCP_TX_BURST), 1);
}

TEST(RadioSpinelTransmit, shouldRejectTransmitAndEnergyScanDuringBurst)
{
    FakeCoprocessorPlatform platform;
    uint8_t                 frameBuffer[OT_RADIO_FRAME_MAX_SIZE];
    Mac::TxFrame            txFrame;
    std::vector<RcpFrame>   frames;

    constexpr spinel_tid_t kBurstTid     = 1;
    constexpr spinel_tid_t kRawTid       = 2;
    constexpr spinel_tid_t kBurst2Tid    = 3;
    constexpr uint8_t      kBurstChannel = 15;
    constexpr uint8_t      kNewChannel   = 20;
    constexpr uint16_t     kBurstCount   = 3;

    PrepareTestFrame(txFrame, frameBuffer, kBurstChannel);
    platform.mTxTiming.mEnabled = true;

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ObserveRcpFrames(platform, frames);
    ASSERT_EQ(SendTxBurst(platform, kBurstTid, txFrame, kBurstCount, /* aBurstPeriodUs */ 10000,
                          /* aBurstChannelMask */ 0),
              kErrorNone);

    platform.GoInUs(5000);
    ASSERT_EQ(platform.mTxRecords.size(), 1u);

    // A single transmission, another burst, and an energy scan are rejected right away.
    txFrame.mChannel = kNewChannel;
    ASSERT_EQ(SendStreamRaw(platform, kRawTid, txFrame), kErrorNone);
    ASSERT_EQ(SendTxBurst(platform, kBurst2Tid, txFrame, kBurstCount, /* aBurstPeriodUs */ 10000,
                          /* aBurstChannelMask */ 0),
              kErrorNone);
    platform.GoInUs(1);
    EXPECT_EQ(platform.mRadioSpinel.EnergyScan(kBurstChannel, /* aScanDuration */ 10), kErrorInvalidState);

    ASSERT_GE(FindRcpFrame(frames, kRawTid, SPINEL_PROP_LAST_STATUS), 0);
    EXPECT_EQ(frames[FindRcpFrame(frames, kRawTid, SPINEL_PROP_LAST_STATUS)].mStatus,
              static_cast<unsigned int>(SPINEL_STATUS_INVALID_STATE));
    ASSERT_GE(FindRcpFrame(frames, kBurst2Tid, SPINEL_PROP_LAST_STATUS), 0);
    EXPECT_EQ(frames[FindRcpFrame(frames, kBurst2Tid, SPINEL_PROP_LAST_STATUS)].mStatus,
              static_cast<unsigned int>(SPINEL_STATUS_BUSY));

    // The burst is not affected.
    platform.GoInMs(50);
    ASSERT_EQ(platform.mTxRecords.size(), kBurstCount);

    for (const auto &record : platform.mTxRecords)
    {
        EXPECT_EQ(record.mChannel, kBurstChannel);
    }

    EXPECT_EQ(CountRcpFrames(frames, /* aTid */ 0, SPINEL_PROP_RCP_TX_BURST), 1);

    // A new transmission is accepted once the burst is done.
    ASSERT_EQ(SendStreamRaw(platform, /* aTid */ 4, txFrame), kErrorNone);
    platform.GoInMs(10);
    ASSERT_EQ(platform.mTxRecords.size(), kBurstCount + 1u);
    EXPECT_EQ(platform.mTxRecords.back().mChannel, kNewChannel);

    // The rejected energy scan left no state behind: an energy scan, then a new burst, are accepted.
    ASSERT_EQ(platform.mRadioSpinel.EnergyScan(kBurstChannel, /* aScanDuration */ 10), kErrorNone);
    platform.GoInMs(20);
    EXPECT_EQ(CountRcpFrames(frames, /* aTid */ 0, SPINEL_PROP_MAC_ENERGY_SCAN_RESULT), 1);

    ASSERT_EQ(SendTxBurst(platform, /* aTid */ 5, txFrame, kBurstCount, /* aBurstPeriodUs */ 10000,
                          /* aBurstChannelMask */ 0),
              kErrorNone);
    platform.GoInUs(1);
    ASSERT_GE(FindRcpFrame(frames, /* aTid */ 5, SPINEL_PROP_RCP_TX_BURST), 0);
    EXPECT_EQ(frames[FindRcpFrame(frames, /* aTid */ 5, SPINEL_PROP_RCP_TX_BURST)].mBurstCount, kBurstCount);
    platform.GoInMs(50);
    EXPECT_EQ(platform.mTxRecords.size(), 2u * kBurstCount + 1u);
}

TEST(RadioSpinelTransmit, shouldRejectBurstDuringSingleTransmit)
{
    FakeCoprocessorPlatform platform;
    uint8_t                 frameBuffer[OT_RADIO_FRAME_MAX_SIZE];
    Mac::TxFrame            txFrame;
    std::vector<RcpFrame>   frames;

    constexpr spinel_tid_t kRawTid   = 1;
    constexpr spinel_tid_t kBurstTid = 2;

    PrepareTestFrame(txFrame, frameBuffer, /* aChannel */ 15);
    txFrame.mLength             = OT_RADIO_FRAME_MAX_SIZE; // 4296 us on air, including ramp-up.
    platform.mTxTiming.mEnabled = true;

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ObserveRcpFrames(platform, frames);
    ASSERT_EQ(SendStreamRaw(platform, kRawTid, txFrame), kErrorNone);
    platform.GoInUs(1000);
    ASSERT_EQ(platform.mTxRecords.size(), 1u);

    ASSERT_EQ(SendTxBurst(platform, kBurstTid, txFrame, /* aBurstCount */ 3, /* aBurstPeriodUs */ 10000,
                          /* aBurstChannelMask */ 0),
              kErrorNone);
    platform.GoInUs(1);
    ASSERT_GE(FindRcpFrame(frames, kBurstTid, SPINEL_PROP_LAST_STATUS), 0);
    EXPECT_EQ(frames[FindRcpFrame(frames, kBurstTid, SPINEL_PROP_LAST_STATUS)].mStatus,
              static_cast<unsigned int>(SPINEL_STATUS_BUSY));

    // The single transmission completes normally.
    platform.GoInMs(10);
    ASSERT_GE(FindRcpFrame(frames, kRawTid, SPINEL_PROP_LAST_STATUS), 0);
    EXPECT_EQ(frames[FindRcpFrame(frames, kRawTid, SPINEL_PROP_LAST_STATUS)].mStatus,
              static_cast<unsigned int>(SPINEL_STATUS_OK));
    EXPECT_EQ(platform.mTxRecords.size(), 1u);
}

TEST(RadioSpinelTransmit, shouldNotifyEndOfBurstWhenLinkRawIsDisabled)
{
    TimedBurstRecorderPlatform platform;
    uint8_t                    frameBuffer[OT_RADIO_FRAME_MAX_SIZE];
    Mac::TxFrame               txFrame;
    std::vector<RcpFrame>      frames;

    PrepareTestFrame(txFrame, frameBuffer, /* aChannel */ 15);

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ObserveRcpFrames(platform, frames);
    ASSERT_EQ(SendTxBurst(platform, /* aTid */ 1, txFrame, /* aBurstCount */ 10, /* aBurstPeriodUs */ 10000,
                          /* aBurstChannelMask */ 0),
              kErrorNone);
    platform.GoInUs(15000);
    ASSERT_EQ(platform.mTxChannels.size(), 2u);

    // Enabling the raw link-layer again (`SPINEL_PROP_PHY_ENABLED` = true) does not end the burst.
    ASSERT_EQ(SendSpinelCommand(platform, SPINEL_CMD_PROP_VALUE_SET, SPINEL_PROP_PHY_ENABLED, /* aTid */ 2,
                                SPINEL_DATATYPE_BOOL_S, true),
              kErrorNone);
    platform.GoInUs(10000);
    ASSERT_EQ(platform.mTxChannels.size(), 3u);
    EXPECT_EQ(CountRcpFrames(frames, /* aTid */ 0, SPINEL_PROP_RCP_TX_BURST), 0);

    // Disabling it ends the burst.
    ASSERT_EQ(SendSpinelCommand(platform, SPINEL_CMD_PROP_VALUE_SET, SPINEL_PROP_PHY_ENABLED, /* aTid */ 3,
                                SPINEL_DATATYPE_BOOL_S, false),
              kErrorNone);
    platform.GoInUs(1);
    EXPECT_EQ(CountRcpFrames(frames, /* aTid */ 0, SPINEL_PROP_RCP_TX_BURST), 1);

    platform.GoInMs(100);
    EXPECT_EQ(platform.mTxChannels.size(), 3u);
}

namespace {

class SleepFailingBurstPlatform : public TimedBurstRecorderPlatform
{
public:
    otError Sleep(void) override
    {
        if (mFailNextSleep)
        {
            mFailNextSleep = false;
            return OT_ERROR_FAILED;
        }

        return TimedBurstRecorderPlatform::Sleep();
    }

    bool mFailNextSleep = false;
};

} // namespace

TEST(RadioSpinelTransmit, shouldKeepIdleStateBetweenTicksWhenSleepRequestFails)
{
    SleepFailingBurstPlatform platform;
    uint8_t                   frameBuffer[OT_RADIO_FRAME_MAX_SIZE];
    Mac::TxFrame              txFrame;
    std::vector<RcpFrame>     frames;

    constexpr uint8_t      kChannel = 15;
    constexpr spinel_tid_t kGetTid  = 2;

    PrepareTestFrame(txFrame, frameBuffer, kChannel);

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ASSERT_EQ(platform.mRadioSpinel.Receive(kChannel), kErrorNone);

    // Ticks are at 0, 10 and 20 ms.
    ASSERT_EQ(SendTxBurst(platform, /* aTid */ 1, txFrame, /* aBurstCount */ 3, /* aBurstPeriodUs */ 10000,
                          /* aBurstChannelMask */ 0),
              kErrorNone);
    platform.GoInUs(5000);
    ASSERT_EQ(platform.mTxChannels.size(), 1u);
    EXPECT_FALSE(platform.IsSleeping());

    // A sleep request that fails between ticks does not change the radio state after the next tick.
    platform.mFailNextSleep = true;
    EXPECT_NE(platform.mRadioSpinel.Sleep(), kErrorNone);
    EXPECT_FALSE(platform.IsSleeping());

    // `SPINEL_PROP_MAC_RAW_STREAM_ENABLED` is not changed by the failed request.
    ObserveRcpFrames(platform, frames);
    ASSERT_EQ(
        platform.mSpinelDriver.SendCommand(SPINEL_CMD_PROP_VALUE_GET, SPINEL_PROP_MAC_RAW_STREAM_ENABLED, kGetTid),
        kErrorNone);
    platform.GoInUs(1);
    ASSERT_GE(FindRcpFrame(frames, kGetTid, SPINEL_PROP_MAC_RAW_STREAM_ENABLED), 0);
    EXPECT_TRUE(frames[FindRcpFrame(frames, kGetTid, SPINEL_PROP_MAC_RAW_STREAM_ENABLED)].mBoolValue);

    platform.GoInUs(10000);
    ASSERT_EQ(platform.mTxChannels.size(), 2u);
    EXPECT_FALSE(platform.IsSleeping());
    EXPECT_EQ(platform.GetReceiveChannel(), kChannel);
}

TEST(RadioSpinelTransmit, shouldClearRawStreamEnabledWhenLinkRawIsDisabled)
{
    FakeCoprocessorPlatform platform;
    std::vector<RcpFrame>   frames;

    constexpr spinel_tid_t kGetTid = 3;

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ASSERT_EQ(platform.mRadioSpinel.Receive(/* aChannel */ 15), kErrorNone);
    ObserveRcpFrames(platform, frames);

    // Disabling the raw link-layer stops receiving, and enabling it again does not resume it.
    ASSERT_EQ(SendSpinelCommand(platform, SPINEL_CMD_PROP_VALUE_SET, SPINEL_PROP_PHY_ENABLED, /* aTid */ 1,
                                SPINEL_DATATYPE_BOOL_S, false),
              kErrorNone);
    ASSERT_EQ(SendSpinelCommand(platform, SPINEL_CMD_PROP_VALUE_SET, SPINEL_PROP_PHY_ENABLED, /* aTid */ 2,
                                SPINEL_DATATYPE_BOOL_S, true),
              kErrorNone);
    ASSERT_EQ(
        platform.mSpinelDriver.SendCommand(SPINEL_CMD_PROP_VALUE_GET, SPINEL_PROP_MAC_RAW_STREAM_ENABLED, kGetTid),
        kErrorNone);
    platform.GoInUs(1);

    ASSERT_GE(FindRcpFrame(frames, kGetTid, SPINEL_PROP_MAC_RAW_STREAM_ENABLED), 0);
    EXPECT_FALSE(frames[FindRcpFrame(frames, kGetTid, SPINEL_PROP_MAC_RAW_STREAM_ENABLED)].mBoolValue);
}

TEST(RadioSpinelTransmit, shouldReceiveWhenLinkRawIsEnabledAfterRawStreamIsEnabled)
{
    FakeCoprocessorPlatform platform;

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ASSERT_EQ(SendSpinelCommand(platform, SPINEL_CMD_PROP_VALUE_SET, SPINEL_PROP_PHY_ENABLED, /* aTid */ 1,
                                SPINEL_DATATYPE_BOOL_S, false),
              kErrorNone);
    platform.GoInUs(1);
    EXPECT_TRUE(platform.IsSleeping());

    // A receive request while the raw link-layer is disabled takes effect when it is enabled.
    ASSERT_EQ(SendSpinelCommand(platform, SPINEL_CMD_PROP_VALUE_SET, SPINEL_PROP_MAC_RAW_STREAM_ENABLED, /* aTid */ 2,
                                SPINEL_DATATYPE_BOOL_S, true),
              kErrorNone);
    platform.GoInUs(1);
    EXPECT_TRUE(platform.IsSleeping());

    ASSERT_EQ(SendSpinelCommand(platform, SPINEL_CMD_PROP_VALUE_SET, SPINEL_PROP_PHY_ENABLED, /* aTid */ 3,
                                SPINEL_DATATYPE_BOOL_S, true),
              kErrorNone);
    platform.GoInUs(1);
    EXPECT_FALSE(platform.IsSleeping());
}

namespace {

class ReceiveFailingPlatform : public FakeCoprocessorPlatform
{
public:
    otError Receive(uint8_t aChannel) override
    {
        if (mFailNextReceive)
        {
            mFailNextReceive = false;
            return OT_ERROR_FAILED;
        }

        return FakeCoprocessorPlatform::Receive(aChannel);
    }

    bool mFailNextReceive = false;
};

} // namespace

TEST(RadioSpinelTransmit, shouldStayDisabledWhenReceiveFailsOnEnable)
{
    ReceiveFailingPlatform platform;
    std::vector<RcpFrame>  frames;

    constexpr spinel_tid_t kEnableTid = 3;
    constexpr spinel_tid_t kGetTid    = 4;

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ASSERT_EQ(SendSpinelCommand(platform, SPINEL_CMD_PROP_VALUE_SET, SPINEL_PROP_PHY_ENABLED, /* aTid */ 1,
                                SPINEL_DATATYPE_BOOL_S, false),
              kErrorNone);
    ASSERT_EQ(SendSpinelCommand(platform, SPINEL_CMD_PROP_VALUE_SET, SPINEL_PROP_MAC_RAW_STREAM_ENABLED, /* aTid */ 2,
                                SPINEL_DATATYPE_BOOL_S, true),
              kErrorNone);
    platform.GoInUs(1);
    ObserveRcpFrames(platform, frames);

    // Enabling fails as a whole if the pending receive request fails.
    platform.mFailNextReceive = true;
    ASSERT_EQ(SendSpinelCommand(platform, SPINEL_CMD_PROP_VALUE_SET, SPINEL_PROP_PHY_ENABLED, kEnableTid,
                                SPINEL_DATATYPE_BOOL_S, true),
              kErrorNone);
    ASSERT_EQ(platform.mSpinelDriver.SendCommand(SPINEL_CMD_PROP_VALUE_GET, SPINEL_PROP_PHY_ENABLED, kGetTid),
              kErrorNone);
    platform.GoInUs(1);

    ASSERT_GE(FindRcpFrame(frames, kEnableTid, SPINEL_PROP_LAST_STATUS), 0);
    EXPECT_NE(frames[FindRcpFrame(frames, kEnableTid, SPINEL_PROP_LAST_STATUS)].mStatus,
              static_cast<unsigned int>(SPINEL_STATUS_OK));
    ASSERT_GE(FindRcpFrame(frames, kGetTid, SPINEL_PROP_PHY_ENABLED), 0);
    EXPECT_FALSE(frames[FindRcpFrame(frames, kGetTid, SPINEL_PROP_PHY_ENABLED)].mBoolValue);
}

TEST(RadioSpinelTransmit, shouldAbortPendingTransmitOnReceive)
{
    FakeCoprocessorPlatform platform;
    uint8_t                 frameBuffer[OT_RADIO_FRAME_MAX_SIZE];
    Mac::TxFrame            txFrame;
    std::vector<RcpFrame>   frames;

    constexpr spinel_tid_t kTxTid = 1;

    PrepareTestFrame(txFrame, frameBuffer, /* aChannel */ 15);

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ObserveRcpFrames(platform, frames);

    ASSERT_EQ(SendTimedStreamRaw(platform, kTxTid, txFrame, /* aTxDelay */ 5000), kErrorNone);
    platform.GoInUs(1000);
    EXPECT_LT(FindRcpFrame(frames, kTxTid, SPINEL_PROP_LAST_STATUS), 0);

    // A receive request drops the frame waiting in `SubMac`, which completes the transmission with an abort.
    ASSERT_EQ(SendSpinelCommand(platform, SPINEL_CMD_PROP_VALUE_SET, SPINEL_PROP_MAC_RAW_STREAM_ENABLED, /* aTid */ 2,
                                SPINEL_DATATYPE_BOOL_S, true),
              kErrorNone);
    platform.GoInUs(1);
    ASSERT_GE(FindRcpFrame(frames, kTxTid, SPINEL_PROP_LAST_STATUS), 0);
    EXPECT_NE(frames[FindRcpFrame(frames, kTxTid, SPINEL_PROP_LAST_STATUS)].mStatus,
              static_cast<unsigned int>(SPINEL_STATUS_OK));

    platform.GoInMs(10);
    EXPECT_TRUE(platform.mTxRecords.empty());
}

TEST(RadioSpinelTransmit, shouldRejectBurstWhileTransmitIsPending)
{
    FakeCoprocessorPlatform platform;
    uint8_t                 frameBuffer[OT_RADIO_FRAME_MAX_SIZE];
    Mac::TxFrame            txFrame;
    std::vector<RcpFrame>   frames;

    constexpr spinel_tid_t kBurstTid = 2;

    PrepareTestFrame(txFrame, frameBuffer, /* aChannel */ 15);

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ObserveRcpFrames(platform, frames);

    // A transmission waiting in `SubMac`, whose TX done is not reported (TID 0).
    ASSERT_EQ(SendTimedStreamRaw(platform, /* aTid */ 0, txFrame, /* aTxDelay */ 5000), kErrorNone);
    platform.GoInUs(1000);

    ASSERT_EQ(SendTxBurst(platform, kBurstTid, txFrame, /* aBurstCount */ 2, /* aBurstPeriodUs */ 10000,
                          /* aBurstChannelMask */ 0),
              kErrorNone);
    platform.GoInUs(1);
    ASSERT_GE(FindRcpFrame(frames, kBurstTid, SPINEL_PROP_LAST_STATUS), 0);
    EXPECT_EQ(frames[FindRcpFrame(frames, kBurstTid, SPINEL_PROP_LAST_STATUS)].mStatus,
              static_cast<unsigned int>(SPINEL_STATUS_BUSY));
}

TEST(RadioSpinelTransmit, shouldAcceptBurstAfterLinkRawIsDisabledDuringTransmit)
{
    FakeCoprocessorPlatform platform;
    uint8_t                 frameBuffer[OT_RADIO_FRAME_MAX_SIZE];
    Mac::TxFrame            txFrame;
    std::vector<RcpFrame>   frames;

    constexpr spinel_tid_t kBurstTid = 4;

    PrepareTestFrame(txFrame, frameBuffer, /* aChannel */ 15);

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ObserveRcpFrames(platform, frames);

    // Disabling the raw link-layer drops the transmission waiting in `SubMac`.
    ASSERT_EQ(SendTimedStreamRaw(platform, /* aTid */ 1, txFrame, /* aTxDelay */ 5000), kErrorNone);
    platform.GoInUs(1000);
    ASSERT_EQ(SendSpinelCommand(platform, SPINEL_CMD_PROP_VALUE_SET, SPINEL_PROP_PHY_ENABLED, /* aTid */ 2,
                                SPINEL_DATATYPE_BOOL_S, false),
              kErrorNone);
    ASSERT_EQ(SendSpinelCommand(platform, SPINEL_CMD_PROP_VALUE_SET, SPINEL_PROP_PHY_ENABLED, /* aTid */ 3,
                                SPINEL_DATATYPE_BOOL_S, true),
              kErrorNone);

    ASSERT_EQ(SendTxBurst(platform, kBurstTid, txFrame, /* aBurstCount */ 2, /* aBurstPeriodUs */ 10000,
                          /* aBurstChannelMask */ 0),
              kErrorNone);
    platform.GoInUs(1);
    EXPECT_LT(FindRcpFrame(frames, kBurstTid, SPINEL_PROP_LAST_STATUS), 0);
    EXPECT_GE(FindRcpFrame(frames, kBurstTid, SPINEL_PROP_RCP_TX_BURST), 0);
}

namespace {

class ReceiveFailingBurstPlatform : public TimedBurstRecorderPlatform
{
public:
    otError Receive(uint8_t aChannel) override
    {
        if (mFailNextReceive)
        {
            mFailNextReceive = false;
            return OT_ERROR_FAILED;
        }

        return TimedBurstRecorderPlatform::Receive(aChannel);
    }

    bool mFailNextReceive = false;
};

} // namespace

TEST(RadioSpinelTransmit, shouldNotSendStoppedBurstFrameWhenReceiveFails)
{
    ReceiveFailingBurstPlatform platform;
    uint8_t                     frameBuffer[OT_RADIO_FRAME_MAX_SIZE];
    Mac::TxFrame                txFrame;

    constexpr uint8_t kChannel = 15;

    PrepareTestFrame(txFrame, frameBuffer, kChannel);

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ASSERT_EQ(platform.mRadioSpinel.Receive(kChannel), kErrorNone);

    // A timed tick at 5 ms is handed to `SubMac` 1 ms before its target TX time.
    txFrame.mInfo.mTxInfo.mTxDelayBaseTime = static_cast<uint32_t>(platform.GetNow());
    txFrame.mInfo.mTxInfo.mTxDelay         = 5000;
    ASSERT_EQ(SendTxBurst(platform, /* aTid */ 1, txFrame, /* aBurstCount */ 2, /* aBurstPeriodUs */ 10000,
                          /* aBurstChannelMask */ 0),
              kErrorNone);
    platform.GoInUs(4500);
    ASSERT_TRUE(platform.mTxChannels.empty());

    // Stopping the burst drops the frame waiting in `SubMac`, even if the radio fails to receive.
    platform.mFailNextReceive = true;
    ASSERT_EQ(SendStopBurst(platform, /* aTid */ 2), kErrorNone);
    platform.GoInMs(50);
    EXPECT_TRUE(platform.mTxChannels.empty());
}

TEST(RadioSpinelTransmit, shouldRejectInvalidBurstConfiguration)
{
    FakeCoprocessorPlatform platform;
    uint8_t                 frameBuffer[OT_RADIO_FRAME_MAX_SIZE];
    Mac::TxFrame            txFrame;
    std::vector<RcpFrame>   frames;

    constexpr uint32_t kTooLongPeriodUs    = 0x7fffffff; // More than `Timer::kMaxDelay` minus the ahead time.
    constexpr uint32_t kUnsupportedMask    = (1UL << 5) | (1UL << 15);
    constexpr uint8_t  kUnsupportedChannel = 5;

    PrepareTestFrame(txFrame, frameBuffer, /* aChannel */ 15);

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ObserveRcpFrames(platform, frames);

    // 1. A period too long for the radio clock and the burst timer.
    ASSERT_EQ(SendTxBurst(platform, /* aTid */ 1, txFrame, /* aBurstCount */ 2, kTooLongPeriodUs,
                          /* aBurstChannelMask */ 0),
              kErrorNone);

    // 2. A channel mask with an unsupported channel.
    ASSERT_EQ(
        SendTxBurst(platform, /* aTid */ 2, txFrame, /* aBurstCount */ 2, /* aBurstPeriodUs */ 10000, kUnsupportedMask),
        kErrorNone);

    // 3. An unsupported channel without a channel mask.
    txFrame.mChannel                            = kUnsupportedChannel;
    txFrame.mInfo.mTxInfo.mRxChannelAfterTxDone = kUnsupportedChannel;
    ASSERT_EQ(SendTxBurst(platform, /* aTid */ 3, txFrame, /* aBurstCount */ 2, /* aBurstPeriodUs */ 10000,
                          /* aBurstChannelMask */ 0),
              kErrorNone);

    platform.GoInMs(50);

    for (spinel_tid_t tid = 1; tid <= 3; tid++)
    {
        int index = FindRcpFrame(frames, tid, SPINEL_PROP_LAST_STATUS);

        ASSERT_GE(index, 0);
        EXPECT_EQ(frames[index].mStatus, static_cast<unsigned int>(SPINEL_STATUS_INVALID_ARGUMENT));
    }

    EXPECT_TRUE(platform.mTxRecords.empty());
    EXPECT_EQ(CountRcpFrames(frames, /* aTid */ 0, SPINEL_PROP_RCP_TX_BURST), 0);
}

TEST(RadioSpinelTransmit, shouldAcceptBurstAfterLinkRawIsDisabledDuringEnergyScan)
{
    TimedBurstRecorderPlatform platform;
    uint8_t                    frameBuffer[OT_RADIO_FRAME_MAX_SIZE];
    Mac::TxFrame               txFrame;
    std::vector<RcpFrame>      frames;

    constexpr spinel_tid_t kBurstTid = 3;

    PrepareTestFrame(txFrame, frameBuffer, /* aChannel */ 15);

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ASSERT_EQ(platform.mRadioSpinel.EnergyScan(/* aScanChannel */ 15, /* aScanDuration */ 10), kErrorNone);
    ObserveRcpFrames(platform, frames);

    // Disabling the raw link-layer drops the energy scan, which never completes.
    ASSERT_EQ(SendSpinelCommand(platform, SPINEL_CMD_PROP_VALUE_SET, SPINEL_PROP_PHY_ENABLED, /* aTid */ 1,
                                SPINEL_DATATYPE_BOOL_S, false),
              kErrorNone);
    ASSERT_EQ(SendSpinelCommand(platform, SPINEL_CMD_PROP_VALUE_SET, SPINEL_PROP_PHY_ENABLED, /* aTid */ 2,
                                SPINEL_DATATYPE_BOOL_S, true),
              kErrorNone);
    platform.GoInMs(20);
    EXPECT_EQ(CountRcpFrames(frames, /* aTid */ 0, SPINEL_PROP_MAC_ENERGY_SCAN_RESULT), 0);

    // No state is left behind: a burst is accepted.
    ASSERT_EQ(SendTxBurst(platform, kBurstTid, txFrame, /* aBurstCount */ 2, /* aBurstPeriodUs */ 1000,
                          /* aBurstChannelMask */ 0),
              kErrorNone);
    platform.GoInUs(1);
    ASSERT_GE(FindRcpFrame(frames, kBurstTid, SPINEL_PROP_RCP_TX_BURST), 0);
    EXPECT_EQ(frames[FindRcpFrame(frames, kBurstTid, SPINEL_PROP_RCP_TX_BURST)].mBurstCount, 2u);

    platform.GoInMs(10);
    EXPECT_EQ(platform.mTxChannels.size(), 2u);
}

TEST(RadioSpinelTransmit, shouldRejectBurstDuringEnergyScan)
{
    TimedBurstRecorderPlatform platform;
    uint8_t                    frameBuffer[OT_RADIO_FRAME_MAX_SIZE];
    Mac::TxFrame               txFrame;
    std::vector<RcpFrame>      frames;

    constexpr spinel_tid_t kBurstTid = 1;

    PrepareTestFrame(txFrame, frameBuffer, /* aChannel */ 15);

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ASSERT_EQ(platform.mRadioSpinel.EnergyScan(/* aScanChannel */ 15, /* aScanDuration */ 10), kErrorNone);
    ObserveRcpFrames(platform, frames);

    ASSERT_EQ(SendTxBurst(platform, kBurstTid, txFrame, /* aBurstCount */ 3, /* aBurstPeriodUs */ 1000,
                          /* aBurstChannelMask */ 0),
              kErrorNone);
    platform.GoInUs(1);
    ASSERT_GE(FindRcpFrame(frames, kBurstTid, SPINEL_PROP_LAST_STATUS), 0);
    EXPECT_EQ(frames[FindRcpFrame(frames, kBurstTid, SPINEL_PROP_LAST_STATUS)].mStatus,
              static_cast<unsigned int>(SPINEL_STATUS_BUSY));

    // The energy scan is not affected and completes.
    platform.GoInMs(50);
    EXPECT_EQ(platform.mTxChannels.size(), 0u);
    EXPECT_EQ(CountRcpFrames(frames, /* aTid */ 0, SPINEL_PROP_MAC_ENERGY_SCAN_RESULT), 1);
}
#endif // OPENTHREAD_CONFIG_LINK_EXT_ENABLE

#if OPENTHREAD_CONFIG_LINK_EXT_ENABLE && (OPENTHREAD_CONFIG_LINK_EXT_PERIODIC_RX_MAX_SCHEDULES > 0)
static_assert(OPENTHREAD_CONFIG_LINK_EXT_PERIODIC_RX_MAX_SCHEDULES == 3,
              "The periodic RX tests assume the default number of schedules");

namespace {

using PeriodicRxSchedule = Spinel::RadioSpinel::PeriodicRxSchedule;

constexpr uint32_t kShrDurationUs   = 160; // SHR duration, part of the minimum kept window fragment.
constexpr uint8_t  kMinNumSchedules = 3;   // Recommended minimum (and default) number of schedules.
constexpr uint32_t kGuardUs         = OPENTHREAD_CONFIG_LINK_EXT_PERIODIC_RX_CHANNEL_SWITCH_GUARD_US;

bool operator==(const PeriodicRxSchedule &aLhs, const PeriodicRxSchedule &aRhs)
{
    return aLhs.mPhaseUs == aRhs.mPhaseUs && aLhs.mPeriodUs == aRhs.mPeriodUs && aLhs.mDurationUs == aRhs.mDurationUs &&
           aLhs.mChannel == aRhs.mChannel;
}

/**
 * Records every `otPlatRadioReceiveAt()` call made by the RCP.
 *
 * `aStart` is interpreted as the lower 32 bits of the absolute radio time, as specified by `otPlatRadioReceiveAt()`.
 */
class PeriodicRxPlatform : public FakeCoprocessorPlatform
{
public:
    struct Window
    {
        uint64_t mCalledAt;
        uint64_t mStart;
        uint32_t mDuration;
        uint8_t  mChannel;
    };

    otRadioCaps GetCaps() const override { return OT_RADIO_CAPS_RECEIVE_TIMING; }

    otError ReceiveAt(uint8_t aChannel, uint32_t aStart, uint32_t aDuration) override
    {
        uint64_t start = mNow + static_cast<int32_t>(aStart - static_cast<uint32_t>(mNow));

        if (aDuration == 0)
        {
            mNumCancels++;
        }
        else
        {
            mWindows.push_back({mNow, start, aDuration, aChannel});
        }

        mReceiveAtChannel = aChannel;
        mReceiveAtStart   = start;
        mReceiveAtEnd     = start + aDuration;

        return OT_ERROR_NONE;
    }

    void SetNow(uint64_t aNow) { mNow = aNow; }

    // Returns the windows whose start is before `aBefore`. The last window is
    // scheduled ahead of its start, so it may be beyond the simulated time.
    std::vector<Window> GetWindowsBefore(uint64_t aBefore) const
    {
        std::vector<Window> windows;

        for (const Window &window : mWindows)
        {
            if (window.mStart < aBefore)
            {
                windows.push_back(window);
            }
        }

        return windows;
    }

    std::vector<Window> mWindows;
    uint32_t            mNumCancels = 0;
};

} // namespace

TEST(RadioSpinelPeriodicRx, shouldGetTheSchedulesThatWereSet)
{
    const PeriodicRxSchedule kSchedules[] = {
        {1000, 500000, 1600, 11},
        {0, 1000000, 65535, 15},
        {0xffffff, 0x7fffffff, 160, 26}, // The longest period (`Timer::kMaxDelay`) and the shortest duration (SHR).
    };

    PeriodicRxPlatform platform;
    PeriodicRxSchedule schedules[kMinNumSchedules];
    uint8_t            numSchedules = kMinNumSchedules;

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);

    numSchedules = kMinNumSchedules;
    ASSERT_EQ(platform.mRadioSpinel.GetPeriodicRxSchedules(schedules, numSchedules), kErrorNone);
    EXPECT_EQ(numSchedules, 0);

    ASSERT_EQ(platform.mRadioSpinel.SetPeriodicRxSchedules(kSchedules, 3), kErrorNone);

    numSchedules = kMinNumSchedules;
    ASSERT_EQ(platform.mRadioSpinel.GetPeriodicRxSchedules(schedules, numSchedules), kErrorNone);
    ASSERT_EQ(numSchedules, 3);

    for (uint8_t i = 0; i < numSchedules; i++)
    {
        EXPECT_TRUE(schedules[i] == kSchedules[i]) << "index " << static_cast<int>(i);
    }

    // A `nullptr` array gets the number of schedules.
    numSchedules = 0;
    ASSERT_EQ(platform.mRadioSpinel.GetPeriodicRxSchedules(nullptr, numSchedules), kErrorNone);
    EXPECT_EQ(numSchedules, 3);

    // A too small array gets the schedules which fit and the number of schedules.
    for (PeriodicRxSchedule &schedule : schedules)
    {
        memset(&schedule, 0, sizeof(schedule));
    }

    numSchedules = 2;
    ASSERT_EQ(platform.mRadioSpinel.GetPeriodicRxSchedules(schedules, numSchedules), kErrorNoBufs);
    EXPECT_EQ(numSchedules, 3);
    EXPECT_TRUE(schedules[0] == kSchedules[0]);
    EXPECT_TRUE(schedules[1] == kSchedules[1]);
    EXPECT_EQ(schedules[2].mPeriodUs, 0u);

    // An empty table disables periodic RX.
    ASSERT_EQ(platform.mRadioSpinel.SetPeriodicRxSchedules(nullptr, 0), kErrorNone);

    numSchedules = kMinNumSchedules;
    ASSERT_EQ(platform.mRadioSpinel.GetPeriodicRxSchedules(schedules, numSchedules), kErrorNone);
    EXPECT_EQ(numSchedules, 0);
}

TEST(RadioSpinelPeriodicRx, shouldRejectInvalidSchedulesAndKeepThePreviousOnes)
{
    const PeriodicRxSchedule kValid[]      = {{1000, 500000, 1600, 11}};
    const PeriodicRxSchedule kInvalid[][1] = {
        {{1000, 500000, 1600, 10}},   // Channel not in the supported channel mask (11-26).
        {{1000, 500000, 1600, 27}},   // Channel not in the supported channel mask (11-26).
        {{0, 0, 1600, 11}},           // Zero period.
        {{500000, 500000, 1600, 11}}, // Phase not less than period.
        {{1000, 500000, 0, 11}},      // Zero duration.
        {{1000, 500000, 159, 11}},    // Duration shorter than the SHR duration.
        {{0, 1000, 1001, 11}},        // Duration longer than period.
        {{0, 0x80000000, 1600, 11}},  // Period longer than `Timer::kMaxDelay` (2^31 - 1).
    };
    const PeriodicRxSchedule kTooMany[] = {
        {0, 500000, 160, 11},
        {0, 500000, 160, 12},
        {0, 500000, 160, 13},
        {0, 500000, 160, 14},
    };

    PeriodicRxPlatform platform;
    PeriodicRxSchedule schedules[kMinNumSchedules];
    uint8_t            numSchedules;

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ASSERT_EQ(platform.mRadioSpinel.SetPeriodicRxSchedules(kValid, 1), kErrorNone);

    for (const auto &invalid : kInvalid)
    {
        EXPECT_EQ(platform.mRadioSpinel.SetPeriodicRxSchedules(invalid, 1), kErrorInvalidArgs);
    }

    // More schedules than the RCP supports (`OPENTHREAD_CONFIG_LINK_EXT_PERIODIC_RX_MAX_SCHEDULES`).
    EXPECT_EQ(platform.mRadioSpinel.SetPeriodicRxSchedules(kTooMany, 4), kErrorNoBufs);

    numSchedules = kMinNumSchedules;
    ASSERT_EQ(platform.mRadioSpinel.GetPeriodicRxSchedules(schedules, numSchedules), kErrorNone);
    ASSERT_EQ(numSchedules, 1);
    EXPECT_TRUE(schedules[0] == kValid[0]);

    // The shortest valid duration is the SHR duration.
    EXPECT_EQ(platform.mRadioSpinel.SetPeriodicRxSchedules(kTooMany, 1), kErrorNone);
}

TEST(RadioSpinelPeriodicRx, shouldRejectSchedulesWhenTheRadioCannotReceiveAtATime)
{
    // `FakeCoprocessorPlatform` doesn't report `OT_RADIO_CAPS_RECEIVE_TIMING`.
    const PeriodicRxSchedule kSchedules[] = {{0, 50000, 1600, 15}};

    FakeCoprocessorPlatform platform;
    PeriodicRxSchedule      schedules[3];
    uint8_t                 numSchedules = 3;

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);

    EXPECT_EQ(platform.mRadioSpinel.SetPeriodicRxSchedules(kSchedules, 1), kErrorNotImplemented);
    EXPECT_EQ(platform.mRadioSpinel.SetPeriodicRxSchedules(nullptr, 0), kErrorNone);

    ASSERT_EQ(platform.mRadioSpinel.GetPeriodicRxSchedules(schedules, numSchedules), kErrorNone);
    EXPECT_EQ(numSchedules, 0);
}

TEST(RadioSpinelPeriodicRx, shouldStartWindowsTheGuardTimeBeforeRadioTimeModuloPeriodEqualsPhase)
{
    // A window starts when the radio time modulo the period equals the phase.
    // It is scheduled the guard time earlier, so that the radio is ready to
    // receive at the window start.
    constexpr uint32_t kPhaseUs    = 3000;
    constexpr uint32_t kPeriodUs   = 100000;
    constexpr uint16_t kDurationUs = 1250;
    constexpr uint8_t  kChannel    = 15;

    const PeriodicRxSchedule kSchedules[] = {{kPhaseUs, kPeriodUs, kDurationUs, kChannel}};

    PeriodicRxPlatform platform;

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ASSERT_EQ(platform.mRadioSpinel.SetPeriodicRxSchedules(kSchedules, 1), kErrorNone);

    platform.GoInMs(1000);

    std::vector<PeriodicRxPlatform::Window> windows = platform.GetWindowsBefore(platform.GetNow());

    ASSERT_GE(windows.size(), 9u);

    for (size_t i = 0; i < windows.size(); i++)
    {
        EXPECT_EQ(windows[i].mStart % kPeriodUs, kPhaseUs - kGuardUs);
        EXPECT_EQ(windows[i].mDuration, kDurationUs + kGuardUs);
        EXPECT_EQ(windows[i].mChannel, kChannel);
        EXPECT_LE(windows[i].mCalledAt, windows[i].mStart);

        if (i > 0)
        {
            EXPECT_EQ(windows[i].mStart - windows[i - 1].mStart, kPeriodUs);
        }
    }
}

TEST(RadioSpinelPeriodicRx, shouldUseFull64BitRadioTimeAcross32BitWrap)
{
    constexpr uint32_t kPhaseUs  = 12345;
    constexpr uint32_t kPeriodUs = 30000;
    constexpr uint64_t kWrap     = (1ULL << 32);

    const PeriodicRxSchedule kSchedules[] = {{kPhaseUs, kPeriodUs, 800, 20}};

    PeriodicRxPlatform platform;

    platform.SetNow(kWrap - 100000);

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ASSERT_EQ(platform.mRadioSpinel.SetPeriodicRxSchedules(kSchedules, 1), kErrorNone);

    platform.GoInMs(300);

    std::vector<PeriodicRxPlatform::Window> windows = platform.GetWindowsBefore(platform.GetNow());
    bool                                    crossed = false;

    ASSERT_GE(windows.size(), 8u);

    for (size_t i = 0; i < windows.size(); i++)
    {
        // `kWrap` is not a multiple of `kPeriodUs`, so this only holds if the
        // modulo is evaluated on the 64-bit time.
        EXPECT_EQ(windows[i].mStart % kPeriodUs, kPhaseUs - kGuardUs);

        if (i > 0)
        {
            EXPECT_EQ(windows[i].mStart - windows[i - 1].mStart, kPeriodUs);
        }

        crossed |= (windows[i].mStart >= kWrap);
    }

    EXPECT_TRUE(crossed);
}

TEST(RadioSpinelPeriodicRx, shouldKeepSchedulingWindowsAcrossReceiveAndSleep)
{
    // Timed RX windows only take effect while the radio is in the Sleep
    // state, which is enforced by the radio platform per the contract of
    // `otPlatRadioReceiveAt()`. The RCP keeps scheduling windows (without
    // drift) while the radio is in Receive state.

    constexpr uint32_t kPhaseUs  = 10000;
    constexpr uint32_t kPeriodUs = 50000;

    const PeriodicRxSchedule kSchedules[] = {{kPhaseUs, kPeriodUs, 1600, 11}};

    PeriodicRxPlatform platform;

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ASSERT_EQ(platform.mRadioSpinel.SetPeriodicRxSchedules(kSchedules, 1), kErrorNone);

    platform.GoInMs(200);
    ASSERT_EQ(platform.mRadioSpinel.Receive(11), kErrorNone);
    platform.GoInMs(500);
    ASSERT_EQ(platform.mRadioSpinel.Sleep(), kErrorNone);
    platform.GoInMs(500);

    std::vector<PeriodicRxPlatform::Window> windows = platform.GetWindowsBefore(platform.GetNow());

    ASSERT_GE(windows.size(), 23u);
    EXPECT_EQ(platform.mNumCancels, 0u);

    for (size_t i = 0; i < windows.size(); i++)
    {
        EXPECT_EQ(windows[i].mStart % kPeriodUs, kPhaseUs - kGuardUs);
        EXPECT_EQ(windows[i].mDuration, 1600u + kGuardUs);

        if (i > 0)
        {
            EXPECT_EQ(windows[i].mStart - windows[i - 1].mStart, kPeriodUs);
        }
    }
}

TEST(RadioSpinelPeriodicRx, shouldStopSchedulingWindowsWhenSchedulesAreCleared)
{
    const PeriodicRxSchedule kSchedules[] = {{0, 50000, 1600, 15}};

    PeriodicRxPlatform platform;

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ASSERT_EQ(platform.mRadioSpinel.SetPeriodicRxSchedules(kSchedules, 1), kErrorNone);
    platform.GoInMs(200);

    ASSERT_EQ(platform.mRadioSpinel.SetPeriodicRxSchedules(nullptr, 0), kErrorNone);

    // A window already scheduled on the radio is not cancelled (it may
    // still occur once), but no further windows are scheduled.
    platform.mWindows.clear();
    platform.GoInMs(500);
    EXPECT_TRUE(platform.mWindows.empty());
}

TEST(RadioSpinelPeriodicRx, shouldKeepTheRemainderOfAWindowPreemptedAtItsStart)
{
    // Schedule 0 on channel 11 at [0, 1600) every 10 ms.
    // Schedule 1 on channel 12 at 1000 + 25000k (1600 us): occurrences
    // alternate between overlapping schedule 0 (1000 mod 10000), which
    // keeps the remainder [1600, 2600) starting when schedule 0 ends, and
    // not (6000 mod 10000), which start the guard time early. The test
    // starts between windows, so that the first windows are full ones.
    const PeriodicRxSchedule kSchedules[] = {
        {0, 10000, 1600, 11},
        {1000, 25000, 1600, 12},
    };

    PeriodicRxPlatform platform;
    size_t             numChannel11 = 0;
    size_t             numFull      = 0;
    size_t             numPartial   = 0;

    platform.SetNow(5000);

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ASSERT_EQ(platform.mRadioSpinel.SetPeriodicRxSchedules(kSchedules, 2), kErrorNone);

    platform.GoInMs(500);

    std::vector<PeriodicRxPlatform::Window> windows = platform.GetWindowsBefore(platform.GetNow());

    for (size_t i = 0; i < windows.size(); i++)
    {
        const PeriodicRxPlatform::Window &window = windows[i];

        if (window.mChannel == 11)
        {
            numChannel11++;
            EXPECT_EQ(window.mStart % 10000, 10000 - kGuardUs);
            EXPECT_EQ(window.mDuration, 1600u + kGuardUs);
        }
        else if (window.mStart % 10000 == 6000 - kGuardUs)
        {
            numFull++;
            EXPECT_EQ(window.mChannel, 12);
            EXPECT_EQ((window.mStart + kGuardUs) % 25000, 1000u);
            EXPECT_EQ(window.mDuration, 1600u + kGuardUs);
        }
        else
        {
            numPartial++;
            EXPECT_EQ(window.mChannel, 12);
            EXPECT_EQ(window.mStart % 10000, 1600u);
            EXPECT_EQ((window.mStart - 600) % 25000, 1000u);
            EXPECT_EQ(window.mDuration, 1000u);
        }

        if (i > 0)
        {
            EXPECT_GE(window.mStart, windows[i - 1].mStart + windows[i - 1].mDuration);
        }
    }

    EXPECT_GE(numChannel11, 45u);
    EXPECT_GE(numFull, 9u);
    EXPECT_GE(numPartial, 9u);
}

TEST(RadioSpinelPeriodicRx, shouldSplitAPreemptedWindowAndTakeTheGuardTimeFromIt)
{
    // Schedule 1 on channel 12 at [4000, 8000) is preempted by schedule 0
    // on channel 11 at [5000, 6600). Schedule 0 starts the guard time
    // early, which is taken from schedule 1. Schedule 1 also starts the
    // guard time early (from idle), and its remainder starts when schedule
    // 0 ends (the guard time is never taken from a higher-priority window):
    //
    //   channel 12: [4000 - guard, 5000 - guard)
    //   channel 11: [5000 - guard, 6600)
    //   channel 12: [6600, 8000)
    static_assert(1000 >= kGuardUs + kShrDurationUs, "the leading fragment must be kept");

    const PeriodicRxSchedule kSchedules[] = {
        {5000, 10000, 1600, 11},
        {4000, 10000, 4000, 12},
    };

    PeriodicRxPlatform platform;

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ASSERT_EQ(platform.mRadioSpinel.SetPeriodicRxSchedules(kSchedules, 2), kErrorNone);

    platform.GoInMs(200);

    std::vector<PeriodicRxPlatform::Window> windows = platform.GetWindowsBefore(platform.GetNow());

    ASSERT_GE(windows.size(), 3 * 19u);

    for (const PeriodicRxPlatform::Window &window : windows)
    {
        switch (window.mStart % 10000)
        {
        case 4000 - kGuardUs:
            EXPECT_EQ(window.mChannel, 12);
            EXPECT_EQ(window.mDuration, 1000u);
            break;
        case 5000 - kGuardUs:
            EXPECT_EQ(window.mChannel, 11);
            EXPECT_EQ(window.mDuration, 1600 + kGuardUs);
            break;
        case 6600:
            EXPECT_EQ(window.mChannel, 12);
            EXPECT_EQ(window.mDuration, 1400u);
            break;
        default:
            ADD_FAILURE() << "unexpected window start " << window.mStart;
            break;
        }
    }
}

TEST(RadioSpinelPeriodicRx, shouldDropAPartialWindowShorterThanTheMinimum)
{
    // Schedule 1 on channel 12 at [kPhase1, kPhase1 + 4000) is preempted by
    // schedule 0 on channel 11 at [5000, 6600). The part before the
    // preemption (including its own guard, guard + 80 us) is shorter than
    // guard + 160 us and is dropped:
    //
    //   channel 11: [5000 - guard, 6600)
    //   channel 12: [6600, kPhase1 + 4000)
    constexpr uint32_t kPhase1 = 5000 - kGuardUs - kShrDurationUs / 2;

    static_assert(kPhase1 + 4000 > 6600, "the trailing fragment must exist");

    const PeriodicRxSchedule kSchedules[] = {
        {5000, 10000, 1600, 11},
        {kPhase1, 10000, 4000, 12},
    };

    PeriodicRxPlatform platform;

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ASSERT_EQ(platform.mRadioSpinel.SetPeriodicRxSchedules(kSchedules, 2), kErrorNone);

    platform.GoInMs(200);

    std::vector<PeriodicRxPlatform::Window> windows = platform.GetWindowsBefore(platform.GetNow());

    ASSERT_GE(windows.size(), 2 * 19u);

    for (const PeriodicRxPlatform::Window &window : windows)
    {
        switch (window.mStart % 10000)
        {
        case 5000 - kGuardUs:
            EXPECT_EQ(window.mChannel, 11);
            EXPECT_EQ(window.mDuration, 1600 + kGuardUs);
            break;
        case 6600:
            EXPECT_EQ(window.mChannel, 12);
            EXPECT_EQ(window.mDuration, kPhase1 + 4000 - 6600);
            break;
        default:
            ADD_FAILURE() << "unexpected window start " << window.mStart;
            break;
        }
    }
}

TEST(RadioSpinelPeriodicRx, shouldMergeOverlappingWindowsOnTheSameChannel)
{
    // [0, 1600) and [1000, 2600) on the same channel merge into [0, 2600),
    // which starts the guard time early.
    const PeriodicRxSchedule kSchedules[] = {
        {0, 10000, 1600, 11},
        {1000, 10000, 1600, 11},
    };

    PeriodicRxPlatform platform;

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ASSERT_EQ(platform.mRadioSpinel.SetPeriodicRxSchedules(kSchedules, 2), kErrorNone);

    platform.GoInMs(200);

    std::vector<PeriodicRxPlatform::Window> windows = platform.GetWindowsBefore(platform.GetNow());

    ASSERT_GE(windows.size(), 18u);

    // The first window may be a partial one if the schedules were set in the
    // middle of a period.
    for (size_t i = 1; i < windows.size(); i++)
    {
        EXPECT_EQ(windows[i].mStart % 10000, 10000 - kGuardUs);
        EXPECT_EQ(windows[i].mDuration, 2600u + kGuardUs);
        EXPECT_EQ(windows[i].mChannel, 11);
    }
}

TEST(RadioSpinelPeriodicRx, shouldSplitContinuousCoverageIntoBackToBackWindows)
{
    // Schedule 0 covers channel 11 continuously (duration == period). With a
    // long-period schedule on the same channel, merging used to walk every
    // event up to the long period. Merging is now bounded, and the continuous
    // coverage is split into back-to-back windows on the same channel.
    const PeriodicRxSchedule kSchedules[] = {
        {0, 160, 160, 11},
        {0, 2000000000UL, 160, 11},
    };

    PeriodicRxPlatform platform;

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ASSERT_EQ(platform.mRadioSpinel.SetPeriodicRxSchedules(kSchedules, 2), kErrorNone);

    platform.GoInMs(100);

    std::vector<PeriodicRxPlatform::Window> windows = platform.GetWindowsBefore(platform.GetNow());

    ASSERT_GE(windows.size(), 2u);

    for (size_t i = 0; i < windows.size(); i++)
    {
        EXPECT_EQ(windows[i].mChannel, 11);
        EXPECT_LT(windows[i].mDuration, 100000u);

        if (i > 0)
        {
            EXPECT_EQ(windows[i].mStart, windows[i - 1].mStart + windows[i - 1].mDuration);
        }
    }
}

namespace {

// Advances time while the radio is called, as on real hardware.
class SlowPeriodicRxPlatform : public PeriodicRxPlatform
{
public:
    otError ReceiveAt(uint8_t aChannel, uint32_t aStart, uint32_t aDuration) override
    {
        otError error = PeriodicRxPlatform::ReceiveAt(aChannel, aStart, aDuration);

        // Stop advancing time after too many calls, so that a
        // regression fails the checks below instead of hanging.
        if (mWindows.size() < kMaxWindows)
        {
            mNow += kRadioCallUs;
        }

        return error;
    }

    static constexpr size_t   kMaxWindows  = 1000;
    static constexpr uint32_t kRadioCallUs = 5;
};

} // namespace

TEST(RadioSpinelPeriodicRx, shouldNotRescheduleRepeatedlyWhenTimeAdvancesDuringContinuousCoverage)
{
    // Schedule 0 covers channel 15 continuously (duration == period), so
    // merged windows are capped at the period. Time advances while the
    // radio is called, as on real hardware, so the listen scheduler never
    // evaluates exactly at a window start. The windows must still be
    // provided once per cap, not re-provided on every evaluation.

    constexpr uint32_t       kPeriodUs      = 10000;
    const PeriodicRxSchedule kSchedules[]   = {{0, kPeriodUs, kPeriodUs, 15}};
    constexpr uint32_t       kTestTimeMs    = 30;
    constexpr size_t         kMaxNumWindows = kTestTimeMs * 1000 / kPeriodUs + 2;

    SlowPeriodicRxPlatform platform;

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ASSERT_EQ(platform.mRadioSpinel.SetPeriodicRxSchedules(kSchedules, 1), kErrorNone);

    platform.GoInMs(kTestTimeMs);

    EXPECT_LE(platform.mWindows.size(), kMaxNumWindows);

    for (size_t i = 0; i < platform.mWindows.size(); i++)
    {
        EXPECT_EQ(platform.mWindows[i].mChannel, 15);

        if (i > 0)
        {
            EXPECT_EQ(platform.mWindows[i].mStart,
                      platform.mWindows[i - 1].mStart + platform.mWindows[i - 1].mDuration);
        }
    }
}

// Checks that the windows scheduled on the radio are the periodic windows (started the guard time early), each
// scheduled once.
static void ExpectEachWindowOnce(const std::vector<PeriodicRxPlatform::Window> &aWindows,
                                 uint32_t                                       aPhaseUs,
                                 uint32_t                                       aPeriodUs)
{
    for (size_t i = 0; i < aWindows.size(); i++)
    {
        EXPECT_EQ(aWindows[i].mStart % aPeriodUs, aPhaseUs - kGuardUs);

        if (i > 0)
        {
            EXPECT_EQ(aWindows[i].mStart - aWindows[i - 1].mStart, aPeriodUs);
        }
    }
}

TEST(RadioSpinelPeriodicRx, shouldScheduleWindowsSetBeforeTheRadioIsEnabled)
{
    constexpr uint32_t kPhaseUs  = 10000;
    constexpr uint32_t kPeriodUs = 50000;

    const PeriodicRxSchedule kSchedules[] = {{kPhaseUs, kPeriodUs, 1600, 11}};

    PeriodicRxPlatform platform;

    ASSERT_EQ(platform.mRadioSpinel.SetPeriodicRxSchedules(kSchedules, 1), kErrorNone);
    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    platform.GoInMs(500);

    std::vector<PeriodicRxPlatform::Window> windows = platform.GetWindowsBefore(platform.GetNow());

    ASSERT_GE(windows.size(), 9u);
    ExpectEachWindowOnce(windows, kPhaseUs, kPeriodUs);
}

TEST(RadioSpinelPeriodicRx, shouldScheduleEachWindowOnceAfterTheRadioIsReEnabled)
{
    // Enabling the radio schedules the first window. It must not be
    // scheduled again when the scheduler is evaluated at its start.

    constexpr uint32_t kPhaseUs  = 10000;
    constexpr uint32_t kPeriodUs = 50000;

    const PeriodicRxSchedule kSchedules[] = {{kPhaseUs, kPeriodUs, 1600, 11}};

    PeriodicRxPlatform platform;

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ASSERT_EQ(platform.mRadioSpinel.SetPeriodicRxSchedules(kSchedules, 1), kErrorNone);
    platform.GoInMs(120);

    ASSERT_EQ(platform.mRadioSpinel.Disable(), kErrorNone);
    platform.GoInMs(10);
    platform.mWindows.clear();

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    platform.GoInMs(500);

    std::vector<PeriodicRxPlatform::Window> windows = platform.GetWindowsBefore(platform.GetNow());

    ASSERT_GE(windows.size(), 9u);
    ExpectEachWindowOnce(windows, kPhaseUs, kPeriodUs);
}

TEST(RadioSpinelPeriodicRx, shouldTakeTheWholeGuardWhenThePreemptedWindowEndsWithinIt)
{
    // Schedule 0 on channel 11 at [5000, 6600) every 20 ms.
    // Schedule 1 on channel 12 at [3000, 4980) every 20 ms (started the
    // guard time early) ends within the guard of schedule 0. Schedule 0
    // takes the whole guard, so the radio switches to channel 11 at
    // 5000 - kGuardUs.
    constexpr uint32_t kPeriodUs = 20000;

    const PeriodicRxSchedule kSchedules[] = {{5000, kPeriodUs, 1600, 11}, {3000, kPeriodUs, 1980, 12}};

    PeriodicRxPlatform platform;

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ASSERT_EQ(platform.mRadioSpinel.SetPeriodicRxSchedules(kSchedules, 2), kErrorNone);
    platform.GoInUs(kPeriodUs);

    std::vector<PeriodicRxPlatform::Window> windows = platform.GetWindowsBefore(kPeriodUs);

    ASSERT_EQ(windows.size(), 2u);
    EXPECT_EQ(windows[0].mChannel, 12);
    EXPECT_EQ(windows[0].mStart, 3000u - kGuardUs);
    EXPECT_EQ(windows[0].mStart + windows[0].mDuration, 5000u - kGuardUs);
    EXPECT_EQ(windows[1].mChannel, 11);
    EXPECT_EQ(windows[1].mStart, 5000u - kGuardUs);
    EXPECT_EQ(windows[1].mStart + windows[1].mDuration, 6600u);
}

TEST(RadioSpinelPeriodicRx, shouldTakeTheGuardWhenAWindowStartsWithinIt)
{
    // Schedule 0 on channel 11 at [5000, 6600) every 20 ms.
    // Schedule 1 on channel 12 at [4980, 8000) every 20 ms starts within the
    // guard of schedule 0 while no window is active. Schedule 0 takes the
    // whole guard. The part of schedule 1 before it (from its own guard
    // start) is shorter than the minimum fragment and dropped.
    constexpr uint32_t kPeriodUs = 20000;

    static_assert(5000 - 4980 < kGuardUs + kShrDurationUs,
                  "the part of schedule 1 before the guard of schedule 0 must be dropped");

    const PeriodicRxSchedule kSchedules[] = {{5000, kPeriodUs, 1600, 11}, {4980, kPeriodUs, 3020, 12}};

    PeriodicRxPlatform platform;

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ASSERT_EQ(platform.mRadioSpinel.SetPeriodicRxSchedules(kSchedules, 2), kErrorNone);
    platform.GoInUs(kPeriodUs);

    std::vector<PeriodicRxPlatform::Window> windows = platform.GetWindowsBefore(kPeriodUs);

    ASSERT_EQ(windows.size(), 2u);
    EXPECT_EQ(windows[0].mChannel, 11);
    EXPECT_EQ(windows[0].mStart, 5000u - kGuardUs);
    EXPECT_EQ(windows[0].mStart + windows[0].mDuration, 6600u);
    EXPECT_EQ(windows[1].mChannel, 12);
    EXPECT_EQ(windows[1].mStart, 6600u);
    EXPECT_EQ(windows[1].mStart + windows[1].mDuration, 8000u);
}

TEST(RadioSpinelPeriodicRx, shouldNotScheduleTheRestOfAnActiveWindowAgainWhenSchedulesAreUpdated)
{
    // Updating the schedules while a window is active does not change it,
    // so the radio is not asked to receive the rest of it again, and the
    // following window is scheduled once.
    constexpr uint32_t kPhaseUs  = 10000;
    constexpr uint32_t kPeriodUs = 50000;

    const PeriodicRxSchedule kSchedules[] = {{kPhaseUs, kPeriodUs, 10000, 11}};

    PeriodicRxPlatform platform;

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ASSERT_EQ(platform.mRadioSpinel.SetPeriodicRxSchedules(kSchedules, 1), kErrorNone);
    platform.GoInUs(2 * kPeriodUs + kPhaseUs + 9000 - platform.GetNow());
    platform.mWindows.clear();

    ASSERT_EQ(platform.mRadioSpinel.SetPeriodicRxSchedules(kSchedules, 1), kErrorNone);
    platform.GoInUs(2 * kPeriodUs);

    std::vector<PeriodicRxPlatform::Window> windows = platform.GetWindowsBefore(platform.GetNow());

    ASSERT_GE(windows.size(), 2u);

    for (const PeriodicRxPlatform::Window &window : windows)
    {
        EXPECT_EQ(window.mStart % kPeriodUs, kPhaseUs - kGuardUs);
        EXPECT_EQ(window.mDuration, 10000u + kGuardUs);
    }

    ExpectEachWindowOnce(windows, kPhaseUs, kPeriodUs);
}

TEST(RadioSpinelPeriodicRx, shouldNotEndAWindowAtADroppedFragment)
{
    // Schedule 0 on channel 11 at [4100, 6000), schedule 1 on channel 12 at
    // [4000, 4500), schedule 2 on channel 11 at [0, 10000), all every 20 ms.
    // Schedule 1 owns [4000 - kGuardUs, 4100 - kGuardUs), which is shorter
    // than the minimum fragment and dropped, so channel 11 continues
    // through it. The window of the next period starts the guard time
    // before it, so only the windows starting before then are checked.
    constexpr uint32_t kPeriodUs = 20000;

    const PeriodicRxSchedule kSchedules[] = {
        {4100, kPeriodUs, 1900, 11}, {4000, kPeriodUs, 500, 12}, {0, kPeriodUs, 10000, 11}};

    PeriodicRxPlatform platform;

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ASSERT_EQ(platform.mRadioSpinel.SetPeriodicRxSchedules(kSchedules, 3), kErrorNone);
    platform.GoInUs(kPeriodUs);

    std::vector<PeriodicRxPlatform::Window> windows = platform.GetWindowsBefore(kPeriodUs - kGuardUs);

    ASSERT_EQ(windows.size(), 1u);
    EXPECT_EQ(windows[0].mChannel, 11);
    EXPECT_EQ(windows[0].mStart, 0u);
    EXPECT_EQ(windows[0].mStart + windows[0].mDuration, 10000u);
}

TEST(RadioSpinelPeriodicRx, shouldKeepTheShortRemainderOfASplitWindow)
{
    // Schedule 1 covers channel 11 continuously, interrupted by schedule 0
    // on channel 12 at [0, 1000) every 29800 + kGuardUs (its guard starts at
    // 29800). The channel 11 coverage [1000, 29800) is split into
    // back-to-back windows after `kPeriodicRxMaxMergeEvents` events. The
    // last one, [29700, 29800), is shorter than the minimum fragment, but it
    // continues the previous window, so it is kept.
    const PeriodicRxSchedule kSchedules[] = {{0, 29800 + kGuardUs, 1000, 12}, {0, 300, 300, 11}};

    PeriodicRxPlatform platform;

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ASSERT_EQ(platform.mRadioSpinel.SetPeriodicRxSchedules(kSchedules, 2), kErrorNone);
    platform.GoInUs(30000);

    std::vector<PeriodicRxPlatform::Window> windows = platform.GetWindowsBefore(29800);

    ASSERT_GE(windows.size(), 3u);
    EXPECT_EQ(windows[0].mChannel, 12);
    EXPECT_EQ(windows[0].mStart + windows[0].mDuration, 1000u);

    for (size_t i = 1; i < windows.size(); i++)
    {
        EXPECT_EQ(windows[i].mChannel, 11);
        EXPECT_EQ(windows[i].mStart, windows[i - 1].mStart + windows[i - 1].mDuration);
    }

    EXPECT_EQ(windows.back().mStart + windows.back().mDuration, 29800u);
    EXPECT_LT(windows.back().mDuration, 200u);
}

TEST(RadioSpinelPeriodicRx, shouldDropTheShortRemainderOfAWindowTheRadioWasNotReceivingIn)
{
    // Schedule 0 on channel 11 at [0, 10000) every 20 ms is set when less
    // than the minimum fragment of a window is left. The radio was not
    // receiving on channel 11 before, so the remainder does not continue a
    // window and is dropped: the first window is the one of the next period
    // (started the guard time early).
    constexpr uint32_t kPeriodUs   = 20000;
    constexpr uint32_t kDurationUs = 10000;
    constexpr uint32_t kSetAtUs    = kPeriodUs + kDurationUs - kShrDurationUs / 2;

    const PeriodicRxSchedule kSchedules[] = {{0, kPeriodUs, kDurationUs, 11}};

    PeriodicRxPlatform platform;

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ASSERT_LT(platform.GetNow(), kSetAtUs);
    platform.GoInUs(kSetAtUs - platform.GetNow());
    platform.mWindows.clear();

    ASSERT_EQ(platform.mRadioSpinel.SetPeriodicRxSchedules(kSchedules, 1), kErrorNone);
    platform.GoInUs(2 * kPeriodUs);

    std::vector<PeriodicRxPlatform::Window> windows = platform.GetWindowsBefore(platform.GetNow());

    ASSERT_GE(windows.size(), 1u);
    EXPECT_EQ(windows[0].mStart, 2 * kPeriodUs - kGuardUs);
    EXPECT_EQ(windows[0].mDuration, kDurationUs + kGuardUs);
    EXPECT_EQ(windows[0].mChannel, 11);
}

TEST(RadioSpinelPeriodicRx, shouldDropAShortWindowPreemptedNearItsStart)
{
    // Schedule 1 on channel 12 at [4200, 4400) every 20 ms is shorter than
    // the minimum fragment. It is preempted by schedule 0 on channel 11 at
    // [4280 + guard, ...) (its guard starts at 4280), so only guard + 80 us
    // of it (from its own guard start) are left, i.e. 80 us once the radio is
    // ready. That is shorter than the full window (duration plus guard) and
    // than the minimum fragment, so it is dropped.
    constexpr uint32_t kPeriodUs = 20000;

    const PeriodicRxSchedule kSchedules[] = {{4280 + kGuardUs, kPeriodUs, 1600, 11}, {4200, kPeriodUs, 200, 12}};

    static_assert(200 < kGuardUs + kShrDurationUs, "schedule 1 must be shorter than the minimum fragment");

    PeriodicRxPlatform platform;

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ASSERT_EQ(platform.mRadioSpinel.SetPeriodicRxSchedules(kSchedules, 2), kErrorNone);
    platform.GoInUs(kPeriodUs);

    std::vector<PeriodicRxPlatform::Window> windows = platform.GetWindowsBefore(kPeriodUs);

    ASSERT_EQ(windows.size(), 1u);
    EXPECT_EQ(windows[0].mChannel, 11);
    EXPECT_EQ(windows[0].mStart, 4280u);
    EXPECT_EQ(windows[0].mDuration, 1600u + kGuardUs);
}

TEST(RadioSpinelPeriodicRx, shouldDropAShortWindowFollowingAHigherPriorityWindow)
{
    // Schedule 1 on channel 12 at [6600, 6800) every 20 ms starts when
    // schedule 0 on channel 11 at [5000, 6600) ends, so it gets no guard
    // time. Without the guard, the radio is ready only for 200 - guard us of
    // it, which is shorter than the SHR duration, so it is dropped.
    constexpr uint32_t kPeriodUs = 20000;

    const PeriodicRxSchedule kSchedules[] = {{5000, kPeriodUs, 1600, 11}, {6600, kPeriodUs, 200, 12}};

    static_assert(200 < kGuardUs + kShrDurationUs, "schedule 1 must be shorter than the minimum fragment");

    PeriodicRxPlatform platform;

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ASSERT_EQ(platform.mRadioSpinel.SetPeriodicRxSchedules(kSchedules, 2), kErrorNone);
    platform.GoInUs(kPeriodUs);

    std::vector<PeriodicRxPlatform::Window> windows = platform.GetWindowsBefore(kPeriodUs);

    ASSERT_EQ(windows.size(), 1u);
    EXPECT_EQ(windows[0].mChannel, 11);
    EXPECT_EQ(windows[0].mStart, 5000u - kGuardUs);
    EXPECT_EQ(windows[0].mStart + windows[0].mDuration, 6600u);
}

TEST(RadioSpinelPeriodicRx, shouldKeepAShortWindowWithItsGuard)
{
    // Schedule 0 on channel 12 at [4200, 4400) every 20 ms is shorter than
    // the minimum fragment, but it is not truncated (it starts the guard
    // time early, from idle), so it is kept.
    constexpr uint32_t kPeriodUs = 20000;

    const PeriodicRxSchedule kSchedules[] = {{4200, kPeriodUs, 200, 12}};

    PeriodicRxPlatform platform;

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ASSERT_EQ(platform.mRadioSpinel.SetPeriodicRxSchedules(kSchedules, 1), kErrorNone);
    platform.GoInUs(kPeriodUs);

    std::vector<PeriodicRxPlatform::Window> windows = platform.GetWindowsBefore(kPeriodUs);

    ASSERT_EQ(windows.size(), 1u);
    EXPECT_EQ(windows[0].mChannel, 12);
    EXPECT_EQ(windows[0].mStart, 4200u - kGuardUs);
    EXPECT_EQ(windows[0].mDuration, 200u + kGuardUs);
}

TEST(RadioSpinelPeriodicRx, shouldNotLetALowerPriorityGuardTakeAHigherPriorityGuardOnTheSameChannel)
{
    // Schedule 2 on channel 11 at [1000, 10000) covers the guards of
    // schedule 0 on channel 11 at [5000, 6000) and schedule 1 on channel 12
    // at [5100, 6100), all every 20 ms. Within both guards, schedule 0 (on
    // the channel of schedule 2) owns the radio, not schedule 1. The
    // schedules are set within both guards, so the first window starts then
    // on channel 11 and continues until schedule 2 ends (schedule 1 only
    // gets fragments shorter than the minimum, which are dropped).
    constexpr uint32_t kPeriodUs = 20000;
    constexpr uint32_t kSetAtUs  = 4950;

    static_assert((kSetAtUs >= 5000 - kGuardUs) && (kSetAtUs >= 5100 - kGuardUs), "must be within both guards");

    const PeriodicRxSchedule kSchedules[] = {
        {5000, kPeriodUs, 1000, 11}, {5100, kPeriodUs, 1000, 12}, {1000, kPeriodUs, 9000, 11}};

    PeriodicRxPlatform platform;

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ASSERT_LT(platform.GetNow(), kSetAtUs);
    platform.GoInUs(kSetAtUs - platform.GetNow());
    platform.mWindows.clear();

    ASSERT_EQ(platform.mRadioSpinel.SetPeriodicRxSchedules(kSchedules, 3), kErrorNone);
    platform.GoInUs(kPeriodUs);

    std::vector<PeriodicRxPlatform::Window> windows = platform.GetWindowsBefore(kPeriodUs);

    ASSERT_EQ(windows.size(), 1u);
    EXPECT_EQ(windows[0].mChannel, 11);
    EXPECT_LT(windows[0].mStart, 5000u);
    EXPECT_EQ(windows[0].mStart + windows[0].mDuration, 10000u);
}

TEST(RadioSpinelPeriodicRx, shouldStartALowerPriorityWindowWhenAHigherPriorityWindowEnds)
{
    // Schedule 0 on channel 11 at [20000, 28000) every 100 ms preempts
    // schedule 1 on channel 15 at [16000, 32000) every 100 ms. The guard time
    // before schedule 0 is taken from schedule 1, and none is taken from
    // schedule 0 when the radio returns to schedule 1:
    //
    //   channel 15: [16000 - guard, 20000 - guard)
    //   channel 11: [20000 - guard, 28000)
    //   channel 15: [28000, 32000)
    constexpr uint32_t kPeriodUs = 100000;

    const PeriodicRxSchedule kSchedules[] = {{20000, kPeriodUs, 8000, 11}, {16000, kPeriodUs, 16000, 15}};

    PeriodicRxPlatform platform;

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ASSERT_EQ(platform.mRadioSpinel.SetPeriodicRxSchedules(kSchedules, 2), kErrorNone);
    platform.GoInUs(kPeriodUs);

    std::vector<PeriodicRxPlatform::Window> windows = platform.GetWindowsBefore(kPeriodUs);

    ASSERT_EQ(windows.size(), 3u);
    EXPECT_EQ(windows[0].mChannel, 15);
    EXPECT_EQ(windows[0].mStart, 16000u - kGuardUs);
    EXPECT_EQ(windows[0].mStart + windows[0].mDuration, 20000u - kGuardUs);
    EXPECT_EQ(windows[1].mChannel, 11);
    EXPECT_EQ(windows[1].mStart, 20000u - kGuardUs);
    EXPECT_EQ(windows[1].mStart + windows[1].mDuration, 28000u);
    EXPECT_EQ(windows[2].mChannel, 15);
    EXPECT_EQ(windows[2].mStart, 28000u);
    EXPECT_EQ(windows[2].mStart + windows[2].mDuration, 32000u);
}

TEST(RadioSpinelPeriodicRx, shouldNotTakeTheGuardFromAHigherPriorityWindow)
{
    // Schedule 0 on channel 11 at [5000, 6600) every 20 ms. Schedule 1 on
    // channel 12 at [6700, 8000) every 20 ms starts less than the guard time
    // after schedule 0 ends. Its guard is not taken from schedule 0, so it
    // starts when schedule 0 ends.
    constexpr uint32_t kPeriodUs = 20000;

    static_assert(6700 - 6600 < kGuardUs, "schedule 1 must start within the guard time after schedule 0");

    const PeriodicRxSchedule kSchedules[] = {{5000, kPeriodUs, 1600, 11}, {6700, kPeriodUs, 1300, 12}};

    PeriodicRxPlatform platform;

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ASSERT_EQ(platform.mRadioSpinel.SetPeriodicRxSchedules(kSchedules, 2), kErrorNone);
    platform.GoInUs(kPeriodUs);

    std::vector<PeriodicRxPlatform::Window> windows = platform.GetWindowsBefore(kPeriodUs);

    ASSERT_EQ(windows.size(), 2u);
    EXPECT_EQ(windows[0].mChannel, 11);
    EXPECT_EQ(windows[0].mStart, 5000u - kGuardUs);
    EXPECT_EQ(windows[0].mStart + windows[0].mDuration, 6600u);
    EXPECT_EQ(windows[1].mChannel, 12);
    EXPECT_EQ(windows[1].mStart, 6600u);
    EXPECT_EQ(windows[1].mStart + windows[1].mDuration, 8000u);
}

#else // OPENTHREAD_CONFIG_LINK_EXT_ENABLE && (OPENTHREAD_CONFIG_LINK_EXT_PERIODIC_RX_MAX_SCHEDULES > 0)

TEST(RadioSpinelPeriodicRx, shouldReportNotImplementedByAnRcpWithoutPeriodicRx)
{
    const Spinel::RadioSpinel::PeriodicRxSchedule kSchedules[] = {{0, 50000, 1600, 11}};

    FakeCoprocessorPlatform                 platform;
    Spinel::RadioSpinel::PeriodicRxSchedule schedules[3];
    uint8_t                                 numSchedules = 3;

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);

    EXPECT_EQ(platform.mRadioSpinel.GetPeriodicRxSchedules(schedules, numSchedules), kErrorNotImplemented);
    EXPECT_EQ(numSchedules, 0);
    EXPECT_EQ(platform.mRadioSpinel.SetPeriodicRxSchedules(kSchedules, 1), kErrorNotImplemented);
}

#endif // OPENTHREAD_CONFIG_LINK_EXT_ENABLE && (OPENTHREAD_CONFIG_LINK_EXT_PERIODIC_RX_MAX_SCHEDULES > 0)

#if OPENTHREAD_CONFIG_LINK_EXT_ENABLE

namespace {

otRadioFrame sLastMacFilterRxFrame;
bool         sMacFilterFrameReceived = false;

void HandleMacFilterReceiveDone(otInstance *, otRadioFrame *aFrame, otError aError)
{
    if (aError == OT_ERROR_NONE && aFrame != nullptr)
    {
        sMacFilterFrameReceived = true;
        sLastMacFilterRxFrame   = *aFrame;
    }
}

void PrepareUnsecuredFrame(Mac::TxFrame       &aTxFrame,
                           uint8_t            *aPsdu,
                           uint8_t            &aLength,
                           Mac::Frame::Version aVersion,
                           const Mac::Address &aSrcAddr,
                           const Mac::Address &aDstAddr,
                           Mac::PanId          aSrcPanId,
                           Mac::PanId          aDstPanId,
                           const uint8_t      *aPayload,
                           uint16_t            aPayloadLength)
{
    aTxFrame.mPsdu = aPsdu;

    Mac::TxFrame::BuildInfo buildInfo;
    buildInfo.mType               = Mac::Frame::kTypeData;
    buildInfo.mVersion            = aVersion;
    buildInfo.mAddrs.mSource      = aSrcAddr;
    buildInfo.mAddrs.mDestination = aDstAddr;
    buildInfo.mPanIds.SetSource(aSrcPanId);
    buildInfo.mPanIds.SetDestination(aDstPanId);
    buildInfo.mSecurityLevel = Mac::Frame::kSecurityNone;

    Mac::TxFrame::PayloadBuilder builder;
    aTxFrame.PrepareHeaders(buildInfo, builder);
    if (aPayload != nullptr && aPayloadLength > 0)
    {
        EXPECT_EQ(builder.AppendBytes(aPayload, aPayloadLength), kErrorNone);
    }
    aTxFrame.FinishPayload(builder);
    aLength = aTxFrame.GetLength();
}

void PrepareSecuredFrame(Mac::TxFrame             &aTxFrame,
                         uint8_t                  *aPsdu,
                         uint8_t                  &aLength,
                         Mac::Frame::Version       aVersion,
                         const Mac::Address       &aSrcAddr,
                         const Mac::Address       &aDstAddr,
                         Mac::PanId                aSrcPanId,
                         Mac::PanId                aDstPanId,
                         Mac::Frame::KeyIdMode     aKeyIdMode,
                         const uint8_t            *aKeyId,
                         uint32_t                  aFrameCounter,
                         const uint8_t            *aPayload,
                         uint16_t                  aPayloadLength,
                         const otMacKey           *aKey,
                         const Mac::ExtAddress    *aNonceExtAddr,
                         Mac::Frame::SecurityLevel aSecurityLevel = Mac::Frame::kSecurityEncMic32)
{
    aTxFrame.mPsdu = aPsdu;

    Mac::TxFrame::BuildInfo buildInfo;
    buildInfo.mType               = Mac::Frame::kTypeData;
    buildInfo.mVersion            = aVersion;
    buildInfo.mAddrs.mSource      = aSrcAddr;
    buildInfo.mAddrs.mDestination = aDstAddr;
    // `kPanIdBroadcast` omits the PAN ID.
    if (aSrcPanId != Mac::kPanIdBroadcast)
    {
        buildInfo.mPanIds.SetSource(aSrcPanId);
    }
    if (aDstPanId != Mac::kPanIdBroadcast)
    {
        buildInfo.mPanIds.SetDestination(aDstPanId);
    }
    buildInfo.mSecurityLevel = aSecurityLevel;
    buildInfo.mKeyIdMode     = aKeyIdMode;

    Mac::TxFrame::PayloadBuilder builder;
    aTxFrame.PrepareHeaders(buildInfo, builder);
    if (aPayload != nullptr && aPayloadLength > 0)
    {
        EXPECT_EQ(builder.AppendBytes(aPayload, aPayloadLength), kErrorNone);
    }
    aTxFrame.FinishPayload(builder);

    Mac::TxFrame::ParseInfo parseInfo;
    EXPECT_EQ(parseInfo.ParseFrom(aTxFrame, Mac::Frame::kParseFully), kErrorNone);
    parseInfo.WriteFrameCounter(aFrameCounter);

    if (aKeyIdMode == Mac::Frame::kKeyIdMode1)
    {
        parseInfo.WriteKeyIndex(aKeyId[0]);
    }
    else if (aKeyIdMode == Mac::Frame::kKeyIdMode2)
    {
        parseInfo.WriteKeySource(aKeyId);
        parseInfo.WriteKeyIndex(aKeyId[4]);
    }

    if (aKey != nullptr && aNonceExtAddr != nullptr)
    {
        Mac::KeyMaterial keyMaterial;

        keyMaterial.SetFrom(AsCoreType(aKey));
        aTxFrame.SetAesKey(keyMaterial);
        parseInfo.ProcessTransmitAesCcm(*aNonceExtAddr);
    }

    aLength = aTxFrame.GetLength();
}

} // namespace

TEST(RadioSpinelMacFilter, PolicySetAndGet)
{
    FakeCoprocessorPlatform platform;
    uint8_t                 policy = 0xff;

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);

    // Initial default policy is 0 (permissive)
    ASSERT_EQ(platform.mRadioSpinel.GetMacFilterPolicy(policy), kErrorNone);
    EXPECT_EQ(policy, 0);

    // Valid policy bit combinations
    EXPECT_EQ(platform.mRadioSpinel.SetMacFilterPolicy(SPINEL_MAC_FILTER_POLICY_DROP_MALFORMED), kErrorNone);
    ASSERT_EQ(platform.mRadioSpinel.GetMacFilterPolicy(policy), kErrorNone);
    EXPECT_EQ(policy, SPINEL_MAC_FILTER_POLICY_DROP_MALFORMED);

    EXPECT_EQ(platform.mRadioSpinel.SetMacFilterPolicy(SPINEL_MAC_FILTER_POLICY_DROP_MALFORMED |
                                                       SPINEL_MAC_FILTER_POLICY_DROP_UNSECURED),
              kErrorNone);
    ASSERT_EQ(platform.mRadioSpinel.GetMacFilterPolicy(policy), kErrorNone);
    EXPECT_EQ(policy, SPINEL_MAC_FILTER_POLICY_DROP_MALFORMED | SPINEL_MAC_FILTER_POLICY_DROP_UNSECURED);

#if (OPENTHREAD_CONFIG_LINK_EXT_MAC_KEY_TABLE_MAX_ENTRIES > 0)
    EXPECT_EQ(platform.mRadioSpinel.SetMacFilterPolicy(
                  SPINEL_MAC_FILTER_POLICY_DROP_MALFORMED | SPINEL_MAC_FILTER_POLICY_DROP_UNSECURED |
                  SPINEL_MAC_FILTER_POLICY_DROP_UNKNOWN_KEY | SPINEL_MAC_FILTER_POLICY_DROP_UNKNOWN_NEIGHBOR |
                  SPINEL_MAC_FILTER_POLICY_DROP_REPLAYED),
              kErrorNone);
    ASSERT_EQ(platform.mRadioSpinel.GetMacFilterPolicy(policy), kErrorNone);
    EXPECT_EQ(policy, 0x1f);
#else
    EXPECT_EQ(platform.mRadioSpinel.SetMacFilterPolicy(SPINEL_MAC_FILTER_POLICY_DROP_UNKNOWN_KEY),
              kErrorNotImplemented);
    EXPECT_EQ(platform.mRadioSpinel.SetMacFilterPolicy(SPINEL_MAC_FILTER_POLICY_DROP_UNKNOWN_NEIGHBOR),
              kErrorNotImplemented);
    EXPECT_EQ(platform.mRadioSpinel.SetMacFilterPolicy(SPINEL_MAC_FILTER_POLICY_DROP_REPLAYED), kErrorNotImplemented);
#endif

    // Reserved bits (bits 5-7) must be rejected
    EXPECT_EQ(platform.mRadioSpinel.SetMacFilterPolicy(0x20), kErrorInvalidArgs);
    EXPECT_EQ(platform.mRadioSpinel.SetMacFilterPolicy(0x40), kErrorInvalidArgs);
    EXPECT_EQ(platform.mRadioSpinel.SetMacFilterPolicy(0x80), kErrorInvalidArgs);
    EXPECT_EQ(platform.mRadioSpinel.SetMacFilterPolicy(0xe0), kErrorInvalidArgs);

    // Reset back to 0
    EXPECT_EQ(platform.mRadioSpinel.SetMacFilterPolicy(0), kErrorNone);
    ASSERT_EQ(platform.mRadioSpinel.GetMacFilterPolicy(policy), kErrorNone);
    EXPECT_EQ(policy, 0);
}

TEST(RadioSpinelMacFilter, KeyTableSetAndGet)
{
    FakeCoprocessorPlatform platform;

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);

#if (OPENTHREAD_CONFIG_LINK_EXT_MAC_KEY_TABLE_MAX_ENTRIES > 0)
    constexpr uint8_t                     kNumEntries = 3;
    Spinel::RadioSpinel::MacKeyTableEntry entries[10];
    Spinel::RadioSpinel::MacKeyTableEntry setEntries[kNumEntries];
    uint8_t                               count = OT_ARRAY_LENGTH(entries);

    // Initial table is empty
    ASSERT_EQ(platform.mRadioSpinel.GetMacKeyTable(entries, count), kErrorNone);
    EXPECT_EQ(count, 0);

    for (Spinel::RadioSpinel::MacKeyTableEntry &entry : setEntries)
    {
        entry.Clear();
        entry.mAllowedSecLevels = (1 << Mac::Frame::kSecurityEncMic32);
    }

    // Entry 0: Key ID Mode 2 (4-byte Key Source + 1-byte Key Index) with a key
    setEntries[0].mKeyIdLength = 5;
    for (uint8_t i = 0; i < 5; i++)
    {
        setEntries[0].mKeyId[i] = i + 0x01;
    }
    setEntries[0].mHasKey = true;
    memset(setEntries[0].mKeyMaterial.m8, 0x11, sizeof(otMacKey));

    // Entry 1: Key ID Mode 1 (1-byte Key Index) with all security levels allowed
    setEntries[1].mKeyIdLength      = 1;
    setEntries[1].mKeyId[0]         = 0x81;
    setEntries[1].mAllowedSecLevels = 0xfe;

    // Entry 2: Key ID Mode 0 (no Key ID)
    setEntries[2].mKeyIdLength = 0;

    ASSERT_EQ(platform.mRadioSpinel.SetMacKeyTable(setEntries, kNumEntries), kErrorNone);

    // Retrieve and verify
    count = OT_ARRAY_LENGTH(entries);
    ASSERT_EQ(platform.mRadioSpinel.GetMacKeyTable(entries, count), kErrorNone);
    ASSERT_EQ(count, kNumEntries);

    for (uint8_t i = 0; i < kNumEntries; i++)
    {
        EXPECT_EQ(entries[i].mKeyIdLength, setEntries[i].mKeyIdLength);
        EXPECT_EQ(memcmp(entries[i].mKeyId, setEntries[i].mKeyId, setEntries[i].mKeyIdLength), 0);
        EXPECT_EQ(entries[i].mHasKey, setEntries[i].mHasKey);
        if (setEntries[i].mHasKey)
        {
            EXPECT_EQ(memcmp(entries[i].mKeyMaterial.m8, setEntries[i].mKeyMaterial.m8, sizeof(otMacKey)), 0);
        }
        EXPECT_EQ(entries[i].mAllowedSecLevels, setEntries[i].mAllowedSecLevels);
    }

    // A `nullptr` table gets the number of entries
    count = 0;
    ASSERT_EQ(platform.mRadioSpinel.GetMacKeyTable(nullptr, count), kErrorNone);
    EXPECT_EQ(count, kNumEntries);

    // A too small table gets the entries which fit and the number of entries
    for (Spinel::RadioSpinel::MacKeyTableEntry &entry : entries)
    {
        entry.Clear();
    }

    count = kNumEntries - 1;
    ASSERT_EQ(platform.mRadioSpinel.GetMacKeyTable(entries, count), kErrorNoBufs);
    EXPECT_EQ(count, kNumEntries);

    for (uint8_t i = 0; i < kNumEntries - 1; i++)
    {
        EXPECT_EQ(entries[i].mKeyIdLength, setEntries[i].mKeyIdLength);
        EXPECT_EQ(entries[i].mAllowedSecLevels, setEntries[i].mAllowedSecLevels);
    }

    EXPECT_EQ(entries[kNumEntries - 1].mAllowedSecLevels, 0);
    // An entry with a key must allow only Security Levels 5, 6, and 7
    setEntries[0].mAllowedSecLevels = (1 << Mac::Frame::kSecurityEncMic32) | (1 << Mac::Frame::kSecurityEncMic64) |
                                      (1 << Mac::Frame::kSecurityEncMic128);
    EXPECT_EQ(platform.mRadioSpinel.SetMacKeyTable(setEntries, 1), kErrorNone);
    setEntries[0].mAllowedSecLevels = (1 << Mac::Frame::kSecurityEnc);
    EXPECT_EQ(platform.mRadioSpinel.SetMacKeyTable(setEntries, 1), kErrorNotCapable);
    setEntries[0].mAllowedSecLevels = (1 << Mac::Frame::kSecurityEncMic32) | (1 << Mac::Frame::kSecurityMic32);
    EXPECT_EQ(platform.mRadioSpinel.SetMacKeyTable(setEntries, 1), kErrorNotCapable);
    setEntries[0].mAllowedSecLevels = (1 << Mac::Frame::kSecurityEncMic32);

    // Clear key table
    ASSERT_EQ(platform.mRadioSpinel.SetMacKeyTable(nullptr, 0), kErrorNone);
    count = OT_ARRAY_LENGTH(entries);
    ASSERT_EQ(platform.mRadioSpinel.GetMacKeyTable(entries, count), kErrorNone);
    EXPECT_EQ(count, 0);

    // A failed SET below must leave this table unchanged
    ASSERT_EQ(platform.mRadioSpinel.SetMacKeyTable(setEntries, kNumEntries), kErrorNone);

    // Unsupported / invalid Key ID lengths (2, 4, 9) must be rejected
    setEntries[0].mKeyIdLength = 2;
    EXPECT_EQ(platform.mRadioSpinel.SetMacKeyTable(setEntries, 1), kErrorInvalidArgs);
    setEntries[0].mKeyIdLength = 4;
    EXPECT_EQ(platform.mRadioSpinel.SetMacKeyTable(setEntries, 1), kErrorInvalidArgs);
    setEntries[0].mKeyIdLength = 9; // Mode 3
    EXPECT_EQ(platform.mRadioSpinel.SetMacKeyTable(setEntries, 1), kErrorInvalidArgs);
    setEntries[0].mKeyIdLength = Spinel::RadioSpinel::MacKeyTableEntry::kMaxKeyIdLength + 1; // Rejected by host
    EXPECT_EQ(platform.mRadioSpinel.SetMacKeyTable(setEntries, 1), kErrorInvalidArgs);
    setEntries[0].mKeyIdLength = 5;

    // Bit 0 (Security Level 0: None) or 0 must return kErrorInvalidArgs
    setEntries[0].mAllowedSecLevels = (1 << Mac::Frame::kSecurityNone);
    EXPECT_EQ(platform.mRadioSpinel.SetMacKeyTable(setEntries, 1), kErrorInvalidArgs);
    setEntries[0].mAllowedSecLevels = (1 << Mac::Frame::kSecurityNone) | (1 << Mac::Frame::kSecurityEncMic32);
    EXPECT_EQ(platform.mRadioSpinel.SetMacKeyTable(setEntries, 1), kErrorInvalidArgs);
    setEntries[0].mAllowedSecLevels = 0;
    EXPECT_EQ(platform.mRadioSpinel.SetMacKeyTable(setEntries, 1), kErrorInvalidArgs);
    setEntries[0].mAllowedSecLevels = (1 << Mac::Frame::kSecurityEncMic32);

    // More entries than supported must be rejected
    Spinel::RadioSpinel::MacKeyTableEntry overflowEntries[OPENTHREAD_CONFIG_LINK_EXT_MAC_KEY_TABLE_MAX_ENTRIES + 1];

    for (Spinel::RadioSpinel::MacKeyTableEntry &entry : overflowEntries)
    {
        entry.Clear();
        entry.mKeyIdLength      = 5;
        entry.mAllowedSecLevels = (1 << Mac::Frame::kSecurityEncMic32);
    }
    EXPECT_EQ(platform.mRadioSpinel.SetMacKeyTable(overflowEntries, OT_ARRAY_LENGTH(overflowEntries)), kErrorNoBufs);

    // The failed SETs above left the table unchanged
    count = OT_ARRAY_LENGTH(entries);
    ASSERT_EQ(platform.mRadioSpinel.GetMacKeyTable(entries, count), kErrorNone);
    ASSERT_EQ(count, kNumEntries);

    for (uint8_t i = 0; i < kNumEntries; i++)
    {
        EXPECT_EQ(entries[i].mKeyIdLength, setEntries[i].mKeyIdLength);
        EXPECT_EQ(memcmp(entries[i].mKeyId, setEntries[i].mKeyId, setEntries[i].mKeyIdLength), 0);
        EXPECT_EQ(entries[i].mAllowedSecLevels, setEntries[i].mAllowedSecLevels);
    }
#else
    Spinel::RadioSpinel::MacKeyTableEntry entry;
    uint8_t                               count = 1;

    entry.Clear();
    EXPECT_EQ(platform.mRadioSpinel.SetMacKeyTable(&entry, 1), kErrorNotImplemented);
    EXPECT_EQ(platform.mRadioSpinel.GetMacKeyTable(&entry, count), kErrorNotImplemented);
#endif
}

TEST(RadioSpinelMacFilter, NeighborTableSetAndGet)
{
    FakeCoprocessorPlatform platform;

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);

#if (OPENTHREAD_CONFIG_LINK_EXT_MAC_KEY_TABLE_MAX_ENTRIES > 0)
    Spinel::RadioSpinel::MacKeyTableEntry      keys[3];
    Spinel::RadioSpinel::MacNeighborTableEntry setNeighbors[2];
    Spinel::RadioSpinel::MacNeighborTableEntry neighbors[OPENTHREAD_CONFIG_LINK_EXT_MAC_NEIGHBOR_TABLE_MAX_ENTRIES];
    uint8_t                                    count;

    // Key table: entry 0 without a key, entries 1 and 2 with keys
    for (uint8_t i = 0; i < OT_ARRAY_LENGTH(keys); i++)
    {
        keys[i].Clear();
        keys[i].mKeyIdLength      = 1;
        keys[i].mKeyId[0]         = 0x81 + i;
        keys[i].mAllowedSecLevels = (1 << Mac::Frame::kSecurityEncMic32);
        keys[i].mHasKey           = (i != 0);
        memset(keys[i].mKeyMaterial.m8, 0x10 + i, sizeof(otMacKey));
    }
    ASSERT_EQ(platform.mRadioSpinel.SetMacKeyTable(keys, OT_ARRAY_LENGTH(keys)), kErrorNone);

    // Initial table is empty
    count = OT_ARRAY_LENGTH(neighbors);
    ASSERT_EQ(platform.mRadioSpinel.GetMacNeighborTable(neighbors, count), kErrorNone);
    EXPECT_EQ(count, 0);

    for (uint8_t i = 0; i < OT_ARRAY_LENGTH(setNeighbors); i++)
    {
        setNeighbors[i].Clear();
        memset(setNeighbors[i].mExtAddress.m8, 0x20 + i, sizeof(otExtAddress));
    }
    setNeighbors[0].mKeyIndex     = 1;
    setNeighbors[0].mFrameCounter = 100;
    setNeighbors[0].mPanId        = 0xabcd;
    setNeighbors[0].mShortAddress = 0x1234;
    setNeighbors[1].mKeyIndex     = 2;
    setNeighbors[1].mFrameCounter = 5;
    setNeighbors[1].mPanId        = 0x4321;

    ASSERT_EQ(platform.mRadioSpinel.SetMacNeighborTable(setNeighbors, 2), kErrorNone);

    count = OT_ARRAY_LENGTH(neighbors);
    ASSERT_EQ(platform.mRadioSpinel.GetMacNeighborTable(neighbors, count), kErrorNone);
    ASSERT_EQ(count, 2);

    for (uint8_t i = 0; i < 2; i++)
    {
        EXPECT_EQ(memcmp(neighbors[i].mExtAddress.m8, setNeighbors[i].mExtAddress.m8, sizeof(otExtAddress)), 0);
        EXPECT_EQ(neighbors[i].mKeyIndex, setNeighbors[i].mKeyIndex);
        EXPECT_EQ(neighbors[i].mFrameCounter, setNeighbors[i].mFrameCounter);
        EXPECT_EQ(neighbors[i].mPanId, setNeighbors[i].mPanId);
        EXPECT_EQ(neighbors[i].mShortAddress, setNeighbors[i].mShortAddress);
    }

    // A `nullptr` table gets the number of entries
    count = 0;
    ASSERT_EQ(platform.mRadioSpinel.GetMacNeighborTable(nullptr, count), kErrorNone);
    EXPECT_EQ(count, 2);

    // A too small table gets the entries which fit and the number of entries
    neighbors[0].Clear();
    neighbors[1].Clear();
    count = 1;
    ASSERT_EQ(platform.mRadioSpinel.GetMacNeighborTable(neighbors, count), kErrorNoBufs);
    EXPECT_EQ(count, 2);
    EXPECT_EQ(memcmp(neighbors[0].mExtAddress.m8, setNeighbors[0].mExtAddress.m8, sizeof(otExtAddress)), 0);
    EXPECT_EQ(neighbors[0].mFrameCounter, setNeighbors[0].mFrameCounter);
    EXPECT_EQ(neighbors[1].mFrameCounter, 0u);

    // The same extended address and key index must not appear twice
    {
        Spinel::RadioSpinel::MacNeighborTableEntry duplicates[2] = {setNeighbors[0], setNeighbors[0]};

        EXPECT_EQ(platform.mRadioSpinel.SetMacNeighborTable(duplicates, 2), kErrorInvalidArgs);

        // The same extended address with a different key index is a different neighbor
        duplicates[1].mKeyIndex = 2;
        EXPECT_EQ(platform.mRadioSpinel.SetMacNeighborTable(duplicates, 2), kErrorNone);
        ASSERT_EQ(platform.mRadioSpinel.SetMacNeighborTable(setNeighbors, 2), kErrorNone);
    }

    // The frame counter of an existing neighbor never moves backwards
    setNeighbors[0].mFrameCounter = 50;
    ASSERT_EQ(platform.mRadioSpinel.SetMacNeighborTable(setNeighbors, 1), kErrorNone);
    count = OT_ARRAY_LENGTH(neighbors);
    ASSERT_EQ(platform.mRadioSpinel.GetMacNeighborTable(neighbors, count), kErrorNone);
    ASSERT_EQ(count, 1);
    EXPECT_EQ(neighbors[0].mFrameCounter, 100u);

    setNeighbors[0].mFrameCounter = 200;
    ASSERT_EQ(platform.mRadioSpinel.SetMacNeighborTable(setNeighbors, 1), kErrorNone);
    count = OT_ARRAY_LENGTH(neighbors);
    ASSERT_EQ(platform.mRadioSpinel.GetMacNeighborTable(neighbors, count), kErrorNone);
    ASSERT_EQ(count, 1);
    EXPECT_EQ(neighbors[0].mFrameCounter, 200u);

    // A key table entry without a key or out of range must be rejected
    setNeighbors[0].mKeyIndex = 0;
    EXPECT_EQ(platform.mRadioSpinel.SetMacNeighborTable(setNeighbors, 1), kErrorInvalidArgs);
    setNeighbors[0].mKeyIndex = 3;
    EXPECT_EQ(platform.mRadioSpinel.SetMacNeighborTable(setNeighbors, 1), kErrorInvalidArgs);
    setNeighbors[0].mKeyIndex = 1;

    // The short address 0xfffe (no short address assigned) is invalid
    {
        Spinel::RadioSpinel::MacNeighborTableEntry invalid = setNeighbors[0];

        invalid.mShortAddress = OT_RADIO_INVALID_SHORT_ADDR;
        EXPECT_EQ(platform.mRadioSpinel.SetMacNeighborTable(&invalid, 1), kErrorInvalidArgs);
    }

    // A null table with entries is rejected by the host
    EXPECT_EQ(platform.mRadioSpinel.SetMacNeighborTable(nullptr, 1), kErrorInvalidArgs);

    // Setting the key table with the same keys keeps the neighbors and their frame counters
    ASSERT_EQ(platform.mRadioSpinel.SetMacNeighborTable(setNeighbors, 2), kErrorNone);
    ASSERT_EQ(platform.mRadioSpinel.SetMacKeyTable(keys, OT_ARRAY_LENGTH(keys)), kErrorNone);
    count = OT_ARRAY_LENGTH(neighbors);
    ASSERT_EQ(platform.mRadioSpinel.GetMacNeighborTable(neighbors, count), kErrorNone);
    ASSERT_EQ(count, 2);
    EXPECT_EQ(neighbors[0].mKeyIndex, 1);
    EXPECT_EQ(neighbors[0].mFrameCounter, 200u);
    EXPECT_EQ(neighbors[1].mKeyIndex, 2);
    EXPECT_EQ(neighbors[1].mFrameCounter, setNeighbors[1].mFrameCounter);

    // Changing the key of an entry removes only the neighbors using that entry
    keys[2].mKeyMaterial.m8[0] ^= 0xff;
    ASSERT_EQ(platform.mRadioSpinel.SetMacKeyTable(keys, OT_ARRAY_LENGTH(keys)), kErrorNone);
    count = OT_ARRAY_LENGTH(neighbors);
    ASSERT_EQ(platform.mRadioSpinel.GetMacNeighborTable(neighbors, count), kErrorNone);
    ASSERT_EQ(count, 1);
    EXPECT_EQ(neighbors[0].mKeyIndex, 1);
    EXPECT_EQ(neighbors[0].mFrameCounter, 200u);
    keys[2].mKeyMaterial.m8[0] ^= 0xff;

    // Removing an entry removes the neighbors using it
    ASSERT_EQ(platform.mRadioSpinel.SetMacKeyTable(keys, 1), kErrorNone);
    count = OT_ARRAY_LENGTH(neighbors);
    ASSERT_EQ(platform.mRadioSpinel.GetMacNeighborTable(neighbors, count), kErrorNone);
    EXPECT_EQ(count, 0);

    // Failed SETs leave both tables unchanged
    ASSERT_EQ(platform.mRadioSpinel.SetMacKeyTable(keys, OT_ARRAY_LENGTH(keys)), kErrorNone);
    ASSERT_EQ(platform.mRadioSpinel.SetMacNeighborTable(setNeighbors, 2), kErrorNone);

    {
        Spinel::RadioSpinel::MacNeighborTableEntry duplicates[2] = {setNeighbors[0], setNeighbors[0]};
        Spinel::RadioSpinel::MacNeighborTableEntry badIndex[2]   = {setNeighbors[1], setNeighbors[0]};
        Spinel::RadioSpinel::MacKeyTableEntry      badKeys[3]    = {keys[0], keys[1], keys[2]};

        badIndex[1].mKeyIndex     = 0;
        badIndex[0].mFrameCounter = 1000;
        EXPECT_EQ(platform.mRadioSpinel.SetMacNeighborTable(duplicates, 2), kErrorInvalidArgs);
        EXPECT_EQ(platform.mRadioSpinel.SetMacNeighborTable(badIndex, 2), kErrorInvalidArgs);

        // The last entry is invalid
        badKeys[2].mAllowedSecLevels = 0;
        EXPECT_EQ(platform.mRadioSpinel.SetMacKeyTable(badKeys, 3), kErrorInvalidArgs);
        badKeys[2].mAllowedSecLevels = (1 << Mac::Frame::kSecurityEnc);
        EXPECT_EQ(platform.mRadioSpinel.SetMacKeyTable(badKeys, 3), kErrorNotCapable);
    }

    count = OT_ARRAY_LENGTH(neighbors);
    ASSERT_EQ(platform.mRadioSpinel.GetMacNeighborTable(neighbors, count), kErrorNone);
    ASSERT_EQ(count, 2);

    for (uint8_t i = 0; i < 2; i++)
    {
        EXPECT_EQ(memcmp(neighbors[i].mExtAddress.m8, setNeighbors[i].mExtAddress.m8, sizeof(otExtAddress)), 0);
        EXPECT_EQ(neighbors[i].mKeyIndex, setNeighbors[i].mKeyIndex);
        EXPECT_EQ(neighbors[i].mPanId, setNeighbors[i].mPanId);
        EXPECT_EQ(neighbors[i].mShortAddress, setNeighbors[i].mShortAddress);
    }

    EXPECT_EQ(neighbors[0].mFrameCounter, 200u);
    EXPECT_EQ(neighbors[1].mFrameCounter, setNeighbors[1].mFrameCounter);

    {
        Spinel::RadioSpinel::MacKeyTableEntry gotKeys[OT_ARRAY_LENGTH(keys)];

        count = OT_ARRAY_LENGTH(gotKeys);
        ASSERT_EQ(platform.mRadioSpinel.GetMacKeyTable(gotKeys, count), kErrorNone);
        ASSERT_EQ(count, OT_ARRAY_LENGTH(keys));

        for (uint8_t i = 0; i < OT_ARRAY_LENGTH(keys); i++)
        {
            EXPECT_EQ(gotKeys[i].mHasKey, keys[i].mHasKey);
            EXPECT_EQ(memcmp(gotKeys[i].mKeyMaterial.m8, keys[i].mKeyMaterial.m8, keys[i].GetKeyLength()), 0);
            EXPECT_EQ(gotKeys[i].mAllowedSecLevels, keys[i].mAllowedSecLevels);
        }
    }

    // More entries than supported must be rejected
    Spinel::RadioSpinel::MacNeighborTableEntry overflow[OPENTHREAD_CONFIG_LINK_EXT_MAC_NEIGHBOR_TABLE_MAX_ENTRIES + 1];

    for (uint8_t i = 0; i < OT_ARRAY_LENGTH(overflow); i++)
    {
        overflow[i]                   = setNeighbors[0];
        overflow[i].mExtAddress.m8[7] = i;
    }
    EXPECT_EQ(platform.mRadioSpinel.SetMacNeighborTable(overflow, OT_ARRAY_LENGTH(overflow)), kErrorNoBufs);
#else
    Spinel::RadioSpinel::MacNeighborTableEntry entry;
    uint8_t                                    count = 1;

    entry.Clear();
    EXPECT_EQ(platform.mRadioSpinel.SetMacNeighborTable(&entry, 1), kErrorNotImplemented);
    EXPECT_EQ(platform.mRadioSpinel.GetMacNeighborTable(&entry, count), kErrorNotImplemented);
#endif
}

TEST(RadioSpinelMacFilter, FrameFilteringPolicy)
{
    FakeCoprocessorPlatform platform;

    Spinel::RadioSpinelCallbacks callbacks{};
    callbacks.mReceiveDone    = HandleMacFilterReceiveDone;
    callbacks.mTransmitDone   = [](otInstance *, otRadioFrame *, otRadioFrame *, otError) {};
    callbacks.mEnergyScanDone = [](otInstance *, int8_t) {};
    callbacks.mTxStarted      = [](otInstance *, otRadioFrame *) {};
    platform.mRadioSpinel.SetCallbacks(callbacks);

    ASSERT_EQ(platform.mRadioSpinel.Enable(FakePlatform::CurrentInstance()), kErrorNone);
    ASSERT_EQ(platform.mRadioSpinel.Receive(11), kErrorNone);

    uint8_t sentFrame[OT_RADIO_FRAME_MAX_SIZE];
    uint8_t sentFrameLength = 0;

    auto deliverFrame = [&](const uint8_t *aPsdu, uint8_t aLength) {
        sMacFilterFrameReceived = false;
        memcpy(sentFrame, aPsdu, aLength);
        sentFrameLength = aLength;
        otRadioFrame rxFrame{};
        rxFrame.mPsdu               = const_cast<uint8_t *>(aPsdu);
        rxFrame.mLength             = aLength;
        rxFrame.mChannel            = 11;
        rxFrame.mInfo.mRxInfo.mRssi = -50;
        rxFrame.mInfo.mRxInfo.mLqi  = 100;
        otPlatRadioReceiveDone(FakePlatform::CurrentInstance(), &rxFrame, OT_ERROR_NONE);
        platform.GoInMs(10);
        platform.mSpinelDriver.Process(nullptr);
    };

    // A delivered frame must be passed to the host unmodified.
    auto expectDeliveredUnmodified = [&]() {
        EXPECT_TRUE(sMacFilterFrameReceived);
        if (sMacFilterFrameReceived)
        {
            EXPECT_EQ(sLastMacFilterRxFrame.mLength, sentFrameLength);
            EXPECT_EQ(memcmp(sLastMacFilterRxFrame.mPsdu, sentFrame, sentFrameLength), 0);
        }
    };

    // 1. Malformed frame check
    uint8_t malformedFrame[] = {0x01, 0x02}; // 2 bytes: invalid IEEE 802.15.4 frame

    // Policy = 0 (permissive): malformed frame is passed through to host
    ASSERT_EQ(platform.mRadioSpinel.SetMacFilterPolicy(0), kErrorNone);
    deliverFrame(malformedFrame, sizeof(malformedFrame));
    expectDeliveredUnmodified();

    // Policy = DROP_MALFORMED: malformed frame is dropped
    ASSERT_EQ(platform.mRadioSpinel.SetMacFilterPolicy(SPINEL_MAC_FILTER_POLICY_DROP_MALFORMED), kErrorNone);
    deliverFrame(malformedFrame, sizeof(malformedFrame));
    EXPECT_FALSE(sMacFilterFrameReceived);

    // 2. Unsecured frame check
    constexpr Mac::PanId   kPanId = 0x1234;
    constexpr otExtAddress kSrcExtAddr{0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
    constexpr otExtAddress kDstExtAddr{0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x00, 0x11};
    constexpr uint8_t      kPayload[] = "Hello OpenThread";
    Mac::Address           srcAddr;
    Mac::Address           dstAddr;
    Mac::ExtAddress        srcExt;
    Mac::ExtAddress        dstExt;

    srcExt.Set(kSrcExtAddr.m8);
    dstExt.Set(kDstExtAddr.m8);
    srcAddr.SetExtended(srcExt);
    dstAddr.SetExtended(dstExt);

    uint8_t      frameBuffer[OT_RADIO_FRAME_MAX_SIZE];
    uint8_t      frameLength = 0;
    Mac::TxFrame txFrame{};

    PrepareUnsecuredFrame(txFrame, frameBuffer, frameLength, Mac::Frame::kVersion2006, srcAddr, dstAddr, kPanId, kPanId,
                          kPayload, sizeof(kPayload));

    // Policy = DROP_MALFORMED: valid unsecured frame is passed
    deliverFrame(frameBuffer, frameLength);
    expectDeliveredUnmodified();

    // Policy = DROP_MALFORMED | DROP_UNSECURED: unsecured frame is dropped
    ASSERT_EQ(platform.mRadioSpinel.SetMacFilterPolicy(SPINEL_MAC_FILTER_POLICY_DROP_MALFORMED |
                                                       SPINEL_MAC_FILTER_POLICY_DROP_UNSECURED),
              kErrorNone);
    deliverFrame(frameBuffer, frameLength);
    EXPECT_FALSE(sMacFilterFrameReceived);

    const uint8_t kKeyIdMode2[5] = {0x01, 0x02, 0x03, 0x04, 0x02};
    otMacKey      testKey;

    memset(testKey.m8, 0x55, sizeof(testKey.m8));

    auto prepareSecuredFrame = [&](Mac::Frame::Version aVersion, const Mac::Address &aSrcAddr, Mac::PanId aSrcPanId,
                                   Mac::PanId aDstPanId, Mac::Frame::KeyIdMode aKeyIdMode, const uint8_t *aKeyId) {
        PrepareSecuredFrame(txFrame, frameBuffer, frameLength, aVersion, aSrcAddr, dstAddr, aSrcPanId, aDstPanId,
                            aKeyIdMode, aKeyId, 100, kPayload, sizeof(kPayload), &testKey, &srcExt);
    };

#if (OPENTHREAD_CONFIG_LINK_EXT_MAC_KEY_TABLE_MAX_ENTRIES > 0)
    const uint8_t kOtherKeyIdMode2[5] = {0x01, 0x02, 0x03, 0x04, 0x03};

    // 3. Key ID matching
    prepareSecuredFrame(Mac::Frame::kVersion2006, srcAddr, kPanId, kPanId, Mac::Frame::kKeyIdMode2, kKeyIdMode2);

    // Case 3a: Empty key table without DROP_UNKNOWN_KEY: secured frame is passed through
    deliverFrame(frameBuffer, frameLength);
    expectDeliveredUnmodified();

    // Case 3b: Empty key table with DROP_UNKNOWN_KEY: secured frame is dropped
    ASSERT_EQ(platform.mRadioSpinel.SetMacFilterPolicy(SPINEL_MAC_FILTER_POLICY_DROP_UNSECURED |
                                                       SPINEL_MAC_FILTER_POLICY_DROP_UNKNOWN_KEY),
              kErrorNone);
    deliverFrame(frameBuffer, frameLength);
    EXPECT_FALSE(sMacFilterFrameReceived);

    // Without DROP_MALFORMED, a malformed frame is delivered without further checks
    deliverFrame(malformedFrame, sizeof(malformedFrame));
    expectDeliveredUnmodified();
    prepareSecuredFrame(Mac::Frame::kVersion2006, srcAddr, kPanId, kPanId, Mac::Frame::kKeyIdMode2, kKeyIdMode2);

    // Case 3c: Matching Key ID: secured frame is passed through unmodified
    Spinel::RadioSpinel::MacKeyTableEntry keyEntry;
    keyEntry.Clear();
    keyEntry.mKeyIdLength = sizeof(kKeyIdMode2);
    memcpy(keyEntry.mKeyId, kKeyIdMode2, sizeof(kKeyIdMode2));
    keyEntry.mAllowedSecLevels = (1 << Mac::Frame::kSecurityEncMic32);
    ASSERT_EQ(platform.mRadioSpinel.SetMacKeyTable(&keyEntry, 1), kErrorNone);

    deliverFrame(frameBuffer, frameLength);
    expectDeliveredUnmodified();

    // Case 3d: Different Key Index: secured frame is dropped
    prepareSecuredFrame(Mac::Frame::kVersion2006, srcAddr, kPanId, kPanId, Mac::Frame::kKeyIdMode2, kOtherKeyIdMode2);
    deliverFrame(frameBuffer, frameLength);
    EXPECT_FALSE(sMacFilterFrameReceived);

    // Case 3e: Security level not allowed by the entry: secured frame is dropped
    keyEntry.mAllowedSecLevels = (1 << Mac::Frame::kSecurityEncMic64);
    ASSERT_EQ(platform.mRadioSpinel.SetMacKeyTable(&keyEntry, 1), kErrorNone);
    prepareSecuredFrame(Mac::Frame::kVersion2006, srcAddr, kPanId, kPanId, Mac::Frame::kKeyIdMode2, kKeyIdMode2);
    deliverFrame(frameBuffer, frameLength);
    EXPECT_FALSE(sMacFilterFrameReceived);
    keyEntry.mAllowedSecLevels = (1 << Mac::Frame::kSecurityEncMic32);

    // 4. Key ID Modes 0 and 1
    {
        const uint8_t                         kKeyIndex[1]      = {0x81};
        const uint8_t                         kOtherKeyIndex[1] = {0x82};
        Spinel::RadioSpinel::MacKeyTableEntry modeEntry;

        modeEntry.Clear();
        modeEntry.mAllowedSecLevels = (1 << Mac::Frame::kSecurityEncMic32);

        // Case 4a: Key ID Mode 1 entry matches frames with the same Key Index only
        modeEntry.mKeyIdLength = 1;
        modeEntry.mKeyId[0]    = kKeyIndex[0];
        ASSERT_EQ(platform.mRadioSpinel.SetMacKeyTable(&modeEntry, 1), kErrorNone);

        prepareSecuredFrame(Mac::Frame::kVersion2006, srcAddr, kPanId, kPanId, Mac::Frame::kKeyIdMode1, kKeyIndex);
        deliverFrame(frameBuffer, frameLength);
        expectDeliveredUnmodified();

        prepareSecuredFrame(Mac::Frame::kVersion2006, srcAddr, kPanId, kPanId, Mac::Frame::kKeyIdMode1, kOtherKeyIndex);
        deliverFrame(frameBuffer, frameLength);
        EXPECT_FALSE(sMacFilterFrameReceived);

        // Case 4b: A Key ID Mode 2 frame does not match a Key ID Mode 1 entry
        prepareSecuredFrame(Mac::Frame::kVersion2006, srcAddr, kPanId, kPanId, Mac::Frame::kKeyIdMode2, kKeyIdMode2);
        deliverFrame(frameBuffer, frameLength);
        EXPECT_FALSE(sMacFilterFrameReceived);

        // Case 4c: Key ID Mode 0 entry matches Key ID Mode 0 frames
        modeEntry.mKeyIdLength = 0;
        ASSERT_EQ(platform.mRadioSpinel.SetMacKeyTable(&modeEntry, 1), kErrorNone);

        prepareSecuredFrame(Mac::Frame::kVersion2006, srcAddr, kPanId, kPanId, Mac::Frame::kKeyIdMode0, nullptr);
        deliverFrame(frameBuffer, frameLength);
        expectDeliveredUnmodified();

        prepareSecuredFrame(Mac::Frame::kVersion2006, srcAddr, kPanId, kPanId, Mac::Frame::kKeyIdMode1, kKeyIndex);
        deliverFrame(frameBuffer, frameLength);
        EXPECT_FALSE(sMacFilterFrameReceived);

        // Case 4d: Key ID Mode 1 frames using the previous, current, or next key index set by
        // `SPINEL_PROP_RCP_MAC_KEY` are known without a key table entry.
        constexpr uint8_t kCurKeyIndex = 10;
        otMacKeyMaterial  macKey;
        uint8_t           keyIndex[1];

        memset(&macKey, 0, sizeof(macKey));
        memcpy(macKey.mKeyMaterial.mKey.m8, testKey.m8, sizeof(otMacKey));
        ASSERT_EQ(platform.mRadioSpinel.SetMacKeyTable(nullptr, 0), kErrorNone);
        ASSERT_EQ(platform.mRadioSpinel.SetMacKey(Mac::Frame::kKeyIdMode1, kCurKeyIndex, &macKey, &macKey, &macKey),
                  kErrorNone);

        for (uint8_t offset = 0; offset < 3; offset++)
        {
            keyIndex[0] = static_cast<uint8_t>(kCurKeyIndex - 1 + offset);
            prepareSecuredFrame(Mac::Frame::kVersion2006, srcAddr, kPanId, kPanId, Mac::Frame::kKeyIdMode1, keyIndex);
            deliverFrame(frameBuffer, frameLength);
            expectDeliveredUnmodified();
        }

        keyIndex[0] = kCurKeyIndex + 2;
        prepareSecuredFrame(Mac::Frame::kVersion2006, srcAddr, kPanId, kPanId, Mac::Frame::kKeyIdMode1, keyIndex);
        deliverFrame(frameBuffer, frameLength);
        EXPECT_FALSE(sMacFilterFrameReceived);

        // Case 4e: The MAC key index is known only for Security Level 5
        keyIndex[0] = kCurKeyIndex;
        PrepareSecuredFrame(txFrame, frameBuffer, frameLength, Mac::Frame::kVersion2006, srcAddr, dstAddr, kPanId,
                            kPanId, Mac::Frame::kKeyIdMode1, keyIndex, 100, kPayload, sizeof(kPayload), nullptr,
                            nullptr, Mac::Frame::kSecurityEnc);
        deliverFrame(frameBuffer, frameLength);
        EXPECT_FALSE(sMacFilterFrameReceived);

        // Case 4f: A key table entry with the Key ID of the frame takes precedence over the MAC key index,
        // e.g. to restrict the allowed security levels
        modeEntry.mKeyIdLength      = 1;
        modeEntry.mKeyId[0]         = kCurKeyIndex;
        modeEntry.mAllowedSecLevels = (1 << Mac::Frame::kSecurityEncMic64);
        ASSERT_EQ(platform.mRadioSpinel.SetMacKeyTable(&modeEntry, 1), kErrorNone);

        prepareSecuredFrame(Mac::Frame::kVersion2006, srcAddr, kPanId, kPanId, Mac::Frame::kKeyIdMode1, keyIndex);
        deliverFrame(frameBuffer, frameLength);
        EXPECT_FALSE(sMacFilterFrameReceived);

        PrepareSecuredFrame(txFrame, frameBuffer, frameLength, Mac::Frame::kVersion2006, srcAddr, dstAddr, kPanId,
                            kPanId, Mac::Frame::kKeyIdMode1, keyIndex, 100, kPayload, sizeof(kPayload), nullptr,
                            nullptr, Mac::Frame::kSecurityEncMic64);
        deliverFrame(frameBuffer, frameLength);
        expectDeliveredUnmodified();

        // The other MAC key indices are still known
        keyIndex[0] = kCurKeyIndex + 1;
        prepareSecuredFrame(Mac::Frame::kVersion2006, srcAddr, kPanId, kPanId, Mac::Frame::kKeyIdMode1, keyIndex);
        deliverFrame(frameBuffer, frameLength);
        expectDeliveredUnmodified();
    }

    // 5. MIC verification and frame counters
    {
        constexpr uint16_t                         kShortSrcAddr = 0x2468;
        otMacKey                                   otherKey;
        Mac::Address                               shortSrcAddr;
        Mac::Address                               otherShortSrcAddr;
        Mac::Address                               noSrcAddr;
        Mac::Address                               otherExtSrcAddr;
        Spinel::RadioSpinel::MacNeighborTableEntry neighbors[2];
        uint8_t                                    count;

        auto prepare = [&](const Mac::Address &aSrcAddr, uint32_t aFrameCounter, const otMacKey &aKey,
                           const Mac::ExtAddress &aNonceExtAddr) {
            PrepareSecuredFrame(txFrame, frameBuffer, frameLength, Mac::Frame::kVersion2006, aSrcAddr, dstAddr, kPanId,
                                kPanId, Mac::Frame::kKeyIdMode2, kKeyIdMode2, aFrameCounter, kPayload, sizeof(kPayload),
                                &aKey, &aNonceExtAddr);
        };

        memset(otherKey.m8, 0x66, sizeof(otherKey.m8));
        shortSrcAddr.SetShort(kShortSrcAddr);
        otherShortSrcAddr.SetShort(kShortSrcAddr + 1);
        noSrcAddr.SetNone();
        otherExtSrcAddr.SetExtended(dstExt);

        keyEntry.Clear();
        keyEntry.mKeyIdLength = sizeof(kKeyIdMode2);
        memcpy(keyEntry.mKeyId, kKeyIdMode2, sizeof(kKeyIdMode2));
        keyEntry.mAllowedSecLevels = (1 << Mac::Frame::kSecurityEncMic32);
        keyEntry.mHasKey           = true;
        memcpy(keyEntry.mKeyMaterial.m8, testKey.m8, sizeof(otMacKey));
        ASSERT_EQ(platform.mRadioSpinel.SetMacKeyTable(&keyEntry, 1), kErrorNone);
        ASSERT_EQ(platform.mRadioSpinel.SetMacFilterPolicy(SPINEL_MAC_FILTER_POLICY_DROP_UNKNOWN_KEY), kErrorNone);

        // Case 5a: No neighbor, extended source address, valid MIC: delivered unmodified
        prepare(srcAddr, 100, testKey, srcExt);
        deliverFrame(frameBuffer, frameLength);
        expectDeliveredUnmodified();

        // Case 5b: Invalid MIC: dropped
        frameBuffer[frameLength - 3] ^= 0xff;
        deliverFrame(frameBuffer, frameLength);
        EXPECT_FALSE(sMacFilterFrameReceived);

        // Case 5c: Same Key ID secured with a different key: dropped
        prepare(srcAddr, 100, otherKey, srcExt);
        deliverFrame(frameBuffer, frameLength);
        EXPECT_FALSE(sMacFilterFrameReceived);

        // Case 5d: No neighbor with DROP_UNKNOWN_NEIGHBOR: dropped
        ASSERT_EQ(platform.mRadioSpinel.SetMacFilterPolicy(SPINEL_MAC_FILTER_POLICY_DROP_UNKNOWN_KEY |
                                                           SPINEL_MAC_FILTER_POLICY_DROP_UNKNOWN_NEIGHBOR |
                                                           SPINEL_MAC_FILTER_POLICY_DROP_REPLAYED),
                  kErrorNone);
        prepare(srcAddr, 100, testKey, srcExt);
        deliverFrame(frameBuffer, frameLength);
        EXPECT_FALSE(sMacFilterFrameReceived);

        // Case 5e: Known neighbor: delivered, and the neighbor's frame counter is updated
        neighbors[0].Clear();
        memcpy(neighbors[0].mExtAddress.m8, srcExt.m8, sizeof(otExtAddress));
        neighbors[0].mKeyIndex     = 0;
        neighbors[0].mShortAddress = kShortSrcAddr;
        ASSERT_EQ(platform.mRadioSpinel.SetMacNeighborTable(neighbors, 1), kErrorNone);

        deliverFrame(frameBuffer, frameLength);
        expectDeliveredUnmodified();

        count = OT_ARRAY_LENGTH(neighbors);
        ASSERT_EQ(platform.mRadioSpinel.GetMacNeighborTable(neighbors, count), kErrorNone);
        ASSERT_EQ(count, 1);
        EXPECT_EQ(neighbors[0].mFrameCounter, 101u);

        // Case 5f: Replayed frame: dropped
        deliverFrame(frameBuffer, frameLength);
        EXPECT_FALSE(sMacFilterFrameReceived);

        // Case 5p: The frame counter 0xffffffff is never accepted, and the neighbor's frame counter is
        // left unchanged
        prepare(srcAddr, UINT32_MAX, testKey, srcExt);
        deliverFrame(frameBuffer, frameLength);
        EXPECT_FALSE(sMacFilterFrameReceived);

        count = OT_ARRAY_LENGTH(neighbors);
        ASSERT_EQ(platform.mRadioSpinel.GetMacNeighborTable(neighbors, count), kErrorNone);
        ASSERT_EQ(count, 1);
        EXPECT_EQ(neighbors[0].mFrameCounter, 101u);

        // Case 5g: Short source address of the neighbor: the neighbor's extended address is used for
        // the nonce.
        prepare(shortSrcAddr, 101, testKey, srcExt);
        deliverFrame(frameBuffer, frameLength);
        expectDeliveredUnmodified();

        // Case 5h: Unknown short source address: the MIC cannot be verified, so
        // DROP_UNKNOWN_NEIGHBOR applies.
        prepare(otherShortSrcAddr, 102, testKey, srcExt);
        deliverFrame(frameBuffer, frameLength);
        EXPECT_FALSE(sMacFilterFrameReceived);

        ASSERT_EQ(platform.mRadioSpinel.SetMacFilterPolicy(SPINEL_MAC_FILTER_POLICY_DROP_UNKNOWN_KEY |
                                                           SPINEL_MAC_FILTER_POLICY_DROP_REPLAYED),
                  kErrorNone);
        deliverFrame(frameBuffer, frameLength);
        expectDeliveredUnmodified();

        // Case 5i: No source address: verified by the neighbor
        prepare(noSrcAddr, 200, testKey, srcExt);
        deliverFrame(frameBuffer, frameLength);
        expectDeliveredUnmodified();

        // Case 5j: Two neighbors share the key with separate frame counters
        neighbors[1].Clear();
        memcpy(neighbors[1].mExtAddress.m8, dstExt.m8, sizeof(otExtAddress));
        neighbors[1].mKeyIndex = 0;
        ASSERT_EQ(platform.mRadioSpinel.SetMacNeighborTable(neighbors, 2), kErrorNone);

        prepare(otherExtSrcAddr, 50, testKey, dstExt);
        deliverFrame(frameBuffer, frameLength);
        expectDeliveredUnmodified();

        // Case 5k: No source address: the frame from the second neighbor is verified after the first
        // neighbor fails.
        prepare(noSrcAddr, 60, testKey, dstExt);
        deliverFrame(frameBuffer, frameLength);
        expectDeliveredUnmodified();

        deliverFrame(frameBuffer, frameLength);
        EXPECT_FALSE(sMacFilterFrameReceived);

        // Case 5l: Without DROP_REPLAYED, a replayed frame is delivered
        ASSERT_EQ(platform.mRadioSpinel.SetMacFilterPolicy(SPINEL_MAC_FILTER_POLICY_DROP_UNKNOWN_KEY), kErrorNone);
        deliverFrame(frameBuffer, frameLength);
        expectDeliveredUnmodified();

        // Case 5n: A neighbor with a PAN ID matches only frames with that source PAN ID
        ASSERT_EQ(platform.mRadioSpinel.SetMacFilterPolicy(SPINEL_MAC_FILTER_POLICY_DROP_UNKNOWN_KEY |
                                                           SPINEL_MAC_FILTER_POLICY_DROP_UNKNOWN_NEIGHBOR |
                                                           SPINEL_MAC_FILTER_POLICY_DROP_REPLAYED),
                  kErrorNone);
        neighbors[0].mPanId = kPanId;
        ASSERT_EQ(platform.mRadioSpinel.SetMacNeighborTable(neighbors, 1), kErrorNone);

        prepare(shortSrcAddr, 1000, testKey, srcExt);
        deliverFrame(frameBuffer, frameLength);
        expectDeliveredUnmodified();

        PrepareSecuredFrame(txFrame, frameBuffer, frameLength, Mac::Frame::kVersion2006, shortSrcAddr, dstAddr, 0x5678,
                            kPanId, Mac::Frame::kKeyIdMode2, kKeyIdMode2, 1001, kPayload, sizeof(kPayload), &testKey,
                            &srcExt);
        deliverFrame(frameBuffer, frameLength);
        EXPECT_FALSE(sMacFilterFrameReceived);

        // An extended source address matches the neighbor regardless of the PAN ID, so the frame
        // counter is checked and updated
        PrepareSecuredFrame(txFrame, frameBuffer, frameLength, Mac::Frame::kVersion2006, srcAddr, dstAddr, 0x5678,
                            kPanId, Mac::Frame::kKeyIdMode2, kKeyIdMode2, 1001, kPayload, sizeof(kPayload), &testKey,
                            &srcExt);
        deliverFrame(frameBuffer, frameLength);
        expectDeliveredUnmodified();

        deliverFrame(frameBuffer, frameLength);
        EXPECT_FALSE(sMacFilterFrameReceived); // Replayed

        // Case 5o: A frame whose source PAN ID is unknown (2015 frame without PAN IDs, such as a
        // wake frame) still matches the neighbor by its extended source address, so its frame
        // counter is checked.
        PrepareSecuredFrame(txFrame, frameBuffer, frameLength, Mac::Frame::kVersion2015, srcAddr, noSrcAddr,
                            Mac::kPanIdBroadcast, Mac::kPanIdBroadcast, Mac::Frame::kKeyIdMode2, kKeyIdMode2, 1002,
                            kPayload, sizeof(kPayload), &testKey, &srcExt);
        {
            Mac::TxFrame::ParseInfo txParseInfo;

            ASSERT_EQ(txParseInfo.ParseFrom(txFrame, Mac::Frame::kParseFully), kErrorNone);
            ASSERT_FALSE(txParseInfo.mPanIds.IsSourcePresent());
            ASSERT_FALSE(txParseInfo.mPanIds.IsDestinationPresent());
        }
        deliverFrame(frameBuffer, frameLength);
        expectDeliveredUnmodified();

        deliverFrame(frameBuffer, frameLength);
        EXPECT_FALSE(sMacFilterFrameReceived); // Replayed

        ASSERT_EQ(platform.mRadioSpinel.SetMacFilterPolicy(SPINEL_MAC_FILTER_POLICY_DROP_UNKNOWN_KEY), kErrorNone);

        // Case 5m: A matching entry without a key accepts the frame regardless of entries with a key
        Spinel::RadioSpinel::MacKeyTableEntry keys[2];

        keys[0] = keyEntry;
        memcpy(keys[0].mKeyMaterial.m8, otherKey.m8, sizeof(otMacKey));
        keys[1]         = keyEntry;
        keys[1].mHasKey = false;
        ASSERT_EQ(platform.mRadioSpinel.SetMacKeyTable(keys, 2), kErrorNone);
        prepare(srcAddr, 300, testKey, srcExt);
        deliverFrame(frameBuffer, frameLength);
        expectDeliveredUnmodified();

        // Case 5q: The MIC is verified at Security Levels 6 and 7 (8-byte and 16-byte MICs)
        keyEntry.mAllowedSecLevels = (1 << Mac::Frame::kSecurityEncMic32) | (1 << Mac::Frame::kSecurityEncMic64) |
                                     (1 << Mac::Frame::kSecurityEncMic128);
        ASSERT_EQ(platform.mRadioSpinel.SetMacKeyTable(&keyEntry, 1), kErrorNone);
        ASSERT_EQ(platform.mRadioSpinel.SetMacFilterPolicy(SPINEL_MAC_FILTER_POLICY_DROP_UNKNOWN_KEY), kErrorNone);

        for (Mac::Frame::SecurityLevel level : {Mac::Frame::kSecurityEncMic64, Mac::Frame::kSecurityEncMic128})
        {
            PrepareSecuredFrame(txFrame, frameBuffer, frameLength, Mac::Frame::kVersion2006, srcAddr, dstAddr, kPanId,
                                kPanId, Mac::Frame::kKeyIdMode2, kKeyIdMode2, 400, kPayload, sizeof(kPayload), &testKey,
                                &srcExt, level);
            deliverFrame(frameBuffer, frameLength);
            expectDeliveredUnmodified();

            // Invalid MIC: dropped
            frameBuffer[frameLength - 3] ^= 0xff;
            deliverFrame(frameBuffer, frameLength);
            EXPECT_FALSE(sMacFilterFrameReceived);

            // Different key: dropped
            PrepareSecuredFrame(txFrame, frameBuffer, frameLength, Mac::Frame::kVersion2006, srcAddr, dstAddr, kPanId,
                                kPanId, Mac::Frame::kKeyIdMode2, kKeyIdMode2, 400, kPayload, sizeof(kPayload),
                                &otherKey, &srcExt, level);
            deliverFrame(frameBuffer, frameLength);
            EXPECT_FALSE(sMacFilterFrameReceived);
        }

        // Case 5r: A matching entry without a key accepts the frame even if it follows an entry with a
        // key that would drop it (replayed, or frame counter 0xffffffff), so the order of the entries
        // does not matter
        keyEntry.mAllowedSecLevels = (1 << Mac::Frame::kSecurityEncMic32);
        keys[0]                    = keyEntry;
        keys[1]                    = keyEntry;
        keys[1].mHasKey            = false;
        ASSERT_EQ(platform.mRadioSpinel.SetMacKeyTable(keys, 2), kErrorNone);

        neighbors[0].Clear();
        memcpy(neighbors[0].mExtAddress.m8, srcExt.m8, sizeof(otExtAddress));
        neighbors[0].mKeyIndex     = 0;
        neighbors[0].mFrameCounter = 500;
        ASSERT_EQ(platform.mRadioSpinel.SetMacNeighborTable(neighbors, 1), kErrorNone);
        ASSERT_EQ(platform.mRadioSpinel.SetMacFilterPolicy(SPINEL_MAC_FILTER_POLICY_DROP_UNKNOWN_KEY |
                                                           SPINEL_MAC_FILTER_POLICY_DROP_UNKNOWN_NEIGHBOR |
                                                           SPINEL_MAC_FILTER_POLICY_DROP_REPLAYED),
                  kErrorNone);

        prepare(srcAddr, 400, testKey, srcExt);
        deliverFrame(frameBuffer, frameLength);
        expectDeliveredUnmodified();

        prepare(srcAddr, UINT32_MAX, testKey, srcExt);
        deliverFrame(frameBuffer, frameLength);
        expectDeliveredUnmodified();

        // Without the entry without a key, both frames are dropped
        ASSERT_EQ(platform.mRadioSpinel.SetMacKeyTable(keys, 1), kErrorNone);

        prepare(srcAddr, 400, testKey, srcExt);
        deliverFrame(frameBuffer, frameLength);
        EXPECT_FALSE(sMacFilterFrameReceived);

        prepare(srcAddr, UINT32_MAX, testKey, srcExt);
        deliverFrame(frameBuffer, frameLength);
        EXPECT_FALSE(sMacFilterFrameReceived);
    }
#else
    // When MAX_ENTRIES == 0, the key table is disabled.
    // Setting policies requiring the key table must fail.
    EXPECT_EQ(platform.mRadioSpinel.SetMacFilterPolicy(SPINEL_MAC_FILTER_POLICY_DROP_UNKNOWN_KEY),
              kErrorNotImplemented);

    // Secured frame is passed through unmodified
    prepareSecuredFrame(Mac::Frame::kVersion2006, srcAddr, kPanId, kPanId, Mac::Frame::kKeyIdMode2, kKeyIdMode2);
    ASSERT_EQ(platform.mRadioSpinel.SetMacFilterPolicy(SPINEL_MAC_FILTER_POLICY_DROP_MALFORMED), kErrorNone);
    deliverFrame(frameBuffer, frameLength);
    expectDeliveredUnmodified();
#endif
}

#endif // OPENTHREAD_CONFIG_LINK_EXT_ENABLE
