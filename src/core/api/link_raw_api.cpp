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
 *   This file implements the OpenThread Link Raw API.
 */

#include <openthread/config.h>

#include <openthread/platform/random.h>

#include "openthread-instance.h"
#include "common/debug.hpp"
#include "common/logging.hpp"
#include "mac/mac.hpp"

#if OPENTHREAD_RADIO || OPENTHREAD_ENABLE_RAW_LINK_API

otError otLinkRawSetEnable(otInstance *aInstance, bool aEnabled)
{
    otError error = OT_ERROR_NONE;

    otLogInfoPlat(aInstance, "LinkRaw Enabled=%d", aEnabled ? 1 : 0);
#if OPENTHREAD_RADIO

    if (aEnabled)
    {
        otPlatRadioEnable(aInstance);
    }
    else
    {
        otPlatRadioDisable(aInstance);
    }

#else
    VerifyOrExit(!aInstance->mThreadNetif.IsUp(), error = OT_ERROR_INVALID_STATE);
    aInstance->mLinkRaw.SetEnabled(aEnabled);

exit:
#endif
    return error;
}

bool otLinkRawIsEnabled(otInstance *aInstance)
{
#if OPENTHREAD_RADIO
    OT_UNUSED_VARIABLE(aInstance);
    return true;
#else
    return aInstance->mLinkRaw.IsEnabled();
#endif
}

otError otLinkRawSetPanId(otInstance *aInstance, uint16_t aPanId)
{
    otError error = OT_ERROR_NONE;

    VerifyOrExit(aInstance->mLinkRaw.IsEnabled(), error = OT_ERROR_INVALID_STATE);

    otPlatRadioSetPanId(aInstance, aPanId);
#if OPENTHREAD_RADIO
    aInstance->mLinkRaw.SetPanId(aPanId);
#endif

exit:
    return error;
}

otError otLinkRawSetExtendedAddress(otInstance *aInstance, const otExtAddress *aExtAddress)
{
    otError error = OT_ERROR_NONE;
    otExtAddress addr;

    VerifyOrExit(aInstance->mLinkRaw.IsEnabled(), error = OT_ERROR_INVALID_STATE);

    for (size_t i = 0; i < sizeof(addr); i++)
    {
        addr.m8[i] = aExtAddress->m8[7 - i];
    }

    otPlatRadioSetExtendedAddress(aInstance, &addr);
#if OPENTHREAD_RADIO
    aInstance->mLinkRaw.SetExtAddress(*aExtAddress);
#endif

exit:
    return error;
}

otError otLinkRawSetShortAddress(otInstance *aInstance, uint16_t aShortAddress)
{
    otError error = OT_ERROR_NONE;

    VerifyOrExit(aInstance->mLinkRaw.IsEnabled(), error = OT_ERROR_INVALID_STATE);

    otPlatRadioSetShortAddress(aInstance, aShortAddress);
#if OPENTHREAD_RADIO
    aInstance->mLinkRaw.SetShortAddress(aShortAddress);
#endif

exit:
    return error;
}

bool otLinkRawGetPromiscuous(otInstance *aInstance)
{
    return otPlatRadioGetPromiscuous(aInstance);
}

otError otLinkRawSetPromiscuous(otInstance *aInstance, bool aEnable)
{
    otError error = OT_ERROR_NONE;

    VerifyOrExit(aInstance->mLinkRaw.IsEnabled(), error = OT_ERROR_INVALID_STATE);

    otLogInfoPlat(aInstance, "LinkRaw Promiscuous=%d", aEnable ? 1 : 0);

    otPlatRadioSetPromiscuous(aInstance, aEnable);

exit:
    return error;
}

otError otLinkRawSleep(otInstance *aInstance)
{
    otError error = OT_ERROR_NONE;

    VerifyOrExit(aInstance->mLinkRaw.IsEnabled(), error = OT_ERROR_INVALID_STATE);

    otLogInfoPlat(aInstance, "LinkRaw Sleep");

    error = otPlatRadioSleep(aInstance);

exit:
    return error;
}

otError otLinkRawReceive(otInstance *aInstance, uint8_t aChannel, otLinkRawReceiveDone aCallback)
{
    otLogInfoPlat(aInstance, "LinkRaw Recv (Channel %d)", aChannel);
    return aInstance->mLinkRaw.Receive(aChannel, aCallback);
}

otRadioFrame *otLinkRawGetTransmitBuffer(otInstance *aInstance)
{
    otRadioFrame *buffer = NULL;

    VerifyOrExit(aInstance->mLinkRaw.IsEnabled());

    buffer = otPlatRadioGetTransmitBuffer(aInstance);

exit:
    return buffer;
}

otError otLinkRawTransmit(otInstance *aInstance, otRadioFrame *aFrame, otLinkRawTransmitDone aCallback)
{
    otLogInfoPlat(aInstance, "LinkRaw Transmit (%d bytes on channel %d)", aFrame->mLength, aFrame->mChannel);
    return aInstance->mLinkRaw.Transmit(aFrame, aCallback);
}

int8_t otLinkRawGetRssi(otInstance *aInstance)
{
    return otPlatRadioGetRssi(aInstance);
}

otRadioCaps otLinkRawGetCaps(otInstance *aInstance)
{
    return aInstance->mLinkRaw.GetCaps();
}

otError otLinkRawEnergyScan(otInstance *aInstance, uint8_t aScanChannel, uint16_t aScanDuration,
                            otLinkRawEnergyScanDone aCallback)
{
    return aInstance->mLinkRaw.EnergyScan(aScanChannel, aScanDuration, aCallback);
}

otError otLinkRawSrcMatchEnable(otInstance *aInstance, bool aEnable)
{
    otError error = OT_ERROR_NONE;

    VerifyOrExit(aInstance->mLinkRaw.IsEnabled(), error = OT_ERROR_INVALID_STATE);

    otPlatRadioEnableSrcMatch(aInstance, aEnable);

exit:
    return error;
}

otError otLinkRawSrcMatchAddShortEntry(otInstance *aInstance, const uint16_t aShortAddress)
{
    otError error = OT_ERROR_NONE;

    VerifyOrExit(aInstance->mLinkRaw.IsEnabled(), error = OT_ERROR_INVALID_STATE);

    error = otPlatRadioAddSrcMatchShortEntry(aInstance, aShortAddress);

exit:
    return error;
}

otError otLinkRawSrcMatchAddExtEntry(otInstance *aInstance, const otExtAddress *aExtAddress)
{
    otError error = OT_ERROR_NONE;
    otExtAddress addr;

    VerifyOrExit(aInstance->mLinkRaw.IsEnabled(), error = OT_ERROR_INVALID_STATE);

    for (uint8_t i = 0; i < sizeof(addr); i++)
    {
        addr.m8[i] = aExtAddress->m8[sizeof(addr) - 1 - i];
    }

    error = otPlatRadioAddSrcMatchExtEntry(aInstance, &addr);

exit:
    return error;
}

otError otLinkRawSrcMatchClearShortEntry(otInstance *aInstance, const uint16_t aShortAddress)
{
    otError error = OT_ERROR_NONE;

    VerifyOrExit(aInstance->mLinkRaw.IsEnabled(), error = OT_ERROR_INVALID_STATE);

    error = otPlatRadioClearSrcMatchShortEntry(aInstance, aShortAddress);

exit:
    return error;
}

otError otLinkRawSrcMatchClearExtEntry(otInstance *aInstance, const otExtAddress *aExtAddress)
{
    otError error = OT_ERROR_NONE;
    otExtAddress addr;

    VerifyOrExit(aInstance->mLinkRaw.IsEnabled(), error = OT_ERROR_INVALID_STATE);

    for (uint8_t i = 0; i < sizeof(addr); i++)
    {
        addr.m8[i] = aExtAddress->m8[sizeof(addr) - 1 - i];
    }

    error = otPlatRadioClearSrcMatchExtEntry(aInstance, &addr);

exit:
    return error;
}

otError otLinkRawSrcMatchClearShortEntries(otInstance *aInstance)
{
    otError error = OT_ERROR_NONE;

    VerifyOrExit(aInstance->mLinkRaw.IsEnabled(), error = OT_ERROR_INVALID_STATE);

    otPlatRadioClearSrcMatchShortEntries(aInstance);

exit:
    return error;
}

otError otLinkRawSrcMatchClearExtEntries(otInstance *aInstance)
{
    otError error = OT_ERROR_NONE;

    VerifyOrExit(aInstance->mLinkRaw.IsEnabled(), error = OT_ERROR_INVALID_STATE);

    otPlatRadioClearSrcMatchExtEntries(aInstance);

exit:
    return error;
}

#if OPENTHREAD_RADIO
int8_t otLinkRawGetMaxTransmitPower(otInstance *aInstance)
{
    return aInstance->mLinkRaw.GetMaxTxPower();
}

void otLinkRawSetMaxTransmitPower(otInstance *aInstance, int8_t aPower)
{
    otPlatRadioSetDefaultTxPower(aInstance, aPower);
    aInstance->mLinkRaw.SetMaxTxPower(aPower);
}

uint16_t otLinkRawGetShortAddress(otInstance *aInstance)
{
    return aInstance->mLinkRaw.GetShortAddress();
}

const otExtAddress *otLinkRawGetExtendedAddress(otInstance *aInstance)
{
    return &aInstance->mLinkRaw.GetExtAddress();
}

otPanId otLinkRawGetPanId(otInstance *aInstance)
{
    return aInstance->mLinkRaw.GetPanId();
}
#endif // OPENTHREAD_RADIO

namespace ot {

LinkRaw::LinkRaw(otInstance &aInstance):
    mInstance(aInstance),
#if !OPENTHREAD_RADIO
    mEnabled(false),
#endif
    mReceiveChannel(OPENTHREAD_CONFIG_DEFAULT_CHANNEL),
    mReceiveDoneCallback(NULL),
    mTransmitDoneCallback(NULL),
    mEnergyScanDoneCallback(NULL)
#if OPENTHREAD_LINKRAW_TIMER_REQUIRED
    , mTimer(aInstance, &LinkRaw::HandleTimer, this)
    , mTimerReason(kTimerReasonNone)
#if OPENTHREAD_CONFIG_ENABLE_PLATFORM_USEC_TIMER
    , mTimerMicro(aInstance, &LinkRaw::HandleTimer, this)
#endif
#endif // OPENTHREAD_LINKRAW_TIMER_REQUIRED
#if OPENTHREAD_CONFIG_ENABLE_SOFTWARE_ENERGY_SCAN
    , mEnergyScanTask(aInstance, &LinkRaw::HandleEnergyScanTask, this)
#endif // OPENTHREAD_CONFIG_ENABLE_SOFTWARE_ENERGY_SCAN
{
    // Query the capabilities to check asserts
    (void)GetCaps();
}

otRadioCaps LinkRaw::GetCaps()
{
    otRadioCaps RadioCaps = otPlatRadioGetCaps(&mInstance);

    // The radio shouldn't support a capability if it is being compile
    // time included into the raw link-layer code.

#if OPENTHREAD_CONFIG_ENABLE_SOFTWARE_ACK_TIMEOUT
    assert((RadioCaps & OT_RADIO_CAPS_ACK_TIMEOUT) == 0);
    RadioCaps = (otRadioCaps)(RadioCaps | OT_RADIO_CAPS_ACK_TIMEOUT);
#endif // OPENTHREAD_CONFIG_ENABLE_SOFTWARE_ACK_TIMEOUT

#if OPENTHREAD_CONFIG_ENABLE_SOFTWARE_RETRANSMIT
    assert((RadioCaps & OT_RADIO_CAPS_TRANSMIT_RETRIES) == 0);
    RadioCaps = (otRadioCaps)(RadioCaps | OT_RADIO_CAPS_TRANSMIT_RETRIES);
#endif // OPENTHREAD_CONFIG_ENABLE_SOFTWARE_RETRANSMIT

#if OPENTHREAD_CONFIG_ENABLE_SOFTWARE_ENERGY_SCAN
    assert((RadioCaps & OT_RADIO_CAPS_ENERGY_SCAN) == 0);
    RadioCaps = (otRadioCaps)(RadioCaps | OT_RADIO_CAPS_ENERGY_SCAN);
#endif // OPENTHREAD_CONFIG_ENABLE_SOFTWARE_ENERGY_SCAN

    return RadioCaps;
}

otError LinkRaw::Receive(uint8_t aChannel, otLinkRawReceiveDone aCallback)
{
    otError error = OT_ERROR_INVALID_STATE;

#if !OPENTHREAD_RADIO

    if (mEnabled)
#endif
    {
        mReceiveChannel = aChannel;
        mReceiveDoneCallback = aCallback;
        error = otPlatRadioReceive(&mInstance, aChannel);
    }

    return error;
}

void LinkRaw::InvokeReceiveDone(otRadioFrame *aFrame, otError aError)
{
    if (mReceiveDoneCallback)
    {
        if (aError == OT_ERROR_NONE)
        {
            otLogInfoPlat(&mInstance, "LinkRaw Invoke Receive Done (%d bytes)", aFrame->mLength);
            mReceiveDoneCallback(&mInstance, aFrame, aError);
        }
        else
        {
            otLogWarnPlat(&mInstance, "LinkRaw Invoke Receive Done (err=0x%x)", aError);
        }
    }
}

otError LinkRaw::Transmit(otRadioFrame *aFrame, otLinkRawTransmitDone aCallback)
{
    otError error = OT_ERROR_INVALID_STATE;

#if !OPENTHREAD_RADIO

    if (mEnabled)
#endif
    {
        mTransmitDoneCallback = aCallback;

#if OPENTHREAD_CONFIG_ENABLE_SOFTWARE_RETRANSMIT
        OT_UNUSED_VARIABLE(aFrame);
        mTransmitAttempts = 0;
        mCsmaAttempts = 0;

        // Start the transmission backlog logic
        StartCsmaBackoff();
        error = OT_ERROR_NONE;
#else
        // Let the hardware do the transmission logic
        error = otPlatRadioTransmit(&mInstance, aFrame);
#endif
    }

    return error;
}

void LinkRaw::InvokeTransmitDone(otRadioFrame *aFrame, otRadioFrame *aAckFrame, otError aError)
{
    otLogDebgPlat(&mInstance, "LinkRaw Transmit Done (err=0x%x)", aError);

#if OPENTHREAD_CONFIG_ENABLE_SOFTWARE_ACK_TIMEOUT
    mTimer.Stop();
#endif

#if OPENTHREAD_CONFIG_ENABLE_SOFTWARE_RETRANSMIT

    if (aError == OT_ERROR_CHANNEL_ACCESS_FAILURE)
    {
        if (mCsmaAttempts < Mac::kMaxCSMABackoffs)
        {
            mCsmaAttempts++;
            StartCsmaBackoff();
            goto exit;
        }
    }
    else
    {
        mCsmaAttempts = 0;
    }

    if (aError == OT_ERROR_NO_ACK)
    {
        if (mTransmitAttempts < aFrame->mMaxTxAttempts)
        {
            mTransmitAttempts++;
            StartCsmaBackoff();
            goto exit;
        }
    }

#endif

    // Transition back to receive state on previous channel
    otPlatRadioReceive(&mInstance, mReceiveChannel);

    if (mTransmitDoneCallback)
    {
        if (aError == OT_ERROR_NONE)
        {
            otLogInfoPlat(&mInstance, "LinkRaw Invoke Transmit Done");
        }
        else
        {
            otLogWarnPlat(&mInstance, "LinkRaw Invoke Transmit Failed (err=0x%x)", aError);
        }

        mTransmitDoneCallback(&mInstance, aFrame, aAckFrame, aError);
        mTransmitDoneCallback = NULL;
    }

#if OPENTHREAD_CONFIG_ENABLE_SOFTWARE_RETRANSMIT
exit:
    return;
#endif
}

otError LinkRaw::EnergyScan(uint8_t aScanChannel, uint16_t aScanDuration, otLinkRawEnergyScanDone aCallback)
{
    otError error = OT_ERROR_INVALID_STATE;

#if !OPENTHREAD_RADIO

    if (mEnabled)
#endif
    {
        mEnergyScanDoneCallback = aCallback;

#if OPENTHREAD_CONFIG_ENABLE_SOFTWARE_ENERGY_SCAN
        // Start listening on the scan channel
        otPlatRadioReceive(&mInstance, aScanChannel);

        // Reset the RSSI value and start scanning
        mEnergyScanRssi = kInvalidRssiValue;
        mTimerReason = kTimerReasonEnergyScanComplete;
        mTimer.Start(aScanDuration);
        mEnergyScanTask.Post();
#else
        // Do the HW offloaded energy scan
        error = otPlatRadioEnergyScan(&mInstance, aScanChannel, aScanDuration);
#endif
    }

    return error;
}

void LinkRaw::InvokeEnergyScanDone(int8_t aEnergyScanMaxRssi)
{
    if (mEnergyScanDoneCallback)
    {
        mEnergyScanDoneCallback(&mInstance, aEnergyScanMaxRssi);
        mEnergyScanDoneCallback = NULL;
    }
}

void LinkRaw::TransmitStarted(otRadioFrame *aFrame)
{
#if OPENTHREAD_CONFIG_ENABLE_SOFTWARE_ACK_TIMEOUT

    // If we are implementing the ACK timeout logic, start a timer here (if ACK request)
    // to fire if we don't get a transmit done callback in time.
    if (static_cast<Mac::Frame *>(aFrame)->GetAckRequest())
    {
        otLogDebgPlat(aInstance, "LinkRaw Starting AckTimeout Timer");
        mTimerReason = kTimerReasonAckTimeout;
        mTimer.Start(Mac::kAckTimeout);
    }

#else
    OT_UNUSED_VARIABLE(mInstance);
    OT_UNUSED_VARIABLE(aFrame);
#endif
}

#if OPENTHREAD_LINKRAW_TIMER_REQUIRED

void LinkRaw::HandleTimer(Timer &aTimer)
{
    GetOwner(aTimer).HandleTimer();
}

void LinkRaw::HandleTimer(void)
{
    TimerReason timerReason = mTimerReason;
    mTimerReason = kTimerReasonNone;

    switch (timerReason)
    {
#if OPENTHREAD_CONFIG_ENABLE_SOFTWARE_ACK_TIMEOUT

    case kTimerReasonAckTimeout:
    {
        // Transition back to receive state on previous channel
        otPlatRadioReceive(&mInstance, mReceiveChannel);

        // Invoke completion callback for transmit
        InvokeTransmitDone(otPlatRadioGetTransmitBuffer(&mInstance), NULL, OT_ERROR_NO_ACK);
        break;
    }

#endif // OPENTHREAD_CONFIG_ENABLE_SOFTWARE_ACK_TIMEOUT

#if OPENTHREAD_CONFIG_ENABLE_SOFTWARE_RETRANSMIT

    case kTimerReasonRetransmitTimeout:
    {
        otRadioFrame *aFrame = otPlatRadioGetTransmitBuffer(&mInstance);

        // Start the  transmit now
        otError error = otPlatRadioTransmit(&mInstance, aFrame);

        if (error != OT_ERROR_NONE)
        {
            InvokeTransmitDone(aFrame, NULL, error);
        }

        break;
    }

#endif // OPENTHREAD_CONFIG_ENABLE_SOFTWARE_RETRANSMIT

#if OPENTHREAD_CONFIG_ENABLE_SOFTWARE_ENERGY_SCAN

    case kTimerReasonEnergyScanComplete:
    {
        // Invoke completion callback for the energy scan
        InvokeEnergyScanDone(mEnergyScanRssi);
        break;
    }

#endif // OPENTHREAD_CONFIG_ENABLE_SOFTWARE_ENERGY_SCAN

    default:
        assert(false);
    }
}

#endif // OPENTHREAD_LINKRAW_TIMER_REQUIRED

#if OPENTHREAD_CONFIG_ENABLE_SOFTWARE_RETRANSMIT

void LinkRaw::StartCsmaBackoff(void)
{
    uint32_t backoffExponent = Mac::kMinBE + mTransmitAttempts + mCsmaAttempts;
    uint32_t backoff;

    if (backoffExponent > Mac::kMaxBE)
    {
        backoffExponent = Mac::kMaxBE;
    }

    backoff = (otPlatRandomGet() % (1UL << backoffExponent));
    backoff *= (static_cast<uint32_t>(Mac::kUnitBackoffPeriod) * OT_RADIO_SYMBOL_TIME);

    otLogDebgPlat(&mInstance, "LinkRaw Starting RetransmitTimeout Timer (%d ms)", backoff);
    mTimerReason = kTimerReasonRetransmitTimeout;

#if OPENTHREAD_CONFIG_ENABLE_PLATFORM_USEC_TIMER
    mTimerMicro.Start(backoff);
#else // OPENTHREAD_CONFIG_ENABLE_PLATFORM_USEC_TIMER
    mTimer.Start(backoff / 1000UL);
#endif // OPENTHREAD_CONFIG_ENABLE_PLATFORM_USEC_TIMER
}

#endif // OPENTHREAD_CONFIG_ENABLE_SOFTWARE_RETRANSMIT

#if OPENTHREAD_CONFIG_ENABLE_SOFTWARE_ENERGY_SCAN

void LinkRaw::HandleEnergyScanTask(Tasklet &aTasklet)
{
    GetOwner(aTasklet).HandleEnergyScanTask();
}

void LinkRaw::HandleEnergyScanTask(void)
{
    // Only process task if we are still energy scanning
    if (mTimerReason == kTimerReasonEnergyScanComplete)
    {
        int8_t rssi = otPlatRadioGetRssi(&mInstance);

        // Only apply the RSSI if it was a valid value
        if (rssi != kInvalidRssiValue)
        {
            if ((mEnergyScanRssi == kInvalidRssiValue) || (rssi > mEnergyScanRssi))
            {
                mEnergyScanRssi = rssi;
            }
        }

        // Post another instance of tha task, since we are
        // still doing the energy scan.
        mEnergyScanTask.Post();
    }
}

#endif // OPENTHREAD_CONFIG_ENABLE_SOFTWARE_ENERGY_SCAN

LinkRaw &LinkRaw::GetOwner(const Context &aContext)
{
#if OPENTHREAD_ENABLE_MULTIPLE_INSTANCES
    LinkRaw &link = *static_cast<LinkRaw *>(aContext.GetContext());
#else
    LinkRaw &link = otGetInstance()->mLinkRaw;
    OT_UNUSED_VARIABLE(aContext);
#endif
    return link;
}

} // namespace ot

#if OPENTHREAD_RADIO
extern "C" void otPlatRadioReceiveDone(otInstance *aInstance, otRadioFrame *aFrame, otError aError)
{
    aInstance->mLinkRaw.InvokeReceiveDone(aFrame, aError);
}

extern "C" void otPlatRadioTxDone(otInstance *aInstance, otRadioFrame *aFrame, otRadioFrame *aAckFrame,
                                  otError aError)
{
    aInstance->mLinkRaw.InvokeTransmitDone(aFrame, aAckFrame, aError);
}

extern "C" void otPlatRadioTxStarted(otInstance *aInstance, otRadioFrame *aFrame)
{
    aInstance->mLinkRaw.TransmitStarted(aFrame);
}

extern "C" otDeviceRole otThreadGetDeviceRole(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);
    return OT_DEVICE_ROLE_DISABLED;
}

extern "C" void otPlatRadioEnergyScanDone(otInstance *aInstance, int8_t aEnergyScanMaxRssi)
{
    OT_UNUSED_VARIABLE(aInstance);
    OT_UNUSED_VARIABLE(aEnergyScanMaxRssi);
}
#endif // OPENTHREAD_RADIO

#endif // OPENTHREAD_RADIO || OPENTHREAD_ENABLE_RAW_LINK_API
